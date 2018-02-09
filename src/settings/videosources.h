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

#ifndef VIDEOSOURCES_H
#define VIDEOSOURCES_H

#include <QWidget>
#include <QSettings>

QT_BEGIN_NAMESPACE
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
QT_END_NAMESPACE

class VideoSources : public QWidget
{
    Q_OBJECT
    QTreeWidget* listSources;
    QPushButton* btnDetails;
    QPushButton* btnRemove;

    void updateDeviceList();
    void addItem(const QSettings& settings);
    void addDevice
        ( const QString& deviceId
        , const QString& deviceName
        , const QString& deviceType
        );

public:
    Q_INVOKABLE explicit VideoSources(QWidget *parent = 0);

protected:
    virtual void showEvent(QShowEvent *);

private slots:
    void onAddScreenCaptureClicked();
    void onAddTestClicked();
    void onRemoveClicked();
    void onEditClicked();
    void onTreeItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void onItemDoubleClicked(QTreeWidgetItem*, int);

public slots:
    void save(QSettings& settings);
};

#endif // VIDEOSOURCES_H
