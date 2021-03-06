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

#include "patientdatadialog.h"
#include "product.h"
#include "defaults.h"
#include "mandatoryfieldgroup.h"

#ifdef WITH_DICOM
#ifdef UNICODE
#define DCMTK_UNICODE_BUG_WORKAROUND
#undef UNICODE
#endif

// From DCMTK SDK
//
#define HAVE_CONFIG_H
#include <dcmtk/config/osconfig.h>   /* make sure OS specific configuration is included first */
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcdeftag.h>

#ifdef DCMTK_UNICODE_BUG_WORKAROUND
#define UNICODE
#undef DCMTK_UNICODE_BUG_WORKAROUND
#endif
#endif

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#ifdef WITH_QT_DBUS
#include <QDBusInterface>
#endif
#include <QDebug>
#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QMoveEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <qxtlineedit.h>

PatientDataDialog::PatientDataDialog(bool noWorklist, const QString& settingsKey, QWidget *parent)
    : QDialog(parent)
    , settingsKey(settingsKey)
    , moveCounter(0)
{
    QSettings settings;
    auto const& listMandatory = settings.value("ui/patient-data-mandatory-fields",
        DEFAULT_MANDATORY_FIELDS).toStringList();
    auto showAccessionNumber = settings.value("ui/patient-data-show-accession-number").toBool();

    setWindowTitle(tr("Patient data"));
    setMinimumSize(600, 300);

    auto layoutMain = new QFormLayout;
    textAccessionNumber = new QxtLineEdit;
    if (showAccessionNumber)
    {
        layoutMain->addRow(tr("&Accession number"), textAccessionNumber);
    }
    layoutMain->addRow(tr("&Patient ID"), textPatientId = new QxtLineEdit);
    layoutMain->addRow(tr("&Name"), textPatientName = new QxtLineEdit);
    layoutMain->addRow(tr("&Sex"), cbPatientSex = new QComboBox);
    cbPatientSex->setLineEdit(new QxtLineEdit);
    cbPatientSex->addItems(QStringList() << "" << tr("female") << tr("male") << tr("other"));
    QChar patientSexCodes[] = {'U', 'F', 'M', 'O'};
    for (int i = 0; i < cbPatientSex->count(); ++i)
    {
        cbPatientSex->setItemData(i, patientSexCodes[i]);
    }
    cbPatientSex->setEditable(true);

    layoutMain->addRow(tr("&Birthday"), dateBirthday = new QDateEdit);
    dateBirthday->setCalendarPopup(true);
    dateBirthday->setDisplayFormat(tr("MM/dd/yyyy"));

    layoutMain->addRow(tr("P&hysician"), cbPhysician = new QComboBox);
    cbPhysician->setLineEdit(new QxtLineEdit);
    cbPhysician->addItems(settings.value("physicians").toStringList());
    cbPhysician->setCurrentIndex(0); // Select first, if any
    cbPhysician->setEditable(true);

    layoutMain->addRow(tr("Study &type"), cbStudyDescription = new QComboBox);
    cbStudyDescription->setLineEdit(new QxtLineEdit);
    cbStudyDescription->addItems(settings.value("studies").toStringList());
    cbStudyDescription->setCurrentIndex(0); // Select first, if any
    cbStudyDescription->setEditable(true);

    layoutMain->addRow(tr("&Organization"), textIssuerOfPatientId = new QxtLineEdit);

    // Empty row
    layoutMain->addRow(new QLabel, new QLabel);

    checkDontShow = new QCheckBox(
        tr("Show this dialog if the Shift key is down or some data is required."));
    layoutMain->addRow(nullptr, checkDontShow);
    if (!qApp->keyboardModifiers().testFlag(Qt::ShiftModifier))
    {
        settings.beginGroup("confirmations");
        checkDontShow->setChecked(settings.value(settingsKey).toBool());
        settings.endGroup();
    }

    auto layoutBtns = new QHBoxLayout;

#ifdef WITH_DICOM
    if (!noWorklist)
    {
        auto btnWorklist = new QPushButton(QIcon(":buttons/show_worklist"), nullptr);
        btnWorklist->setToolTip(tr("Show work list"));
        connect(btnWorklist, SIGNAL(clicked()), this, SLOT(onShowWorklist()));
        layoutBtns->addWidget(btnWorklist);
        btnWorklist->setEnabled(!settings.value("dicom/mwl-server").toString().isEmpty());
    }
#else
    // Suppres warning
    Q_UNUSED(noWorklist)
#endif

    layoutBtns->addStretch(1);

    auto btnReject = new QPushButton(tr("Cancel"));
    connect(btnReject, SIGNAL(clicked()), this, SLOT(reject()));
    layoutBtns->addWidget(btnReject);

    btnStart = new QPushButton(tr("Start"));
    connect(btnStart, SIGNAL(clicked()), this, SLOT(accept()));
    btnStart->setDefault(true);
    layoutBtns->addWidget(btnStart);

    layoutMain->addRow(layoutBtns);

    setLayout(layoutMain);
    restoreGeometry(settings.value("ui/patient-data-geometry").toByteArray());
    setWindowState((Qt::WindowState)settings.value("ui/patient-data-state").toInt());

    if (!listMandatory.isEmpty())
    {
        auto group = new MandatoryFieldGroup(this);
        if (listMandatory.contains("AccessionNumber") && showAccessionNumber)
            group->add(textAccessionNumber);
        if (listMandatory.contains("PatientID"))
            group->add(textPatientId);
        if (listMandatory.contains("IssuerOfPatientID"))
            group->add(textIssuerOfPatientId);
        if (listMandatory.contains("Name"))
            group->add(textPatientName);
        if (listMandatory.contains("Sex"))
            group->add(cbPatientSex);
        if (listMandatory.contains("Birthday"))
            group->add(dateBirthday);
        if (listMandatory.contains("Physician"))
            group->add(cbPhysician);
        if (listMandatory.contains("StudyType"))
            group->add(cbStudyDescription);
        group->setOkButton(btnStart);
    }
}

