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

#include "videosourcedetails.h"
#include "videosources.h"
#include "elementproperties.h"
#include "../defaults.h"
#include "../platform.h"
#include "../qwaitcursor.h"
#include <algorithm>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QSpinBox>
#include <QTextEdit>
#include <qxtlineedit.h>

#include <QGlib/Error>
#include <QGlib/ParamSpec>
#include <QGlib/Value>
#include <QGst/Caps>
#include <QGst/ElementFactory>
#include <QGst/IntRange>
#include <QGst/Pad>
#include <QGst/Parse>
#include <QGst/Pipeline>
#include <QGst/Structure>
#include <gst/gst.h>

#ifdef WITH_LIBV4L2
  #include <libv4l2.h>
  #include <libv4l2rds.h>
#endif

QString getDeviceIdPropName(const QString& deviceType)
{
    if (deviceType == "videotestsrc")  return "pattern";
    if (deviceType == "ksvideosrc")    return "device-name";
    if (deviceType == "v4l2src")       return "device";
    if (deviceType == "avfvideosrc")   return "device-index";

    // Unknown device type
    return QString();
}

VideoSourceDetails::VideoSourceDetails
    ( const QVariantMap& parameters
    , QWidget *parent
    )
    : QDialog(parent)
    , checkFps(nullptr)
    , spinFps(nullptr)
    , parameters(parameters)
{
    selectedChannel = parameters.value("video-channel");
    selectedFormat  = parameters.value("format").toString();
    selectedSize    = parameters.value("size").toSize();

    auto layoutMain = new QFormLayout();

    editAlias= new QLineEdit(parameters.value("alias").toString());
    layoutMain->addRow(tr("&Alias"), editAlias);

#ifdef WITH_DICOM
    // Modality override
    //
    editModality  = new QxtLineEdit(parameters.value("modality").toString());
    editModality->setSampleText(tr("(default)"));
    layoutMain->addRow(tr("&Modality"), editModality);
#endif

    layoutMain->addRow(tr("I&nput channel"), listChannels = new QComboBox());
    listChannels->addItem(tr("(default)"));
    connect(listChannels, SIGNAL(currentIndexChanged(int)), this, SLOT(inputChannelChanged(int)));
    layoutMain->addRow(tr("Pixel &format"), listFormats = new QComboBox());
    connect(listFormats, SIGNAL(currentIndexChanged(int)), this, SLOT(formatChanged(int)));
    layoutMain->addRow(tr("Frame &size"), listSizes = new QComboBox());
    widgetWithExtraButton(layoutMain, tr("Video &codec"), listVideoCodecs = new QComboBox());

    auto const& elm = QGst::ElementFactory::make("videorate");
    if (elm && elm->findProperty("max-rate"))
    {
        layoutMain->addRow(checkFps = new QCheckBox(tr("&Limit rate")), spinFps = new QSpinBox());
        connect(checkFps, SIGNAL(toggled(bool)), spinFps, SLOT(setEnabled(bool)));
        checkFps->setChecked(parameters.value("limit-video-fps", DEFAULT_LIMIT_VIDEO_FPS).toBool());

        spinFps->setRange(1, 200);
        spinFps->setValue(parameters.value("video-max-fps", DEFAULT_VIDEO_MAX_FPS).toInt());
        spinFps->setSuffix(tr(" frames per second"));
        spinFps->setEnabled(checkFps->isChecked());
    }
    layoutMain->addRow(tr("Video &bitrate"), spinBitrate = new QSpinBox());
    spinBitrate->setRange(0, 102400);
    spinBitrate->setSingleStep(100);
    spinBitrate->setSuffix(tr(" kbit per second"));
    spinBitrate->setValue(parameters.value("bitrate", DEFAULT_VIDEOBITRATE).toInt());

    auto deinterlaceLayout = new QHBoxLayout;
    checkDeinterlace = new QCheckBox(tr("De&interlace"));
    checkDeinterlace->setChecked(parameters.value("video-deinterlace").toBool());
    checkDeinterlace->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    deinterlaceLayout->addWidget(checkDeinterlace);

    auto btnDeinterlace = new QPushButton("\u2026");
    btnDeinterlace->setEnabled(checkDeinterlace->isChecked());
    connect(checkDeinterlace, SIGNAL(toggled(bool)), btnDeinterlace, SLOT(setEnabled(bool)));
    connect(btnDeinterlace, SIGNAL(clicked()), this, SLOT(onDeinterlaceClick()));

    deinterlaceLayout->addWidget(btnDeinterlace);
    layoutMain->addRow(nullptr, deinterlaceLayout);

    layoutMain->addRow(nullptr,
        checkLogOnly = new QCheckBox(tr("Use this source for video log &only")));
    checkLogOnly->setChecked(parameters.value("log-only").toBool());

    widgetWithExtraButton(layoutMain, tr("Video m&uxer"), listVideoMuxers = new QComboBox());
    widgetWithExtraButton(layoutMain, tr("Ima&ge codec"), listImageCodecs = new QComboBox());
    widgetWithExtraButton(layoutMain, tr("RTP &payloader"), listRtpPayloaders = new QComboBox());
    widgetWithExtraButton(layoutMain, tr("&Display sink"), listDisplaySinks = new QComboBox());

    // UDP streaming
    //
    editRtpClients = new QLineEdit(parameters.value("rtp-clients").toString());
    layoutMain->addRow(checkEnableRtp = new QCheckBox(tr("&RTP clients")), editRtpClients);
    connect(checkEnableRtp, SIGNAL(toggled(bool)), editRtpClients, SLOT(setEnabled(bool)));
    checkEnableRtp->setChecked(parameters.value("enable-rtp").toBool());
    editRtpClients->setEnabled(checkEnableRtp->isChecked());

    // Http streaming
    //
    editHttpPushUrl = new QLineEdit(parameters.value("http-push-url").toString());
    layoutMain->addRow(checkEnableHttp = new QCheckBox(tr("&HTTP push URL")), editHttpPushUrl);
    connect(checkEnableHttp, SIGNAL(toggled(bool)), editHttpPushUrl, SLOT(setEnabled(bool)));
    checkEnableHttp->setChecked(parameters.value("enable-http").toBool());
    editHttpPushUrl->setEnabled(checkEnableHttp->isChecked());

    // Rtmp streaming
    //
    editRtmpPushUrl = new QLineEdit(parameters.value("rtmp-push-url").toString());
    layoutMain->addRow(checkEnableRtmp = new QCheckBox(tr("R&TMP push URL")), editRtmpPushUrl);
    connect(checkEnableRtmp, SIGNAL(toggled(bool)), editRtmpPushUrl, SLOT(setEnabled(bool)));
    checkEnableRtmp->setChecked(parameters.value("enable-rtmp").toBool());
    editRtmpPushUrl->setEnabled(checkEnableRtmp->isChecked());

    // Buttons row
    //
    auto layoutBtns = new QHBoxLayout;
    auto btnAdvanced = new QPushButton(tr("A&dvanced"));
    connect(btnAdvanced, SIGNAL(clicked()), this, SLOT(onAdvancedClick()));
    layoutBtns->addWidget(btnAdvanced);
    layoutBtns->addStretch(1);
    auto btnCancel = new QPushButton(tr("Cancel"));
    connect(btnCancel, SIGNAL(clicked()), this, SLOT(reject()));
    layoutBtns->addWidget(btnCancel);
    auto btnSave = new QPushButton(tr("Save"));
    connect(btnSave, SIGNAL(clicked()), this, SLOT(accept()));
    btnSave->setDefault(true);
    layoutBtns->addWidget(btnSave);
    layoutMain->addRow(layoutBtns);

    setLayout(layoutMain);

    bool useAv = QGst::ElementFactory::find("avenc_mpeg2video");
    auto const& defaultEncoder = useAv? "avenc_mpeg2video": "ffenc_mpeg2video";

    // Refill the boxes every time the page is shown
    //
    auto const& selectedCodec = updateGstList("video-encoder", defaultEncoder,
        GST_ELEMENT_FACTORY_TYPE_ENCODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, listVideoCodecs);
    listVideoCodecs->insertItem(0, tr("(none)"));
    if (selectedCodec.isEmpty())
    {
        listVideoCodecs->setCurrentIndex(0);
    }
    auto const& selectedMuxer = updateGstList("video-muxer",
        DEFAULT_VIDEO_MUXER,   GST_ELEMENT_FACTORY_TYPE_MUXER, listVideoMuxers);
    listVideoMuxers->insertItem(0, tr("(none)"));
    if (selectedMuxer.isEmpty())
    {
        listVideoMuxers->setCurrentIndex(0);
    }
    updateGstList("image-encoder", DEFAULT_IMAGE_ENCODER,
        GST_ELEMENT_FACTORY_TYPE_ENCODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_IMAGE, listImageCodecs);
    updateGstList("rtp-payloader", DEFAULT_RTP_PAYLOADER, GST_ELEMENT_FACTORY_TYPE_PAYLOADER,
        listRtpPayloaders);
    updateGstList("display-sink", DEFAULT_DISPLAY_SINK,
        GST_ELEMENT_FACTORY_TYPE_SINK | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, listDisplaySinks);
}

