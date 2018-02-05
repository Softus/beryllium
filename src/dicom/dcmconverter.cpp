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

#include "../product.h"
#include "../defaults.h"
#include <stdexcept>

#ifdef UNICODE
#define DCMTK_UNICODE_BUG_WORKAROUND
#undef UNICODE
#endif

#include "dcmconverter.h"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcuid.h>

#include <dcmtk/dcmdata/dcpixel.h>   /* for DcmPixelData */
#include <dcmtk/dcmdata/dcpixseq.h>  /* for DcmPixelSequence */
#include <dcmtk/dcmdata/dcpxitem.h>  /* for DcmPixelItem */

#ifdef DCMTK_UNICODE_BUG_WORKAROUND
#define UNICODE
#undef DCMTK_UNICODE_BUG_WORKAROUND
#endif

#ifdef DCDEFINE_H
#define OFFIS_DCMTK_VER 0x030601
#else
#define OFFIS_DCMTK_VER 0x030600
#endif

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

#if defined(UNICODE) || defined (_UNICODE)
#include <MediaInfo/MediaInfo.h>
#else
#define UNICODE
#include <MediaInfo/MediaInfo.h>
#undef UNICODE
#endif

static QString getGenericSopClass(const QString& mimeType)
{
    if (mimeType == "application/pdf")
    {
        return UID_EncapsulatedPDFStorage;
    }

    return UID_RawDataStorage;
}

static QString getImageSopClass(const QSettings& settings)
{
    Q_ASSERT(settings.group() == "dicom");
    auto sopClass = settings.value("image-sopclass").toString();
    if (!sopClass.isEmpty())
    {
        return sopClass;
    }

    auto modality = settings.value("modality", DEFAULT_MODALITY).toString();

    return
        modality == "ES"? UID_VLEndoscopicImageStorage:
        modality == "US"? UID_UltrasoundImageStorage:
        modality == "GM"? UID_VLMicroscopicImageStorage:
        modality == "XC"? UID_VLPhotographicImageStorage:
        "??";
}

static QString getVideoSopClass(const QSettings& settings)
{
    Q_ASSERT(settings.group() == "dicom");
    auto sopClass = settings.value("video-sopclass").toString();
    if (!sopClass.isEmpty())
    {
        return sopClass;
    }

    auto modality = settings.value("modality", DEFAULT_MODALITY).toString();

    return
        modality == "ES"? UID_VideoEndoscopicImageStorage:
        modality == "US"? UID_UltrasoundMultiframeImageStorage:
        modality == "GM"? UID_VideoMicroscopicImageStorage:
        modality == "XC"? UID_VideoPhotographicImageStorage:
        "??";
}

class DcmConverter
{
    MediaInfoLib::MediaInfo mi;
    MediaInfoLib::stream_t  type;

    QString getStr(const MediaInfoLib::String &parameter)
    {
        return QString::fromStdWString(mi.Get(type, 0, parameter));
    }

    Uint16 getUint16(const MediaInfoLib::String &parameter)
    {
        return getStr(parameter).toUShort();
    }

public:
    DcmConverter()
        : type(MediaInfoLib::Stream_General)
    {
    }

    ~DcmConverter()
    {
        mi.Close();
    }

