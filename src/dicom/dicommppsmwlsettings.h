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

#ifndef DICOMMPPSMWLSETTINGS_H
#define DICOMMPPSMWLSETTINGS_H

#include <QWidget>
#include <QSettings>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QComboBox;
QT_END_NAMESPACE

class DicomMppsMwlSettings : public QWidget
{
    Q_OBJECT
    QCheckBox* checkUseMwl;
    QComboBox* cbMwlServer;

    QCheckBox* checkStartWithMpps;
    QCheckBox* checkCompleteWithMpps;
    QCheckBox* checkUseMpps;
    QComboBox* cbMppsServer;

public:
    Q_INVOKABLE explicit DicomMppsMwlSettings(QWidget *parent = 0);
    
signals:
    
public slots:
    void save(QSettings& settings);
private slots:
    void onUpdateServers();
    void onUseToggle(bool checked);
};

#endif // DICOMMPPSMWLSETTINGS_H