QString VideoSourceDetails::updateGstList
    ( const char* settingName
    , const char* def
    , unsigned long long type
    , QComboBox* cb
    )
{
    cb->clear();
    auto currentIndex = -1;
    auto const& currentElement = parameters.value(settingName, def).toString();
    auto extra = parameters.value(QString(settingName)+"-extra").toBool()
        || qApp->queryKeyboardModifiers().testFlag(Qt::ShiftModifier);
    auto elmList = gst_element_factory_list_get_elements(type,
        extra? GST_RANK_NONE: GST_RANK_SECONDARY);
    for (auto curr = elmList; curr; curr = curr->next)
    {
        auto const& factory = QGst::ElementFactoryPtr::wrap(GST_ELEMENT_FACTORY(curr->data), true);
        if (currentElement == factory->name())
        {
            currentIndex = cb->count();
        }
        cb->addItem(tr("%1: %2").arg(factory->name())
            .arg(factory->metadata(GST_ELEMENT_METADATA_LONGNAME)), factory->name());
    }

    // The previously selected itme may be filtered out by the rank at this time.
    //
    if (currentIndex < 0)
    {
        currentIndex = cb->count();
        auto const& factory = QGst::ElementFactory::find(currentElement);
        if (factory)
        {
            cb->addItem(tr("%1: %2").arg(factory->name())
                .arg(factory->metadata(GST_ELEMENT_METADATA_LONGNAME)), factory->name());
        }
        else
        {
            // An unknown element. The pipeline may fail to start, but this helps to
            // keep the config unchanged.
            //
            cb->addItem(tr("%1: %2").arg(currentElement).arg(tr("(not found)")), currentElement);
        }
    }

    cb->setCurrentIndex(currentIndex);

    gst_plugin_feature_list_free(elmList);
    return currentElement;
}

