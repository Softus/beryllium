#include "enumsrc.h"
#include <QStringList>
#include <libavc1394/avc1394.h>
#include <libavc1394/rom1394.h>
#include <gudev/gudev.h>

static QStringList enum1394Devices()
{
  QStringList ret;
  rom1394_directory directory;
  raw1394handle_t handle = raw1394_new_handle ();

  if (handle)
  {
      auto num_ports = raw1394_get_port_info (handle, nullptr, 0);
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
                if (rom1394_get_node_type (&directory) == ROM1394_NODE_TYPE_AVC &&
                    avc1394_check_subunit_type (handle, node, AVC1394_SUBUNIT_TYPE_VCR))
                {
                    ret << QString::number(rom1394_get_guid(handle, node));
                }
            }
        }
    }

    return ret;
}

static QStringList enumV4l2Devices()
{
    QStringList ret;
    static const gchar *subsystems[] = { "video4linux", nullptr };
    auto client = g_udev_client_new(subsystems);
    auto devices = g_udev_client_query_by_subsystem(client, subsystems[0]);

    while (devices)
    {
        GUdevDevice *udev_device = (GUdevDevice *)devices->data;

        if (g_udev_device_get_property_as_int(udev_device, "ID_V4L_VERSION") >= 2)
        {
            ret << g_udev_device_get_device_file(udev_device);;
        }
        devices = g_list_remove (devices, udev_device);
        g_object_unref(udev_device);
    }

    g_object_unref(client);
    return ret;
}

QStringList enumSources(const QString& elmName, const QString&)
{
    return elmName == "dv1394src"? enum1394Devices(): enumV4l2Devices();
}
