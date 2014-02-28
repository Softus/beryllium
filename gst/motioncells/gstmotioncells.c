/*
 * GStreamer MotioCells detect areas of motion
 * Copyright (C) 2011 Robert Jobbagy <jobbagy.robert@gmail.com>
 * Copyright (C) 2011 Nicola Murino <nicola.murino@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-motioncells
 *
 * Performs motion detection on videos.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 videotestsrc pattern=18 ! videorate ! videoscale ! video/x-raw-yuv,width=320,height=240,framerate=5/1 ! ffmpegcolorspace ! motioncells ! ffmpegcolorspace ! xvimagesink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <limits.h>

#include <glib.h>
#include "gstmotioncells.h"
#include "motioncells_wrapper.h"
#include <gst/video/video.h>

GST_DEBUG_CATEGORY_STATIC (gst_motion_cells_debug);
#define GST_CAT_DEFAULT gst_motion_cells_debug

#define GRID_DEF 10
#define GRID_MIN 8
#define GRID_MAX 32
#define SENSITIVITY_DEFAULT 0.5
#define SENSITIVITY_MIN 0
#define SENSITIVITY_MAX 1
#define THRESHOLD_MIN 0
#define THRESHOLD_DEFAULT 0.01
#define THRESHOLD_MAX 1.0
#define GAP_MIN 1
#define GAP_DEF 5
#define GAP_MAX 60
#define POST_NO_MOTION_MIN 0
#define POST_NO_MOTION_DEF 0
#define POST_NO_MOTION_MAX 180
#define MINIMUM_MOTION_FRAMES_MIN 1
#define MINIMUM_MOTION_FRAMES_DEF 1
#define MINIMUM_MOTION_FRAMES_MAX 60
#define THICKNESS_MIN -1
#define THICKNESS_DEF 1
#define THICKNESS_MAX 5
#define DATE_MIN 0
#define DATE_DEF 1
#define DATE_MAX LONG_MAX
#define DEF_DATAFILEEXT "vamc"
#define MSGLEN 6
#define BUSMSGLEN 20

#define GFREE(POINTER)\
		{\
			g_free(POINTER);\
			POINTER = NULL;\
		}

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_GRID_X,
  PROP_GRID_Y,
  PROP_SENSITIVITY,
  PROP_THRESHOLD,
  PROP_DISPLAY,
  PROP_DATE,
  PROP_DATAFILE,
  PROP_DATAFILE_EXT,
  PROP_MOTIONMASKCOORD,
  PROP_MOTIONMASKCELLSPOS,
  PROP_CELLSCOLOR,
  PROP_MOTIONCELLSIDX,
  PROP_GAP,
  PROP_POSTNOMOTION,
  PROP_MINIMUNMOTIONFRAMES,
  PROP_CALCULATEMOTION,
  PROP_POSTALLMOTION,
  PROP_USEALPHA,
  PROP_MOTIONCELLTHICKNESS
};

/* the capabilities of the inputs and outputs.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_RGB));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_RGB));

GST_BOILERPLATE (GstMotioncells, gst_motion_cells, GstElement,
    GST_TYPE_ELEMENT);

static void gst_motion_cells_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_motion_cells_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_motion_cells_set_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_motion_cells_chain (GstPad * pad, GstBuffer * buf);

static void gst_motioncells_update_motion_cells (GstMotioncells * filter);
static void gst_motioncells_update_motion_masks (GstMotioncells * filter);

/* Clean up */
static void
gst_motion_cells_finalize (GObject * obj)
{
  GstMotioncells *filter = gst_motion_cells (obj);

  motion_cells_free (filter->id);

  //freeing previously allocated dynamic array
  if (filter->motionmaskcoord_count > 0) {
    GFREE (filter->motionmaskcoords);
  }

  if (filter->motionmaskcells_count > 0) {
    GFREE (filter->motionmaskcellsidx);
  }
  if (filter->motioncells_count > 0) {
    GFREE (filter->motioncellsidx);
  }

  if (filter->cvImage) {
    cvReleaseImage (&filter->cvImage);
  }

  GFREE (filter->motioncellscolor);
  GFREE (filter->prev_datafile);
  GFREE (filter->cur_datafile);
  GFREE (filter->basename_datafile);
  GFREE (filter->datafile_extension);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/* GObject vmethod implementations */
static void
gst_motion_cells_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "motioncells",
      "Filter/Effect/Video",
      "Performs motion detection on videos and images, providing detected motion cells index via bus messages",
      "Robert Jobbagy <jobbagy dot robert at gmail dot com>, Nicola Murino <nicola dot murino at gmail.com>");

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);
}

