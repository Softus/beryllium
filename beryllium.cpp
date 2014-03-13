/*
 * Copyright (C) 2013-2014 Irkutsk Diagnostic Center.
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

#ifdef WITH_TOUCH
#include "touch/clickfilter.h"
#endif

#include "product.h"
#include "archivewindow.h"
#include "defaults.h"
#include "mainwindow.h"
#include "mainwindowdbusadaptor.h"
#include "videoeditor.h"
#include "settings.h"

#include <signal.h>

#include <QApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QIcon>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
#ifdef Q_WS_X11
#include <QX11Info>
#include <X11/Xlib.h>
#endif

#include <glib/gi18n.h>
#include <gst/gst.h>
#include <QGst/Init>

#ifdef WITH_DICOM
#define HAVE_CONFIG_H
#include <dcmtk/config/osconfig.h>   /* make sure OS specific configuration is included first */
#include <dcmtk/oflog/logger.h>
#include <dcmtk/oflog/fileap.h>
#include <dcmtk/oflog/configrt.h>
#endif

#ifdef Q_OS_WIN
#define DATA_FOLDER qApp->applicationDirPath()
#else
#define DATA_FOLDER qApp->applicationDirPath() + "/../share/" PRODUCT_SHORT_NAME
#endif

// The mpegps type finder is a bit broken,
// this code fixes it.
//
extern bool gstApplyFixes();

volatile sig_atomic_t fatal_error_in_progress = 0;
void sighandler(int signum)
{
    // Since this handler is established for more than one kind of signal,
    // it might still get invoked recursively by delivery of some other kind
    // of signal.  Use a static variable to keep track of that.
    //
    if (fatal_error_in_progress)
    {
        raise(signum);
    }

    fatal_error_in_progress = 1;
    QSettings().setValue("safe-mode", true);

    // Now call default signal handler which generates the core file.
    //
    signal(signum, SIG_DFL);
}

static gboolean
setValueCallback(const gchar *name, const gchar *value, gpointer, GError **err)
{
    if (0 == qstrcmp(name, "--safe-mode"))
    {
        value = "safe-mode=true";
    }

    auto idx = nullptr == value? nullptr: strchr(value, '=');
    if (nullptr == idx)
    {
        if (err)
        {
            *err = g_error_new(G_OPTION_ERROR,
                G_OPTION_ERROR_BAD_VALUE, N_("Bad argument '%s' (must be name=value)"), value);
        }
        return false;
    }

    QSettings().setValue(QString::fromLocal8Bit(value, idx - value), QString::fromLocal8Bit(idx + 1));
    return true;
}

static gboolean
xSyncCallback(const gchar *, const gchar *, gpointer, GError **)
{
#ifdef Q_WS_X11
    XSynchronize(QX11Info::display(), true);
#endif
    return true;
}

static gchar   windowType = '\x0';
static QString windowArg;

static gboolean
setModeCallback(const gchar *name, const gchar *value, gpointer, GError **)
{
    windowType = name[2]; // --settings => 's', --archive => 'a'
    windowArg = QString::fromLocal8Bit(value);
    return true;
}

static void setupGstDebug(const QSettings& settings)
{
    if (!settings.value("gst-debug-on", DEFAULT_GST_DEBUG_ON).toBool())
    {
        return;
    }

    gst_debug_set_active(true);
    auto debugDotDir = settings.value("gst-debug-dot-dir", DEFAULT_GST_DEBUG_DOT_DIR).toString();
    if (!debugDotDir.isEmpty())
    {
        QDir(debugDotDir).mkpath(".");
        qputenv("GST_DEBUG_DUMP_DOT_DIR", debugDotDir.toLocal8Bit());
    }
    auto debugLogFile = settings.value("gst-debug-log-file", DEFAULT_GST_DEBUG_LOG_FILE).toString();
    if (!debugLogFile.isEmpty())
    {
        QFileInfo(debugLogFile).absoluteDir().mkpath(".");
        qputenv("GST_DEBUG_FILE", debugLogFile.toLocal8Bit());
    }
    auto gstDebug = settings.value("gst-debug", DEFAULT_GST_DEBUG).toString();
    if (!gstDebug.isEmpty())
    {
        qputenv("GST_DEBUG", gstDebug.toLocal8Bit());
    }
    gst_debug_set_colored(!settings.value("gst-debug-no-color", DEFAULT_GST_DEBUG_NO_COLOR).toBool());
    auto gstDebugLevel = (GstDebugLevel)settings.value("gst-debug-level", DEFAULT_GST_DEBUG_LEVEL).toInt();
    gst_debug_set_default_threshold(gstDebugLevel);
}

#ifdef WITH_DICOM
static gboolean
dcmtkLogLevelCallback(const gchar *, const gchar *value, gpointer, GError **)
{
    auto level = log4cplus::getLogLevelManager().fromString(value);
    log4cplus::Logger::getRoot().setLogLevel(level);
    return true;
}