void VideoSourceDetails::updateDevice(const QString& device)
{
    int idx = 0;

    auto const& pipeline = QGst::Pipeline::create();
    if (!pipeline)
    {
        QMessageBox::critical(this, windowTitle(), tr("Failed to create pipeline"));
        return;
    }

    auto const& deviceType = parameters.value("device-type").toString();
    auto const& src = QGst::ElementFactory::make(deviceType);
    if (!src)
    {
        QMessageBox::critical(this, windowTitle(),
            tr("Failed to create element '%1'").arg(deviceType));
        return;
    }
    pipeline->add(src);

    auto const& sink = QGst::ElementFactory::make("fakesink");
    if (sink)
    {
        pipeline->add(sink);
        src->link(sink);
    }

    auto const& propName = getDeviceIdPropName(deviceType);
    auto const& srcPad = src->getStaticPad("src");
    if (srcPad)
    {
        if (!propName.isEmpty())
        {
            // To set this property, the device must be in Null state
            //
            src->setProperty(propName.toUtf8(), device);
        }

        // To query the caps, the test pipeline must be in Ready state
        //
        pipeline->setState(QGst::StateReady);
        pipeline->getState(nullptr, nullptr, GST_SECOND * 10);

        caps = srcPad->allowedCaps();
        qDebug() << caps->toString();
        auto const& selectedChannelLabel = selectedChannel.toString();

#ifdef WITH_LIBV4L2
        int fd = src->property("device-fd").toInt();
        uint n = 0;
        struct v4l2_input input;

        for (;;)
        {
            memset(&input, 0, sizeof (input));
            input.index = n++;
            if (v4l2_ioctl(fd, VIDIOC_ENUMINPUT, &input) < 0)
            {
                break;
            }
            auto const& label = QString::fromUtf8(reinterpret_cast<const char*>(input.name));
            if (selectedChannelLabel == label)
            {
                idx = listChannels->count();
            }
            listChannels->addItem(label, label);
        }
#endif
        {
            if (deviceType == "dv1394src")
            {
                for (int i = 1; i <= 64; ++i)
                {
                    listChannels->addItem(QString::number(i), i);
                }
                idx = selectedChannel.toInt();
            }
            else if (deviceType == "videotestsrc")
            {
                auto const& pattern = src->findProperty("pattern");
                auto cls = G_PARAM_SPEC_ENUM(static_cast<GParamSpec *>(pattern))->enum_class;
                for (guint i = 0; i < cls->n_values; ++i)
                {
                    listChannels->addItem(cls->values[i].value_name, i);
                }

                // Enum values are sequential
                //
                idx = selectedChannel.toInt() + 1;
            }
            else if (deviceType == PLATFORM_SPECIFIC_SCREEN_CAPTURE)
            {
#ifdef Q_OS_OSX
                // For macosx video and screen capture are performed by the same element.
                // We need to check one additional parameter to find out which one it is.
                //
                auto const& extra = parameters[PLATFORM_SPECIFIC_SCREEN_CAPTURE "-parameters"].toString();
                if (!extra.contains("capture-screen=true"))
                {
                    // It's a video source.
                    //
                    listChannels->addItem(tr("Wide angle"), "wide-angle");
                    listChannels->addItem(tr("Telephoto"), "telephoto");
                    listChannels->addItem(tr("Dual"), "dual");
                    idx = listChannels->findData(selectedChannelLabel);
                }
                else
#endif
                {
                    foreach (auto const& screen, QGuiApplication::screens())
                    {
                        if (selectedChannelLabel == screen->name())
                        {
                            idx = listChannels->count();
                        }
                        auto const& geom = screen->geometry();
                        listChannels->addItem(tr("%1 (%2,%3) - (%4,%5)").arg(screen->name())
                                .arg(geom.left()).arg(geom.top())
                                .arg(geom.right()).arg(geom.bottom()),
                            screen->name());
                    }
                }
            }
        }

        // Now switch the pipeline back to Null state (release resources)
        //
        pipeline->setState(QGst::StateNull);
        pipeline->getState(nullptr, nullptr, GST_SECOND * 10);
    }

    listChannels->setCurrentIndex(-1);
    listChannels->setCurrentIndex(idx);
}