/* initialize the motioncells's class */
static void
gst_motion_cells_class_init (GstMotioncellsClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;
  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_motion_cells_finalize);
  gobject_class->set_property = gst_motion_cells_set_property;
  gobject_class->get_property = gst_motion_cells_get_property;

  g_object_class_install_property (gobject_class, PROP_GRID_X,
      g_param_spec_int ("gridx", "Number of Horizontal Grids",
          "You can give number of horizontal grid cells.", GRID_MIN, GRID_MAX,
          GRID_DEF, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_GRID_Y,
      g_param_spec_int ("gridy", "Number of Vertical Grids",
          "You can give number of vertical grid cells.", GRID_MIN, GRID_MAX,
          GRID_DEF, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SENSITIVITY,
      g_param_spec_double ("sensitivity", "Motion Sensitivity",
          "You can tunning the element motion sensitivity.", SENSITIVITY_MIN,
          SENSITIVITY_MAX, SENSITIVITY_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_THRESHOLD,
      g_param_spec_double ("threshold", "Lower bound of motion cells number",
          "Threshold value for motion, when motion cells number greater sum cells * threshold, we show motion.",
          THRESHOLD_MIN, THRESHOLD_MAX, THRESHOLD_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_GAP,
      g_param_spec_int ("gap",
          "Gap is time in second, elapsed time from last motion timestamp. ",
          "If elapsed time minus form last motion timestamp is greater or equal than gap then we post motion finished bus message. ",
          GAP_MIN, GAP_MAX, GAP_DEF,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_POSTNOMOTION,
      g_param_spec_int ("postnomotion", "POSTNOMOTION",
          "If non 0 post a no_motion event is posted on the bus if no motion is detected for N seconds",
          POST_NO_MOTION_MIN, POST_NO_MOTION_MAX, POST_NO_MOTION_DEF,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MINIMUNMOTIONFRAMES,
      g_param_spec_int ("minimummotionframes", "MINIMUN MOTION FRAMES",
          "Define the minimum number of motion frames that trigger a motion event",
          MINIMUM_MOTION_FRAMES_MIN, MINIMUM_MOTION_FRAMES_MAX,
          MINIMUM_MOTION_FRAMES_DEF,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DISPLAY,
      g_param_spec_boolean ("display", "Display",
          "Motion Cells visible or not on Current Frame", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_POSTALLMOTION,
      g_param_spec_boolean ("postallmotion", "Post All Motion",
          "Element post bus msg for every motion frame or just motion start and motion stop",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_USEALPHA,
      g_param_spec_boolean ("usealpha", "Use alpha",
          "Use or not alpha blending on frames with motion cells", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DATE,
      g_param_spec_long ("date", "Motion Cell Date",
          "Current Date in milliseconds", DATE_MIN, DATE_MAX, DATE_DEF,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DATAFILE,
      g_param_spec_string ("datafile", "DataFile",
          "Location of motioncells data file (empty string means no saving)",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DATAFILE_EXT,
      g_param_spec_string ("datafileextension", "DataFile Extension",
          "Extension of datafile", DEF_DATAFILEEXT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MOTIONMASKCOORD,
      g_param_spec_string ("motionmaskcoords", "Motion Mask with Coordinates",
          "The upper left x, y and lower right x, y coordinates separated with \":\", "
          "describe a region. Regions separated with \",\"", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MOTIONMASKCELLSPOS,
      g_param_spec_string ("motionmaskcellspos",
          "Motion Mask with Cells Position",
          "The line and column idx separated with \":\" what cells want we mask-out, "
          "describe a cell. Cells separated with \",\"", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CELLSCOLOR,
      g_param_spec_string ("cellscolor", "Color of Motion Cells",
          "The color of motion cells separated with \",\"", "255,255,0",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MOTIONCELLSIDX,
      g_param_spec_string ("motioncellsidx", "Motion Cells Of Interest(MOCI)",
          "The line and column idx separated with \":\", "
          "describe a cell. Cells separated with \",\"", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CALCULATEMOTION,
      g_param_spec_boolean ("calculatemotion", "Calculate Motion",
          "If needs calculate motion on frame you need this property setting true otherwise false",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MOTIONCELLTHICKNESS,
      g_param_spec_int ("motioncellthickness", "Motion Cell Thickness",
          "Motion Cell Border Thickness, if it's -1 then motion cell will be fill",
          THICKNESS_MIN, THICKNESS_MAX, THICKNESS_DEF,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_motion_cells_init (GstMotioncells * filter, GstMotioncellsClass * gclass)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_setcaps_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_motion_cells_set_caps));
  gst_pad_set_getcaps_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_pad_proxy_getcaps));
  gst_pad_set_chain_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_motion_cells_chain));

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_getcaps_function (filter->srcpad,
      GST_DEBUG_FUNCPTR (gst_pad_proxy_getcaps));

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->display = TRUE;
  filter->calculate_motion = TRUE;

  filter->prevgridx = 0;
  filter->prevgridy = 0;
  filter->gridx = GRID_DEF;
  filter->gridy = GRID_DEF;
  filter->gap = GAP_DEF;
  filter->postnomotion = POST_NO_MOTION_DEF;
  filter->minimum_motion_frames = MINIMUM_MOTION_FRAMES_DEF;

  filter->prev_datafile = g_strdup (NULL);
  filter->cur_datafile = g_strdup (NULL);
  filter->basename_datafile = g_strdup (NULL);
  filter->datafile_extension = g_strdup (DEF_DATAFILEEXT);
  filter->sensitivity = SENSITIVITY_DEFAULT;
  filter->threshold = THRESHOLD_DEFAULT;

  filter->motionmaskcoord_count = 0;
  filter->motionmaskcoords = NULL;
  filter->motionmaskcells_count = 0;
  filter->motionmaskcellsidx = NULL;
  filter->motioncellscolor = g_new0 (cellscolor, 1);
  filter->motioncellscolor->R_channel_value = 255;
  filter->motioncellscolor->G_channel_value = 255;
  filter->motioncellscolor->B_channel_value = 0;
  filter->motioncellsidx = NULL;
  filter->motioncells_count = 0;
  filter->motion_begin_timestamp = 0;
  filter->last_motion_timestamp = 0;
  filter->last_nomotion_notified = 0;
  filter->consecutive_motion = 0;
  filter->motion_timestamp = 0;
  filter->prev_buff_timestamp = 0;
  filter->cur_buff_timestamp = 0;
  filter->diff_timestamp = -1;
  gettimeofday (&filter->tv, NULL);
  filter->starttime = 1000 * filter->tv.tv_sec;
  filter->previous_motion = false;
  filter->changed_datafile = false;
  filter->postallmotion = false;
  filter->usealpha = true;
  filter->firstdatafile = false;
  filter->firstgridx = true;
  filter->firstgridy = true;
  filter->changed_gridx = false;
  filter->changed_gridy = false;
  filter->firstframe = true;
  filter->changed_startime = false;
  filter->sent_init_error_msg = false;
  filter->sent_save_error_msg = false;
  filter->thickness = THICKNESS_DEF;

  filter->datafileidx = 0;
  filter->id = motion_cells_init ();

}

static void
gst_motion_cells_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMotioncells *filter = gst_motion_cells (object);
  //variables for overlay regions setup
  gchar **strs, **colorstr, **motioncellsstr, **motionmaskcellsstr;
  int i, ux, uy, lx, ly;
  int r, g, b;
  int cellscolorscnt = 0;
  int linidx, colidx, masklinidx, maskcolidx;
  int tmpux = -1;
  int tmpuy = -1;
  int tmplx = -1;
  int tmply = -1;

  GST_OBJECT_LOCK (filter);
  switch (prop_id) {
    case PROP_GRID_X:
      filter->gridx = g_value_get_int (value);
      if (filter->prevgridx != filter->gridx && !filter->firstframe) {
        filter->changed_gridx = true;
      }
      filter->prevgridx = filter->gridx;
      break;
    case PROP_GRID_Y:
      filter->gridy = g_value_get_int (value);
      if (filter->prevgridy != filter->gridy && !filter->firstframe) {
        filter->changed_gridy = true;
      }
      filter->prevgridy = filter->gridy;
      break;
    case PROP_GAP:
      filter->gap = g_value_get_int (value);
      break;
    case PROP_POSTNOMOTION:
      filter->postnomotion = g_value_get_int (value);
      break;
    case PROP_MINIMUNMOTIONFRAMES:
      filter->minimum_motion_frames = g_value_get_int (value);
      break;
    case PROP_SENSITIVITY:
      filter->sensitivity = g_value_get_double (value);
      break;
    case PROP_THRESHOLD:
      filter->threshold = g_value_get_double (value);
      break;
    case PROP_DISPLAY:
      filter->display = g_value_get_boolean (value);
      break;
    case PROP_POSTALLMOTION:
      filter->postallmotion = g_value_get_boolean (value);
      break;
    case PROP_USEALPHA:
      filter->usealpha = g_value_get_boolean (value);
      break;
    case PROP_CALCULATEMOTION:
      filter->calculate_motion = g_value_get_boolean (value);
      break;
    case PROP_DATE:
      if (!filter->firstframe) {
        filter->changed_startime = true;
      }
      filter->starttime = g_value_get_long (value);
      break;
    case PROP_DATAFILE:
      GFREE (filter->cur_datafile);
      GFREE (filter->basename_datafile);
      filter->basename_datafile = g_value_dup_string (value);

      if (strlen (filter->basename_datafile) == 0) {
        filter->cur_datafile = g_strdup (NULL);
        break;
      }
      filter->cur_datafile =
          g_strdup_printf ("%s-0.%s", filter->basename_datafile,
          filter->datafile_extension);
      if (g_strcmp0 (filter->prev_datafile, filter->basename_datafile) != 0) {
        filter->changed_datafile = TRUE;
        filter->sent_init_error_msg = FALSE;
        filter->sent_save_error_msg = FALSE;
        filter->datafileidx = 0;
        motion_cells_free_resources (filter->id);
      } else {
        filter->changed_datafile = FALSE;
      }

      GFREE (filter->prev_datafile);
      filter->prev_datafile = g_strdup (filter->basename_datafile);
      break;
    case PROP_DATAFILE_EXT:
      GFREE (filter->datafile_extension);
      filter->datafile_extension = g_value_dup_string (value);
      break;
    case PROP_MOTIONMASKCOORD:
      strs = g_strsplit (g_value_get_string (value), ",", 255);
      GFREE (filter->motionmaskcoords);
      //setting number of regions
      for (filter->motionmaskcoord_count = 0;
          strs[filter->motionmaskcoord_count] != NULL;
          ++filter->motionmaskcoord_count);
      if (filter->motionmaskcoord_count > 0) {
        sscanf (strs[0], "%d:%d:%d:%d", &tmpux, &tmpuy, &tmplx, &tmply);
        if (tmpux > -1 && tmpuy > -1 && tmplx > -1 && tmply > -1) {
          filter->motionmaskcoords =
              g_new0 (motionmaskcoordrect, filter->motionmaskcoord_count);

          for (i = 0; i < filter->motionmaskcoord_count; ++i) {
            sscanf (strs[i], "%d:%d:%d:%d", &ux, &uy, &lx, &ly);
            ux = CLAMP (ux, 0, filter->width - 1);
            uy = CLAMP (uy, 0, filter->height - 1);
            lx = CLAMP (lx, 0, filter->width - 1);
            ly = CLAMP (ly, 0, filter->height - 1);
            filter->motionmaskcoords[i].upper_left_x = ux;
            filter->motionmaskcoords[i].upper_left_y = uy;
            filter->motionmaskcoords[i].lower_right_x = lx;
            filter->motionmaskcoords[i].lower_right_y = ly;
          }
        } else {
          filter->motionmaskcoord_count = 0;
        }
      }
      if (strs)
        g_strfreev (strs);
      tmpux = -1;
      tmpuy = -1;
      tmplx = -1;
      tmply = -1;
      break;
    case PROP_MOTIONMASKCELLSPOS:
      motionmaskcellsstr = g_strsplit (g_value_get_string (value), ",", 255);
      GFREE (filter->motionmaskcellsidx);
      //setting number of regions
      for (filter->motionmaskcells_count = 0;
          motionmaskcellsstr[filter->motionmaskcells_count] != NULL;
          ++filter->motionmaskcells_count);
      if (filter->motionmaskcells_count > 0) {
        sscanf (motionmaskcellsstr[0], "%d:%d", &tmpux, &tmpuy);
        if (tmpux > -1 && tmpuy > -1) {
          filter->motionmaskcellsidx =
              g_new0 (motioncellidx, filter->motionmaskcells_count);
          for (i = 0; i < filter->motionmaskcells_count; ++i) {
            sscanf (motionmaskcellsstr[i], "%d:%d", &masklinidx, &maskcolidx);
            filter->motionmaskcellsidx[i].lineidx = masklinidx;
            filter->motionmaskcellsidx[i].columnidx = maskcolidx;
          }
        } else {
          filter->motionmaskcells_count = 0;
        }
      }
      if (motionmaskcellsstr)
        g_strfreev (motionmaskcellsstr);
      tmpux = -1;
      tmpuy = -1;
      tmplx = -1;
      tmply = -1;
      break;
    case PROP_CELLSCOLOR:
      colorstr = g_strsplit (g_value_get_string (value), ",", 255);
      for (cellscolorscnt = 0; colorstr[cellscolorscnt] != NULL;
          ++cellscolorscnt);
      if (cellscolorscnt == 3) {
        sscanf (colorstr[0], "%d", &r);
        sscanf (colorstr[1], "%d", &g);
        sscanf (colorstr[2], "%d", &b);
        //check right RGB color format
        r = CLAMP (r, 1, 255);
        g = CLAMP (g, 1, 255);
        b = CLAMP (b, 1, 255);
        filter->motioncellscolor->R_channel_value = r;
        filter->motioncellscolor->G_channel_value = g;
        filter->motioncellscolor->B_channel_value = b;
      }
      if (colorstr)
        g_strfreev (colorstr);
      break;
    case PROP_MOTIONCELLSIDX:
      motioncellsstr = g_strsplit (g_value_get_string (value), ",", 255);

      //setting number of regions
      for (filter->motioncells_count = 0;
          motioncellsstr[filter->motioncells_count] != NULL;
          ++filter->motioncells_count);
      if (filter->motioncells_count > 0) {
        sscanf (motioncellsstr[0], "%d:%d", &tmpux, &tmpuy);
        if (tmpux > -1 && tmpuy > -1) {
          GFREE (filter->motioncellsidx);

          filter->motioncellsidx =
              g_new0 (motioncellidx, filter->motioncells_count);

          for (i = 0; i < filter->motioncells_count; ++i) {
            sscanf (motioncellsstr[i], "%d:%d", &linidx, &colidx);
            filter->motioncellsidx[i].lineidx = linidx;
            filter->motioncellsidx[i].columnidx = colidx;
          }
        } else {
          filter->motioncells_count = 0;
        }
      }
      if (motioncellsstr)
        g_strfreev (motioncellsstr);
      tmpux = -1;
      tmpuy = -1;
      tmplx = -1;
      tmply = -1;
      break;
    case PROP_MOTIONCELLTHICKNESS:
      filter->thickness = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (filter);
}

static void
gst_motion_cells_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMotioncells *filter = gst_motion_cells (object);
  GString *str;
  int i;

  GST_OBJECT_LOCK (filter);
  switch (prop_id) {
    case PROP_GRID_X:
      g_value_set_int (value, filter->gridx);
      break;
    case PROP_GRID_Y:
      g_value_set_int (value, filter->gridy);
      break;
    case PROP_GAP:
      g_value_set_int (value, filter->gap);
      break;
    case PROP_POSTNOMOTION:
      g_value_set_int (value, filter->postnomotion);
      break;
    case PROP_MINIMUNMOTIONFRAMES:
      g_value_set_int (value, filter->minimum_motion_frames);
      break;
    case PROP_SENSITIVITY:
      g_value_set_double (value, filter->sensitivity);
      break;
    case PROP_THRESHOLD:
      g_value_set_double (value, filter->threshold);
      break;
    case PROP_DISPLAY:
      g_value_set_boolean (value, filter->display);
      break;
    case PROP_POSTALLMOTION:
      g_value_set_boolean (value, filter->postallmotion);
      break;
    case PROP_USEALPHA:
      g_value_set_boolean (value, filter->usealpha);
      break;
    case PROP_CALCULATEMOTION:
      g_value_set_boolean (value, filter->calculate_motion);
      break;
    case PROP_DATE:
      g_value_set_long (value, filter->starttime);
      break;
    case PROP_DATAFILE:
      g_value_set_string (value, filter->basename_datafile);
      break;
    case PROP_DATAFILE_EXT:
      g_value_set_string (value, filter->datafile_extension);
      break;
    case PROP_MOTIONMASKCOORD:
      str = g_string_new ("");
      for (i = 0; i < filter->motionmaskcoord_count; ++i) {
        if (i < filter->motionmaskcoord_count - 1)
          g_string_append_printf (str, "%d:%d:%d:%d,",
              filter->motionmaskcoords[i].upper_left_x,
              filter->motionmaskcoords[i].upper_left_y,
              filter->motionmaskcoords[i].lower_right_x,
              filter->motionmaskcoords[i].lower_right_y);
        else
          g_string_append_printf (str, "%d:%d:%d:%d",
              filter->motionmaskcoords[i].upper_left_x,
              filter->motionmaskcoords[i].upper_left_y,
              filter->motionmaskcoords[i].lower_right_x,
              filter->motionmaskcoords[i].lower_right_y);

      }
      g_value_set_string (value, str->str);
      g_string_free (str, TRUE);
      break;
    case PROP_MOTIONMASKCELLSPOS:
      str = g_string_new ("");
      for (i = 0; i < filter->motionmaskcells_count; ++i) {
        if (i < filter->motionmaskcells_count - 1)
          g_string_append_printf (str, "%d:%d,",
              filter->motionmaskcellsidx[i].lineidx,
              filter->motionmaskcellsidx[i].columnidx);
        else
          g_string_append_printf (str, "%d:%d",
              filter->motionmaskcellsidx[i].lineidx,
              filter->motionmaskcellsidx[i].columnidx);
      }
      g_value_set_string (value, str->str);
      g_string_free (str, TRUE);
      break;
    case PROP_CELLSCOLOR:
      str = g_string_new ("");

      g_string_printf (str, "%d,%d,%d",
          filter->motioncellscolor->R_channel_value,
          filter->motioncellscolor->G_channel_value,
          filter->motioncellscolor->B_channel_value);

      g_value_set_string (value, str->str);
      g_string_free (str, TRUE);
      break;
    case PROP_MOTIONCELLSIDX:
      str = g_string_new ("");
      for (i = 0; i < filter->motioncells_count; ++i) {
        if (i < filter->motioncells_count - 1)
          g_string_append_printf (str, "%d:%d,",
              filter->motioncellsidx[i].lineidx,
              filter->motioncellsidx[i].columnidx);
        else
          g_string_append_printf (str, "%d:%d",
              filter->motioncellsidx[i].lineidx,
              filter->motioncellsidx[i].columnidx);
      }
      g_value_set_string (value, str->str);
      g_string_free (str, TRUE);
      break;
    case PROP_MOTIONCELLTHICKNESS:
      g_value_set_int (value, filter->thickness);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (filter);
}

static void
gst_motioncells_update_motion_cells (GstMotioncells * filter)
{
  int i = 0;
  int cellscnt = 0;
  int j = 0;
  int newcellscnt;
  motioncellidx *motioncellsidx;
  for (i = 0; i < filter->motioncells_count; i++) {
    if ((filter->gridx <= filter->motioncellsidx[i].columnidx) ||
        (filter->gridy <= filter->motioncellsidx[i].lineidx)) {
      cellscnt++;
    }
  }
  newcellscnt = filter->motioncells_count - cellscnt;
  motioncellsidx = g_new0 (motioncellidx, newcellscnt);
  for (i = 0; i < filter->motioncells_count; i++) {
    if ((filter->motioncellsidx[i].lineidx < filter->gridy) &&
        (filter->motioncellsidx[i].columnidx < filter->gridx)) {
      motioncellsidx[j].lineidx = filter->motioncellsidx[i].lineidx;
      motioncellsidx[j].columnidx = filter->motioncellsidx[i].columnidx;
      j++;
    }
  }
  GFREE (filter->motioncellsidx);
  filter->motioncells_count = newcellscnt;
  filter->motioncellsidx = g_new0 (motioncellidx, filter->motioncells_count);
  j = 0;
  for (i = 0; i < filter->motioncells_count; i++) {
    filter->motioncellsidx[i].lineidx = motioncellsidx[j].lineidx;
    filter->motioncellsidx[i].columnidx = motioncellsidx[j].columnidx;
    j++;
  }
  GFREE (motioncellsidx);
}

static void
gst_motioncells_update_motion_masks (GstMotioncells * filter)
{

  int i = 0;
  int maskcnt = 0;
  int j = 0;
  int newmaskcnt;
  motioncellidx *motionmaskcellsidx;
  for (i = 0; i < filter->motionmaskcells_count; i++) {
    if ((filter->gridx <= filter->motionmaskcellsidx[i].columnidx) ||
        (filter->gridy <= filter->motionmaskcellsidx[i].lineidx)) {
      maskcnt++;
    }
  }
  newmaskcnt = filter->motionmaskcells_count - maskcnt;
  motionmaskcellsidx = g_new0 (motioncellidx, newmaskcnt);
  for (i = 0; i < filter->motionmaskcells_count; i++) {
    if ((filter->motionmaskcellsidx[i].lineidx < filter->gridy) &&
        (filter->motionmaskcellsidx[i].columnidx < filter->gridx)) {
      motionmaskcellsidx[j].lineidx = filter->motionmaskcellsidx[i].lineidx;
      motionmaskcellsidx[j].columnidx = filter->motionmaskcellsidx[i].columnidx;
      j++;
    }
  }
  GFREE (filter->motionmaskcellsidx);
  filter->motionmaskcells_count = newmaskcnt;
  filter->motionmaskcellsidx =
      g_new0 (motioncellidx, filter->motionmaskcells_count);
  j = 0;
  for (i = 0; i < filter->motionmaskcells_count; i++) {
    filter->motionmaskcellsidx[i].lineidx = motionmaskcellsidx[j].lineidx;
    filter->motionmaskcellsidx[i].columnidx = motionmaskcellsidx[j].columnidx;
    j++;
  }
  GFREE (motionmaskcellsidx);
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_motion_cells_set_caps (GstPad * pad, GstCaps * caps)
{
  GstMotioncells *filter;
  GstPad *otherpad;
  GstStructure *structure;
  int numerator, denominator;

  filter = gst_motion_cells (gst_pad_get_parent (pad));
  structure = gst_caps_get_structure (caps, 0);
  gst_structure_get_int (structure, "width", &filter->width);
  gst_structure_get_int (structure, "height", &filter->height);
  gst_structure_get_fraction (structure, "framerate", &numerator, &denominator);
  filter->framerate = (double) numerator / (double) denominator;
  if (filter->cvImage)
    cvReleaseImage (&filter->cvImage);
  filter->cvImage =
      cvCreateImage (cvSize (filter->width, filter->height), IPL_DEPTH_8U, 3);

  otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
  gst_object_unref (filter);

  return gst_pad_set_caps (otherpad, caps);
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_motion_cells_chain (GstPad * pad, GstBuffer * buf)
{
  GstMotioncells *filter;
  filter = gst_motion_cells (GST_OBJECT_PARENT (pad));
  GST_OBJECT_LOCK (filter);
  if (filter->calculate_motion) {
    double sensitivity;
    int framerate, gridx, gridy, motionmaskcells_count, motionmaskcoord_count,
        motioncells_count, i;
    int thickness, success, motioncellsidxcnt, numberOfCells,
        motioncellsnumber, cellsOfInterestNumber;
    int mincellsOfInterestNumber, motiondetect, minimum_motion_frames,
        postnomotion;
    char *datafile;
    bool display, changed_datafile, useAlpha;
    gint64 starttime;
    motionmaskcoordrect *motionmaskcoords;
    motioncellidx *motionmaskcellsidx;
    cellscolor motioncellscolor;
    motioncellidx *motioncellsidx;
    buf = gst_buffer_make_writable (buf);
    filter->cvImage->imageData = (char *) GST_BUFFER_DATA (buf);
    if (filter->firstframe) {
      setPrevFrame (filter->cvImage, filter->id);
      filter->firstframe = FALSE;
    }
    minimum_motion_frames = filter->minimum_motion_frames;
    postnomotion = filter->postnomotion;
    sensitivity = filter->sensitivity;
    framerate = filter->framerate;
    gridx = filter->gridx;
    gridy = filter->gridy;
    display = filter->display;
    motionmaskcoord_count = filter->motionmaskcoord_count;
    motionmaskcoords =
        g_new0 (motionmaskcoordrect, filter->motionmaskcoord_count);
    for (i = 0; i < filter->motionmaskcoord_count; i++) {       //we need divide 2 because we use gauss pyramid in C++ side
      motionmaskcoords[i].upper_left_x =
          filter->motionmaskcoords[i].upper_left_x / 2;
      motionmaskcoords[i].upper_left_y =
          filter->motionmaskcoords[i].upper_left_y / 2;
      motionmaskcoords[i].lower_right_x =
          filter->motionmaskcoords[i].lower_right_x / 2;
      motionmaskcoords[i].lower_right_y =
          filter->motionmaskcoords[i].lower_right_y / 2;
    }

    motioncellscolor.R_channel_value =
        filter->motioncellscolor->R_channel_value;
    motioncellscolor.G_channel_value =
        filter->motioncellscolor->G_channel_value;
    motioncellscolor.B_channel_value =
        filter->motioncellscolor->B_channel_value;

    if ((filter->changed_gridx || filter->changed_gridy
            || filter->changed_startime)) {
      if ((g_strcmp0 (filter->cur_datafile, NULL) != 0)) {
        GFREE (filter->cur_datafile);
        filter->datafileidx++;
        filter->cur_datafile =
            g_strdup_printf ("%s-%d.%s", filter->basename_datafile,
            filter->datafileidx, filter->datafile_extension);
        filter->changed_datafile = TRUE;
        motion_cells_free_resources (filter->id);
      }
      if (filter->motioncells_count > 0)
        gst_motioncells_update_motion_cells (filter);
      if (filter->motionmaskcells_count > 0)
        gst_motioncells_update_motion_masks (filter);
      filter->changed_gridx = FALSE;
      filter->changed_gridy = FALSE;
      filter->changed_startime = FALSE;
    }
    datafile = g_strdup (filter->cur_datafile);
    filter->cur_buff_timestamp = (GST_BUFFER_TIMESTAMP (buf) / GST_MSECOND);
    filter->starttime +=
        (filter->cur_buff_timestamp - filter->prev_buff_timestamp);
    starttime = filter->starttime;
    if (filter->changed_datafile || filter->diff_timestamp < 0)
      filter->diff_timestamp =
          (gint64) (GST_BUFFER_TIMESTAMP (buf) / GST_MSECOND);
    changed_datafile = filter->changed_datafile;
    motionmaskcells_count = filter->motionmaskcells_count;
    motionmaskcellsidx = g_new0 (motioncellidx, filter->motionmaskcells_count);
    for (i = 0; i < filter->motionmaskcells_count; i++) {
      motionmaskcellsidx[i].lineidx = filter->motionmaskcellsidx[i].lineidx;
      motionmaskcellsidx[i].columnidx = filter->motionmaskcellsidx[i].columnidx;
    }
    motioncells_count = filter->motioncells_count;
    motioncellsidx = g_new0 (motioncellidx, filter->motioncells_count);
    for (i = 0; i < filter->motioncells_count; i++) {
      motioncellsidx[i].lineidx = filter->motioncellsidx[i].lineidx;
      motioncellsidx[i].columnidx = filter->motioncellsidx[i].columnidx;
    }
    useAlpha = filter->usealpha;
    thickness = filter->thickness;
    success =
        perform_detection_motion_cells (filter->cvImage, sensitivity, framerate,
        gridx, gridy,
        (gint64) (GST_BUFFER_TIMESTAMP (buf) / GST_MSECOND) -
        filter->diff_timestamp, display, useAlpha, motionmaskcoord_count,
        motionmaskcoords, motionmaskcells_count, motionmaskcellsidx,
        motioncellscolor, motioncells_count, motioncellsidx, starttime,
        datafile, changed_datafile, thickness, filter->id);

    if ((success == 1) && (filter->sent_init_error_msg == false)) {
      char *initfailedreason;
      int initerrorcode;
      GstStructure *s;
      GstMessage *m;
      initfailedreason = getInitDataFileFailed (filter->id);
      initerrorcode = getInitErrorCode (filter->id);
      s = gst_structure_new ("motion", "init_error_code", G_TYPE_INT,
          initerrorcode, "details", G_TYPE_STRING, initfailedreason, NULL);
      m = gst_message_new_element (GST_OBJECT (filter), s);
      gst_element_post_message (GST_ELEMENT (filter), m);
      filter->sent_init_error_msg = TRUE;
    }
    if ((success == -1) && (filter->sent_save_error_msg == false)) {
      char *savefailedreason;
      int saveerrorcode;
      GstStructure *s;
      GstMessage *m;
      savefailedreason = getSaveDataFileFailed (filter->id);
      saveerrorcode = getSaveErrorCode (filter->id);
      s = gst_structure_new ("motion", "save_error_code", G_TYPE_INT,
          saveerrorcode, "details", G_TYPE_STRING, savefailedreason, NULL);
      m = gst_message_new_element (GST_OBJECT (filter), s);
      gst_element_post_message (GST_ELEMENT (filter), m);
      filter->sent_save_error_msg = TRUE;
    }
    if (success == -2) {        //frame dropped
      filter->prev_buff_timestamp = filter->cur_buff_timestamp;
      //free
      GFREE (datafile);
      GFREE (motionmaskcoords);
      GFREE (motionmaskcellsidx);
      GFREE (motioncellsidx);
      GST_OBJECT_UNLOCK (filter);
      return gst_pad_push (filter->srcpad, buf);
    }
    filter->changed_datafile = getChangedDataFile (filter->id);
    motioncellsidxcnt = getMotionCellsIdxCnt (filter->id);
    numberOfCells = filter->gridx * filter->gridy;
    motioncellsnumber = motioncellsidxcnt / MSGLEN;
    cellsOfInterestNumber = (filter->motioncells_count > 0) ?   //how many cells interest for us
        (filter->motioncells_count) : (numberOfCells);
    mincellsOfInterestNumber =
        floor ((double) cellsOfInterestNumber * filter->threshold);
    GST_OBJECT_UNLOCK (filter);
    motiondetect = (motioncellsnumber >= mincellsOfInterestNumber) ? 1 : 0;
    if ((motioncellsidxcnt > 0) && (motiondetect == 1)) {
      char *detectedmotioncells;
      filter->last_motion_timestamp = GST_BUFFER_TIMESTAMP (buf);
      detectedmotioncells = getMotionCellsIdx (filter->id);
      if (detectedmotioncells) {
        filter->consecutive_motion++;
        if ((filter->previous_motion == false)
            && (filter->consecutive_motion >= minimum_motion_frames)) {
          GstStructure *s;
          GstMessage *m;
          filter->previous_motion = true;
          filter->motion_begin_timestamp = GST_BUFFER_TIMESTAMP (buf);
          s = gst_structure_new ("motion", "motion_cells_indices",
              G_TYPE_STRING, detectedmotioncells, "motion_begin", G_TYPE_UINT64,
              filter->motion_begin_timestamp, NULL);
          m = gst_message_new_element (GST_OBJECT (filter), s);
          gst_element_post_message (GST_ELEMENT (filter), m);
        } else if (filter->postallmotion) {
          GstStructure *s;
          GstMessage *m;
          filter->motion_timestamp = GST_BUFFER_TIMESTAMP (buf);
          s = gst_structure_new ("motion", "motion_cells_indices",
              G_TYPE_STRING, detectedmotioncells, "motion", G_TYPE_UINT64,
              filter->motion_timestamp, NULL);
          m = gst_message_new_element (GST_OBJECT (filter), s);
          gst_element_post_message (GST_ELEMENT (filter), m);
        }
      } else {
        GstStructure *s;
        GstMessage *m;
        s = gst_structure_new ("motion", "motion_cells_indices", G_TYPE_STRING,
            "error", NULL);
        m = gst_message_new_element (GST_OBJECT (filter), s);
        gst_element_post_message (GST_ELEMENT (filter), m);
      }
    } else {
      filter->consecutive_motion = 0;
      if ((((GST_BUFFER_TIMESTAMP (buf) -
                      filter->last_motion_timestamp) / 1000000000l) >=
              filter->gap)
          && (filter->last_motion_timestamp > 0)) {
        GST_DEBUG ("POST MOTION FINISHED MSG\n");
        if (filter->previous_motion) {
          GstStructure *s;
          GstMessage *m;
          filter->previous_motion = false;
          s = gst_structure_new ("motion", "motion_finished", G_TYPE_UINT64,
              filter->last_motion_timestamp, NULL);
          m = gst_message_new_element (GST_OBJECT (filter), s);
          gst_element_post_message (GST_ELEMENT (filter), m);
        }
      }
    }
    if (postnomotion > 0) {
      guint64 last_buf_timestamp = GST_BUFFER_TIMESTAMP (buf) / 1000000000l;
      if ((last_buf_timestamp -
              (filter->last_motion_timestamp / 1000000000l)) >=
          filter->postnomotion) {
        GST_DEBUG ("POST NO MOTION MSG\n");
        if ((last_buf_timestamp -
                (filter->last_nomotion_notified / 1000000000l)) >=
            filter->postnomotion) {
          GstStructure *s;
          GstMessage *m;
          filter->last_nomotion_notified = GST_BUFFER_TIMESTAMP (buf);
          s = gst_structure_new ("motion", "no_motion", G_TYPE_UINT64,
              filter->last_motion_timestamp, NULL);
          m = gst_message_new_element (GST_OBJECT (filter), s);
          gst_element_post_message (GST_ELEMENT (filter), m);
        }
      }
    }
    filter->prev_buff_timestamp = filter->cur_buff_timestamp;
    //free
    GFREE (datafile);
    GFREE (motionmaskcoords);
    GFREE (motionmaskcellsidx);
    GFREE (motioncellsidx);
  } else {
    GST_OBJECT_UNLOCK (filter);
  }
  return gst_pad_push (filter->srcpad, buf);
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
gboolean
gst_motion_cells_plugin_init (GstPlugin * plugin)
{
  /* debug category for fltering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_motion_cells_debug,
      "motioncells",
      0,
      "Performs motion detection on videos, providing detected positions via bus messages");

  return gst_element_register (plugin, "motioncells", GST_RANK_NONE,
      GST_TYPE_MOTIONCELLS);
}
