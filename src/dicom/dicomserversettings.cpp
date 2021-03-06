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

#include "dicomserversettings.h"
#include "dicomserverdetails.h"
#include <QBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QSettings>

DicomServerSettings::DicomServerSettings(QWidget *parent) :
    QWidget(parent)
{
    QSettings settings;
    QHBoxLayout* mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(4,0,4,0);

    const QString columnNames[] =
    {
        tr("Name"),
        tr("AE title"),
        tr("IP address"),
        tr("Port"),
        //tr("Timeout"),
    };

    mainLayout->addWidget(listServers = new QTableWidget);
    connect(listServers, SIGNAL(currentItemChanged(QTableWidgetItem*,QTableWidgetItem*)), this,
        SLOT(onCellChanged(QTableWidgetItem*,QTableWidgetItem*)));
    connect(listServers, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this,
        SLOT(onCellDoubleClicked(QTableWidgetItem*)));

    auto columns = int(sizeof(columnNames) / sizeof(columnNames[0]));
    listServers->setColumnCount(columns);
    for (auto i = 0; i < columns; ++i)
    {
        listServers->setHorizontalHeaderItem(i, new QTableWidgetItem(columnNames[i]));
    }

    listServers->setSelectionBehavior(QAbstractItemView::SelectRows);
    listServers->setEditTriggers(QAbstractItemView::NoEditTriggers);
    listServers->setSortingEnabled(true);

    settings.beginGroup("servers");
    auto const& keys = settings.allKeys();
    listServers->setUpdatesEnabled(false);
    listServers->setRowCount(keys.count());
    for (int row = 0; row < keys.count(); ++row)
    {
        auto item = new QTableWidgetItem(keys[row]);
        auto const& values = settings.value(keys[row]).toStringList();
        listServers->setItem(row, 0, item);
        item->setData(Qt::UserRole, values);
        updateColumns(row, values);
    }
    settings.endGroup();
    listServers->resizeColumnsToContents();
    listServers->setUpdatesEnabled(true);

    QBoxLayout* buttonsLayout = new QVBoxLayout;
    QPushButton* btnAdd = new QPushButton(tr("&Add"));
    connect(btnAdd, SIGNAL(clicked()), this, SLOT(onAddClicked()));
    buttonsLayout->addWidget(btnAdd);

    btnEdit = new QPushButton(tr("&Edit"));
    btnEdit->setEnabled(false);
    connect(btnEdit, SIGNAL(clicked()), this, SLOT(onEditClicked()));
    buttonsLayout->addWidget(btnEdit);

    btnRemove = new QPushButton(tr("&Remove"));
    btnRemove->setEnabled(false);
    connect(btnRemove, SIGNAL(clicked()), this, SLOT(onRemoveClicked()));
    buttonsLayout->addWidget(btnRemove);
    buttonsLayout->addStretch(1);

    mainLayout->addLayout(buttonsLayout);
    setLayout(mainLayout);
}

void DicomServerSettings::updateColumns(int row, const QStringList& values)
{
    for (auto i = 1; i < listServers->columnCount() && values.count() >= i; ++i)
    {
        listServers->setItem(row, i, new QTableWidgetItem(values[i-1]));
    }
}

void DicomServerSettings::updateServerList()
{
    QStringList serverNames;

    for (auto i = 0; i < listServers->rowCount(); ++i)
    {
        serverNames << listServers->item(i, 0)->text();
    }

    parent()->setProperty("DICOM servers", serverNames);
}

void DicomServerSettings::onCellChanged(QTableWidgetItem* current, QTableWidgetItem*)
{
    btnEdit->setEnabled(current != nullptr);
    btnRemove->setEnabled(current != nullptr);
}

void DicomServerSettings::onCellDoubleClicked(QTableWidgetItem*)
{
    onEditClicked();
}

void DicomServerSettings::onAddClicked()
{
    DicomServerDetails dlg(this);
    if (dlg.exec())
    {
        auto row = listServers->rowCount();
        listServers->setRowCount(row + 1);

        auto item = new QTableWidgetItem(dlg.name());
        listServers->setItem(row, 0, item);

        auto const& values = dlg.values();
        item->setData(Qt::UserRole, values);
        updateColumns(row, values);
        updateServerList();
    }
}

void DicomServerSettings::onEditClicked()
{
    auto item = listServers->item(listServers->currentRow(), 0);
    DicomServerDetails dlg(this);
    dlg.setValues(item->text(), item->data(Qt::UserRole).toStringList());
    if (dlg.exec())
    {
        auto const& values = dlg.values();
        item->setData(Qt::UserRole, values);
        item->setText(dlg.name());
        updateColumns(item->row(), values);
        updateServerList();
    }
}

void DicomServerSettings::onRemoveClicked()
{
    listServers->removeRow(listServers->currentRow());
    btnEdit->setEnabled(listServers->rowCount());
    btnRemove->setEnabled(listServers->rowCount());
    updateServerList();
}

void DicomServerSettings::save(QSettings& settings)
{
    settings.remove("servers");
    settings.beginGroup("servers");
    for (int i = 0; i < listServers->rowCount(); ++i)
    {
        auto item = listServers->item(i, 0);
        settings.setValue(item->text(), item->data(Qt::UserRole));
    }
    settings.endGroup();
}
