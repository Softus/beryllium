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

#include "touch/clickfilter.h"

#include "product.h"
#include "archivewindow.h"
#include "defaults.h"
#include "mainwindow.h"
#ifdef WITH_QT_DBUS
#include "dbusconnect.h"
#include "mainwindowdbusadaptor.h"
#endif
#include "darkthemestyle.h"
#include "videoeditor.h"
#include "settingsdialog.h"
#include "smartshortcut.h"

#include <signal.h>

#include <QApplication>
#ifdef WITH_QT_DBUS
#include <QDBusConnection>
#include <QDBusInterface>
#endif
#include <QIcon>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

#ifdef Q_OS_LINUX
#ifdef WITH_QT_X11EXTRAS
#include <QX11Info>
#endif
#include <X11/Xlib.h>
#endif

#include <glib/gstdio.h>
#include <gst/gst.h>
#include <QGst/Init>

#ifdef WITH_DICOM
#define HAVE_CONFIG_H
#include <dcmtk/config/osconfig.h>   /* make sure OS specific configuration is included first */
#include <dcmtk/oflog/logger.h>
#include <dcmtk/oflog/fileap.h>
#include <dcmtk/oflog/configrt.h>
namespace dcmtk{}
using namespace dcmtk;
#endif

static volatile sig_atomic_t fatal_error_in_progress = 0;
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

// Holder for defaut settings values. Must be kept in the memory.
//
static QSettings systemSettings(QSettings::SystemScope, ORGANIZATION_DOMAIN, PRODUCT_SHORT_NAME);

static gboolean
cfgPathCallback(const gchar *, const gchar *value, gpointer, GError **)
{
    auto const& path = QString::fromLocal8Bit(value);
    if (QFileInfo(path).isDir())
    {
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, path);
        QSettings fileSettings(QSettings::IniFormat, QSettings::SystemScope,
            ORGANIZATION_DOMAIN, PRODUCT_SHORT_NAME);
        // Copy settings from file to memory
        //
        foreach (auto const& key, fileSettings.allKeys())
        {
            systemSettings.setValue(key, fileSettings.value(key));
        }
    }
    else
    {
        QSettings fileSettings(path, QSettings::IniFormat);
        // Copy settings from file to memory
        //
        foreach (auto const& key, fileSettings.allKeys())
        {
            systemSettings.setValue(key, fileSettings.value(key));
        }
    }
    return true;
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
                G_OPTION_ERROR_BAD_VALUE, QT_TRANSLATE_NOOP_UTF8("cmdline",
                    "Bad argument '%s' (must be name=value)"),
                value);
        }
        return false;
    }

    QSettings().setValue(QString::fromLocal8Bit(value, idx - value),
                         QString::fromLocal8Bit(idx + 1));
    return true;
}

#ifdef WITH_QT_X11EXTRAS
static gboolean
xSyncCallback(const gchar *, const gchar *, gpointer, GError **)
{
#ifdef Q_OS_LINUX
    XSynchronize(QX11Info::display(), true);
#endif
    return true;
}
#endif

static gchar   windowType = '\x0';
static QString windowArg;

static gboolean
setModeCallback(const gchar *name, const gchar *value, gpointer, GError **)
{
    windowType = name[2]; // --settings => 's', --archive => 'a'
    windowArg = QString::fromUtf8(value);
    return true;
}

