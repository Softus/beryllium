/*
 * Copyright (C) 2013 Irkutsk Diagnostic Center.
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

#include "product.h"
#include "worklist.h"
#include "dcmclient.h"
#include "dcmconverter.h"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcdicent.h>
#include <dcmtk/dcmdata/dcdict.h>
#include <dcmtk/dcmdata/dcfilefo.h>

#include <dcmtk/dcmnet/assoc.h>
#include <dcmtk/dcmnet/dimse.h>
#include <dcmtk/dcmnet/diutil.h>

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>

#ifdef QT_DEBUG
#define DEFAULT_TIMEOUT 3 // 3 seconds for test builds
#else
#define DEFAULT_TIMEOUT 30 // 30 seconds for prodaction builds
#endif

static void CopyPatientData(/*const*/ DcmDataset* src, DcmDataset* dst)
{
    src->findAndInsertCopyOfElement(DCM_AccessionNumber, dst);
    src->findAndInsertCopyOfElement(DCM_PatientName, dst);
    src->findAndInsertCopyOfElement(DCM_PatientID, dst);
    src->findAndInsertCopyOfElement(DCM_PatientBirthDate, dst);
    src->findAndInsertCopyOfElement(DCM_PatientSex, dst);
}

static void BuildCFindDataSet(DcmDataset& ds)
{
    QSettings settings;
    QString modality = settings.value("worklist-modality").toString().toUpper();
    QString aet = settings.value("worklist-aet").toString();
    if (aet.isEmpty())
    {
        aet = settings.value("aet").toString();
        if (aet.isEmpty())
            aet = qApp->applicationName().toUpper();
    }
    auto scheduledDate = settings.value("worklist-by-date", 1).toInt();

    ds.putAndInsertString(DCM_SpecificCharacterSet, "ISO_IR 192"); // UTF-8
    ds.insertEmptyElement(DCM_AccessionNumber);
    ds.insertEmptyElement(DCM_PatientName);
    ds.insertEmptyElement(DCM_PatientID);
    ds.insertEmptyElement(DCM_PatientBirthDate);
    ds.insertEmptyElement(DCM_PatientSex);
    ds.insertEmptyElement(DCM_PatientSize),
    ds.insertEmptyElement(DCM_PatientWeight),
    ds.insertEmptyElement(DCM_StudyInstanceUID);
    ds.insertEmptyElement(DCM_SeriesInstanceUID);
    ds.insertEmptyElement(DCM_StudyID);
    ds.insertEmptyElement(DCM_RequestedProcedureDescription);
    ds.insertEmptyElement(DCM_RequestedProcedureID);

    DcmItem* sps = nullptr;
    ds.findOrCreateSequenceItem(DCM_ScheduledProcedureStepSequence, sps);
    if (sps)
    {
        sps->putAndInsertString(DCM_Modality, modality.toUtf8());
        sps->putAndInsertString(DCM_ScheduledStationAETitle, aet.toUtf8());
        switch (scheduledDate)
        {
        case 0: // Everything
            sps->insertEmptyElement(DCM_ScheduledProcedureStepStartDate);
            break;
        case 1: // Today
            sps->putAndInsertString(DCM_ScheduledProcedureStepStartDate,
                QDate::currentDate().toString("yyyyMMdd").toUtf8());
            break;
        case 2: // Today +- days
            {
                auto deltaDays = settings.value("worklist-delta", 30).toInt();
                auto range = QDate::currentDate().addDays(-deltaDays).toString("yyyyMMdd") + "-" +
                    QDate::currentDate().addDays(+deltaDays).toString("yyyyMMdd");
                sps->putAndInsertString(DCM_ScheduledProcedureStepStartDate, range.toUtf8());
            }
            break;
        case 3: // From .. to
            {
                auto range = settings.value("worklist-from").toDate().toString("yyyyMMdd") + "-" +
                    settings.value("worklist-to").toDate().toString("yyyyMMdd");
                sps->putAndInsertString(DCM_ScheduledProcedureStepStartDate, range.toUtf8());
            }
            break;
        }

        sps->insertEmptyElement(DCM_ScheduledProcedureStepStartTime);
        sps->insertEmptyElement(DCM_ScheduledPerformingPhysicianName);
        sps->insertEmptyElement(DCM_ScheduledProcedureStepDescription);
        sps->insertEmptyElement(DCM_ScheduledProcedureStepStatus);
        sps->insertEmptyElement(DCM_ScheduledProcedureStepID);
    }
}

