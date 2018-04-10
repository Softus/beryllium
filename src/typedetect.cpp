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
#define QT_NO_EMIT
#include "typedetect.h"
#include "product.h"
#include <gio/gio.h>

#include <QDebug>
#include <QImage>
#include <QSettings>
#include <QUrl>

#include <QGlib/Signal>
#include <QGst/Buffer>
#include <QGst/Caps>
#include <QGst/ClockTime>
#include <QGst/Event>
#include <QGst/ElementFactory>
#include <QGst/Fourcc>
#include <QGst/Pipeline>
#include <QGst/Sample>
#include <QGst/Structure>

bool setFileExtAttribute(const QString& filePath, const QString& name, const QString& value)
{
    auto const& encodedValue = QUrl::toPercentEncoding(value);

    bool ret = false;
    auto file = g_file_new_for_path(filePath.toLocal8Bit());
    if (file)
    {
        GError* err = nullptr;
        QString attr("xattr::" PRODUCT_NAMESPACE ".");
        attr.append(name);
        ret = g_file_set_attribute_string (file, attr.toLocal8Bit(),
            encodedValue.data(), G_FILE_QUERY_INFO_NONE, nullptr, &err);

        if (err)
        {
#ifdef Q_OS_WIN
            // Backup route for windows
            //
            if (err->domain == G_IO_ERROR && err->code == G_IO_ERROR_NOT_SUPPORTED)
            {
                QSettings settings(filePath + ":" + PRODUCT_NAMESPACE, QSettings::IniFormat);
                settings.setValue(name, encodedValue);
                settings.sync();
                ret = (settings.status() == QSettings::NoError);
            }
#else
            qDebug() << QString::fromLocal8Bit(err->message);
#endif
            g_error_free(err);
        }

        g_object_unref(file);
    }
    return ret;
}

QString getFileExtAttribute(const QString& filePath, const QString& name)
{
    QByteArray encodedValue;
    auto file = g_file_new_for_path(filePath.toLocal8Bit());
    if (file)
    {
        QString attr("xattr::" PRODUCT_NAMESPACE ".");
        attr.append(name);

        GError* err = nullptr;
        auto info = g_file_query_info(file, attr.toLocal8Bit(),
            G_FILE_QUERY_INFO_NONE, nullptr, &err);

        if (info)
        {
            auto value = g_file_info_get_attribute_string(info, attr.toLocal8Bit());
            if (value)
            {
                encodedValue.append(value);
            }
            g_object_unref(info);
        }

#ifdef Q_OS_WIN
        if (encodedValue.isNull())
        {
            // Backup route for windows
            //
            QSettings settings(filePath + ":" + PRODUCT_NAMESPACE, QSettings::IniFormat);
            encodedValue = settings.value(name).toByteArray();
        }
#endif
        if (err)
        {
            qDebug() << QString::fromUtf8(err->message);
            g_error_free(err);
        }

        g_object_unref(file);
    }
    return QUrl::fromPercentEncoding(encodedValue);
}

QString typeDetect(const QString& filePath)
{
    QGst::State   state;
    QGst::CapsPtr caps;
    auto const& pipeline = QGst::Pipeline::create("typedetect");
    auto const& source   = QGst::ElementFactory::make("filesrc", "source");
    auto const& typefind = QGst::ElementFactory::make("typefind", "typefind");
    auto const& fakesink = QGst::ElementFactory::make("fakesink", "fakesink");

    if (pipeline && source && typefind && fakesink)
    {
        pipeline->add(source, typefind, fakesink);
        QGst::Element::linkMany(source, typefind, fakesink);

        source->setProperty("location", filePath);
        pipeline->setState(QGst::StatePaused);
        QGst::ClockTime timeout(10ULL * 1000 * 1000 * 1000); // 10 Sec
        if (QGst::StateChangeSuccess == pipeline->getState(&state, nullptr, timeout))
        {
            auto const& prop = typefind->property("caps");
            if (prop)
            {
                caps = prop.get<QGst::CapsPtr>();
            }
        }
        pipeline->setState(QGst::StateNull);
        pipeline->getState(&state, nullptr, timeout);
    }

    if (caps)
    {
        auto const& str = caps->internalStructure(0);
        if (str)
        {
            return str->name();
        }
    }

    return "";
}

QImage extractRgbImage(const QGst::BufferPtr& buf, const QGst::CapsPtr& caps, int width = 0)
{
    auto const& structure = caps->internalStructure(0);
    auto imgWidth  = structure->value("width").toInt();
    auto imgHeight = structure->value("height").toInt();

    QGst::MapInfo map;
    auto data = buf->map(map, QGst::MapRead)? map.data(): nullptr;
    QImage img(data, imgWidth, imgHeight, QImage::Format_RGB888);
    buf->unmap(map);

    // Must copy image bits, they will be unavailable after the pipeline stops
    //
    return width > 0? img.scaledToWidth(width): img.copy();
}

QImage extractImage(const QGst::BufferPtr& buf, const QGst::CapsPtr& caps, int width = 0)
{
    QImage img;
    QGst::State   state;
    auto const& structure = caps->internalStructure(0);
    if (structure->name() == "video/x-raw" && structure->value("format").toString() == "RGB"
        && structure->value("bpp").toInt() == 24)
    {
        // Already good enought buffer
        //
        return extractRgbImage(buf, caps, width);
    }

    auto const& pipeline = QGst::Pipeline::create("imgconvert");
    auto const& src   = QGst::ElementFactory::make("appsrc", "src");
    auto const& vaapi = structure->name() == "video/x-surface"?
         QGst::ElementFactory::make("vaapidownload", "vaapi"): QGst::ElementPtr();
    auto const& cvt   = QGst::ElementFactory::make("videoconvert", "cvt");
    auto const& sink  = QGst::ElementFactory::make("appsink", "sink");

    if (pipeline && src && cvt && sink)
    {
        //qDebug() << caps->toString();
        pipeline->add(src, cvt, sink);
        if (vaapi)
        {
            pipeline->add(vaapi);
        }
        src->setProperty("caps", caps);
        sink->setProperty("caps", QGst::Caps::fromString("video/x-raw,format=RGB,bpp=24"));
        sink->setProperty("async", false);

        if (vaapi ? QGst::Element::linkMany(src, vaapi, cvt, sink)
                  : QGst::Element::linkMany(src, cvt, sink))
        {
            pipeline->setState(QGst::StatePaused);
            QGst::ClockTime timeout(200ULL * 1000 * 1000); // 200 msec
            if (QGst::StateChangeSuccess == pipeline->getState(&state, nullptr, timeout))
            {
                QGlib::emit<void>(src, "push-buffer", buf);
                auto const& rgbSample = QGlib::emit<QGst::SamplePtr>(sink, "pull-preroll");
                if (rgbSample)
                {
                    img = extractRgbImage(rgbSample->buffer(), rgbSample->caps(), width);
                }
            }
            pipeline->setState(QGst::StateNull);
            pipeline->getState(&state, nullptr, timeout);
        }
    }

    return img;
}
