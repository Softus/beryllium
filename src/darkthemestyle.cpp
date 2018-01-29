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

#include "darkthemestyle.h"

DarkThemeStyle::DarkThemeStyle(QStyle *style)
    : QProxyStyle(style)
{
}

void DarkThemeStyle::drawItemPixmap
    ( QPainter *painter
    , const QRect &rect
    , int alignment
    , const QPixmap &pixmap
    ) const
{
    auto img = pixmap.toImage();
    img.invertPixels();
    QProxyStyle::drawItemPixmap(painter, rect, alignment, QPixmap::fromImage(img));
}

