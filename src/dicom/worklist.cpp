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

#include "worklist.h"
#include "../defaults.h"
#include "../smartshortcut.h"
#include "../qwaitcursor.h"
#include "../touch/slidingstackedwidget.h"
#include "detailsdialog.h"
#include "transcyrillic.h"

#ifdef UNICODE
#define DCMTK_UNICODE_BUG_WORKAROUND
#undef UNICODE
#endif
#include "dcmclient.h"

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcitem.h>
#include <dcmtk/dcmdata/dcuid.h>

#ifdef DCMTK_UNICODE_BUG_WORKAROUND
#define UNICODE
#undef DCMTK_UNICODE_BUG_WORKAROUND
#endif

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QDateTime>
#include <QDebug>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>

Q_DECLARE_METATYPE(DcmDataset)
static int DcmDatasetMetaType = qRegisterMetaType<DcmDataset>();

Worklist::Worklist(QWidget *parent) :
    QWidget(parent),
    timeColumn(-1),
    dateColumn(-1),
    activeConnection(nullptr)
{
    QSettings settings;
    settings.beginGroup("dicom");


    auto cols = settings.value("worklist-columns").toStringList();
    if (cols.size() == 0)
    {
        // Defaults are id, name, bithday, sex, procedure description, date, time
        cols = DEFAULT_WORKLIST_COLUMNS;
    }
    settings.endGroup();

    table = new QTableWidget(0, cols.size());
    connect(table, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this,
        SLOT(onItemDoubleClicked(QTableWidgetItem*)));
    connect(table, SIGNAL(currentItemChanged(QTableWidgetItem*,QTableWidgetItem*)), this,
        SLOT(onCurrentItemChanged(QTableWidgetItem*,QTableWidgetItem*)));

    for (auto i = 0; i < cols.size(); ++i)
    {
        DcmTag tag;
        auto text = DcmTag::findTagFromName(cols[i].toUtf8(), tag).good()
            ? QString::fromUtf8(tag.getTagName()) : cols[i];
        auto item = new QTableWidgetItem(text);
        item->setData(Qt::UserRole, (tag.getGroup() << 16) | tag.getElement());
        table->setHorizontalHeaderItem(i, item);

        if (tag == DCM_ScheduledProcedureStepStartDate)
        {
            dateColumn = i;
        }
        else if (tag == DCM_ScheduledProcedureStepStartTime)
        {
            timeColumn = i;
        }
    }
    table->resizeColumnsToContents();
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto layoutMain = new QVBoxLayout();
    layoutMain->addWidget(table);
    layoutMain->addWidget(createToolBar());
    table->viewport()->ungrabGesture(Qt::PanGesture);
    setLayout(layoutMain);

    setMinimumSize(640, 480);

    settings.beginGroup("ui");
    table->horizontalHeader()->restoreState(settings.value("worklist-columns-width").toByteArray());
    settings.endGroup();

    settings.beginGroup("hotkeys");
    SmartShortcut::updateShortcut(actionDetail,     settings.value("worklist-show-details",
        DEFAULT_HOTKEY_SHOW_DETAILS).toInt());
    SmartShortcut::updateShortcut(actionStartStudy, settings.value("worklist-start",
        DEFAULT_HOTKEY_START).toInt());
    SmartShortcut::updateShortcut(actionLoad,       settings.value("worklist-refresh",
        DEFAULT_HOTKEY_REFRESH).toInt());
    SmartShortcut::updateShortcut(actionBack,       settings.value("worklist-back",
        DEFAULT_HOTKEY_BACK).toInt());
    settings.endGroup();

    setAttribute(Qt::WA_DeleteOnClose, false);
}

QToolBar* Worklist::createToolBar()
{
    QToolBar* bar = new QToolBar(tr("Worklist"));
    bar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    actionBack   = bar->addAction(QIcon(":buttons/back"), tr("Back"), this,
        SLOT(onBackToMainWindowClick()));
    actionLoad   = bar->addAction(QIcon(":/buttons/refresh"), tr("&Refresh"), this,
        SLOT(onLoadClick()));
    actionDetail = bar->addAction(QIcon(":/buttons/details"), tr("&Details"), this,
        SLOT(onShowDetailsClick()));
    actionDetail->setEnabled(false);
    actionStartStudy = bar->addAction(QIcon(":/buttons/start"), tr("Start &study"), this,
        SLOT(onStartStudyClick()));
    actionStartStudy->setEnabled(false);

    return bar;
}

