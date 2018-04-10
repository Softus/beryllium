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

#include "elementproperties.h"
#include "../defaults.h"
#include "../platform.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <qxtcheckcombobox.h>
#include <qxtlineedit.h>

#include <QGst/Parse>
#include <gst/gst.h>

/**
 * @brief Filters out some properties that are handled by VideoSourceDetails
 *        class itself to avoid collisions.
 * @param elmType element type name.
 * @param propName property name.
 * @return true if the property should be hidden.
 */
static bool isBlacklistedProp
    ( const QString& elmType
    , const QString& propName
    )
{
    return propName == "bitrate" || getDeviceIdPropName(elmType) == propName
            || (propName == "capture-screen" && elmType == "avfvideosrc")
            || (propName == "device-type"    && elmType == "avfvideosrc");
}

static QWidget* createEditor
    ( QGlib::ParamSpecPtr prop
    , QGst::ElementPtr elm
    )
{
    QWidget* widget{};
    auto const& elmValue = elm->property(prop->name().toUtf8());
    QVariant qtDefValue;

    switch (prop->valueType())
    {
    case QGlib::Type::Boolean:
        {
            auto spec = G_PARAM_SPEC_BOOLEAN(static_cast<GParamSpec *>(prop));
            auto check = new QCheckBox;
            check->setChecked(elmValue.toBool());
            qtDefValue = spec->default_value;
            widget = check;
        }
        break;
    case QGlib::Type::Int:
        {
            auto spec = G_PARAM_SPEC_INT(static_cast<GParamSpec *>(prop));
            auto spin = new QSpinBox;
            spin->setMinimum(spec->minimum);
            spin->setMaximum(spec->maximum);
            spin->setValue(elmValue.toInt());
            qtDefValue = spec->default_value;
            widget = spin;
        }
        break;
    case QGlib::Type::Long:
        {
            auto spec = G_PARAM_SPEC_LONG(static_cast<GParamSpec *>(prop));
            auto spin = new QSpinBox;
            spin->setMinimum(spec->minimum);
            spin->setMaximum(spec->maximum);
            spin->setValue(elmValue.toLong());
            qtDefValue = (int)spec->default_value;
            widget = spin;
        }
        break;
    case QGlib::Type::Uint:
        {
            auto spec = G_PARAM_SPEC_UINT(static_cast<GParamSpec *>(prop));
            auto spin = new QSpinBox;
            spin->setMinimum(spec->minimum);
            spin->setMaximum(spec->maximum);
            spin->setValue(elmValue.toUInt());
            qtDefValue = spec->default_value;
            widget = spin;
        }
        break;
    case QGlib::Type::Ulong:
        {
            auto spec = G_PARAM_SPEC_ULONG(static_cast<GParamSpec *>(prop));
            auto spin = new QSpinBox;
            spin->setMinimum(int(spec->minimum));
            spin->setMaximum(int(spec->maximum));
            spin->setValue(int(elmValue.toULong()));
            qtDefValue = uint(spec->default_value);
            widget = spin;
        }
        break;
    case QGlib::Type::Int64:
        {
            auto spec = G_PARAM_SPEC_INT64(static_cast<GParamSpec *>(prop));
            auto spin = new QSpinBox;
            if (spec->minimum < std::numeric_limits<int>::min())
                spec->minimum = std::numeric_limits<int>::min();
            spin->setMinimum(int(spec->minimum));
            if (spec->maximum > std::numeric_limits<int>::max())
                spec->maximum = std::numeric_limits<int>::max();
            spin->setMaximum(int(spec->maximum));
            spin->setValue(int(elmValue.toInt64()));
            qtDefValue = qlonglong(spec->default_value);
            widget = spin;
        }
        break;
    case QGlib::Type::Uint64:
        {
            auto spec = G_PARAM_SPEC_UINT64(static_cast<GParamSpec *>(prop));
            auto spin = new QSpinBox;
            spin->setMinimum(int(spec->minimum));
            if (spec->maximum > guint(std::numeric_limits<int>::max()))
                spec->maximum = std::numeric_limits<int>::max();
            spin->setMaximum(int(spec->maximum));
            spin->setValue(int(elmValue.toUInt64()));
            qtDefValue = qulonglong(spec->default_value);
            widget = spin;
        }
        break;
    case QGlib::Type::Float:
        {
            auto spec = G_PARAM_SPEC_FLOAT(static_cast<GParamSpec *>(prop));
            auto spin = new QDoubleSpinBox;
            spin->setMinimum(double(spec->minimum));
            spin->setMaximum(double(spec->maximum));
            spin->setValue(double(elmValue.get<float>()));
            spin->setProperty("epsilon", spec->epsilon);
            qtDefValue = spec->default_value;
            widget = spin;
        }
        break;
    case QGlib::Type::Double:
        {
            auto spec = G_PARAM_SPEC_DOUBLE(static_cast<GParamSpec *>(prop));
            auto spin = new QDoubleSpinBox;
            spin->setMinimum(spec->minimum);
            spin->setMaximum(spec->maximum);
            spin->setValue(elmValue.get<double>());
            spin->setProperty("epsilon", spec->epsilon);
            qtDefValue = spec->default_value;
            widget = spin;
        }
        break;
    case QGlib::Type::String:
        {
            auto spec = G_PARAM_SPEC_STRING(static_cast<GParamSpec *>(prop));
            qtDefValue = spec->default_value;
            auto edit = new QxtLineEdit(elmValue.toString());
            edit->setSampleText(spec->default_value);
            widget = edit;
        }
        break;
    default:
        if (G_IS_PARAM_SPEC_ENUM(static_cast<GParamSpec *>(prop)))
        {
            auto spec = G_PARAM_SPEC_ENUM(static_cast<GParamSpec *>(prop));
            auto combo = new QComboBox;
            GEnumClass *cls = spec->enum_class;
            int idx{1};
            gint value = elmValue.toInt();
            for (guint i = 0; i < cls->n_values; ++i)
            {
                combo->addItem(cls->values[i].value_name, cls->values[i].value);
                if (value == cls->values[i].value)
                {
                    idx = int(i);
                }
            }
            combo->setCurrentIndex(idx);
            qtDefValue = spec->default_value;
            widget = combo;
        }
        else if (G_IS_PARAM_SPEC_FLAGS(static_cast<GParamSpec *>(prop)))
        {
            auto spec = G_PARAM_SPEC_FLAGS(static_cast<GParamSpec *>(prop));
            auto combo = new QxtCheckComboBox;
            GFlagsClass *cls = spec->flags_class;
            for (guint i = 0; i < cls->n_values; ++i)
            {
                combo->addItem(cls->values[i].value_name, cls->values[i].value);
            }
            combo->setMask(elmValue.toUInt64());
            qtDefValue = spec->default_value;
            widget = combo;
        }
        else

        {
            qDebug() << prop->name() << "has unhandled type" << prop->valueType();
            widget = new QLineEdit(elmValue.toString());
        }
        break;
    }

    widget->setProperty("property-name", prop->name());
    widget->setProperty("default-value", qtDefValue);
    widget->setToolTip(prop->description());
    return widget;
}

