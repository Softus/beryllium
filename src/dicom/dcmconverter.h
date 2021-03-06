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

#ifndef DCMCONVERTER_H
#define DCMCONVERTER_H

#include <QString>

#define HAVE_CONFIG_H
#include <dcmtk/config/osconfig.h> /* make sure OS specific configuration is included first */
#include <dcmtk/ofstd/ofcond.h>    /* for class OFCondition */
#include <dcmtk/dcmdata/dcxfer.h>

class DcmDataset;

OFCondition
readAndInsertPixelData
    ( const QString& fileName
    , DcmDataset* dset
    , E_TransferSyntax& outputTS
    );

OFCondition
readAndInsertGenericData
    ( const QString& fileName
    , DcmDataset* dset
    , const QString& mimeType
    );

#endif // DCMCONVERTER_H