static void BuildCStoreDataSet(/*const*/ DcmDataset& patientDs, DcmDataset& cStoreDs, const QString& seriesUID)
{
    auto now = QDateTime::currentDateTime();
    auto modality = QSettings().value("modality").toString().toUpper();

    patientDs.findAndInsertCopyOfElement(DCM_SpecificCharacterSet, &cStoreDs);
    CopyPatientData(&patientDs, &cStoreDs);

    patientDs.findAndInsertCopyOfElement(DCM_StudyInstanceUID, &cStoreDs);
    cStoreDs.putAndInsertString(DCM_SeriesInstanceUID, seriesUID.toUtf8());

    cStoreDs.putAndInsertString(DCM_StudyDate, now.toString("yyyyMMdd").toUtf8());
    cStoreDs.putAndInsertString(DCM_StudyTime, now.toString("HHmmss").toUtf8());

    cStoreDs.putAndInsertString(DCM_Manufacturer, ORGANIZATION_FULL_NAME);
    cStoreDs.putAndInsertString(DCM_ManufacturerModelName, PRODUCT_FULL_NAME);
    cStoreDs.putAndInsertString(DCM_Modality, modality.toUtf8());
}

static void BuildNCreateDataSet(/*const*/ DcmDataset& patientDs, DcmDataset& nCreateDs)
{
    QDateTime now = QDateTime::currentDateTime();
    QSettings settings;
    QString modality = settings.value("modality").toString().toUpper();
    QString aet = settings.value("aet", qApp->applicationName().toUpper()).toString();

    nCreateDs.putAndInsertString(DCM_SpecificCharacterSet, "ISO_IR 192"); // UTF-8
    CopyPatientData(&patientDs, &nCreateDs);
    patientDs.findAndInsertCopyOfElement(DCM_ReferencedPatientSequence, &nCreateDs);
    // MPPS ID has not logic on it. It can be anything
    // The SCU have to create it but it doesn't have to be unique and the SCP
    // should not relay on its uniqueness. Here we use a timestamp
    //
    nCreateDs.putAndInsertString(DCM_PerformedProcedureStepID, now.toString("yyyyMMddHHmmsszzz").toUtf8());
    nCreateDs.putAndInsertString(DCM_PerformedStationAETitle, aet.toUtf8());

    nCreateDs.insertEmptyElement(DCM_PerformedStationName);
    nCreateDs.insertEmptyElement(DCM_PerformedLocation);
    nCreateDs.putAndInsertString(DCM_PerformedProcedureStepStartDate, now.toString("yyyyMMdd").toUtf8());
    nCreateDs.putAndInsertString(DCM_PerformedProcedureStepStartTime, now.toString("HHmmss").toUtf8());
    // Initial status must be 'IN PROGRESS'
    //
    nCreateDs.putAndInsertString(DCM_PerformedProcedureStepStatus, "IN PROGRESS");

    nCreateDs.insertEmptyElement(DCM_PerformedProcedureStepDescription);
    nCreateDs.insertEmptyElement(DCM_PerformedProcedureTypeDescription);
    nCreateDs.insertEmptyElement(DCM_ProcedureCodeSequence);
    nCreateDs.insertEmptyElement(DCM_PerformedProcedureStepEndDate);
    nCreateDs.insertEmptyElement(DCM_PerformedProcedureStepEndTime);

    nCreateDs.putAndInsertString(DCM_Modality, modality.toUtf8());
    patientDs.findAndInsertCopyOfElement(DCM_StudyID, &nCreateDs);
    nCreateDs.insertEmptyElement(DCM_PerformedProtocolCodeSequence);
    nCreateDs.insertEmptyElement(DCM_PerformedSeriesSequence);

    DcmItem* ssas = nullptr;
    nCreateDs.findOrCreateSequenceItem(DCM_ScheduledStepAttributesSequence, ssas);
    if (ssas)
    {
        patientDs.findAndInsertCopyOfElement(DCM_StudyInstanceUID, ssas);
        patientDs.findAndInsertCopyOfElement(DCM_AccessionNumber, ssas);
        patientDs.findAndInsertCopyOfElement(DCM_RequestedProcedureID, ssas);
        patientDs.findAndInsertCopyOfElement(DCM_RequestedProcedureDescription, ssas);

        ssas->insertEmptyElement(DCM_ReferencedStudySequence);

        DcmItem* spss = nullptr;
        patientDs.findAndGetSequenceItem(DCM_ScheduledProcedureStepSequence, spss);
        if (spss)
        {
            spss->findAndInsertCopyOfElement(DCM_ScheduledProcedureStepID, ssas);
            spss->findAndInsertCopyOfElement(DCM_ScheduledProcedureStepDescription, ssas);
            ssas->insertEmptyElement(DCM_ScheduledProtocolCodeSequence);
        }
    }
}

