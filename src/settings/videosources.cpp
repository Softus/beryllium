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

#include "videosources.h"
#include "../defaults.h"
#include "../platform.h"
#include "../qwaitcursor.h"
#include "videosourcedetails.h"

#if (defined WITH_LIBAVC1394) && defined (WITH_LIBRAW1394)
  #include <libavc1394/avc1394.h>
  #include <libavc1394/rom1394.h>
#endif

#include <QApplication>
#include <QBoxLayout>
#include <QTreeWidget>
#include <QMessageBox>
#include <QPushButton>

#include <QGst/Caps>
#include <QGst/Element>
#include <QGst/ElementFactory>
#include <QGst/Structure>
#include <gst/gst.h>

static QTreeWidgetItem*
newItem(const QString& name, const QString& device, const QVariantMap& parameters, bool enabled)
{
    auto title = name.isEmpty()? device: device + " (" + name + ")";
    auto item = new QTreeWidgetItem(QStringList()
                                    << title
                                    << parameters.value("modality").toString()
                                    << parameters.value("alias").toString()
                                    );
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
    item->setCheckState(0, enabled? Qt::Checked: Qt::Unchecked);
    item->setData(0, Qt::UserRole, device);
    item->setData(1, Qt::UserRole, name);
    item->setData(2, Qt::UserRole, parameters);
    return item;
}

VideoSources::VideoSources(QWidget *parent) :
    QWidget(parent)
{
    QSettings settings;
    QLayout* mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(4,0,4,0);
    listSources = new QTreeWidget;

    listSources->setColumnCount(3);
    listSources->setHeaderLabels(QStringList() << tr("Device") << tr("Modality") << tr("Alias"));
    listSources->setColumnWidth(0, 320);
    listSources->setColumnWidth(1, 80);
    connect(listSources, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
            this, SLOT(onTreeItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));
    connect(listSources, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(onItemDoubleClicked(QTreeWidgetItem*,int)));

    mainLayout->addWidget(listSources);
    QBoxLayout* buttonsLayout = new QVBoxLayout;

    btnDetails = new QPushButton(tr("&Edit"));
    connect(btnDetails, SIGNAL(clicked()), this, SLOT(onEditClicked()));
    buttonsLayout->addWidget(btnDetails);

    btnRemove = new QPushButton(tr("&Remove"));
    connect(btnRemove, SIGNAL(clicked()), this, SLOT(onRemoveClicked()));
    buttonsLayout->addWidget(btnRemove);

    buttonsLayout->addSpacing(100);

    auto btnAddScreenCapture = new QPushButton(tr("&Add screen\ncapture"));
    connect(btnAddScreenCapture, SIGNAL(clicked()), this, SLOT(onAddScreenCaptureClicked()));
    buttonsLayout->addWidget(btnAddScreenCapture);

    // For debug purposes
    //
    auto btnAddTest = new QPushButton(tr("&Add test\nsource"));
    connect(btnAddTest, SIGNAL(clicked()), this, SLOT(onAddTestClicked()));
    buttonsLayout->addWidget(btnAddTest);

    buttonsLayout->addStretch(1);
    mainLayout->addItem(buttonsLayout);
    setLayout(mainLayout);

    settings.beginGroup("gst");
    auto cnt = settings.beginReadArray("src");
    for (int i = 0; i < cnt; ++i)
    {
        settings.setArrayIndex(i);
        addItem(settings);
    }
    settings.endArray();
    settings.endGroup();

    btnDetails->setEnabled(false);
    btnRemove->setEnabled(false);
}

void VideoSources::addItem(const QSettings& settings)
{
    QString device;
    QString friendlyName;
    bool    enabled = true;
    QVariantMap parameters;

    foreach (auto key, settings.childKeys())
    {
        if (key == "device")
        {
            device = settings.value(key).toString();
        }
        else if (key == "device-name")
        {
            friendlyName = settings.value(key).toString();
        }
        else if (key == "enabled")
        {
            enabled = settings.value(key).toBool();
        }
        else
        {
            parameters[key] = settings.value(key);
        }
    }
    auto item = newItem(friendlyName, device, parameters, enabled);
    listSources->addTopLevelItem(item);
}

void VideoSources::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);

    // Populate cameras list
    //
    updateDeviceList();
}