    OFCondition readPixelData
        ( DcmDataset* dset
        , uchar*  pixData
        , qint64 length
        , E_TransferSyntax& ts
        )
    {
        OFCondition cond;
        QSettings settings;
        settings.beginGroup("dicom");

        try
        {
            if (!mi.Open_Buffer_Init(length))
            {
                return makeOFCondition(0, 2, OF_error, "Failed to open buffer");
            }
            mi.Open_Buffer_Continue(pixData, length);
            qDebug() << QString::fromStdWString(mi.Inform());
        }
        catch (std::logic_error e)
        {
            return makeOFCondition(0, 2, OF_error, e.what());
        }

        if (getUint16(__T("ImageCount")) > 0)
        {
            type = MediaInfoLib::Stream_Image;
        }
        else if (getUint16(__T("VideoCount")) > 0)
        {
            type = MediaInfoLib::Stream_Video;
        }
        else
        {
            return makeOFCondition(0, 3, OF_error, "Unsupported format");
        }

        cond = dset->putAndInsertUint16(DCM_Rows, getUint16(__T("Height")));
        if (cond.bad())
          return cond;

        cond = dset->putAndInsertUint16(DCM_Columns, getUint16(__T("Width")));
        if (cond.bad())
          return cond;

        auto bitDepth = getUint16(__T("BitDepth"));

        cond = dset->putAndInsertUint16(DCM_BitsAllocated, bitDepth);
        if (cond.bad())
          return cond;

        cond = dset->putAndInsertUint16(DCM_BitsStored, bitDepth);
        if (cond.bad())
          return cond;

        cond = dset->putAndInsertUint16(DCM_HighBit, bitDepth - 1);
        if (cond.bad())
          return cond;

        cond = dset->putAndInsertUint16(DCM_PixelRepresentation, 0);
        if (cond.bad())
          return cond;

        auto subsampling = getStr(__T("ChromaSubsampling"));
        if (subsampling == "4:2:0")
        {
            cond = dset->putAndInsertString(DCM_PhotometricInterpretation, "YBR_FULL");
            if (cond.bad())
              return cond;

            cond = dset->putAndInsertUint16(DCM_SamplesPerPixel, 3);
            if (cond.bad())
              return cond;

            // Should only be written if Samples per Pixel > 1
            //
            cond = dset->putAndInsertUint16(DCM_PlanarConfiguration, 0);
            if (cond.bad())
              return cond;
        }
        else if (subsampling == "4:2:2")
        {
            cond = dset->putAndInsertString(DCM_PhotometricInterpretation, "YBR_FULL_422");
            if (cond.bad())
              return cond;

            cond = dset->putAndInsertUint16(DCM_SamplesPerPixel, 3);
            if (cond.bad())
              return cond;

            // Should only be written if Samples per Pixel > 1
            //
            cond = dset->putAndInsertUint16(DCM_PlanarConfiguration, 0);
            if (cond.bad())
              return cond;
        }
        else
        {
            cond = dset->putAndInsertUint16(DCM_SamplesPerPixel, 1);
            if (cond.bad())
              return cond;

            cond = dset->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME1");
            if (cond.bad())
              return cond;
        }

        auto codec = getStr(__T("Codec"));

        if (type == MediaInfoLib::Stream_Image)
        {
            cond = dset->putAndInsertString(DCM_SOPClassUID, getImageSopClass(settings).toUtf8());
            if (cond.bad())
              return cond;

            if (getStr(__T("Compression_Mode")) == "Lossy")
            {
                cond = dset->putAndInsertOFStringArray(DCM_LossyImageCompression, "01");
                if (cond.bad())
                  return cond;

                cond = dset->putAndInsertOFStringArray(DCM_LossyImageCompressionMethod,
                    "ISO_10918_1");
                if (cond.bad())
                  return cond;
            }

            if (0 == codec.compare("JPEG", Qt::CaseInsensitive))
            {
#if OFFIS_DCMTK_VER < 0x030601
                ts = EXS_JPEGProcess1TransferSyntax;
#else
                ts = EXS_JPEGProcess1;
#endif
            }
            else
            {
                return makeOFCondition(0, 5, OF_error, "Unsupported image format");
            }
        }
        else
        {
            auto frameRate = getStr(__T("FrameRate")).toDouble();
            cond = dset->putAndInsertString(DCM_CineRate,
                QString::number((Uint16)(frameRate + 0.5)).toUtf8());
            if (cond.bad())
              return cond;

            cond = dset->putAndInsertString(DCM_FrameTime,
                QString::number(1000.0 / frameRate).toUtf8());
            if (cond.bad())
              return cond;

            cond = dset->putAndInsertString(DCM_NumberOfFrames, getStr(__T("FrameCount")).toUtf8());
            if (cond.bad())
              return cond;

#if OFFIS_DCMTK_VER >= 0x030601
            cond = dset->putAndInsertTagKey(DCM_FrameIncrementPointer, DCM_FrameTime);
            if (cond.bad())
              return cond;
#endif

            if (settings.value("store-video-as-binary", DEFAULT_STORE_VIDEO_AS_BINARY).toBool())
            {
                ts = EXS_LittleEndianExplicit;

                cond = dset->putAndInsertString(DCM_SOPClassUID, UID_RawDataStorage);
                if (cond.bad())
                  return cond;
            }
            else
            {
                auto sopClass = getVideoSopClass(settings);
                cond = dset->putAndInsertString(DCM_SOPClassUID, sopClass.toUtf8());
                if (cond.bad())
                  return cond;

                auto codecProfile = getStr(__T("Codec_Profile"));
                if (0 == codec.compare("MPEG-2V", Qt::CaseInsensitive))
                {
                    if (codecProfile.startsWith("main@", Qt::CaseInsensitive))
                    {
                        ts = EXS_MPEG2MainProfileAtMainLevel;
                    }
                    else
                    {
                        ts = EXS_MPEG2MainProfileAtHighLevel;
                    }
                }
#if OFFIS_DCMTK_VER >= 0x030601
                else if (0 == codec.compare("AVC", Qt::CaseInsensitive))
                {
                    if (codecProfile.startsWith("main@", Qt::CaseInsensitive))
                    {
                        ts = EXS_MPEG4HighProfileLevel4_1;
                    }
                    else
                    {
                        ts = EXS_MPEG4BDcompatibleHighProfileLevel4_1;
                    }
                }
#endif
                else
                {
                    qDebug() << "Unknown 'codec " << codec << "' the file will be stored as binary";
                    ts = EXS_LittleEndianExplicit;
                }
            }
        }
        return EC_Normal;
    }
};