static QString BuildNSetDataSet(/*const*/ DcmDataset& patientDs, DcmDataset& nSetDs, const char* seriesUID, bool completed)
{
    QDateTime now = QDateTime::currentDateTime();
    nSetDs.putAndInsertString(DCM_SpecificCharacterSet, "ISO_IR 192"); // UTF-8
    CopyPatientData(&patientDs, &nSetDs);

    nSetDs.putAndInsertString(DCM_PerformedProcedureStepStatus, completed ? "COMPLETED" : "DISCONTINUED");
    nSetDs.putAndInsertString(DCM_PerformedProcedureStepEndDate, now.toString("yyyyMMdd").toUtf8());
    nSetDs.putAndInsertString(DCM_PerformedProcedureStepEndTime, now.toString("HHmmss").toUtf8());

    nSetDs.insertEmptyElement(DCM_PerformedProcedureStepDescription);
    nSetDs.insertEmptyElement(DCM_PerformedProcedureTypeDescription);
    nSetDs.insertEmptyElement(DCM_ProcedureCodeSequence);
    nSetDs.insertEmptyElement(DCM_PerformedProtocolCodeSequence);

    DcmItem* pss = nullptr;
    nSetDs.findOrCreateSequenceItem(DCM_PerformedSeriesSequence, pss);
    if (pss)
    {
        pss->insertEmptyElement(DCM_PerformingPhysicianName);
//      pss->putAndInsertString(DCM_ProtocolName, "SOME PROTOCOL"); // Is it required?
        pss->insertEmptyElement(DCM_OperatorsName);

        patientDs.findAndInsertCopyOfElement(DCM_StudyInstanceUID, pss);
        pss->putAndInsertString(DCM_SeriesInstanceUID, seriesUID);
        pss->insertEmptyElement(DCM_SeriesDescription);
        pss->insertEmptyElement(DCM_RetrieveAETitle);
        pss->insertEmptyElement(DCM_ReferencedImageSequence);
        pss->insertEmptyElement(DCM_ReferencedNonImageCompositeSOPInstanceSequence);
    }

    return QString::fromUtf8(seriesUID);
}

DcmClient::~DcmClient()
{
    if (assoc)
    {
        qDebug() << "Releasing Association";
        ASC_releaseAssociation(assoc);
        ASC_destroyAssociation(&assoc);
        assoc = nullptr;
    }
    ASC_dropNetwork(&net);
}