static QString valueToString(const QGlib::Value& value)
{
    return G_TYPE_STRING == value.type() ? "(string)" + value.toString() : value.toString();
}

static QList<QGlib::Value> getFormats(const QGlib::Value& value)
{
    QList<QGlib::Value> formats;

    if (GST_TYPE_LIST == value.type() || GST_TYPE_ARRAY == value.type())
    {
        for (uint idx = 0; idx < gst_value_list_get_size(value); ++idx)
        {
            auto elm = gst_value_list_get_value(value, idx);
            formats << QGlib::Value(elm);
        }
    }
    else
    {
        formats << value;
    }

    return formats;
}

void VideoSourceDetails::inputChannelChanged(int index)
{
    listFormats->clear();
    listFormats->addItem(tr("(default)"));

    if (index < 0 || !caps)
    {
        return;
    }

    for (uint i = 0; i < caps->size(); ++i)
    {
        auto const& s = caps->internalStructure(i);
        foreach (auto const& format, getFormats(s->value("format")))
        {
            auto const& formatName = !format.isValid()
                ? s->name() : s->name().append(",format=").append(valueToString(format));
            if (listFormats->findData(formatName) >= 0)
            {
                continue;
            }
            auto const& displayName = !format.isValid()
                ? s->name() : s->name().append(" (").append(format.toString()).append(")");
            listFormats->addItem(displayName, formatName);
            if (formatName == selectedFormat)
            {
                listFormats->setCurrentIndex(listFormats->count() - 1);
            }
        }
    }
}