void setupGstDebug(const QSettings& settings)
{
    Q_ASSERT(settings.group() == "debug");

    if (!settings.value("gst-debug-on", DEFAULT_GST_DEBUG_ON).toBool())
    {
        return;
    }

    gst_debug_set_active(true);

    auto const& gstDebug = settings.value("gst-debug", DEFAULT_GST_DEBUG).toString();
    if (!gstDebug.isEmpty())
    {
        gst_debug_set_threshold_from_string(gstDebug.toLocal8Bit(), true);
    }

    auto const& debugLogFile = settings.value("gst-debug-log-file", DEFAULT_GST_DEBUG_LOG_FILE).toString();
    if (debugLogFile.isEmpty())
    {
        gst_debug_remove_log_function (gst_debug_log_default);
        gst_debug_add_log_function (gst_debug_log_default, stderr, nullptr);
    }
    else
    {
        QFileInfo(debugLogFile).absoluteDir().mkpath(".");
        gst_debug_remove_log_function(gst_debug_log_default);
        gst_debug_add_log_function(gst_debug_log_default, g_fopen(debugLogFile.toLocal8Bit(), "w"),
            nullptr);
    }

    gst_debug_set_colored(
        !settings.value("gst-debug-no-color", DEFAULT_GST_DEBUG_NO_COLOR).toBool());
    auto gstDebugLevel =
        GstDebugLevel(settings.value("gst-debug-level", DEFAULT_GST_DEBUG_LEVEL).toInt());
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
    {"dcmtk-log-file", '\x0', 0, G_OPTION_ARG_CALLBACK, gpointer(dcmtkLogFileCallback),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "DCMTK log output file."),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "FILE")},
    {"dcmtk-log-level", '\x0', 0, G_OPTION_ARG_CALLBACK, gpointer(dcmtkLogLevelCallback),
        QT_TRANSLATE_NOOP_UTF8("cmdline",
            "DCMTK logging level: fatal, error, warn, info, debug, trace."),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "LEVEL")},
    {"dcmtk-log-config", '\x0', 0, G_OPTION_ARG_CALLBACK, gpointer(dcmtkLogConfigCallback),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Config file for DCMTK logger."),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "FILE")},
    {nullptr, '\x0', 0, G_OPTION_ARG_NONE, nullptr, nullptr, nullptr},
};

