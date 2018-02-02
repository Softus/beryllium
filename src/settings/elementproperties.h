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

#ifndef ELEMENTPROPERTIES_H
#define ELEMENTPROPERTIES_H

#include <QDialog>

class ElementProperties : public QDialog
{
    Q_OBJECT

public:
    ElementProperties
        ( const QString& deviceType
        , const QString &properties
        , QWidget *parent = 0
        );
    QString getProperties();
signals:
    
private slots:
};

#endif // ELEMENTPROPERTIES_H
