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

#ifndef DICOMDEVICESETTINGS_H
#define DICOMDEVICESETTINGS_H

#include <QWidget>
#include <QSettings>

QT_BEGIN_NAMESPACE
class QComboBox;
class QCheckBox;
class QLineEdit;
class QSpinBox;
QT_END_NAMESPACE

class DicomDeviceSettings : public QWidget
{
    Q_OBJECT
    QLineEdit *textAet;
    QComboBox *cbModality;
    QSpinBox  *spinPort;
    QCheckBox *checkExportClips;
    QCheckBox *checkExportVideo;
    QCheckBox *checkTransCyr;

public:
    Q_INVOKABLE explicit DicomDeviceSettings(QWidget *parent = 0);
    
signals:
    
public slots:
    void save(QSettings& settings);

};

#endif // DICOMDEVICESETTINGS_H
