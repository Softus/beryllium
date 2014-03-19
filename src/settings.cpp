/*
 * Copyright (C) 2013-2014 Irkutsk Diagnostic Center.
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

#include "settings.h"

#include "settings/confirmationsettings.h"
#include "settings/debugsettings.h"
#include "settings/hotkeysettings.h"
#include "settings/mandatoryfieldssettings.h"
#include "settings/physicianssettings.h"
#include "settings/storagesettings.h"
#include "settings/studiessettings.h"
#include "settings/videorecordsettings.h"
#include "settings/videosourcesettings.h"

#ifdef WITH_DICOM
#include "dicom/dicomdevicesettings.h"
#include "dicom/dicomserversettings.h"
#include "dicom/dicommppsmwlsettings.h"
#include "dicom/dicomstoragesettings.h"
#include "dicom/worklistcolumnsettings.h"
#include "dicom/worklistquerysettings.h"
#endif

#ifdef WITH_TOUCH
#include "touch/slidingstackedwidget.h"
#endif

#include <QListWidget>
#include <QBoxLayout>
#include <QDebug>
#include <QLabel>
#include <QStackedWidget>
#include <QSettings>
#include <QPushButton>
#include "qwaitcursor.h"

Q_DECLARE_METATYPE(QMetaObject)
static int QMetaObjectMetaType = qRegisterMetaType<QMetaObject>();

Settings::Settings(const QString& pageTitle, QWidget *parent, Qt::WindowFlags flags)
    : QDialog(parent, flags)
{
    QSettings settings;

    listWidget = new QListWidget;
    listWidget->setMovement(QListView::Static);
    listWidget->setMaximumWidth(280);
    listWidget->setSpacing(2);

    pagesWidget = new QStackedWidget;
    pagesWidget->setMinimumSize(640, 520); // Video source settings is a big one

    createPages();
    listWidget->setCurrentRow(0);

    auto layoutContent = new QHBoxLayout;
    layoutContent->addWidget(listWidget);
    layoutContent->addWidget(pagesWidget, 1);

    auto layoutBtns = new QHBoxLayout;
    if (!settings.isWritable())
    {
        layoutBtns->addWidget(new QLabel(tr("NOTE: all changes will be lost when the application closes.")));
    }
    layoutBtns->addStretch(1);

    auto btnApply = new QPushButton(tr("Appl&y"));
    connect(btnApply, SIGNAL(clicked()), this, SLOT(onClickApply()));
    layoutBtns->addWidget(btnApply);
    btnCancel = new QPushButton(tr("Cancel"));
    connect(btnCancel, SIGNAL(clicked()), this, SLOT(reject()));
    layoutBtns->addWidget(btnCancel);
    auto btnOk = new QPushButton(tr("Ok"));
    connect(btnOk, SIGNAL(clicked()), this, SLOT(accept()));
    btnOk->setDefault(true);
    layoutBtns->addWidget(btnOk);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addLayout(layoutContent);
    mainLayout->addSpacing(12);
    mainLayout->addLayout(layoutBtns);
    setLayout(mainLayout);

    setWindowTitle(tr("Settings"));
    QString selectedPage = pageTitle.isEmpty()? settings.value("settings-page").toString(): pageTitle;

    if (!selectedPage.isEmpty())
    {
        auto idx = selectedPage.toInt();
        if (idx > 0)
        {
            listWidget->setCurrentRow(idx - 1);
        }
        else
        {
            auto pages = listWidget->findItems(selectedPage, Qt::MatchExactly);
            if (pages.empty())
            {
                pages = listWidget->findItems(selectedPage, Qt::MatchContains);
            }
            if (!pages.empty())
            {
                listWidget->setCurrentItem(pages.first());
            }
        }
    }
}

void Settings::createPage(const QString& title, const QMetaObject& page)
{
    auto item = new QListWidgetItem(title, listWidget);
    item->setData(Qt::UserRole, QVariant::fromValue(page));
}

void Settings::createPages()
{
#ifdef WITH_DICOM
    createPage(tr("DICOM device"), DicomDeviceSettings::staticMetaObject);
    createPage(tr("DICOM servers"), DicomServerSettings::staticMetaObject);
    createPage(tr("DICOM MWL/MPPS"), DicomMppsMwlSettings::staticMetaObject);
    createPage(tr("DICOM storage"), DicomStorageSettings::staticMetaObject);
    createPage(tr("Worklist column display"), WorklistColumnSettings::staticMetaObject);
    createPage(tr("Worklist query settings"), WorklistQuerySettings::staticMetaObject);
#endif

    createPage(tr("Video source"), VideoSourceSettings::staticMetaObject);
    createPage(tr("Video recording"), VideoRecordSettings::staticMetaObject);
    createPage(tr("Storage"), StorageSettings::staticMetaObject);
    createPage(tr("Physicians"), PhysiciansSettings::staticMetaObject);
    createPage(tr("Studies"), StudiesSettings::staticMetaObject);
    createPage(tr("Mandatory fields"), MandatoryFieldsSettings::staticMetaObject);
    createPage(tr("Hotkeys"), HotKeySettings::staticMetaObject);
    createPage(tr("Confirmations"), ConfirmationSettings::staticMetaObject);
    createPage(tr("Debug"), DebugSettings::staticMetaObject);

    // Sort pages using current locale
    //
    //listWidget->sortItems();

    connect(listWidget,
        SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
        this, SLOT(changePage(QListWidgetItem*,QListWidgetItem*)));
}

void Settings::changePage(QListWidgetItem *current, QListWidgetItem *previous)
{
    if (!current)
        current = previous;

    QWaitCursor wait(this);
    const QVariant& data = current->data(Qt::UserRole);
    if (data.type() == QVariant::UserType)
    {
        const QMetaObject& mobj = data.value<QMetaObject>();
        QWidget* page = static_cast<QWidget*>(mobj.newInstance());
        connect(this, SIGNAL(save()), page, SLOT(save()));
        auto count = pagesWidget->count();
        current->setData(Qt::UserRole, count);
        pagesWidget->addWidget(page);
        pagesWidget->setCurrentIndex(count);
    }
    else
    {
        pagesWidget->setCurrentIndex(current->data(Qt::UserRole).toInt());
    }
}

void Settings::onClickApply()
{
    QWaitCursor wait(this);

    // Save add pages data.
    //
    save();

    // After all pages has been saved, we can trigger another signal
    // to tell the application that all settings are ready to be read.
    //
    apply();

    // Replace "Cancel" with "Close" since we can not cancel anymore
    //
    btnCancel->setText(tr("Close"));
}

void Settings::accept()
{
    save();
    QDialog::accept();
    QSettings().setValue("settings-page", listWidget->currentItem()->text());
}