/*
 * Copyright (C) 2013-2016 Irkutsk Diagnostic Center.
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

#ifndef BERYLLIUM_GSTCOMPAT_H
#define BERYLLIUM_GSTCOMPAT_H

#include <glib.h>
#include <gst/gstversion.h>
#if GST_CHECK_VERSION(1,0,0)
#define PLAYBIN_ELEMENT "playbin"
#define DEFAULT_VIDEO_CONVERTER "videoconvert"
#define PREPARE_WINDOW_HANDLE_MESSAGE "prepare-window-handle"
#define WIN_VIDEO_SOURCE "ksvideosrc"
#else
#define PLAYBIN_ELEMENT "playbin2"
#define DEFAULT_VIDEO_CONVERTER "ffmpegcolorspace"
#define PREPARE_WINDOW_HANDLE_MESSAGE "prepare-xwindow-id"
#define WIN_VIDEO_SOURCE "dshowvideosrc"
#endif

#endif // BERYLLIUM_GSTCOMPAT_H
