/*
 * Copyright (C) 2013-2015 Irkutsk Diagnostic Center.
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

#include <../src/product.h>
#include <gst/gst.h>

#include "soup/gstsouphttpclientsink.h"
#include "rtmp/gstrtmpsink.h"

#if !GST_CHECK_VERSION(1,0,0)
#define MPEG_SYS_CAPS gst_static_caps_get(&mpeg_sys_caps)

static const gchar *mpeg_sys_exts[] = { "mpe", "mpeg", "mpg", NULL };

/*** video/mpeg systemstream ***/
static GstStaticCaps mpeg_sys_caps = GST_STATIC_CAPS ("video/mpeg, "
    "systemstream = (boolean) true, mpegversion = (int) [ 1, 2 ]");

extern void mpeg_sys_type_find (GstTypeFind * tf, gpointer /*unused*/);
#endif

extern "C" gboolean gst_motion_cells_plugin_init (GstPlugin * plugin);

bool gstApplyFixes()
{
    return gst_plugin_register_static(GST_VERSION_MAJOR, GST_VERSION_MINOR, "motioncells", "",
        gst_motion_cells_plugin_init, PRODUCT_VERSION_STR, "LGPL", "gst", PRODUCT_SHORT_NAME, PRODUCT_SITE_URL)

    && gst_element_register (nullptr, "rtmpsink", GST_RANK_NONE, GST_TYPE_RTMP_SINK)

#if !GST_CHECK_VERSION(1,0,0)
    && gst_element_register (nullptr, "souphttpclientsink", GST_RANK_NONE,
        GST_TYPE_SOUP_HTTP_CLIENT_SINK)
    && gst_type_find_register(nullptr, "video/mpegps", GST_RANK_NONE,
        mpeg_sys_type_find, (gchar **)mpeg_sys_exts, MPEG_SYS_CAPS, NULL, NULL)
#endif
    ;
}