static gboolean
dcmtkLogFileCallback(const gchar *, const gchar *value, gpointer, GError **)
{
        log4cplus::SharedAppenderPtr file(new log4cplus::FileAppender(value));
        log4cplus::Logger::getRoot().addAppender(file);
        return true;
}

static gboolean
dcmtkLogConfigCallback(const gchar *, const gchar *value, gpointer, GError **)
{
    log4cplus::PropertyConfigurator(value).configure();
    return true;
}

// Pass some arguments to dcmtk.
// For example --dcmtk-log-level trace
// or --dcmtk-log-config log.cfg
// See http://support.dcmtk.org/docs-dcmrt/file_filelog.html for details
//
static GOptionEntry dcmtkOptions[] = {
    {"dcmtk-log-file", '\x0', 0, G_OPTION_ARG_CALLBACK, (gpointer)dcmtkLogFileCallback,
        N_("DCMTK log output file."), N_("FILE")},
    {"dcmtk-log-level", '\x0', 0, G_OPTION_ARG_CALLBACK, (gpointer)dcmtkLogLevelCallback,
        N_("DCMTK logging level: fatal, error, warn, info, debug, trace."), N_("LEVEL")},
    {"dcmtk-log-config", '\x0', 0, G_OPTION_ARG_CALLBACK, (gpointer)dcmtkLogConfigCallback,
        N_("Config file for DCMTK logger."), N_("FILE")},
    {nullptr, '\x0', 0, G_OPTION_ARG_NONE, nullptr,
        nullptr, nullptr},
};

static void setupDcmtkDebug(const QSettings& settings)
{
    if (!settings.value("dcmtk-debug-on", DEFAULT_DCMTK_DEBUG_ON).toBool())
    {
        return;
    }

    for (auto o = dcmtkOptions; o->long_name; ++o)
    {
        auto value = settings.value(o->long_name).toString();
        if (!value.isEmpty())
        {
            ((GOptionArgFunc)o->arg_data)(o->long_name, value.toLocal8Bit().constData(), nullptr, nullptr);
        }
    }
}
#endif

static gchar *accessionNumber = nullptr;
static gchar *patientBirthdate = nullptr;
static gchar *patientId = nullptr;
static gchar *patientName = nullptr;
static gchar *patientSex = nullptr;
static gchar *physician = nullptr;
static gchar *studyDescription = nullptr;
static gboolean autoStart = false;

static GOptionEntry options[] = {
    {"archive", '\x0', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, (gpointer)setModeCallback,
        N_("Show the archive window."), N_("PATH")},
    {"edit-video", '\x0', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, (gpointer)setModeCallback,
        N_("Show the video editor window."), N_("FILE")},
    {"settings", '\x0', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, (gpointer)setModeCallback,
        N_("Show the settings window."), N_("PAGE")},
    {"safe-mode", '\x0', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, (gpointer)setValueCallback,
        N_("Run the program in safe mode."), nullptr},
    {"sync", '\x0', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, (gpointer)xSyncCallback,
        N_("Run the program in X synchronous mode."), nullptr},

    {"study-id", 'a', 0, G_OPTION_ARG_STRING, (gpointer)&accessionNumber,
        N_("Study accession number (id)."), "ID"},
    {"patient-birthdate", 'b', 0, G_OPTION_ARG_STRING, (gpointer)&patientBirthdate,
        N_("Patient birthdate."), "YYYYMMDD"},
    {"study-description", 'd', 0, G_OPTION_ARG_STRING, (gpointer)&studyDescription,
        N_("Study description."), "STRING"},
    {"patient-id", 'i', 0, G_OPTION_ARG_STRING, (gpointer)&patientId,
        N_("Patient id."), "ID"},
    {"patient-name", 'n', 0, G_OPTION_ARG_STRING, (gpointer)&patientName,
        N_("Patient name."), "STRING"},
    {"physician", 'p', 0, G_OPTION_ARG_STRING, (gpointer)&physician,
        N_("Performing physician name."), "STRING"},
    {"patient-sex", 's', 0, G_OPTION_ARG_STRING, (gpointer)&patientSex,
        N_("Patient sex."), "F|M|O|U"},
    {"auto-start", '\x0', 0, G_OPTION_ARG_NONE, (gpointer)&autoStart,
        N_("Automatically start the study."), nullptr},

    {G_OPTION_REMAINING, '\x0', 0, G_OPTION_ARG_CALLBACK, (gpointer)setValueCallback,
        nullptr, nullptr},
    {nullptr, '\x0', 0, G_OPTION_ARG_NONE, nullptr,
        nullptr, nullptr},
};

bool switchToRunningInstance()
{
    auto msg = QDBusInterface(PRODUCT_NAMESPACE, "/com/irkdc/Beryllium/Main", "com.irkdc.beryllium.Main")
         .call("startStudy"
            , accessionNumber
            , patientId
            , patientName
            , patientSex
            , patientBirthdate
            , physician
            , studyDescription
            , (bool)autoStart
            );
    //qDebug() << msg;
    return msg.type() == QDBusMessage::ReplyMessage && msg.arguments().first().toBool();
}