void VideoSources::updateDeviceList()
{
    QWaitCursor wait(this);

    auto monitor = gst_device_monitor_new();
    gst_device_monitor_add_filter(monitor, "Video/Source", nullptr);
    if (gst_device_monitor_start(monitor))
    {
        auto devices = gst_device_monitor_get_devices(monitor);
        while (devices)
        {
            auto device = static_cast<GstDevice *>(devices->data);
            {
                auto name = gst_device_get_display_name(device);
                auto src = QGst::ElementPtr::wrap(gst_device_create_element(device, name));
                g_free(name);

                auto deviceType = QGlib::Type::fromInstance(src).name().mid(3).toLower();
                auto deviceIdPropName = getDeviceIdPropName(deviceType);

                if (deviceIdPropName.isEmpty())
                {
                    qWarning() << "Unsupported device type" << deviceType;
                }
                else
                {
                    auto deviceId = src->property(deviceIdPropName.toUtf8()).toString();
                    auto deviceName = src->property("name").toString();
                    addDevice(deviceId, deviceName, deviceType);
                }
            }

            devices = g_list_remove(devices, device);
            gst_object_unref(device);
        }
        gst_device_monitor_stop(monitor);
    }

    gst_object_unref(monitor);

#if (defined WITH_LIBAVC1394) && defined (WITH_LIBRAW1394)
    if (QGst::ElementFactory::find("dv1394src"))
    {
        rom1394_directory directory;
        raw1394handle_t handle = raw1394_new_handle ();

        if (handle)
        {
            auto num_ports = raw1394_get_port_info(handle, nullptr, 0);
            for (int port = 0; port < num_ports; port++)
            {
                if (raw1394_set_port (handle, port) < 0)
                {
                    continue;
                }

                auto num_nodes = raw1394_get_nodecount (handle);
                for (int node = 0; node < num_nodes; node++)
                {
                    rom1394_get_directory (handle, node, &directory);
                    if (rom1394_get_node_type (&directory) == ROM1394_NODE_TYPE_AVC
                        && avc1394_check_subunit_type (handle, node, AVC1394_SUBUNIT_TYPE_VCR))
                    {
                        addDevice(QString::number(rom1394_get_guid(handle, node)),
                            "DV source", "dv1394src");
                    }
                }
            }
            raw1394_destroy_handle(handle);
        }
    }
#endif

}

void VideoSources::addDevice
    ( const QString& deviceId
    , const QString& deviceName
    , const QString& deviceType
    )
{
    foreach (auto item, listSources->findItems(deviceId, Qt::MatchStartsWith))
    {
        auto currDeviceId   = item->data(0, Qt::UserRole).toString();
        auto currDeviceName = item->data(1, Qt::UserRole).toString();

        if (currDeviceName == deviceName && currDeviceId == deviceId)
        {
            // Already known source.
            return;
        }
    }

    auto alias = QString("src%1").arg(listSources->topLevelItemCount());
    QVariantMap parameters;
    parameters["alias"] = alias;
    parameters["device-type"] = deviceType;
    listSources->addTopLevelItem(newItem(deviceName, deviceId, parameters, false));
}

void VideoSources::onTreeItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *)
{
    btnDetails->setEnabled(current != nullptr);
    btnRemove->setEnabled(current != nullptr);
}

void VideoSources::onItemDoubleClicked(QTreeWidgetItem*, int)
{
    onEditClicked();
}

void VideoSources::onEditClicked()
{
    auto item = listSources->currentItem();
    if (!item)
    {
        return;
    }
    auto device     = item->data(0, Qt::UserRole).toString();
    auto parameters = item->data(2, Qt::UserRole).toMap();

    QWaitCursor wait(this);
    VideoSourceDetails dlg(parameters, this);
    dlg.setWindowTitle(item->text(0));
    dlg.updateDevice(device);

    if (dlg.exec())
    {
        dlg.getParameters(parameters);
        item->setData(2, Qt::UserRole, parameters);
        item->setText(1, parameters["modality"].toString());
        item->setText(2, parameters["alias"].toString());
    }
}

void VideoSources::onRemoveClicked()
{
    auto item = listSources->currentItem();
    delete item;
}

void VideoSources::onAddScreenCaptureClicked()
{
    QVariantMap parameters;
    parameters["alias"] = QString("src%1").arg(listSources->topLevelItemCount());
    parameters["device-type"] = PLATFORM_SPECIFIC_SCREEN_CAPTURE;
    listSources->addTopLevelItem(newItem("", "Screen capture", parameters, true));
}

void VideoSources::onAddTestClicked()
{
    QVariantMap parameters;
    parameters["alias"] = QString("src%1").arg(listSources->topLevelItemCount());
    parameters["device-type"] = "videotestsrc";
    listSources->addTopLevelItem(newItem("", "Video test source", parameters, true));
}

void VideoSources::save(QSettings& settings)
{
    QWaitCursor wait(this);
    settings.beginGroup("gst");
    settings.beginWriteArray("src");

    for (int i = 0; i < listSources->topLevelItemCount(); ++i)
    {
        settings.setArrayIndex(i);
        auto item = listSources->topLevelItem(i);
        settings.setValue("device", item->data(0, Qt::UserRole));
        settings.setValue("device-name", item->data(1, Qt::UserRole));
        settings.setValue("enabled", item->checkState(0) == Qt::Checked);
        auto parameters = item->data(2, Qt::UserRole).toMap();
        for (auto p = parameters.begin(); p != parameters.end(); ++p)
        {
            settings.setValue(p.key(), p.value());
        }
    }
    settings.endArray();
    settings.endGroup();
}