static QGst::IntRange getRange(const QGlib::Value& value)
{
    // First, try extract a single int value (more common)
    //
    bool ok = false;
    int intValue = value.toInt(&ok);
    if (ok)
    {
        return QGst::IntRange(intValue, intValue);
    }

    // If failed, try extract a range value
    //
    return ok? QGst::IntRange(intValue, intValue): value.get<QGst::IntRange>();
}

void VideoSourceDetails::formatChanged(int index)
{
    listSizes->clear();
    listSizes->addItem(tr("(default)"));

    auto const& selectedFormatStr = listFormats->itemData(index).toString();
    if (index < 0 || !caps || selectedFormatStr.isEmpty())
    {
        return;
    }

    // It is difficult to store QGst::Caps in QVariant, so we are using string serialization.
    //
    auto const& selectedFormat = QGst::Caps::fromString(selectedFormatStr);

    QList<QSize> sizes;
    for (uint i = 0; i < caps->size(); ++i)
    {
        // Format may be an array on strings, so silly strcmp may not work
        //
        if (!selectedFormat->canIntersect(caps->copyNth(i)))
        {
            continue;
        }

        auto const& s = caps->internalStructure(i);
        auto widthRange = getRange(s->value("width"));
        auto heightRange = getRange(s->value("height"));

        if (widthRange.end <= 0 || heightRange.end <= 0)
        {
            continue;
        }

        // Videotestsrc can produce frames till 2147483647x2147483647
        // Add some reasonable limits
        //
        widthRange .start = qMax(widthRange .start, 32);
        heightRange.start = qMax(heightRange.start, 24);
        widthRange .end = qMin(widthRange .end, 32768);
        heightRange.end = qMin(heightRange.end, 24576);

        // Iterate from min to max (160x120,320x240,640x480)
        //
        auto width = widthRange.start;
        auto height = heightRange.start;

        while (width <= widthRange.end && height <= heightRange.end)
        {
            sizes.append(QSize(width, height));
            width <<= 1;
            height <<= 1;
        }

        if ((width >> 1) != widthRange.end || (height >> 1) != heightRange.end)
        {
            // Iterate from max to min  (720x576,360x288,180x144)
            //
            width = widthRange.end;
            height = heightRange.end;
            while (width > widthRange.start && height > heightRange.start)
            {
                sizes.append(QSize(width, height));
                width >>= 1;
                height >>= 1;
            }
        }
    }

    // Sort resolutions descending
    //
    qSort(sizes.begin(), sizes.end(), [](const QSize& s1, const QSize& s2)
        {
            return s1.width() * s1.height() > s2.width() * s2.height();
        });

    // And remove duplicates
    //
    sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());

    foreach (auto const& size, sizes)
    {
        QString name = tr("%1x%2").arg(size.width()).arg(size.height());
        listSizes->addItem(name, size);
        if (size == selectedSize)
        {
            listSizes->setCurrentIndex(listSizes->count() - 1);
        }
    }
}

