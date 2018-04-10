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

const QIcon DarkThemeStyle::invertIcon(const QIcon& icon)
{
    QIcon inverted;

    foreach (auto const& size, icon.availableSizes())
    {
        inverted.addPixmap(invertPixmap(icon.pixmap(size)));
    }

    return inverted;
}

const QPixmap DarkThemeStyle::invertPixmap(const QPixmap& pixmap)
{
    auto img = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    img.invertPixels();
    return QPixmap::fromImage(img);
}

void DarkThemeStyle::drawItemPixmap
    ( QPainter *painter
    , const QRect &rect
    , int alignment
    , const QPixmap &pixmap
    ) const
{
    QProxyStyle::drawItemPixmap(painter, rect, alignment, invertPixmap(pixmap));
}