void setupDcmtkDebug(const QSettings& settings)
{
    Q_ASSERT(settings.group() == "debug");

    if (!settings.value("dcmtk-debug-on", DEFAULT_DCMTK_DEBUG_ON).toBool())
    {
        return;
    }

    for (auto o = dcmtkOptions; o->long_name; ++o)
    {
        auto const& value = settings.value(o->long_name).toString();
        if (!value.isEmpty())
        {
            (GOptionArgFunc(o->arg_data))(o->long_name, value.toLocal8Bit().constData(), nullptr,
                nullptr);
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
static gboolean printVersion = false;

static GOptionEntry options[] = {
    {"archive", '\x0', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK,
        gpointer(setModeCallback),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Show the archive window."),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "PATH")},
    {"edit-video", '\x0', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK,
        gpointer(setModeCallback),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Show the video editor window."),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "FILE")},
    {"settings", '\x0', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK,
        gpointer(setModeCallback),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Show the settings window."),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "PAGE")},
    {"safe-mode", '\x0', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, gpointer(setValueCallback),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Run the program in safe mode."), nullptr},
#ifdef WITH_QT_X11EXTRAS
    {"sync", '\x0', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, gpointer(xSyncCallback),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Run the program in X synchronous mode."), nullptr},
#endif
    {"config-path", 'c', G_OPTION_FLAG_FILENAME, G_OPTION_ARG_CALLBACK, gpointer(cfgPathCallback),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Set root path to the settings file."),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "PATH")},

    {"study-id", 'a', 0, G_OPTION_ARG_STRING, gpointer(&accessionNumber),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Study accession number (id)."),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "ID")},
    {"patient-birthdate", 'b', 0, G_OPTION_ARG_STRING, gpointer(&patientBirthdate),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Patient birthdate."),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "YYYYMMDD")},
    {"study-description", 'd', 0, G_OPTION_ARG_STRING, gpointer(&studyDescription),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Study description."),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "STRING")},
    {"patient-id", 'i', 0, G_OPTION_ARG_STRING, gpointer(&patientId),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Patient id."),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "ID")},
    {"patient-name", 'n', 0, G_OPTION_ARG_STRING, gpointer(&patientName),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Patient name."),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "STRING")},
    {"physician", 'p', 0, G_OPTION_ARG_STRING, gpointer(&physician),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Performing physician name."),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "STRING")},
    {"patient-sex", 's', 0, G_OPTION_ARG_STRING, gpointer(&patientSex),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Patient sex."),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "F|M|O|U")},
    {"auto-start", '\x0', 0, G_OPTION_ARG_NONE, gpointer(&autoStart),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Automatically start the study."), nullptr},
    {"version", '\x0', 0, G_OPTION_ARG_NONE, gpointer(&printVersion),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Print version information and exit."), nullptr},

    {G_OPTION_REMAINING, '\x0', 0, G_OPTION_ARG_CALLBACK, gpointer(setValueCallback),
        nullptr, nullptr},
    {nullptr, '\x0', 0, G_OPTION_ARG_NONE, nullptr,
        nullptr, nullptr},
};

#ifdef WITH_QT_DBUS
bool switchToRunningInstance()
{
    auto const& msg = QDBusInterface(PRODUCT_NAMESPACE, "/org/softus/Beryllium/Main",
            "org.softus.beryllium.Main")
         .call("startStudy", accessionNumber, patientId, patientName, patientSex, patientBirthdate,
               physician, studyDescription, !!autoStart);
    //qDebug() << msg;
    return msg.type() == QDBusMessage::ReplyMessage && msg.arguments().first().toBool();
}
#endif

int main(int argc, char *argv[])
{
    qDebug() << PRODUCT_FULL_NAME << PRODUCT_VERSION_STR;
    qDebug() << PRODUCT_SITE_URL;

    signal(SIGSEGV, sighandler);

    int errCode = 0;

    QApplication::setOrganizationName(ORGANIZATION_DOMAIN);
    QApplication::setApplicationName(PRODUCT_SHORT_NAME);
    QApplication::setApplicationVersion(PRODUCT_VERSION_STR);

#ifdef Q_OS_WIN
    QStringList paths = QCoreApplication::libraryPaths();
        paths.prepend(".");
        paths.prepend("imageformats");
        paths.prepend("platforms");
    QCoreApplication::setLibraryPaths(paths);
    QSettings::setDefaultFormat(QSettings::IniFormat);
#endif

    // Pass some arguments to gstreamer.
    // For example --gst-debug-level=5
    //
    GError* err = nullptr;
    auto ctx = g_option_context_new("[var=value] [var=value] ...");
    g_option_context_add_main_entries(ctx, options, PRODUCT_SHORT_NAME);

#ifdef WITH_DICOM
    auto dcmtkGroup = g_option_group_new ("dcmtk",
        QT_TRANSLATE_NOOP_UTF8("cmdline", "DCMTK Options"),
        QT_TRANSLATE_NOOP_UTF8("cmdline", "Show DCMTK Options"), nullptr, nullptr);
    g_option_context_add_group(ctx, dcmtkGroup);
    g_option_group_add_entries (dcmtkGroup, dcmtkOptions);
#endif

    g_option_context_add_group(ctx, gst_init_get_option_group());
    g_option_context_set_ignore_unknown_options(ctx, false);

    // Mysterious abrakadabra to solve glib encoding issue.
    //
    auto const& activeLocale = std::locale::global(std::locale(""));
    g_option_context_parse(ctx, &argc, &argv, &err);
    g_option_context_free(ctx);
    std::locale::global(activeLocale);

    if (err)
    {
        g_print(QT_TRANSLATE_NOOP_UTF8("cmdline", "Error initializing: %s\n"),
            GST_STR_NULL(err->message));
        g_error_free(err);
        return 1;
    }

    if (printVersion)
    {
        return 0;
    }

    qDebug() << "Built by " AUX_STR(USERNAME) " on " AUX_STR(OS_DISTRO)
             << AUX_STR(OS_REVISION) " at " __DATE__ " " __TIME__;

    // At this time it is safe to use QSettings
    //
    QSettings settings;
    settings.beginGroup("debug");
    setupGstDebug(settings);
#ifdef WITH_DICOM
    setupDcmtkDebug(settings);
#endif
    settings.endGroup();

    // QGStreamer stuff
    //
    QGst::init();

    // Get rid of vaapisink until somebody get it fixed
    //
    auto registry = gst_registry_get();
    auto const& blacklisted = settings.value("gst/blacklisted", DEFAULT_GST_BLACKLISTED).toStringList();
    foreach (auto const& elm, blacklisted)
    {
        auto feature = gst_registry_lookup_feature(registry, elm.toUtf8());
        if (feature)
        {
            gst_plugin_feature_set_rank(feature, GST_RANK_NONE);
            gst_object_unref(feature);
            qDebug() << elm << "rank set to" << GST_RANK_NONE;
        }
    }

    // QT init
    //
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
    QApplication::setAttribute(Qt::AA_X11InitThreads);
    QApplication app(argc, argv);
    QIcon appIcon(":/app/product");

    settings.beginGroup("ui");

    // Apply dark aware theme if button text is bright or the dart theme is forced
    //
    QString iconSet = settings.value("icon-set", DEFAULT_ICON_SET).toString();
    if (iconSet.compare("dark", Qt::CaseInsensitive) == 0
            || (iconSet.compare("auto", Qt::CaseInsensitive) == 0
                && app.palette().color(QPalette::Active, QPalette::ButtonText).lightness() > 128))
    {
        app.setStyle(new DarkThemeStyle(QApplication::style()));
        appIcon = DarkThemeStyle::invertIcon(appIcon);
    }
    app.setWindowIcon(appIcon);

    bool    fullScreen          = settings.value("show-fullscreen").toBool();
    bool    emulateDoubleClicks = settings.value("long-tap-to-double-click").toBool();
    QString locale              = settings.value("locale").toString();


    // Override some style sheets
    //
    app.setStyleSheet(settings.value("css").toString());

    settings.endGroup();

    // Translations
    //
    QTranslator  translator;
    if (locale.isEmpty())
    {
        locale = QLocale::system().name();
    }

    if (translator.load(PRODUCT_SHORT_NAME "_" + locale, TRANSLATIONS_FOLDER))
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
        wnd = new SettingsDialog(windowArg);
        break;
    default:
        {
            auto wndMain = new MainWindow;
#ifdef WITH_QT_DBUS
            auto bus = QDBusConnection::sessionBus();

            // connect to DBus and register as an object
            //
            auto adapter = new MainWindowDBusAdaptor(wndMain);
            bus.registerObject("/org/softus/Beryllium/Main", wndMain);

            // If registerService succeeded, there is no other instances.
            // If failed, then another instance is possible running, or DBus is complitelly broken.
            //
            if (bus.registerService(PRODUCT_NAMESPACE) || !switchToRunningInstance())
            {
                adapter->startStudy(accessionNumber, patientId, patientName, patientSex,
                    patientBirthdate, physician, studyDescription, !!autoStart);

                auto const& dbusService = settings.value("connect-to-dbus-service").toStringList();
                if (dbusService.length() >= 4)
                {
                    if (!connectToDbusService(adapter,
                        0 == dbusService.at(0).compare("system", Qt::CaseInsensitive),
                        dbusService.at(1), dbusService.at(2), dbusService.at(3)))
                    {
                        qDebug() << "Failed to connect to" << dbusService;
                    }
                }
                wnd = wndMain;
            }
            else
            {
                delete wndMain;
            }
#else
            wnd = wndMain;
#endif
        }
        break;
    }

    if (wnd)
    {
        if (emulateDoubleClicks)
        {
            new ClickFilter(wnd);
            wnd->grabGesture(Qt::TapAndHoldGesture);
        }

        // SmartShortcut scope
        {
            new SmartShortcut(wnd);
            fullScreen? wnd->showFullScreen(): wnd->show();
            errCode = app.exec();
        }
        delete wnd;
#ifdef WITH_QT_DBUS
        QDBusConnection::sessionBus().unregisterObject("/org/softus/Beryllium/Main");
#endif
    }

    QGst::cleanup();
    return errCode;
}
