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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDir>
#include <QSystemTrayIcon>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QBoxLayout;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QMenuBar;
class QResizeEvent;
class QSystemTrayIcon;
class QTimer;
class QToolBar;
QT_END_NAMESPACE

class ArchiveWindow;
class DcmDataset;
class Pipeline;
class SlidingStackedWidget;
class Sound;
class Worklist;
class PatientDataDialog;

class MainWindow : public QWidget
{
    Q_OBJECT

    // UI
    //
    PatientDataDialog *dlgPatient;
    QLabel*      extraTitle;
    QAction*     actionExit;
    QAction*     actionFullscreen;
    QAction*     actionStart;
    QAction*     actionRecordStart;
    QAction*     actionRecordStop;
    QAction*     actionSnapshot;
    QAction*     actionAbout;
    QAction*     actionSettings;
    QAction*     actionArchive;
    ArchiveWindow* archiveWindow;
    QBoxLayout*  layoutSources;
    QBoxLayout*  layoutVideo;
    QSystemTrayIcon* trayIcon;
#ifdef WITH_DICOM
    QAction*      actionWorklist;
    DcmDataset*   pendingPatient;
    Worklist*     worklist;
    QString       pendingSOPInstanceUID;
#endif
    SlidingStackedWidget* mainStack;
    QListWidget*  listImagesAndClips;
    QDir          outputPath;
    QDir          videoOutputPath;

    QString       accessionNumber;
    QString       patientId;
    QString       issuerOfPatientId;
    QString       patientSex;
    QString       patientName;
    QString       patientBirthDate;
    QString       physician;
    QString       studyName;

    ushort        imageNo;
    ushort        clipNo;
    int           studyNo;
    Sound*        sound;

    void      createTrayIcon();
    QMenuBar* createMenuBar();
    QToolBar* createToolBar();
    QAction*  addButton
        ( QToolBar* bar
        , const QString& icon
        , const QString& text
        , const char* handler
        );
    void updateStartButton();

    // State machine
    //
    bool running;

    // GStreamer pipelines
    //
    Pipeline*        activePipeline;
    QList<Pipeline*> pipelines;
    QSize            mainSrcSize;
    QSize            altSrcSize;

    QString replace(QString str, int seqNo = 0);
    void updateWindowTitle();
    QDir checkPath(const QString tpl, bool needUnique);
    void updateOutputPath();
    bool checkPipelines();
    void rebuildPipelines();
    void createPipeline(int index, int order);

    bool startVideoRecord();
    void updateStartDialog();
    bool confirmStopStudy();
    bool takeSnapshot
        ( Pipeline* pipeline = nullptr
        , const QString& imageTemplate = QString()
        );
    bool startRecord
        ( int duration = 0
        , Pipeline* pipeline = nullptr
        , const QString &clipFileTemplate = QString()
        );
    void doRecord
        ( int recordLimit
        , bool saveThumbnails
        , Pipeline* pipeline
        , const QString &actualTemplate
        );
    void stopRecord(Pipeline* pipeline = nullptr);
    Pipeline* findPipeline(const QString& alias);

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    virtual void closeEvent(QCloseEvent*);
    virtual void showEvent(QShowEvent*);
    virtual void hideEvent(QHideEvent*);
    virtual void resizeEvent(QResizeEvent*);
#ifdef Q_OS_WIN
    virtual bool nativeEvent(const QByteArray& eventType, void *msg, long *result);
#endif

signals:
    void enableAction(QAction*, bool);

public slots:
    bool applySettings();
    void toggleSetting();
    void playSound(const QString& file);
    void onClipRecordComplete();

private slots:
#ifdef WITH_DICOM
    void onShowWorkListClick();
    void onStartStudy(DcmDataset* patient = nullptr);
#else
    void onStartStudy();
#endif
    void onClipFrameReady();
    void onEnableAction(QAction* action, bool);
    void onImageSaved(const QString& filename, const QString& tooltip, const QPixmap& pm);
    void onPipelineError(const QString& text);
    void onPrepareSettingsMenu();
    void onRecordStartClick();
    void onRecordStopClick();
    void onShowAboutClick();
    void onShowArchiveClick();
    void onShowSettingsClick();
    void onSnapshotClick();
    void onSourceClick();
    void onSourceSnapshot();
    void onStartClick();
    void onStopStudy();
    void onSwapSources(QWidget *src, QWidget *dst);
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onActivateWindow();
    void onToggleFullscreen();

    friend class MainWindowDBusAdaptor;
};

#endif // MAINWINDOW_H