void PatientDataDialog::moveEvent(QMoveEvent *evt)
{
    ++moveCounter;
    QDialog::moveEvent(evt);
}

void PatientDataDialog::resizeEvent(QResizeEvent *evt)
{
    ++moveCounter;
    QDialog::resizeEvent(evt);
}

void PatientDataDialog::showEvent(QShowEvent *evt)
{
    QSettings settings;
#ifdef WITH_QT_DBUS
    if (settings.value("show-onboard").toBool())
    {
        QDBusInterface("org.onboard.Onboard", "/org/onboard/Onboard/Keyboard",
                       "org.onboard.Onboard.Keyboard").call( "Show");
    }
#endif
    QDialog::showEvent(evt);
}

void PatientDataDialog::hideEvent(QHideEvent *evt)
{
    QSettings settings;
    if (moveCounter > 10)
    {
        settings.setValue("ui/patient-data-geometry", saveGeometry());
    }
    settings.setValue("ui/patient-data-state", (int)windowState() & ~Qt::WindowMinimized);

#ifdef WITH_QT_DBUS
    if (settings.value("show-onboard").toBool())
    {
        QDBusInterface("org.onboard.Onboard", "/org/onboard/Onboard/Keyboard",
                       "org.onboard.Onboard.Keyboard").call( "Hide");
    }
#endif
    QDialog::hideEvent(evt);
}

void PatientDataDialog::done(int result)
{
    if (result == QDialog::Accepted && checkDontShow->isChecked())
    {
        QSettings settings;
        settings.beginGroup("confirmations");
        settings.setValue(settingsKey, true);
    }

    QDialog::done(result);
}

int PatientDataDialog::exec()
{
    QSettings settings;
    settings.beginGroup("confirmations");
    if (!qApp->keyboardModifiers().testFlag(Qt::ShiftModifier)
        && settings.value(settingsKey).toBool() && btnStart->isEnabled())
    {
        return QDialog::Accepted;
    }

    settings.setValue(settingsKey, false);
    return QDialog::exec();
}

QString PatientDataDialog::accessionNumber() const
{
    return textAccessionNumber->text();
}

QString PatientDataDialog::patientId() const
{
    return textPatientId->text();
}

QString PatientDataDialog::issuerOfPatientId() const
{
    return textIssuerOfPatientId->text();
}

QString PatientDataDialog::patientName() const
{
    return textPatientName->text();
}

QDate PatientDataDialog::patientBirthDate() const
{
    return dateBirthday->date();
}

QString PatientDataDialog::patientBirthDateStr() const
{
    return patientBirthDate().toString("yyyyMMdd");
}

QString PatientDataDialog::patientSex() const
{
    return cbPatientSex->currentText();
}

QChar PatientDataDialog::patientSexCode() const
{
    auto idx = cbPatientSex->currentIndex();
    return idx < 0? '\x0': cbPatientSex->itemData(idx).toChar();
}

QString PatientDataDialog::studyDescription() const
{
    return cbStudyDescription->currentText();
}

QString PatientDataDialog::physician() const
{
    return cbPhysician->currentText();
}

void PatientDataDialog::setAccessionNumber(const QString& accessionNumber)
{
    textAccessionNumber->setText(accessionNumber);
}

void PatientDataDialog::setPatientId(const QString& id)
{
    textPatientId->setText(id);
}

void PatientDataDialog::setIssuerOfPatientId(const QString& id)
{
    textIssuerOfPatientId->setText(id);
}

void PatientDataDialog::setPatientName(const QString& name)
{
    textPatientName->setText(name);
}

void PatientDataDialog::setPatientSex(const QString& sex)
{
    // For 'Female' search for text, for 'F' search for data
    //
    auto idx = sex.length() != 1 ? cbPatientSex->findText(sex)
                                 : cbPatientSex->findData(sex[0].toUpper());
    if (idx < 0)
    {
        cbPatientSex->setEditText(sex);
    }
    else
    {
        cbPatientSex->setCurrentIndex(idx);
    }
}

void PatientDataDialog::setPatientBirthDate(const QDate& date)
{
    dateBirthday->setDate(date);
}

void PatientDataDialog::setPatientBirthDateStr(const QString& dateStr)
{
    setPatientBirthDate(QDate::fromString(dateStr, "yyyyMMdd"));
}

