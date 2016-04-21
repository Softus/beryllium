/*
 * Copyright (C) 2013-2016 Irkutsk Diagnostic Center.
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

#ifndef TYPEDETECT_H
#define TYPEDETECT_H

#include <QString>

QString typeDetect(const QString& filePath);

QString getFileExtAttribute(const QString& filePath, const QString &name);
bool    setFileExtAttribute(const QString& filePath, const QString &name, const QString &value);

#endif // TYPEDETECT_H