ElementProperties::ElementProperties
    ( const QString& elementType
    , const QString& properties
    , QWidget *parent
    )
    : QDialog(parent)
{
    setWindowTitle(tr("%1 advanced options").arg(elementType));

    auto layoutMain = new QVBoxLayout();
    auto layoutForm = new QFormLayout();
    try
    {
        auto const& elm = QGst::Parse::launch(QString().append(elementType).append(" ").append(properties));
        auto const& elmType = QGlib::Type::fromInstance(elm);

        foreach (auto const& prop, elm->listProperties())
        {
            if (prop->flags() & QGlib::ParamSpec::Writable && prop->ownerType() == elmType
                    && (prop->valueType().isFundamental()
                        || G_IS_PARAM_SPEC_ENUM(static_cast<GParamSpec *>(prop))
                        || G_IS_PARAM_SPEC_FLAGS(static_cast<GParamSpec *>(prop)))
                    && !isBlacklistedProp(elementType, prop->name()))
            {
                layoutForm->addRow(prop->nick(), createEditor(prop, elm));
            }
        }
    }
    catch (const QGlib::Error& ex)
    {
        const QString msg = ex.message();
        qCritical() << msg;
        QMessageBox::critical(this, windowTitle(), msg, QMessageBox::Ok);
    }

    // Wrap into QScrollArea
    //
    auto scrollAreaContent = new QWidget;
    scrollAreaContent->setLayout(layoutForm);
    auto scrollArea = new QScrollArea;
    scrollArea->setHorizontalScrollBarPolicy (Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy (Qt::ScrollBarAsNeeded);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(scrollAreaContent);
    layoutMain->addWidget(scrollArea);

    // Add buttons row
    //
    auto layoutBtns = new QHBoxLayout;
    layoutBtns->addStretch(1);
    auto btnCancel = new QPushButton(tr("Cancel"));
    connect(btnCancel, SIGNAL(clicked()), this, SLOT(reject()));
    layoutBtns->addWidget(btnCancel);
    auto btnSave = new QPushButton(tr("Save"));
    connect(btnSave, SIGNAL(clicked()), this, SLOT(accept()));
    btnSave->setDefault(true);
    layoutBtns->addWidget(btnSave);
    layoutMain->addStretch(1);
    layoutMain->addLayout(layoutBtns);

    setLayout(layoutMain);
}

QString ElementProperties::getProperties()
{
    QString ret;
    foreach(auto const& widget, findChildren<QWidget*>())
    {
        auto propName = widget->property("property-name");
        if (propName.isNull())
        {
            continue;
        }

        QVariant value;
        QVariant defValue = widget->property("default-value");
        if (widget->inherits("QCheckBox"))
        {
            value = static_cast<QCheckBox*>(widget)->isChecked();
        }
        else if (widget->inherits("QxtCheckComboBox"))
        {
            value = static_cast<QxtCheckComboBox*>(widget)->mask();
        }
        else if (widget->inherits("QComboBox"))
        {
            auto cb = static_cast<QComboBox*>(widget);
            value = cb->itemData(cb->currentIndex());
        }
        else if (widget->inherits("QSpinBox"))
        {
            value = static_cast<QSpinBox*>(widget)->value();
        }
        else if (widget->inherits("QDoubleSpinBox"))
        {
            value = static_cast<QDoubleSpinBox*>(widget)->value();
            if (defValue.type() == QVariant::Double)
            {
                auto epsilon = widget->property("epsilon").toDouble();
                if (std::abs(value.toDouble() - defValue.toDouble()) < epsilon)
                {
                    value = defValue;
                }
            }
            else
            {
                auto epsilon = widget->property("epsilon").toFloat();
                if (std::abs(value.toFloat() - defValue.toFloat()) < epsilon)
                {
                    value = defValue;
                }
            }
        }
        else if (widget->inherits("QLineEdit"))
        {
            value = static_cast<QLineEdit*>(widget)->text();
        }
        else
        {
            qDebug() << widget->metaObject()->className() << propName << "not handled";
            continue;
        }

        if (value != defValue && !value.toString().isEmpty())
        {
            ret.append(" ").append(propName.toString()).append("=");
            if (value.type() == QVariant::String )
            {
                ret.append("\"").append(value.toString()).append("\"");
            }
            else
            {
                ret.append(value.toString());
            }
        }
    }

    return ret;
}