int DcmClient::timeout() const
{
    return QSettings().value("timeout", DEFAULT_TIMEOUT).toInt();
}

void DcmClient::loadCallback(void *callbackData,
    T_DIMSE_C_FindRQ* /*request*/, int /*responseCount*/,
    T_DIMSE_C_FindRSP* /*rsp*/, DcmDataset* dset)
{
    static_cast<DcmClient*>(callbackData)->addRow(dset);
}

void DcmClient::abort()
{
    if (assoc)
    {
        qDebug() << "Aborting Association";
        ASC_abortAssociation(assoc);
    }
}

T_ASC_Parameters* DcmClient::initAssocParams(const QString& server, const char* transferSyntax)
{
    auto srvName = QString("servers/").append(server);
    QSettings settings;
    QString missedParameter = tr("Required settings parameter %1 is missing");

    auto values = settings.value(srvName).toStringList();
    if (values.count() < 6)
    {
        cond = makeOFCondition(0, 2, OF_error, missedParameter.arg(srvName).toLocal8Bit());
        return nullptr;
    }

    auto peerAet = values[0].toUtf8();
    auto peerAddress = QString("%1:%2").arg(values[1], values[2]);
    auto timeout = values[3].toInt();

    return initAssocParams(peerAet, peerAddress, timeout, transferSyntax);
}

T_ASC_Parameters* DcmClient::initAssocParams(const QString& peerAet, const QString& peerAddress, int timeout, const char* transferSyntax)
{
    QSettings settings;
    DIC_NODENAME localHost;
    T_ASC_Parameters* params = nullptr;

    cond = ASC_initializeNetwork(NET_REQUESTOR, settings.value("local-port").toInt(), timeout, &net);
    if (cond.good())
    {
        cond = ASC_createAssociationParameters(&params, settings.value("pdu-size", ASC_DEFAULTMAXPDU).toInt());
        if (cond.good())
        {
            ASC_setAPTitles(params, settings.value("aet", qApp->applicationName().toUpper()).toString().toUtf8(), peerAet.toUtf8(), nullptr);

            /* Figure out the presentation addresses and copy the */
            /* corresponding values into the DcmAssoc parameters.*/
            gethostname(localHost, sizeof(localHost) - 1);
            ASC_setPresentationAddresses(params, localHost, peerAddress.toUtf8());

            if (transferSyntax)
            {
                const char* arr[] = { transferSyntax };
                cond = ASC_addPresentationContext(params, 1, abstractSyntax, arr, 1);
            }
            else
            {
                /* Set the presentation contexts which will be negotiated */
                /* when the network connection will be established */
                const char* transferSyntaxes[] =
                {
#if __BYTE_ORDER == __LITTLE_ENDIAN
                    UID_LittleEndianExplicitTransferSyntax, UID_BigEndianExplicitTransferSyntax,
#elif __BYTE_ORDER == __BIG_ENDIAN
                    UID_BigEndianExplicitTransferSyntax, UID_LittleEndianExplicitTransferSyntax,
#else
#error "Unsupported byte order"
#endif
                    UID_LittleEndianImplicitTransferSyntax
                };

                cond = ASC_addPresentationContext(params, 1, abstractSyntax,
                    transferSyntaxes, sizeof(transferSyntaxes)/sizeof(transferSyntaxes[0]));
            }

            if (cond.good())
            {
                return params;
            }

            ASC_destroyAssociationParameters(&params);
        }
    }

    qDebug() << cond.text();
    return nullptr;
}