void PatientDataDialog::setStudyDescription(const QString& name)
{
    auto idx = cbStudyDescription->findText(name);
    cbStudyDescription->setCurrentIndex(idx);

    if (idx < 0)
    {
        cbStudyDescription->setEditText(name);
    }
}

void PatientDataDialog::setPhysician(const QString& name)
{
    auto idx = cbPhysician->findText(name);
    cbPhysician->setCurrentIndex(idx);

    if (idx < 0)
    {
        cbPhysician->setEditText(name);
    }
}

void PatientDataDialog::readPatientData(QSettings& settings)
{
    settings.beginGroup(PRODUCT_SHORT_NAME);
    setAccessionNumber(settings.value("accession-number").toString());
    setPatientId(settings.value("patient-id").toString());
    setIssuerOfPatientId(settings.value("issuer-of-patient-id").toString());
    setPatientName(settings.value("name").toString());
    setPatientSex(settings.value("sex").toString());
    setPatientBirthDateStr(settings.value("birthday").toString());
    setPhysician(settings.value("physician").toString());
    setStudyDescription(settings.value("study-description").toString());
    settings.endGroup();
}

void PatientDataDialog::savePatientData(QSettings& settings)
{
    settings.beginGroup(PRODUCT_SHORT_NAME);
    settings.setValue("accession-number", accessionNumber());
    settings.setValue("patient-id", patientId());
    settings.setValue("issuer-of-patient-id", issuerOfPatientId());
    settings.setValue("name", patientName());
    settings.setValue("sex", QString(patientSexCode()));
    settings.setValue("birthday", patientBirthDateStr());
    settings.setValue("physician", physician());
    settings.setValue("study-description", studyDescription());
    settings.endGroup();
}

void PatientDataDialog::onShowWorklist()
{
    done(SHOW_WORKLIST_RESULT);
}

#ifdef WITH_DICOM
void PatientDataDialog::readPatientData(DcmDataset* patient)
{
    if (!patient)
        return;

    const char *str = nullptr;
    if (patient->findAndGetString(DCM_AccessionNumber, str, true).good())
    {
        setAccessionNumber(QString::fromUtf8(str));
    }

    if (patient->findAndGetString(DCM_PatientID, str, true).good())
    {
        setPatientId(QString::fromUtf8(str));
    }

    if (patient->findAndGetString(DCM_IssuerOfPatientID, str, true).good())
    {
        setIssuerOfPatientId(QString::fromUtf8(str));
    }

    if (patient->findAndGetString(DCM_PatientName, str, true).good())
    {
        setPatientName(QString::fromUtf8(str));
    }

    if (patient->findAndGetString(DCM_PatientBirthDate, str, true).good())
    {
        setPatientBirthDateStr(QString::fromUtf8(str));
    }

    if (patient->findAndGetString(DCM_PatientSex, str, true).good())
    {
        setPatientSex(QString::fromUtf8(str));
    }

    if (patient->findAndGetString(DCM_ScheduledPerformingPhysicianName, str, true).good() ||
        patient->findAndGetString(DCM_PerformingPhysicianName, str, true).good())
    {
        setPhysician(QString::fromUtf8(str));
    }

    if (patient->findAndGetString(DCM_ScheduledProcedureStepDescription, str, true).good() ||
        patient->findAndGetString(DCM_StudyDescription, str, true).good())
    {
        setStudyDescription(QString::fromUtf8(str));
    }
}

void PatientDataDialog::savePatientData(DcmDataset* patient)
{
    if (!patient)
        return;

    OFString studyInstanceUID;
    char uuid[100] = {0};
    if (patient->findAndGetOFString(DCM_StudyInstanceUID, studyInstanceUID).bad()
        || studyInstanceUID.length() == 0)
    {
        patient->putAndInsertString(DCM_StudyInstanceUID,
            dcmGenerateUniqueIdentifier(uuid, SITE_STUDY_UID_ROOT));
    }

    patient->putAndInsertString(DCM_SpecificCharacterSet, "ISO_IR 192");
    patient->putAndInsertString(DCM_AccessionNumber, accessionNumber().toUtf8());
    patient->putAndInsertString(DCM_PatientID, patientId().toUtf8());
    patient->putAndInsertString(DCM_IssuerOfPatientID, issuerOfPatientId().toUtf8());
    patient->putAndInsertString(DCM_PatientName, patientName().toUtf8());
    patient->putAndInsertString(DCM_PatientBirthDate, patientBirthDateStr().toUtf8());
    patient->putAndInsertString(DCM_PatientSex, QString().append(patientSexCode()).toUtf8());
    patient->putAndInsertString(DCM_PerformingPhysicianName, physician().toUtf8());
    patient->putAndInsertString(DCM_StudyDescription, studyDescription().toUtf8());
    patient->putAndInsertString(DCM_SeriesDescription, studyDescription().toUtf8());
    patient->putAndInsertString(DCM_SOPInstanceUID,
        dcmGenerateUniqueIdentifier(uuid, SITE_INSTANCE_UID_ROOT));
}

#endif