void VideoSourceDetails::onAdvancedClick()
{
    QWaitCursor wait(this);
    auto const& deviceType = parameters.value("device-type").toString();
    auto const& deviceParams = deviceType + "-parameters";

    ElementProperties dlg(deviceType, parameters.value(deviceParams).toString(), this);
    if (dlg.exec())
    {
        parameters[deviceParams]=dlg.getProperties();
    }
}

void VideoSourceDetails::onDeinterlaceClick()
{
    QWaitCursor wait(this);
    QString const& pluginType = "deinterlace";
    auto const& pluginParams = pluginType + "-parameters";

    ElementProperties dlg(pluginType, parameters.value(pluginParams).toString(), this);
    if (dlg.exec())
    {
        parameters[pluginParams]=dlg.getProperties();
    }
}

void VideoSourceDetails::widgetWithExtraButton
    ( QFormLayout* form
    , const QString& text
    , QWidget* widget
    )
{
    auto layout = new QHBoxLayout;

    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);;
    layout->addWidget(widget);

    auto btn = new QPushButton("\u2026");
    connect(btn, SIGNAL(pressed()), this, SLOT(onExtraButtonPressed()));
    layout->addWidget(btn);

    auto label = new QLabel(text);
    label->setBuddy(widget);

    form->addRow(label, layout);
}

/**
 * @brief QComboBox::currentData() for Qt 5.0.
 * @param cb combobox to use.
 * @return data associated with current item.
 */
static QVariant getListData(const QComboBox* cb)
{
    auto idx = cb->currentIndex();
    return cb->itemData(idx);
}

void VideoSourceDetails::onExtraButtonPressed()
{
    QWaitCursor wait(this);

    auto btn = static_cast<QPushButton*>(sender());
    auto cb = static_cast<QComboBox*>(btn->previousInFocusChain());
    auto const& elmType = getListData(cb).toString();
    auto const& elmParams = elmType + "-parameters";

    ElementProperties dlg(elmType, parameters.value(elmParams).toString(), this);
    if (dlg.exec())
    {
        parameters[elmParams]=dlg.getProperties();
    }
}

void VideoSourceDetails::getParameters(QVariantMap& settings)
{
    // Copy advanced options for source & codecs
    //
    foreach(auto const& param, parameters.keys())
    {
        if (param.endsWith("-parameters"))
        {
            settings[param] = parameters[param];
        }
    }

    settings["alias"]             = editAlias->text();
#ifdef WITH_DICOM
    settings["modality"]          = editModality->text();
#endif
    settings["video-channel"]     = getListData(listChannels);
    settings["format"]            = getListData(listFormats);
    settings["size"]              = getListData(listSizes);
    settings["video-encoder"]     = getListData(listVideoCodecs);
    settings["video-muxer"]       = getListData(listVideoMuxers);
    settings["image-encoder"]     = getListData(listImageCodecs);
    settings["rtp-payloader"]     = getListData(listRtpPayloaders);
    settings["display-sink"]      = getListData(listDisplaySinks);
    settings["enable-rtp"]        = checkEnableRtp->isChecked();
    settings["rtp-clients"]       = editRtpClients->text();
    settings["enable-http"]       = checkEnableHttp->isChecked();
    settings["http-push-url"]     = editHttpPushUrl->text();
    settings["enable-rtmp"]       = checkEnableRtmp->isChecked();
    settings["rtmp-push-url"]     = editRtmpPushUrl->text();
    settings["bitrate"]           = spinBitrate->value();
    settings["video-deinterlace"] = checkDeinterlace->isChecked();
    settings["log-only"]          = checkLogOnly->isChecked();

    if (spinFps)
    {
        settings["limit-video-fps"] = checkFps->isChecked();
        settings["video-max-fps"]   = spinFps->value() > 0 ? spinFps->value() : QVariant();
    }
}