bool DcmClient::createAssociation(const QString& server, const char* transferSyntax)
{
    if (assoc)
    {
        // Already got a server association
        //
        return true;
    }

    /* create DcmAssoc, i.e. try to establish a network connection to another */
    /* DICOM application. This call creates an instance of T_ASC_DcmAssoc*. */
    T_ASC_Parameters* params = initAssocParams(server, transferSyntax);
    if (params)
    {
        qDebug() << "Requesting DcmAssoc";
        cond = ASC_requestAssociation(net, params, &assoc);
        if (cond.good())
        {
            /* dump general information concerning the establishment of the network connection if required */
            qDebug() << "DcmAssoc accepted (max send PDV: " << assoc->sendPDVLength << ")";

            /* figure out which of the accepted presentation contexts should be used */
            presId = transferSyntax? ASC_findAcceptedPresentationContextID(assoc, abstractSyntax, transferSyntax):
                                     ASC_findAcceptedPresentationContextID(assoc, abstractSyntax);
            if (presId != 0)
            {
                return true;
            }
            cond = makeOFCondition(0, 1, OF_error, tr("Accepted presentation context ID not found").toUtf8());
            ASC_releaseAssociation(assoc);
            ASC_destroyAssociation(&assoc);
            assoc = nullptr;
        }
    }

    qDebug() << QString::fromUtf8(cond.text());
    return false;
}

QString DcmClient::cEcho(const QString &peerAet, const QString &peerAddress, int timeout)
{
    T_ASC_Association *assoc = nullptr;
    cond = ASC_requestAssociation(net, initAssocParams(peerAet, peerAddress, timeout), &assoc);

    if (cond.good())
    {
        DIC_US msgId = assoc->nextMsgID++;
        DIC_US status = -1;

        /* send C-ECHO-RQ and handle response */
        cond = DIMSE_echoUser(assoc, msgId, 0 == timeout? DIMSE_BLOCKING: DIMSE_NONBLOCKING, timeout, &status, nullptr);

        ASC_releaseAssociation(assoc);
        ASC_destroyAssociation(&assoc);

        if (cond.good())
        {
            return QString::fromUtf8(DU_cstoreStatusString(status));
        }
    }

    return cond.text();
}

bool DcmClient::findSCU()
{
    if (!createAssociation(QSettings().value("mwl-server").toString()))
    {
        return false;
    }

    DcmDataset ds;
    BuildCFindDataSet(ds);

    /* prepare C-FIND-RQ message */
    T_DIMSE_C_FindRQ req;
    T_DIMSE_C_FindRSP rsp;
    bzero(OFreinterpret_cast(char*, &req), sizeof(req));
    strcpy(req.AffectedSOPClassUID, abstractSyntax);
    req.DataSetType = DIMSE_DATASET_PRESENT;
    req.Priority = DIMSE_PRIORITY_LOW;
    /* complete preparation of C-FIND-RQ message */
    req.MessageID = assoc->nextMsgID++;

    /* finally conduct transmission of data */
    DcmDataset *statusDetail = nullptr;
    int tout = timeout();
    cond = DIMSE_findUser(assoc, presId, &req, &ds,
        loadCallback, static_cast<void*>(this),
        0 == tout? DIMSE_BLOCKING: DIMSE_NONBLOCKING, tout,
        &rsp, &statusDetail);

    if (statusDetail != nullptr)
    {
        delete statusDetail;
    }

    return cond.good();
}

