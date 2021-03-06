/*
 * Copyright (C) 2013-2018 Softus Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mainwindow.h"
#include "product.h"
#include "aboutdialog.h"
#include "archivewindow.h"
#include "defaults.h"
#include "pipeline.h"
#include "patientdatadialog.h"
#include "qwaitcursor.h"
#include "settingsdialog.h"
#include "smartshortcut.h"
#include "sound.h"
#include "thumbnaillist.h"

#ifdef WITH_DICOM
#ifdef UNICODE
#define DCMTK_UNICODE_BUG_WORKAROUND
#undef UNICODE
#endif

#include "dicom/worklist.h"
#include "dicom/dcmclient.h"
#include "dicom/transcyrillic.h"

// From DCMTK SDK
//
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdict.h>
#include <dcmtk/dcmdata/dcdicent.h>
#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include "dcmtk/dcmdata/dctagkey.h"

static DcmTagKey DCM_ImageNo(0x5000, 0x8001);
static DcmTagKey DCM_ClipNo(0x5000,  0x8002);
#ifdef DCMTK_UNICODE_BUG_WORKAROUND
#define UNICODE
#undef DCMTK_UNICODE_BUG_WORKAROUND
#endif
#endif

#include "touch/slidingstackedwidget.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDesktopServices>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QResizeEvent>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QxtConfirmationMessage>

#define SAFE_MODE_KEYS (Qt::AltModifier | Qt::ControlModifier | Qt::ShiftModifier)
#ifdef Q_OS_WIN
  #include <initguid.h>
  #include <qt_windows.h>
  #include <dbt.h>
  #include <winternl.h> // for DEVICE_TYPE
  #include <ntddstor.h> // for GUID_DEVINTERFACE_PARTITION
  #include <qpa/qplatformnativeinterface.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , dlgPatient(nullptr)
    , archiveWindow(nullptr)
    , trayIcon(nullptr)
#ifdef WITH_DICOM
    , pendingPatient(nullptr)
    , worklist(nullptr)
#endif
    , imageNo(0)
    , clipNo(0)
    , studyNo(0)
    , running(false)
    , activePipeline(nullptr)
{
    QSettings settings;
    studyNo = settings.value("study-no").toInt();

    // This magic required for updating widgets from worker threads on Microsoft (R) Windows (TM)
    //
    connect(this, SIGNAL(enableAction(QAction*, bool)), this,
        SLOT(onEnableAction(QAction*, bool)), Qt::QueuedConnection);

    auto layoutMain = new QVBoxLayout();
    extraTitle = new QLabel;
    auto font = extraTitle->font();
    font.setPointSize(font.pointSize() * 3 / 2);
    extraTitle->setFont(font);
    layoutMain->addWidget(extraTitle);
    listImagesAndClips = new ThumbnailList();
    listImagesAndClips->setViewMode(QListView::IconMode);
    listImagesAndClips->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    listImagesAndClips->setMinimumHeight(144); // 576/4
    listImagesAndClips->setMaximumHeight(176);
    listImagesAndClips->setIconSize(QSize(144,144));
    listImagesAndClips->setMovement(QListView::Snap);

    mainStack = new SlidingStackedWidget();
    layoutMain->addWidget(mainStack);

    actionExit = new QAction(tr("E&xit"), this);
    actionExit->setMenuRole(QAction::QuitRole);
    connect(actionExit, SIGNAL(triggered()), qApp, SLOT(quit()));

    actionFullscreen = new QAction(tr("&Fullscreen"), this);
    connect(actionFullscreen, SIGNAL(triggered()), this, SLOT(onToggleFullscreen()));

    auto studyLayout = new QVBoxLayout;
    layoutVideo = new QHBoxLayout;
    layoutSources = new QVBoxLayout;
    layoutSources->addStretch();
    layoutVideo->addLayout(layoutSources);
    studyLayout->addLayout(layoutVideo);
    studyLayout->addWidget(listImagesAndClips);
    studyLayout->addWidget(createToolBar());
    auto studyWidget = new QWidget;
    studyWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    studyWidget->setLayout(studyLayout);
    studyWidget->setObjectName("Main");
    mainStack->addWidget(studyWidget);
    mainStack->setCurrentWidget(studyWidget);

    archiveWindow = new ArchiveWindow();
    archiveWindow->updateRoot();
    archiveWindow->setObjectName("Archive");
    mainStack->addWidget(archiveWindow);

    settings.beginGroup("ui");
    altSrcSize = settings.value("alt-src-size", DEFAULT_ALT_SRC_SIZE).toSize();
    mainSrcSize = settings.value("main-src-size", DEFAULT_MAIN_SRC_SIZE).toSize();
    if (settings.value("enable-menu").toBool())
    {
        layoutMain->setMenuBar(createMenuBar());
    }
    setLayout(layoutMain);

    restoreGeometry(settings.value("mainwindow-geometry").toByteArray());
    setWindowState(Qt::WindowState(settings.value("mainwindow-state").toInt()));
    settings.endGroup();

    updateStartButton();
    updateWindowTitle();

    sound = new Sound(this);

    outputPath.setPath(settings.value("storage/output-path", DEFAULT_OUTPUT_PATH).toString());

#ifdef WITH_DICOM
    DcmDataDictionary& d = dcmDataDict.wrlock();
    d.addEntry(new DcmDictEntry(DCM_ImageNo.getGroup(), DCM_ImageNo.getElement(), EVR_US, "ImageNo",
                                0, 0, nullptr, false, nullptr));
    d.addEntry(new DcmDictEntry(DCM_ClipNo.getGroup(), DCM_ClipNo.getElement(), EVR_US, "ClipNo",
                                0, 0, nullptr, false, nullptr));
#if OFFIS_DCMTK_VERSION_NUMBER >= 364
    dcmDataDict.wrunlock();
#else
    dcmDataDict.unlock();
#endif
#endif
}

MainWindow::~MainWindow()
{
    foreach (auto const& p, pipelines)
    {
        delete p;
    }
    pipelines.clear();
    activePipeline = nullptr;

    delete archiveWindow;
    archiveWindow = nullptr;

#ifdef WITH_DICOM
    delete worklist;
    worklist = nullptr;
#endif
}

void MainWindow::closeEvent(QCloseEvent *evt)
{
    // The user has triggered "Quit" in the menu bar or pressed the exit shortcut.
    //
    if (!evt->spontaneous() || !isVisible()) {
        return;
    }

    // Save keyboard modifiers state, since it may change after the confirm dialog will be shown.
    //
    auto fromMouse = qApp->keyboardModifiers() == Qt::NoModifier;

    // If a study is in progress, the user must confirm stoppping the app.
    //
    if (running && !confirmStopStudy())
    {
        evt->ignore();
        return;
    }

    // Don't trick the user, if she press the Alt+F4/Ctrl+Q.
    // Only for mouse clicks on close button.
    //
    if (fromMouse && QSettings().value("ui/hide-on-close").toBool())
    {
        evt->ignore();
        hide();

        return;
    }


    QWidget::closeEvent(evt);
}

void MainWindow::showEvent(QShowEvent *evt)
{
    if (!activePipeline || !activePipeline->pipeline)
    {
        QSettings settings;
        auto safeMode = settings.value("ui/enable-settings", DEFAULT_ENABLE_SETTINGS).toBool()
            && (qApp->queryKeyboardModifiers() == SAFE_MODE_KEYS
                || settings.value("safe-mode", true).toBool());

        if (safeMode)
        {
            settings.setValue("safe-mode", false);
            QTimer::singleShot(0, this, SLOT(onShowSettingsClick()));
        }
        else
        {
            actionStart->setEnabled(applySettings());
        }
#ifdef Q_OS_WIN
        // Archive window need this, but only the top level window may receive this message
        //
        DEV_BROADCAST_DEVICEINTERFACE dbd;
        ZeroMemory(&dbd, sizeof(DEV_BROADCAST_DEVICEINTERFACE));
        dbd.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
        dbd.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        dbd.dbcc_classguid = GUID_DEVINTERFACE_PARTITION;
        HWND hWnd  = (HWND)qApp->platformNativeInterface()->nativeResourceForWindow(
            QByteArrayLiteral("handle"), windowHandle());
        qDebug() << hWnd << RegisterDeviceNotification(hWnd, &dbd, DEVICE_NOTIFY_WINDOW_HANDLE);

        // Register notifications for all floppy drives and SD readers.
        TCHAR drvPath[] = {'\\','\\', '.', '\\', '0', ':', '\x0'}; // Full path: "\\.\X:"
        TCHAR drvLetter[] = {'0', ':', '\\', '\x0'}; // Drive name with slash: "X:\"

        DWORD drives = GetLogicalDrives();
        for (int i = 0; i < 26; ++i)
        {
            if (!(drives & (1 << i)))
                continue;

            drvLetter[0] = drvPath[4] = 'A' + i;

            if (GetDriveType(drvLetter) != DRIVE_REMOVABLE)
                continue;

            HANDLE hDir = CreateFile(drvPath, 0, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,
                NULL, OPEN_EXISTING, 0, NULL);

            if (INVALID_HANDLE_VALUE == hDir)
                continue;

            DEV_BROADCAST_HANDLE dbh;
            ZeroMemory (&dbh, sizeof (DEV_BROADCAST_HANDLE));
            dbh.dbch_size = sizeof (DEV_BROADCAST_HANDLE);
            dbh.dbch_devicetype = DBT_DEVTYP_HANDLE;
            dbh.dbch_handle = hDir;

            qDebug() << drvLetter << RegisterDeviceNotification(hWnd, &dbh,
                DEVICE_NOTIFY_WINDOW_HANDLE);
            CloseHandle(hDir);
        }
#endif
    }

    QWidget::showEvent(evt);
}

void MainWindow::hideEvent(QHideEvent *evt)
{
    QSettings settings;
    settings.beginGroup("ui");
    settings.setValue("mainwindow-geometry", saveGeometry());
    // Do not save window state, if it is in fullscreen mode or minimized
    //
    auto state = int(windowState() & ~(Qt::WindowMinimized | Qt::WindowFullScreen));
    settings.setValue("mainwindow-state", state);
    settings.endGroup();
    QWidget::hideEvent(evt);
}

void MainWindow::resizeEvent(QResizeEvent *evt)
{
    extraTitle->setVisible(isFullScreen());
    QWidget::resizeEvent(evt);
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray& eventType, void *msg, long *result)
{
    MSG* message = reinterpret_cast<MSG*>(msg);
    if (archiveWindow && message->message == WM_DEVICECHANGE)
    {
        archiveWindow->onUsbDiskChanged();
    }
    return QWidget::nativeEvent(eventType, msg, result);
}
#endif // Q_OS_WIN


QMenuBar* MainWindow::createMenuBar()
{
    auto menuBar = new QMenuBar();
    auto menuMain    = new QMenu(tr("&Menu"));

    menuMain->addAction(actionAbout);
    actionAbout->setMenuRole(QAction::AboutRole);

    menuMain->addAction(actionArchive);

#ifdef WITH_DICOM
    menuMain->addAction(actionWorklist);
#endif
    menuMain->addSeparator();

    auto actionFullVideo = menuMain->addAction(tr("Record &video log"), this, SLOT(toggleSetting()));
    actionFullVideo->setCheckable(true);
    actionFullVideo->setData("gst/enable-video");

    actionSettings->setMenuRole(QAction::PreferencesRole);
    menuMain->addAction(actionSettings);
    menuMain->addSeparator();
    menuMain->addAction(actionExit);

    connect(menuMain, SIGNAL(aboutToShow()), this, SLOT(onPrepareSettingsMenu()));
    menuBar->addMenu(menuMain);

    auto menuStudy    = new QMenu(tr("&Study"));
    menuStudy->addAction(actionStart);
    menuStudy->addAction(actionSnapshot);
    menuStudy->addAction(actionRecordStart);
    menuStudy->addAction(actionRecordStop);
    menuBar->addMenu(menuStudy);

    return menuBar;
}

QAction* MainWindow::addButton
    ( QToolBar* bar
    , const QString& icon
    , const QString& text
    , const char* handler
    )
{
    auto action = bar->addAction(QIcon(":/buttons/" + icon), text, this, handler);
    auto btn = static_cast<QToolButton*>(bar->widgetForAction(action));
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setMinimumWidth(178);

    return action;
}

QToolBar* MainWindow::createToolBar()
{
    QToolBar* bar = new QToolBar(tr("Main"));

    // The text of this action will be updated when the pipeline gets ready
    //
    actionStart       = addButton(bar, QString(),     QString(),           SLOT(onStartClick()));
    actionSnapshot    = addButton(bar, "camera",      tr("Take snapshot"), SLOT(onSnapshotClick()));
    actionRecordStart = addButton(bar, "record",      tr("Start clip"),    SLOT(onRecordStartClick()));
    actionRecordStop  = addButton(bar, "record_done", tr("Clip done"),     SLOT(onRecordStopClick()));

    QWidget* spacer = new QWidget;
    spacer->setMinimumWidth(1);
    spacer->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    bar->addWidget(spacer);

#ifdef WITH_DICOM
    actionWorklist = bar->addAction(QIcon(":/buttons/show_worklist"), tr("&Worlkist"),
                                    this, SLOT(onShowWorkListClick()));
#endif

    actionArchive = bar->addAction(QIcon(":/buttons/database"), tr("&Archive"), this,
        SLOT(onShowArchiveClick()));
    actionArchive->setToolTip(tr("Show studies archive"));

    actionSettings = bar->addAction(QIcon(":/buttons/settings"), tr("&Preferences").append(0x2026),
        this, SLOT(onShowSettingsClick()));
    actionSettings->setToolTip(tr("Edit settings"));

    actionAbout = bar->addAction(QIcon(":/buttons/about"),
        tr("A&bout %1").arg(PRODUCT_FULL_NAME).append(0x2026), this, SLOT(onShowAboutClick()));
    actionAbout->setToolTip(tr("About %1").arg(PRODUCT_FULL_NAME));

    return bar;
}

void MainWindow::createTrayIcon()
{
    trayIcon = new QSystemTrayIcon(qApp->windowIcon(), this);
    trayIcon->setToolTip(windowTitle());

    auto menuTray = new QMenu(PRODUCT_FULL_NAME);
    auto actionShow = menuTray->addAction(tr("Show %1").arg(PRODUCT_FULL_NAME), this,
        SLOT(onActivateWindow()));
    menuTray->setDefaultAction(actionShow);
    menuTray->addSeparator();
    menuTray->addAction(actionStart);
    menuTray->addAction(actionSnapshot);
    menuTray->addAction(actionRecordStart);
    menuTray->addAction(actionRecordStop);
    menuTray->addSeparator();
    menuTray->addAction(actionExit);
    trayIcon->setContextMenu(menuTray);
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
    trayIcon->show();
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason != QSystemTrayIcon::Context)
    {
        onActivateWindow();
    }
}

void MainWindow::onActivateWindow()
{
    if (!isMaximized() && !isFullScreen())
    {
        if (QSettings().value("ui/show-fullscreen").toBool())
        {
            showFullScreen();
        }
        else
        {
            showNormal();
        }
    }

    activateWindow();
}

void MainWindow::onToggleFullscreen()
{
    if (isFullScreen())
    {
        showNormal();
    }
    else
    {
        showFullScreen();
    }
}

// mpegpsmux > mpg, jpegenc > jpg, pngenc > png, oggmux > ogg, avimux > avi, matrosskamux > mat
//
static QString getExt(QString str)
{
    if (str.startsWith("ffmux_"))
    {
        str = str.mid(6);
    }
    return QString(".").append(str.remove('e').left(3));
}

static QString fixFileName(QString str)
{
    if (!str.isNull())
    {
        for (int i = 0; i < str.length(); ++i)
        {
            switch (str[i].unicode())
            {
            case '<':
            case '>':
            case ':':
            case '\"':
            case '/':
            case '\\':
            case '|':
            case '?':
            case '*':
                str[i] = '_';
                break;
            }
        }
    }
    return str;
}

QString MainWindow::replace(QString str, int seqNo)
{
    auto const& nn = seqNo >= 10? QString::number(seqNo): QString("0").append('0' + seqNo);
    auto const& ts = QDateTime::currentDateTime();

    return str
        .replace("%an%",        accessionNumber,     Qt::CaseInsensitive)
        .replace("%name%",      patientName,         Qt::CaseInsensitive)
        .replace("%id%",        patientId,           Qt::CaseInsensitive)
        .replace("%org%",       issuerOfPatientId,   Qt::CaseInsensitive)
        .replace("%sex%",       patientSex,          Qt::CaseInsensitive)
        .replace("%birthdate%", patientBirthDate,    Qt::CaseInsensitive)
        .replace("%physician%", physician,           Qt::CaseInsensitive)
        .replace("%study%",     studyName,           Qt::CaseInsensitive)
        .replace("%yyyy%",      ts.toString("yyyy"), Qt::CaseInsensitive)
        .replace("%yy%",        ts.toString("yy"),   Qt::CaseInsensitive)
        .replace("%mm%",        ts.toString("MM"),   Qt::CaseInsensitive)
        .replace("%mmm%",       ts.toString("MMM"),  Qt::CaseInsensitive)
        .replace("%mmmm%",      ts.toString("MMMM"), Qt::CaseInsensitive)
        .replace("%dd%",        ts.toString("dd"),   Qt::CaseInsensitive)
        .replace("%ddd%",       ts.toString("ddd"),  Qt::CaseInsensitive)
        .replace("%dddd%",      ts.toString("dddd"), Qt::CaseInsensitive)
        .replace("%hh%",        ts.toString("hh"),   Qt::CaseInsensitive)
        .replace("%min%",       ts.toString("mm"),   Qt::CaseInsensitive)
        .replace("%ss%",        ts.toString("ss"),   Qt::CaseInsensitive)
        .replace("%zzz%",       ts.toString("zzz"),  Qt::CaseInsensitive)
        .replace("%ap%",        ts.toString("ap"),   Qt::CaseInsensitive)
        .replace("%nn%",        nn,                  Qt::CaseInsensitive)
        ;
}

bool MainWindow::checkPipelines()
{
    auto nPipeline = 0;
    QSettings settings;
    settings.beginGroup("gst");
    auto nSources = settings.beginReadArray("src");

    for (int i = 0; i < nSources; ++i)
    {
        settings.setArrayIndex(i);
        if (!settings.value("enabled", true).toBool())
        {
            continue;
        }

        if (pipelines.size() <= nPipeline)
        {
            return false;
        }

        auto const& alias = settings.value("alias").toString();
        if (pipelines[nPipeline]->index != i || pipelines[nPipeline]->alias != alias)
        {
            return false;
        }
        ++nPipeline;
    }

    settings.endArray();
    settings.endGroup();

    return nPipeline == pipelines.size();
}

Pipeline* MainWindow::findPipeline(const QString& alias)
{
    if (alias.isEmpty())
    {
        return nullptr;
    }

    foreach (auto const& p, pipelines)
    {
        if (p->alias == alias)
        {
            return p;
        }
    }

    return nullptr;
}

void MainWindow::createPipeline(int index, int order)
{
    auto p = new Pipeline(index, this);
    connect(p, SIGNAL(imageSaved(const QString&, const QString&, const QPixmap&)), this,
        SLOT(onImageSaved(const QString&, const QString&, const QPixmap&)), Qt::QueuedConnection);
    connect(p, SIGNAL(clipFrameReady()), this, SLOT(onClipFrameReady()), Qt::QueuedConnection);
    connect(p, SIGNAL(clipRecordComplete()), this,
        SLOT(onClipRecordComplete()), Qt::QueuedConnection);
    connect(p, SIGNAL(pipelineError(const QString&)), this,
        SLOT(onPipelineError(const QString&)), Qt::QueuedConnection);
    connect(p, SIGNAL(playSound(QString)), this, SLOT(playSound(QString)), Qt::QueuedConnection);
    connect(p->displayWidget, SIGNAL(swapWith(QWidget*,QWidget*)), this,
        SLOT(onSwapSources(QWidget*,QWidget*)));
    connect(p->displayWidget, SIGNAL(click()), this, SLOT(onSourceClick()));
    connect(p->displayWidget, SIGNAL(copy()), this, SLOT(onSourceSnapshot()));
    pipelines.push_back(p);

    if (order < 0 && !activePipeline)
    {
        p->displayWidget->setMinimumSize(mainSrcSize);
        p->displayWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layoutVideo->insertWidget(0, p->displayWidget);
        activePipeline = p;
    }
    else
    {
        p->displayWidget->setMinimumSize(altSrcSize);
        p->displayWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        layoutSources->insertWidget(order < 0 ? layoutSources->count() : order,
            p->displayWidget, 0, Qt::AlignTop);
    }

    p->updatePipeline();
}

void MainWindow::rebuildPipelines()
{
    activePipeline = nullptr;

    foreach (auto const& p, pipelines)
    {
        delete p;
    }
    pipelines.clear();

    QSettings settings;
    settings.beginGroup("gst");
    auto nSources = settings.beginReadArray("src");

    if (nSources == 0)
    {
        createPipeline(-1, 0);
    }
    else
    {
        for (int i = 0; i < nSources; ++i)
        {
            settings.setArrayIndex(i);
            if (!settings.value("enabled", true).toBool())
            {
                continue;
            }

            auto order = settings.value("order", -1).toInt();
            createPipeline(i, order);
        }
    }
    settings.endArray();
    settings.endGroup();

    if (!activePipeline && !pipelines.isEmpty())
    {
        activePipeline = pipelines.front();
        activePipeline->displayWidget->setMinimumSize(mainSrcSize);
        activePipeline->displayWidget->setSizePolicy(QSizePolicy::Expanding,
            QSizePolicy::Expanding);
        layoutSources->removeWidget(activePipeline->displayWidget);
        layoutVideo->insertWidget(0, activePipeline->displayWidget);
    }
}

bool MainWindow::applySettings()
{
    QWaitCursor wait(this);

    SmartShortcut::reloadSettings();

    if (checkPipelines())
    {
        // No new pipelines, just reconfigure (if need) all existing
        //
        foreach (auto const& p, pipelines)
        {
            p->updatePipeline();
        }
    }
    else
    {
        rebuildPipelines();
    }

    QSettings settings;

    bool allPipelinesAreReady = false;
    foreach (auto const& p, pipelines)
    {
        allPipelinesAreReady = p->pipeline;
        if (!allPipelinesAreReady)
        {
            break;
        }
    }

    if (archiveWindow != nullptr)
    {
        archiveWindow->updateRoot();
        archiveWindow->updateHotkeys(settings);
    }

    auto showSettings = settings.value("ui/enable-settings", DEFAULT_ENABLE_SETTINGS).toBool();
    actionSettings->setEnabled(showSettings);
    actionSettings->setVisible(showSettings);

    settings.beginGroup("hotkeys");
    SmartShortcut::updateShortcut(actionStart,       settings.value("capture-start",
        DEFAULT_HOTKEY_START).toInt());
    SmartShortcut::updateShortcut(actionSnapshot,    settings.value("capture-snapshot",
        DEFAULT_HOTKEY_SNAPSHOT).toInt());
    SmartShortcut::updateShortcut(actionRecordStart, settings.value("capture-record-start",
        DEFAULT_HOTKEY_RECORD_START).toInt());
    SmartShortcut::updateShortcut(actionRecordStop,  settings.value("capture-record-stop",
        DEFAULT_HOTKEY_RECORD_STOP).toInt());
    SmartShortcut::updateShortcut(actionArchive,     settings.value("capture-archive",
        DEFAULT_HOTKEY_ARCHIVE).toInt());
    SmartShortcut::updateShortcut(actionSettings,    settings.value("capture-settings",
        DEFAULT_HOTKEY_SETTINGS).toInt());
    SmartShortcut::updateShortcut(actionAbout,       settings.value("capture-about",
        DEFAULT_HOTKEY_ABOUT).toInt());
    SmartShortcut::updateShortcut(actionExit,        settings.value("capture-exit",
        DEFAULT_HOTKEY_EXIT).toInt());
    SmartShortcut::updateShortcut(actionFullscreen,  settings.value("capture-fullscreen",
        DEFAULT_HOTKEY_FULLSCREEN).toInt());

#ifdef WITH_DICOM
    // Recreate worklist just in case the columns/servers were changed
    //
    delete worklist;
    worklist = new Worklist();
    connect(worklist, SIGNAL(startStudy(DcmDataset*)), this, SLOT(onStartStudy(DcmDataset*)));
    worklist->setObjectName("Worklist");
    mainStack->addWidget(worklist);
    SmartShortcut::updateShortcut(actionWorklist, settings.value("capture-worklist",
        DEFAULT_HOTKEY_WORKLIST).toInt());
#endif
    settings.endGroup();

#ifdef WITH_DICOM
    actionWorklist->setEnabled(!settings.value("dicom/mwl-server").toString().isEmpty());
#endif

    auto needTrayIcon = settings.value("ui/show-tray-icon").toBool();
    if (needTrayIcon && !trayIcon)
    {
        if (QSystemTrayIcon::isSystemTrayAvailable())
        {
            createTrayIcon();
        }
        else
        {
            qWarning() << "QSystemTrayIcon is not supported on this platform";
        }
    }
    else if (!needTrayIcon && trayIcon)
    {
        delete trayIcon;
        trayIcon = nullptr;
    }

    return activePipeline && allPipelinesAreReady;
}

void MainWindow::updateWindowTitle()
{
    QString windowTitle(PRODUCT_FULL_NAME);
    if (running)
    {
//      if (!accessionNumber.isEmpty())
//      {
//          windowTitle.append(tr(" - ")).append(accessionNumber);
//      }

        if (!patientId.isEmpty())
        {
            windowTitle.append(tr(" - ")).append(patientId);
        }

        if (!patientName.isEmpty())
        {
            windowTitle.append(tr(" - ")).append(patientName);
        }

        if (!physician.isEmpty())
        {
            windowTitle.append(tr(" - ")).append(physician);
        }

        if (!studyName.isEmpty())
        {
            windowTitle.append(tr(" - ")).append(studyName);
        }

        if (!issuerOfPatientId.isEmpty())
        {
            windowTitle.append(tr(" - ")).append(issuerOfPatientId);
        }
    }

    setWindowTitle(windowTitle);
    extraTitle->setText(windowTitle);
}

QDir MainWindow::checkPath(const QString tpl, bool needUnique)
{
    QDir dir(tpl);

    if (needUnique && dir.exists()
        && !dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty())
    {
        int cnt = 1;
        QString alt;
        do
        {
            alt = dir.absolutePath()
                .append(" (").append(QString::number(++cnt)).append(')');
        }
        while (dir.exists(alt)
            && !dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty());
        dir.setPath(alt);
    }
    qDebug() << "Output path" << dir.absolutePath();

    if (!dir.mkpath("."))
    {
        QString msg = tr("Failed to create '%1'").arg(dir.absolutePath());
        qCritical() << msg;
        QMessageBox::critical(this, windowTitle(), msg, QMessageBox::Ok);
    }

    return dir;
}

void MainWindow::updateOutputPath()
{
    QSettings settings;
    settings.beginGroup("storage");

    auto const& root = settings.value("output-path", DEFAULT_OUTPUT_PATH).toString();
    auto const& tpl = settings.value("folder-template", DEFAULT_FOLDER_TEMPLATE).toString();
    auto const& path = replace(root + tpl, studyNo);
    auto const& unique = settings.value("output-unique", DEFAULT_OUTPUT_UNIQUE).toBool();
    outputPath = checkPath(path, unique);

    auto videoRoot = settings.value("video-output-path").toString();
    if (videoRoot.isEmpty())
    {
        videoRoot = root;
    }
    auto videoTpl = settings.value("video-folder-template").toString();
    if (videoTpl.isEmpty())
    {
        videoTpl = tpl;
    }

    auto const& videoPath = replace(videoRoot + videoTpl, studyNo);

    // If video path is same as images path, omit checkPath,
    // since it is already checked.
    //
    videoOutputPath = (videoPath == path)? outputPath: checkPath(videoPath, unique);
}

void MainWindow::onClipFrameReady()
{
    enableAction(actionRecordStart, true);
    showNotification(tr("The clip is recording"));

    auto pipeline = static_cast<Pipeline*>(sender());
    if (!pipeline->clipPreviewFileName.isEmpty())
    {

        // Once an image will be ready, the valve will be turned off again.
        //
        enableAction(actionSnapshot, false);
    }

    enableAction(actionRecordStop, activePipeline->recording);
}

void MainWindow::onPipelineError(const QString& text)
{
    QMessageBox::critical(this, windowTitle(), text, QMessageBox::Ok);
    onStopStudy();
}

void MainWindow::onImageSaved
    ( const QString& filename
    , const QString &tooltip
    , const QPixmap& pixmap
    )
{
    QPixmap pm = pixmap.copy();

    auto baseName = QFileInfo(filename).completeBaseName();
    if (baseName.startsWith('.'))
    {
        baseName = QFileInfo(baseName.mid(1)).completeBaseName();
    }
    auto existent = listImagesAndClips->findItems(baseName, Qt::MatchExactly);
    auto item = !existent.isEmpty()
        ? existent.first() : new QListWidgetItem(baseName, listImagesAndClips);
    item->setToolTip(tooltip);
    item->setIcon(QIcon(pm));
    item->setSizeHint(QSize(176, 144));
    listImagesAndClips->setItemSelected(item, true);
    listImagesAndClips->scrollToItem(item);

    actionSnapshot->setEnabled(running);
    showNotification(tr("The snapshot is ready"));
}

bool MainWindow::startVideoRecord()
{
    if (!activePipeline || !activePipeline->pipeline)
    {
        // How we get here?
        //
        QMessageBox::critical(this, windowTitle(),
            tr("Failed to start recording.\nPlease, adjust the video source settings."),
            QMessageBox::Ok);
        return false;
    }

    QSettings settings;
    auto ok = true;
    if (settings.value("gst/enable-video").toBool())
    {
        auto split = settings.value("gst/split-video-files",
            DEFAULT_SPLIT_VIDEO_FILES).toBool();
        auto fileTemplate = settings.value("storage/video-template",
            DEFAULT_VIDEO_TEMPLATE).toString();

        foreach (auto const& p, pipelines)
        {
            auto videoFileName = p->appendVideoTail(videoOutputPath, "video",
                 replace(fileTemplate, studyNo), split);

            if (videoFileName.isEmpty())
            {
                QMessageBox::critical(this, windowTitle(),
                    tr("Failed to start recording.\nCheck the error log for details."),
                    QMessageBox::Ok);
                ok = false;
                break;
            }
            p->updateOverlayText();
        }

        if (ok)
        {
            foreach (auto const& p, pipelines)
            {
                p->enableVideo(true);
            }
        }
        else
        {
            foreach (auto const& p, pipelines)
            {
                p->removeVideoTail("video");
            }
        }
    }

    return ok;
}

void MainWindow::onStartClick()
{
    if (!running)
    {
        if (activePipeline)
        {
            onStartStudy();
        }
        else
        {
            QMessageBox::critical(this, windowTitle(),
                tr("Failed to start recording.\nPlease, adjust the video source settings."),
                QMessageBox::Ok);
        }
    }
    else
    {
        confirmStopStudy();
    }
}

bool MainWindow::confirmStopStudy()
{
    QxtConfirmationMessage msg(QMessageBox::Question, windowTitle()
        , tr("End the study?"), QString(), QMessageBox::Yes | QMessageBox::No, this);
    msg.setSettingsPath("confirmations");
    msg.setOverrideSettingsKey("end-study");
    msg.setRememberOnReject(false);
    msg.setDefaultButton(QMessageBox::Yes);
    if (qApp->queryKeyboardModifiers().testFlag(Qt::ShiftModifier))
    {
        msg.reset();
    }

    if (QMessageBox::Yes == msg.exec())
    {
        onStopStudy();

        // Open the archive window after end of study
        //
        QTimer::singleShot(100, this, SLOT(onShowArchiveClick()));
        return true;
    }

    return false;
}

void MainWindow::onSnapshotClick()
{
    takeSnapshot();
}

void MainWindow::onSourceSnapshot()
{
    foreach (auto const& p, pipelines)
    {
        if (p->displayWidget == sender())
        {
            takeSnapshot(p);
            break;
        }
    }
}

void MainWindow::onSourceClick()
{
    auto src = static_cast<QWidget*>(sender());
    if (activePipeline->displayWidget == src)
    {
        takeSnapshot(activePipeline);
    }
    else
    {
        onSwapSources(src, activePipeline->displayWidget);
    }
}

void MainWindow::playSound(const QString& file)
{
    sound->play(SOUND_FOLDER + file + ".ac3");
}

void MainWindow::showNotification(const QString& message)
{
    if (trayIcon && (isHidden() || isMinimized()))
    {
        QSettings settings;
        settings.beginGroup("ui");
        if (settings.value("show-tray-messages").toBool())
        {
            trayIcon->showMessage(windowTitle(), message, QSystemTrayIcon::Information,
                settings.value("tray-message-delay", DEFAULT_TRAY_MESSAGE_DELAY).toInt());
        }
        settings.endGroup();
    }
}

bool MainWindow::takeSnapshot(Pipeline* pipeline, const QString& imageTemplate)
{
    if (!running)
    {
        return false;
    }

    QSettings settings;
    auto actualImageTemplate = !imageTemplate.isEmpty()? imageTemplate:
            settings.value("storage/image-template", DEFAULT_IMAGE_TEMPLATE).toString();
    auto imageFileName = replace(actualImageTemplate, ++imageNo);

    playSound("shutter");

    settings.beginReadArray("gst/src");
    if (pipeline)
    {
        settings.setArrayIndex(pipeline->index);
        auto imageExt = getExt(settings.value("image-encoder", DEFAULT_IMAGE_ENCODER).toString());
        pipeline->takeSnapshot(outputPath.absoluteFilePath(imageFileName).append(imageExt));
    }
    else
    {
        // If no pipeline given, use all available
        //
        foreach (auto const& p, pipelines)
        {
            settings.setArrayIndex(p->index);
            if (p != activePipeline && settings.value("log-only").toBool())
            {
                // This source is for full video log only
                //
                continue;
            }
            auto imageExt = getExt(settings.value("image-encoder",
                DEFAULT_IMAGE_ENCODER).toString());
            p->takeSnapshot(outputPath.absoluteFilePath(imageFileName).append(imageExt));
        }
    }
    settings.endArray();

    // Once the image will be ready, the valve will be turned off again.
    //
    actionSnapshot->setEnabled(false);
    return true;
}

void MainWindow::onRecordStartClick()
{
    startRecord();
}

void MainWindow::onRecordStopClick()
{
    stopRecord();
}

bool MainWindow::startRecord
    ( int duration
    , Pipeline* pipeline
    , const QString &clipFileTemplate
    )
{
    if (!running)
    {
        return false;
    }

    QSettings settings;
    auto actualTemplate = !clipFileTemplate.isEmpty()? clipFileTemplate:
        settings.value("storage/clip-template", DEFAULT_CLIP_TEMPLATE).toString();
    actualTemplate = replace(actualTemplate, ++clipNo);

    settings.beginGroup("gst");
    auto saveThumbnails = settings.value("save-clip-thumbnails",
        DEFAULT_SAVE_CLIP_THUMBNAILS).toBool();

    auto recordLimit = duration > 0 ? duration:
        settings.value("clip-limit", DEFAULT_CLIP_LIMIT).toBool() ?
            settings.value("clip-countdown", DEFAULT_CLIP_COUNTDOWN).toInt() : 0;

    if (pipeline)
    {
        doRecord(recordLimit, saveThumbnails, pipeline, actualTemplate);
    }
    else
    {
        // If no pipeline given, use all available
        //
        settings.beginReadArray("src");
        foreach (auto const& p, pipelines)
        {
            settings.setArrayIndex(p->index);
            if (p != activePipeline && settings.value("log-only").toBool())
            {
                // This source is for full video log only
                //
                continue;
            }
            doRecord(recordLimit, saveThumbnails, p, actualTemplate);
        }
        settings.endArray();
    }

    playSound("record");
    return true;
}

void MainWindow::doRecord
    ( int recordLimit
    , bool saveThumbnails
    , Pipeline* pipeline
    , const QString &actualTemplate
    )
{
    pipeline->recordLimit = recordLimit;

    if (!pipeline->recording)
    {
        auto clipFileName = pipeline->appendVideoTail(outputPath, "clip", actualTemplate, false);
        qDebug() << clipFileName;

        if (!clipFileName.isEmpty())
        {
            if (!saveThumbnails)
            {
                auto item = new QListWidgetItem(QFileInfo(clipFileName).baseName(),
                    listImagesAndClips);
                item->setToolTip(clipFileName);
                item->setIcon(QIcon(":/buttons/movie"));
                listImagesAndClips->setItemSelected(item, true);
                listImagesAndClips->scrollToItem(item);
            }

            // Until the real clip recording starts, we should disable this button
            //
            actionRecordStart->setEnabled(false);
            pipeline->recording = true;
            pipeline->enableClip(true);
        }
        else
        {
            pipeline->removeVideoTail("clip");
            QMessageBox::critical(this, windowTitle(),
                tr("Failed to start recording.\nCheck the error log for details."),
                QMessageBox::Ok);
        }
    }
    else
    {
        // Extend recording time
        //
        pipeline->countdown = pipeline->recordLimit;
    }
}

void MainWindow::stopRecord(Pipeline* pipeline)
{
    if (pipeline)
    {
        pipeline->stopRecordingVideoClip();
    }
    else
    {
        // If no pipeline given, use all available
        //
        foreach (auto const& p, pipelines)
        {
            p->stopRecordingVideoClip();
        }
    }
}

void MainWindow::onClipRecordComplete()
{
    showNotification(tr("The clip is done"));
    actionRecordStop->setEnabled(running && activePipeline->recording);
}

void MainWindow::updateStartButton()
{
    QIcon icon(running? ":/buttons/stop": ":/buttons/start");
    QString strOnOff(running? tr("End study"): tr("Start study"));
    actionStart->setIcon(icon);
    auto shortcut = actionStart->shortcut(); // save the shortcut...
    actionStart->setText(strOnOff);
    actionStart->setShortcut(shortcut); // ...and restore

    actionRecordStart->setEnabled(running);
    actionRecordStop->setEnabled(running && activePipeline->recording);
    actionSnapshot->setEnabled(running);
    actionSettings->setDisabled(running);
#ifdef WITH_DICOM
    actionWorklist->setDisabled(running);
    if (worklist)
    {
        worklist->setDisabled(running);
    }
#endif
}

void MainWindow::onPrepareSettingsMenu()
{
    QSettings settings;

    auto menu = static_cast<QMenu*>(sender());
    foreach (auto const& action, menu->actions())
    {
        auto propName = action->data().toString();
        if (!propName.isEmpty())
        {
            action->setChecked(settings.value(propName).toBool());
            action->setDisabled(running);
        }
    }
}

void MainWindow::toggleSetting()
{
    QSettings settings;
    auto propName = static_cast<QAction*>(sender())->data().toString();
    bool enable = !settings.value(propName).toBool();
    settings.setValue(propName, enable);
    actionStart->setEnabled(applySettings());
}

void MainWindow::onShowAboutClick()
{
    AboutDialog(this).exec();
}

void MainWindow::onShowArchiveClick()
{
    archiveWindow->setPath(outputPath.absolutePath());
    mainStack->slideInWidget(archiveWindow);
}

void MainWindow::onShowSettingsClick()
{
    SettingsDialog dlg(QString(), this);
    connect(&dlg, SIGNAL(apply()), this, SLOT(applySettings()));

    // Some actions may be available from the system tray icon
    //
    auto isStartEnabled = actionStart->isEnabled();
    actionStart->setEnabled(false);
    if (dlg.exec())
    {
        isStartEnabled = applySettings();
    }

    actionStart->setEnabled(isStartEnabled);
}

void MainWindow::onEnableAction(QAction *action, bool enable)
{
    action->setEnabled(enable);
}

void MainWindow::updateStartDialog()
{
    if (!accessionNumber.isEmpty())
        dlgPatient->setAccessionNumber(accessionNumber);
    if (!patientId.isEmpty())
        dlgPatient->setPatientId(patientId);
    if (!issuerOfPatientId.isEmpty())
        dlgPatient->setIssuerOfPatientId(issuerOfPatientId);
    if (!patientName.isEmpty())
        dlgPatient->setPatientName(patientName);
    if (!patientSex.isEmpty())
        dlgPatient->setPatientSex(patientSex);
    if (!patientBirthDate.isEmpty())
        dlgPatient->setPatientBirthDateStr(patientBirthDate);
    if (!physician.isEmpty())
        dlgPatient->setPhysician(physician);
    if (!studyName.isEmpty())
        dlgPatient->setStudyDescription(studyName);
}

#ifdef WITH_DICOM
void MainWindow::onStartStudy(DcmDataset* patient)
#else
void MainWindow::onStartStudy()
#endif
{
    QWaitCursor wait(this);

    // Switch focus to the main window
    //
    onActivateWindow();

    mainStack->slideInWidget("Main");

    if (running)
    {
        QMessageBox::warning(this, windowTitle(),
            tr("Failed to start a study.\nAnother study is in progress."));
        return;
    }

    if (dlgPatient)
    {
        updateStartDialog();
        return;
    }

    QSettings settings;
    listImagesAndClips->clear();
    dlgPatient = new PatientDataDialog(false, "start-study", this);

#ifdef WITH_DICOM
    if (patient)
    {
        dlgPatient->readPatientData(patient);
    }
    else
#endif
    {
        updateStartDialog();
    }

    switch (dlgPatient->exec())
    {
#ifdef WITH_DICOM
    case SHOW_WORKLIST_RESULT:
        onShowWorkListClick();
        // falls through
#endif
    case QDialog::Rejected:
        delete dlgPatient;
        dlgPatient = nullptr;
        return;
    }

    accessionNumber  = fixFileName(dlgPatient->accessionNumber());
    patientId        = fixFileName(dlgPatient->patientId());
    issuerOfPatientId= fixFileName(dlgPatient->issuerOfPatientId());
    patientName      = fixFileName(dlgPatient->patientName());
    patientSex       = fixFileName(dlgPatient->patientSex());
    patientBirthDate = fixFileName(dlgPatient->patientBirthDateStr());
    physician        = fixFileName(dlgPatient->physician());
    studyName        = fixFileName(dlgPatient->studyDescription());

    settings.setValue("study-no", ++studyNo);
    updateOutputPath();
    if (archiveWindow)
    {
        archiveWindow->setPath(outputPath.absolutePath());
    }

    // After updateOutputPath the outputPath is usable
    //
#ifdef WITH_DICOM
    if (patient)
    {
        // Make a clone
        //
        pendingPatient = new DcmDataset(*patient);
    }
    else
    {
        pendingPatient = new DcmDataset();
    }

    dlgPatient->savePatientData(pendingPatient);

#if __BYTE_ORDER == __LITTLE_ENDIAN
    const E_TransferSyntax writeXfer = EXS_LittleEndianImplicit;
#elif __BYTE_ORDER == __BIG_ENDIAN
    const E_TransferSyntax writeXfer = EXS_BigEndianImplicit;
#else
#error "Unsupported byte order"
#endif

    imageNo = clipNo = 0;
    auto localPatientInfoFile = outputPath.absoluteFilePath(".patient.dcm");
    if (QFileInfo(localPatientInfoFile).exists())
    {
        DcmDataset ds;
        ds.loadFile(localPatientInfoFile.toLocal8Bit().constData());
        ds.findAndGetUint16(DCM_ImageNo, imageNo);
        ds.findAndGetUint16(DCM_ClipNo, clipNo);
    }

    auto cond = pendingPatient->saveFile(localPatientInfoFile.toLocal8Bit().constData(),
        writeXfer);
    if (cond.bad())
    {
        QMessageBox::critical(this, windowTitle(), QString::fromLocal8Bit(cond.text()));
    }
#ifdef Q_OS_WIN
    else
    {
        SetFileAttributesW(localPatientInfoFile.toStdWString().c_str(), FILE_ATTRIBUTE_HIDDEN);
    }
#endif

    settings.beginGroup("dicom");
    if (settings.value("start-with-mpps", true).toBool()
        && !settings.value("mpps-server").toString().isEmpty())
    {
        DcmClient client(UID_ModalityPerformedProcedureStepSOPClass);
        pendingSOPInstanceUID = client.nCreateRQ(pendingPatient);
        if (pendingSOPInstanceUID.isNull())
        {
            QMessageBox::critical(this, windowTitle(), client.lastError());
        }
        pendingPatient->putAndInsertString(DCM_SOPInstanceUID, pendingSOPInstanceUID.toUtf8());
    }
    settings.endGroup();
#else // WITH_DICOM
    auto localPatientInfoFile = outputPath.absoluteFilePath(".patient");
    QSettings patientData(localPatientInfoFile, QSettings::IniFormat);
    dlgPatient->savePatientData(patientData);
    imageNo = patientData.value("last-image-no", 0).toUInt();
    clipNo  = patientData.value("last-clip-no", 0).toUInt();

#ifdef Q_OS_WIN
    SetFileAttributesW(localPatientInfoFile.toStdWString().c_str(), FILE_ATTRIBUTE_HIDDEN);
#endif
#endif // WITH_DICOM

    running = startVideoRecord();
    activePipeline->enableEncoder(running);
    updateStartButton();
    updateWindowTitle();
    activePipeline->displayWidget->update();
    delete dlgPatient;
    dlgPatient = nullptr;

    if (settings.value("ui/minimize-on-start", true).toBool())
    {
        showMinimized();
    }

    showNotification(tr("The study is started."));
}

void MainWindow::onStopStudy()
{
    QSettings settings;
    QWaitCursor wait(this);

    onActivateWindow();

    showNotification(tr("The study is stopped."));

    onRecordStopClick();

    foreach (auto const& p, pipelines)
    {
        p->removeVideoTail("video");
    }

    running = false;

#ifdef WITH_DICOM
    if (pendingPatient)
    {
        char seriesUID[100] = {0};
        dcmGenerateUniqueIdentifier(seriesUID, SITE_SERIES_UID_ROOT);

        if (!pendingSOPInstanceUID.isEmpty()
            && settings.value("dicom/complete-with-mpps", true).toBool())
        {
            DcmClient client(UID_ModalityPerformedProcedureStepSOPClass);
            if (!client.nSetRQ(seriesUID, pendingPatient, pendingSOPInstanceUID))
            {
                QMessageBox::critical(this, windowTitle(), client.lastError());
            }
        }

#if __BYTE_ORDER == __LITTLE_ENDIAN
    const E_TransferSyntax writeXfer = EXS_LittleEndianImplicit;
#elif __BYTE_ORDER == __BIG_ENDIAN
    const E_TransferSyntax writeXfer = EXS_BigEndianImplicit;
#else
#error "Unsupported byte order"
#endif

        auto localPatientInfoFile = outputPath.absoluteFilePath(".patient.dcm");
        pendingPatient->putAndInsertUint16(DCM_ImageNo, imageNo);
        pendingPatient->putAndInsertUint16(DCM_ClipNo, clipNo);
        pendingPatient->saveFile(localPatientInfoFile.toLocal8Bit().constData(), writeXfer);

        delete pendingPatient;
        pendingPatient = nullptr;
    }

    pendingSOPInstanceUID.clear();
#else // WITH_DICOM
    QSettings patientData(outputPath.absoluteFilePath(".patient"), QSettings::IniFormat);
    patientData.setValue("last-image-no", imageNo);
    patientData.setValue("last-clip-no", clipNo);
#endif // WITH_DICOM

    accessionNumber.clear();
    patientId.clear();
    issuerOfPatientId.clear();
    patientName.clear();
    patientSex.clear();
    patientBirthDate.clear();
    if (settings.value("reset-physician").toBool())
    {
        physician.clear();
    }
    if (settings.value("reset-study-name").toBool())
    {
        studyName.clear();
    }

    updateStartButton();

    foreach (auto const& p, pipelines)
    {
        p->enableEncoder(false);
        p->displayWidget->update();
        p->updateOverlayText();
    }

    // Clear the capture list
    //
    listImagesAndClips->clear();
}

void MainWindow::onSwapSources(QWidget* src, QWidget* dst)
{
    // Swap with main video widget
    //
    if (dst == nullptr)
    {
        dst = activePipeline->displayWidget;
    }

    if (src == dst)
    {
        // Nothing to do.
        //
        return;
    }

    if (src == activePipeline->displayWidget)
    {
        src = dst;
        dst = activePipeline->displayWidget;
    }

    QSettings settings;
    settings.beginGroup("gst");
    if (dst == activePipeline->displayWidget)
    {
        // Swap alt src with main src
        //
        int idx = layoutSources->indexOf(src);

        layoutVideo->removeWidget(dst);
        layoutSources->removeWidget(src);

        src->setMinimumSize(mainSrcSize);
        src->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layoutVideo->insertWidget(0, src);

        dst->setMinimumSize(altSrcSize);
        dst->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        layoutSources->insertWidget(idx, dst, 0, Qt::AlignTop);
    }
    else
    {
        // Swap alt src with another alt src
        //
        int idxSrc = layoutSources->indexOf(src);
        int idxDst = layoutSources->indexOf(dst);

        layoutSources->removeWidget(src);
        layoutSources->insertWidget(idxDst, src, 0, Qt::AlignTop);
        layoutSources->removeWidget(dst);
        layoutSources->insertWidget(idxSrc, dst, 0, Qt::AlignTop);
    }

    settings.beginWriteArray("src");
    foreach (auto const& p, pipelines)
    {
        settings.setArrayIndex(p->index);
        auto idx = layoutSources->indexOf(p->displayWidget);
        settings.setValue("order", idx);
        if (idx < 0)
        {
            activePipeline = p;
            actionRecordStop->setEnabled(running && p->recording);
        }
    }
    settings.endArray();
    settings.endGroup();
}

#ifdef WITH_DICOM
void MainWindow::onShowWorkListClick()
{
    mainStack->slideInWidget(worklist);
}
#endif
