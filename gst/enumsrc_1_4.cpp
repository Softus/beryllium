#include "enumsrc.h"
#include <QStringList>
#include <gst/gst.h>

QStringList enumSources(const QString&, const QString&)
{
    QStringList ret;
    auto monitor = gst_device_monitor_new();
    if (gst_device_monitor_start(monitor))
    {
        auto devices = gst_device_monitor_get_devices(monitor);
        while (devices)
        {
            GstDevice *device = (GstDevice *)devices->data;
            ret << QString::fromUtf8(gst_device_get_display_name(device));

            devices = g_list_remove(devices, device);
            gst_object_unref(device);
        }
    }

    gst_object_unref(monitor);
    return ret;
}
