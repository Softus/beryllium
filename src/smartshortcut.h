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

#ifndef SMARTSHORTCUT_H
#define SMARTSHORTCUT_H

#include <QAction>
#include <QObject>
#include <QKeySequence>

QT_BEGIN_NAMESPACE
class QEvent;
class QAbstractButton;
class QAction;
class QInputEvent;
QT_END_NAMESPACE

#define MOUSE_SHORTCUT_MASK  int(0x80000000)
#define GLOBAL_SHORTCUT_MASK int(0x00800000)
#define LONG_PRESS_MASK      int(0x00400000)

class SmartShortcut : public QObject
{
    Q_OBJECT
    static qint64 longPressTimeoutInMsec;

public:
    static void reloadSettings();
    static void setShortcut(QObject *parent, int key);
    static void updateShortcut(QObject *parent, int key);
    static void removeShortcut(QObject *parent);
    static void removeAll();

    static qint64 timestamp();
    static bool longPressTimeout(qint64 ts);
    static void setEnabled(bool enable);
    static QString toString
        ( int key
        , QKeySequence::SequenceFormat format = QKeySequence::PortableText
        );
    static bool isGlobal(int key);
    static bool isLongPress(int key);
    static bool isMouse(int key);
    SmartShortcut(QObject* parent);
    ~SmartShortcut();

protected:
    bool eventFilter(QObject *o, QEvent *e);
};

#endif // SMARTSHORTCUT_H