void Worklist::onAddRow(DcmDataset* dset)
{
    QDateTime date;
    int row = table->rowCount();
    table->setRowCount(row + 1);

    for (int col = 0; col < table->columnCount(); ++col)
    {
        const char *str = nullptr;
        auto tag = table->horizontalHeaderItem(col)->data(Qt::UserRole).toInt();
        DcmTagKey tagKey(ushort(tag >> 16), tag & 0xFFFF);
        OFCondition cond = dset->findAndGetString(tagKey, str, true);
        auto text = QString::fromUtf8(str? str: cond.text());
        auto item = new QTableWidgetItem(text);
        table->setItem(row, col, item);
        if (col == dateColumn)
        {
            date.setDate(QDate::fromString(text, "yyyyMMdd"));
        }
        else if (col == timeColumn)
        {
            date.setTime(QTime::fromString(text, "HHmm"));
        }
    }

    table->item(row, 0)->setData(Qt::UserRole,
        QVariant::fromValue(*static_cast<DcmDataset*>(dset->clone())));

    if (date < QDateTime::currentDateTime() && date > maxDate)
    {
       maxDate = date;
       table->selectRow(row);
    }

    qApp->processEvents();
}

void Worklist::showEvent(QShowEvent *e)
{
    // Refresh the worklist right after the window is shown
    //
    QTimer::singleShot(500, this, SLOT(onLoadClick()));
    QWidget::showEvent(e);
}

void Worklist::closeEvent(QCloseEvent *e)
{
    // Force drop connection to the server if the worklist still loading
    //
    if (activeConnection)
    {
        activeConnection->abort();
        activeConnection = nullptr;
    }
    QWidget::closeEvent(e);
}

void Worklist::hideEvent(QHideEvent *e)
{
    QSettings settings;
    settings.beginGroup("ui");
    settings.setValue("worklist-columns-width", table->horizontalHeader()->saveState());
    settings.endGroup();
    QWidget::hideEvent(e);
}

void Worklist::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_VolumeUp)
    {
        table->setCurrentCell(table->currentRow() - 1, table->currentColumn());
    }
    else if (e->key() == Qt::Key_VolumeDown)
    {
        table->setCurrentCell(table->currentRow() + 1, table->currentColumn());
    }

    QWidget::keyPressEvent(e);
}

void Worklist::onLoadClick()
{
    QWaitCursor wait(this);
    actionLoad->setEnabled(false);
    actionDetail->setEnabled(false);

    // Clear all data
    //
    table->setUpdatesEnabled(false);
    table->setSortingEnabled(false);
    maxDate.setTime_t(0);
    table->setRowCount(0);

    DcmClient assoc(UID_FINDModalityWorklistInformationModel);

    activeConnection = &assoc;
    connect(&assoc, SIGNAL(addRow(DcmDataset*)), this, SLOT(onAddRow(DcmDataset*)));
    bool ok = assoc.findSCU();
    activeConnection = nullptr;

    if (!ok)
    {
        qCritical() << assoc.lastError();
        QMessageBox::critical(this, windowTitle(), assoc.lastError(), QMessageBox::Ok);
    }

    if (timeColumn >= 0)
    {
        table->sortItems(timeColumn);
        if (dateColumn >= 0)
        {
            table->sortItems(dateColumn);
        }
    }

    actionDetail->setEnabled(table->currentRow() >= 0);
    actionStartStudy->setEnabled(table->currentRow() >= 0);

    table->setSortingEnabled(true);
    table->scrollToItem(table->currentItem());
    table->setFocus();
    table->setUpdatesEnabled(true);

    actionLoad->setEnabled(true);
}

void Worklist::onShowDetailsClick()
{
    QWaitCursor wait(this);
    int row = table->currentRow();
    if (row >= 0)
    {
        auto ds = table->item(row, 0)->data(Qt::UserRole).value<DcmDataset>();
        DetailsDialog dlg(&ds, this);
        dlg.exec();
    }
}

void Worklist::onItemDoubleClicked(QTableWidgetItem* item)
{
    table->setCurrentItem(item);
    //onShowDetailsClick();
    onStartStudyClick();
}

void Worklist::onCurrentItemChanged(QTableWidgetItem *current, QTableWidgetItem *)
{
    actionDetail->setEnabled(current != nullptr);
    actionStartStudy->setEnabled(current != nullptr);
}

void Worklist::onStartStudyClick()
{
    int row = table->currentRow();
    if (row >= 0)
    {
        auto ds = table->item(row, 0)->data(Qt::UserRole).value<DcmDataset>();
        startStudy(&ds);
    }
}

void Worklist::onBackToMainWindowClick()
{
    auto stackWidget = static_cast<SlidingStackedWidget*>(parent()->
        qt_metacast("SlidingStackedWidget"));
    if (stackWidget)
    {
        stackWidget->slideInWidget("Main");
    }
}
