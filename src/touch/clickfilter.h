/*
 * Copyright (C) 2013-2016 Softus Inc.
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

#ifndef CLICKFILTER_H
#define CLICKFILTER_H

#include <QObject>
#include <QMouseEvent>

QT_BEGIN_NAMESPACE
class QEvent;
QT_END_NAMESPACE

class ClickFilter : public QObject
{
    Q_OBJECT
public:
    ClickFilter(QObject *parent);
    ~ClickFilter();
protected:
    bool eventFilter(QObject *o, QEvent *e);
private:
    QMouseEvent me;
};

#endif // CLICKFILTER_H
