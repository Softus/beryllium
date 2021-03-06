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

#ifndef DARKTHEMESTYLE_H
#define DARKTHEMESTYLE_H

#include <QProxyStyle>

class DarkThemeStyle : public QProxyStyle
{
public:
    DarkThemeStyle(QStyle *style = 0);

    static const QIcon invertIcon(const QIcon& icon);
    static const QPixmap invertPixmap(const QPixmap& pixmap);

protected:
    virtual void drawItemPixmap
        ( QPainter *painter
        , const QRect &rect
        , int alignment
        , const QPixmap &pixmap
        ) const;
};

#endif // DARKTHEMESTYLE_H