static OFCondition
insertEncapsulatedPixelData
    ( DcmDataset* dset
    , uchar *pixData
    , qint64 length
    , E_TransferSyntax outputTS
    )
{
  OFCondition cond;

  // create initial pixel sequence
  DcmPixelSequence* pixelSequence = new DcmPixelSequence(DcmTag(DCM_PixelData, EVR_OB));
  if (pixelSequence == NULL)
    return EC_MemoryExhausted;

  // insert empty offset table into sequence
  DcmPixelItem *offsetTable = new DcmPixelItem(DcmTag(DCM_Item, EVR_OB));
  if (offsetTable == NULL)
  {
    delete pixelSequence; pixelSequence = NULL;
    return EC_MemoryExhausted;
  }
  cond = pixelSequence->insert(offsetTable);
  if (cond.bad())
  {
    delete offsetTable; offsetTable = NULL;
    delete pixelSequence; pixelSequence = NULL;
    return cond;
  }

  // store compressed frame into pixel seqeuence
  DcmOffsetList dummyList;
  cond = pixelSequence->storeCompressedFrame(dummyList, pixData, length, 0);
  if (cond.bad())
  {
    delete pixelSequence; pixelSequence = NULL;
    return cond;
  }

  // insert pixel data attribute incorporating pixel sequence into dataset
  DcmPixelData *pixelData = new DcmPixelData(DCM_PixelData);
  if (pixelData == NULL)
  {
    delete pixelSequence; pixelSequence = NULL;
    return EC_MemoryExhausted;
  }
  /* tell pixel data element that this is the original presentation of the pixel data
   * pixel data and how it compressed
   */
  pixelData->putOriginalRepresentation(outputTS, NULL, pixelSequence);
  cond = dset->insert(pixelData);
  if (cond.bad())
  {
    delete pixelData; pixelData = NULL; // also deletes contained pixel sequence
    return cond;
  }

  return EC_Normal;
}

OFCondition
readAndInsertPixelData
    ( const QString& fileName
    , DcmDataset* dset
    , E_TransferSyntax& outputTS
    )
{
    QFile file(fileName);
    outputTS = EXS_Unknown;

    if (!file.open(QFile::ReadOnly))
    {
        return makeOFCondition(0, 1, OF_error, "Failed to open file");
    }

    qint64 length  = file.size();
    uchar* pixData = file.map(0, length);
    if (!pixData)
    {
        return makeOFCondition(0, 2, OF_error, "Failed to map file");
    }

    DcmConverter cvt;
    OFCondition cond = cvt.readPixelData(dset, pixData, length, outputTS);

    if (cond.good())
    {
        DcmXfer transport(outputTS);
        if (transport.isEncapsulated())
        {
            cond = insertEncapsulatedPixelData(dset, pixData, length, outputTS);
        }
        else
        {
            /* Not encapsulated */
            cond = dset->putAndInsertUint8Array(DCM_PixelData, pixData, length);
        }
    }
    return cond;
}

OFCondition
readAndInsertGenericData
    ( const QString& fileName
    , DcmDataset* dataset
    , const QString& mimeType
    )
{
    OFCondition result = EC_Normal;

    if (result.good())
    {
        result = dataset->putAndInsertString(DCM_SOPClassUID,
            getGenericSopClass(mimeType).toUtf8());
    }
    if (result.good())
    {
        result = dataset->putAndInsertString(DCM_ConversionType, "WSD");
    }
    if (result.good())
    {
        result = dataset->putAndInsertString(DCM_DocumentTitle,
            QFileInfo(fileName).completeBaseName().toUtf8());
    }
    if (result.good())
    {
        result = dataset->putAndInsertString(DCM_MIMETypeOfEncapsulatedDocument, mimeType.toUtf8());
    }

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
    {
        return makeOFCondition(0, 1, OF_error, "Failed to open file");
    }

    DcmPolymorphOBOW *elem = new DcmPolymorphOBOW(DCM_EncapsulatedDocument);
    if (elem)
    {
        Uint32 numBytes = file.size();
        if (numBytes & 1) ++numBytes;
        Uint8 *bytes = NULL;
        result = elem->createUint8Array(numBytes, bytes);
        if (result.good())
        {
            // Blank pad byte
            //
            bytes[numBytes - 1] = 0;

            // Read the content
            //
            if (file.read((char*)bytes, file.size()) != file.size())
            {
                qDebug() << "read error in file " << fileName;
                result = EC_IllegalCall;
            }
        }
    }
    else
    {
        result = EC_MemoryExhausted;
    }

    // If successful, insert the element into the dataset
    //
    if (result.good())
    {
        result = dataset->insert(elem);
    }
    else
    {
        delete elem;
    }

    return result;
}
