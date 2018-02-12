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
#include "ui.h"
#include "../defaults.h"
#include "../product.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSettings>
#include <QSpacerItem>
#include <QSystemTrayIcon>
#include <QTextEdit>

UiSettings::UiSettings(QWidget *parent)
    : QWidget(parent)
{
    QSettings settings;
    settings.beginGroup("ui");

    auto layoutMain = new QFormLayout;

    cbLanguage = new QComboBox;
    cbLanguage->addItem(tr("(auto)"), "");
    cbLanguage->addItem(tr("english (en)"), "en");

    auto currentLocale = settings.value("locale").toString();
    QDir translations(TRANSLATIONS_FOLDER);
    foreach (auto file, translations.entryList(QStringList(PRODUCT_SHORT_NAME "_*.qm")))
    {
        auto start = file.indexOf('_') + 1;
        auto end = file.indexOf('.', start);
        auto localeName = file.mid(start, end - start);
        QLocale locale(localeName);
        cbLanguage->addItem(tr("%1 (%2)").arg(locale.nativeLanguageName()).arg(localeName),
            localeName);
    }
    cbLanguage->setCurrentIndex(cbLanguage->findData(currentLocale));
    layoutMain->addRow(tr("&Language"), cbLanguage);

    cbIconSet = new QComboBox;
    auto currentIconSet = settings.value("icon-set", DEFAULT_ICON_SET).toString();
    QStringList iconSetNames;
    iconSetNames << tr("(auto)") << tr("light") << tr("dark");
    QStringList iconSetValues;
    iconSetValues << "auto" << "light" << "dark";
    for (auto i = 0; i < iconSetValues.length(); ++i)
    {
        cbIconSet->addItem(iconSetNames[i], iconSetValues[i]);
    }
    cbIconSet->setCurrentIndex(cbIconSet->findData(currentIconSet));
    layoutMain->addRow(tr("&Icon set"), cbIconSet);

    checkTrayIcon = new QCheckBox(tr("&Show icon in the system tray"));
    checkTrayIcon->setEnabled(QSystemTrayIcon::isSystemTrayAvailable());
    checkTrayIcon->setChecked(checkTrayIcon->isEnabled()
        && settings.value("show-tray-icon").toBool());
    layoutMain->addWidget(checkTrayIcon);

    checkMinimizeOnStart = new QCheckBox(tr("&Minimize on start study"));
    checkMinimizeOnStart->setChecked(settings.value("minimize-on-start").toBool());
    layoutMain->addWidget(checkMinimizeOnStart);

    checkShowFullscreen = new QCheckBox(tr("Start in &fullscreen mode"));
    checkShowFullscreen->setChecked(settings.value("show-fullscreen").toBool());
    layoutMain->addWidget(checkShowFullscreen);

    checkEmulateDblClick = new QCheckBox(tr("Emulate mouse &double click on tap"));
    checkEmulateDblClick->setChecked(settings.value("long-tap-to-double-click").toBool());
    layoutMain->addWidget(checkEmulateDblClick);

    textCss = new QTextEdit(settings.value("css").toString());
    layoutMain->addRow(tr("Extra &CSS"), textCss);

    layoutMain->addItem(new QSpacerItem(0, 10000, QSizePolicy::Minimum, QSizePolicy::Expanding));
    layoutMain->addWidget(new QLabel(tr("NOTE: some changes on this page will take effect" \
        " the next time the application starts.")));

    setLayout(layoutMain);
}

/**
 * @brief QComboBox::currentData() for Qt 5.0.
 * @param cb combobox to use.
 * @return data associated with current item.
 */
static QVariant getListData(const QComboBox* cb)
{
    auto idx = cb->currentIndex();
    return cb->itemData(idx);
}

void UiSettings::save(QSettings& settings)
{
    settings.beginGroup("ui");
    settings.setValue("locale",                   getListData(cbLanguage));
    settings.setValue("icon-set",                 getListData(cbIconSet));
    settings.setValue("show-tray-icon",           checkTrayIcon->isChecked());
    settings.setValue("show-fullscreen",          checkShowFullscreen->isChecked());
    settings.setValue("minimize-on-start",        checkMinimizeOnStart->isChecked());
    settings.setValue("long-tap-to-double-click", checkEmulateDblClick->isChecked());
    settings.setValue("css",                      textCss->toPlainText());
    settings.endGroup();
}