int main(int argc, char *argv[])
{
    qDebug() << PRODUCT_FULL_NAME << PRODUCT_VERSION_STR << "(" __DATE__ " " __TIME__ ")";
    signal(SIGSEGV, sighandler);

    int errCode = 0;

    QApplication::setOrganizationName(ORGANIZATION_SHORT_NAME);
    QApplication::setApplicationName(PRODUCT_SHORT_NAME);
    QApplication::setApplicationVersion(PRODUCT_VERSION_STR);

    // At this time it is safe to use QSettings
    //
    QSettings settings;

    setupGstDebug(settings);

#if !GLIB_CHECK_VERSION(2, 32, 0)
    // Must initialise the threading system before using any other GLib funtion
    //
    if (!g_thread_supported())
    {
      g_thread_init(nullptr);
    }
#endif

    // Pass some arguments to gstreamer.
    // For example --gst-debug-level=5
    //
    GError* err = nullptr;
    auto ctx = g_option_context_new("[var=value] [var=value] ...");
    g_option_context_add_main_entries(ctx, options, PRODUCT_SHORT_NAME);

#ifdef WITH_DICOM
    setupDcmtkDebug(settings);
    auto dcmtkGroup = g_option_group_new ("dcmtk", _("DCMTK Options"),
        _("Show DCMTK Options"), NULL, NULL);
    g_option_context_add_group(ctx, dcmtkGroup);
    g_option_group_add_entries (dcmtkGroup, dcmtkOptions);
    //g_option_group_set_translation_domain (dcmtkGroup, GETTEXT_PACKAGE);
#endif

    g_option_context_add_group(ctx, gst_init_get_option_group());
    g_option_context_set_ignore_unknown_options(ctx, false);
    g_option_context_parse(ctx, &argc, &argv, &err);
    g_option_context_free(ctx);

    if (err)
    {
      g_print(N_("Error initializing: %s\n"), GST_STR_NULL(err->message));
      g_error_free(err);
      return 1;
    }

    gstApplyFixes();

    // QGStreamer stuff
    //
    QGst::init();

    // QT init
    //
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#if (QT_VERSION >= QT_VERSION_CHECK(4, 8, 0))
    QApplication::setAttribute(Qt::AA_X11InitThreads);
#endif
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/app/product"));

    bool fullScreen = settings.value("show-fullscreen").toBool();

    // Override some style sheets
    //
    app.setStyleSheet(settings.value("css").toString());

    // Translations
    //
    QTranslator  translator;
    QString      locale = settings.value("locale").toString();
    if (locale.isEmpty())
    {
        locale = QLocale::system().name();
    }

    // First, try to load localization in the current directory, if possible.
    // If failed, then try to load from app data path
    //
    if (translator.load(app.applicationDirPath() + "/" PRODUCT_SHORT_NAME "_" + locale) ||
        translator.load(DATA_FOLDER + "/translations/" PRODUCT_SHORT_NAME "_" + locale))
    {
        app.installTranslator(&translator);
    }

    QWidget* wnd = nullptr;

    // UI scope
    //

    switch (windowType)
    {
    case 'a':
        {
            auto wndArc = new ArchiveWindow;
            wndArc->updateRoot();
            wndArc->setPath(windowArg.isEmpty()? QDir::currentPath(): windowArg);
            wnd = wndArc;
        }
        break;
    case 'e':
        wnd = new VideoEditor(windowArg);
        break;
    case 's':
        wnd = new Settings(windowArg);
        break;
    default:
        {
            auto bus = QDBusConnection::sessionBus();
            auto wndMain = new MainWindow;

            // connect to DBus and register as an object
            //
            auto adapter = new MainWindowDBusAdaptor(wndMain);
            bus.registerObject("/com/irkdc/Beryllium/Main", wndMain);

            // If registerService succeeded, there is no other instances.
            // If failed, then another instance is possible running, or DBus is complitelly broken.
            //
            if (bus.registerService(PRODUCT_NAMESPACE) || !switchToRunningInstance())
            {
                adapter->startStudy(accessionNumber
                                    , patientId
                                    , patientName
                                    , patientSex
                                    , patientBirthdate
                                    , physician
                                    , studyDescription
                                    , (bool)autoStart);
                wnd = wndMain;
            }
            else
            {
                delete wndMain;
            }
        }
        break;
    }

    if (wnd)
    {
#ifdef WITH_TOUCH
        ClickFilter filter;
        wnd->installEventFilter(&filter);
        wnd->grabGesture(Qt::TapAndHoldGesture);
#endif
        fullScreen? wnd->showFullScreen(): wnd->show();
        errCode = app.exec();
        delete wnd;
        QDBusConnection::sessionBus().unregisterObject("/com/irkdc/Beryllium/Main");
    }

    QGst::cleanup();
    return errCode;
}