QString DcmClient::nCreateRQ(DcmDataset* patientDs)
{
    if (!createAssociation(QSettings().value("mpps-server").toString()))
    {
        return nullptr;
    }

    DcmDataset nCreateDs;
    BuildNCreateDataSet(*patientDs, nCreateDs);

    patientDs->writeXML(std::cout << "------C-FIND------------" << std::endl);
    nCreateDs.writeXML(std::cout << "------N-Create------------" << std::endl);

    T_DIMSE_Message req, rsp;
    bzero((char*)&req, sizeof(req));
    bzero((char*)&rsp, sizeof(rsp));

    req.CommandField = DIMSE_N_CREATE_RQ;
    req.msg.NCreateRQ.MessageID = assoc->nextMsgID++;;
    strcpy(req.msg.NCreateRQ.AffectedSOPClassUID, abstractSyntax);
    req.msg.NCreateRQ.DataSetType = DIMSE_DATASET_PRESENT;

    cond = DIMSE_sendMessageUsingMemoryData(assoc, presId, &req, nullptr, &nCreateDs, nullptr, nullptr);
    if (cond.bad())
    {
        return nullptr;
    }

    /* receive response */
    int tout = timeout();
    cond = DIMSE_receiveCommand(assoc, 0 == tout? DIMSE_BLOCKING: DIMSE_NONBLOCKING, tout, &presId, &rsp, nullptr);
    if (cond.bad())
    {
        return nullptr;
    }

    if (rsp.CommandField != DIMSE_N_CREATE_RSP)
    {
        qDebug() << "DIMSE: Unexpected Response Command Field: "<< rsp.CommandField;
        return nullptr;
    }

    if (rsp.msg.NCreateRSP.MessageIDBeingRespondedTo != req.msg.NCreateRQ.MessageID)
    {
        qDebug() << "DIMSE: Unexpected Response MsgId: " << rsp.msg.NCreateRSP.MessageIDBeingRespondedTo
                 << " (expected: " << req.msg.NCreateRQ.MessageID << ")";
        return nullptr;
    }

    return QString::fromUtf8(rsp.msg.NCreateRSP.AffectedSOPInstanceUID);
}

bool DcmClient::nSetRQ(const char* seriesUID, DcmDataset* patientDs, const QString& sopInstance)
{
    if (!createAssociation(QSettings().value("mpps-server").toString()))
    {
        return nullptr;
    }

    DcmDataset nSetDs;
    BuildNSetDataSet(*patientDs, nSetDs, seriesUID, true);

    nSetDs.writeXML(std::cout << "------N-Set------------" << std::endl);

    T_DIMSE_Message req, rsp;

    bzero((char*)&req, sizeof(req));
    bzero((char*)&rsp, sizeof(rsp));

    req.CommandField = DIMSE_N_SET_RQ;
    req.msg.NSetRQ.MessageID = assoc->nextMsgID++;;
    strcpy(req.msg.NSetRQ.RequestedSOPClassUID, abstractSyntax);
    strcpy(req.msg.NSetRQ.RequestedSOPInstanceUID, sopInstance.toUtf8());
    req.msg.NSetRQ.DataSetType = DIMSE_DATASET_PRESENT;

    OFCondition cond = DIMSE_sendMessageUsingMemoryData(assoc, presId, &req, nullptr, &nSetDs, nullptr, nullptr);
    if (cond.bad()) return false;

    /* receive response */
    int tout = timeout();
    cond = DIMSE_receiveCommand(assoc, 0 == tout? DIMSE_BLOCKING: DIMSE_NONBLOCKING, tout, &presId, &rsp, nullptr);
    if (cond.bad()) return false;

    if (rsp.CommandField != DIMSE_N_SET_RSP)
    {
        qDebug() << "DIMSE: Unexpected Response Command Field: "<< rsp.CommandField;
        return false;
    }

    if (rsp.msg.NSetRSP.MessageIDBeingRespondedTo != req.msg.NSetRQ.MessageID)
    {
        qDebug() << "DIMSE: Unexpected Response MsgId: " << rsp.msg.NSetRSP.MessageIDBeingRespondedTo
                 << " (expected: " << req.msg.NSetRQ.MessageID << ")";
        return false;
    }

    return true;
}

static void StoreUserCallback
    ( void *callbackData
    , T_DIMSE_StoreProgress *
    , T_DIMSE_C_StoreRQ *
   )
{
    auto pThis = static_cast<DcmClient*>(callbackData);

    if (pThis->progressDlg && pThis->progressDlg->wasCanceled())
    {
        pThis->abort();
    }
    qApp->processEvents();
}

bool DcmClient::cStoreRQ(DcmDataset* dset, const char* sopInstance)
{
    T_DIMSE_C_StoreRQ req;
    T_DIMSE_C_StoreRSP rsp;
    bzero((char*)&req, sizeof(req));
    bzero((char*)&rsp, sizeof(rsp));
    DcmDataset *statusDetail = nullptr;

    /* prepare the transmission of data */
    req.MessageID = assoc->nextMsgID++;
    strcpy(req.AffectedSOPClassUID, abstractSyntax);
    strcpy(req.AffectedSOPInstanceUID, sopInstance);
    req.DataSetType = DIMSE_DATASET_PRESENT;
    req.Priority = DIMSE_PRIORITY_LOW;

    /* finally conduct transmission of data */
    int tout = timeout();
    cond = DIMSE_storeUser(assoc, presId, &req,
      nullptr, dset, StoreUserCallback, this,
      0 == tout? DIMSE_BLOCKING: DIMSE_NONBLOCKING, tout,
      &rsp, &statusDetail);

    delete statusDetail;
    return cond.good();
}

bool DcmClient::sendToServer(const QString& server, DcmDataset* patientDs, const QString& seriesUID,
                             int seriesNumber, const QString& file, int instanceNumber)
{
    E_TransferSyntax writeXfer;

    char instanceUID[100] = {0};
    dcmGenerateUniqueIdentifier(instanceUID,  SITE_INSTANCE_UID_ROOT);

    qDebug() << file;

    DcmDataset ds;
    cond = readAndInsertPixelData(file, &ds, writeXfer);
    if (cond.bad())
    {
        qDebug() << QString::fromUtf8(cond.text());
        // cond.reset()
        //
        cond = EC_Normal;
        return true;
    }

    cond = ds.putAndInsertString(DCM_SOPInstanceUID, instanceUID);
    if (cond.bad())
        return false;
    cond = ds.putAndInsertString(DCM_InstanceNumber, QString::number(instanceNumber).toUtf8());
    if (cond.bad())
        return false;
    cond = ds.putAndInsertString(DCM_SeriesNumber, QString::number(seriesNumber).toUtf8());
    if (cond.bad())
        return false;

    BuildCStoreDataSet(*patientDs, ds, seriesUID);
    //DcmFileFormat dcmff(dset);
    //cond = dcmff.saveFile(src.getImageFile().append(".dcm").c_str(), writeXfer);

    DcmXfer filexfer((E_TransferSyntax)writeXfer);

    if (!createAssociation(server.toUtf8(), filexfer.getXferID()))
    {
        return false;
    }

    cStoreRQ(&ds, instanceUID);

    ASC_releaseAssociation(assoc);
    ASC_destroyAssociation(&assoc);
    assoc = nullptr;

    return cond.good();
}

void DcmClient::sendToServer(QWidget* parent, DcmDataset* patientDs, const QFileInfoList& files, const QString& seriesUID)
{
    QProgressDialog pdlg(parent);
    progressDlg = &pdlg;

    pdlg.setRange(0, files.count());
    pdlg.setMinimumDuration(0);

    // Only single series for now
    //
    int seriesNo = 1;

    foreach (auto server, QSettings().value("storage-servers").toStringList())
    {
        for (auto i = 0; !pdlg.wasCanceled() && i < files.count(); ++i)
        {
            if (QFile::exists(files[i].dir().filePath(files[i].completeBaseName())))
            {
                // Skip clip thumbnail
                //
                continue;
            }

            pdlg.setValue(i);
            pdlg.setLabelText(tr("Storing '%1' to '%2'").arg(files[i].fileName(), server));
            qApp->processEvents();

            auto file = files[i].absoluteFilePath();
            if (!sendToServer(server, patientDs, seriesUID, seriesNo, file, i))
            {
                if (QMessageBox::Yes != QMessageBox::critical(&pdlg, parent->windowTitle(),
                      tr("Faild to send '%1' to '%2':\n%3\nContinue?").arg(file, server, lastError()),
                      QMessageBox::Yes, QMessageBox::No))
                {
                    break;
                }
            }
        }
    }
    progressDlg = nullptr;
}