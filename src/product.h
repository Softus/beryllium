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

#ifndef PRODUCT_H
#define PRODUCT_H

#define ORGANIZATION_FULL_NAME  "Softus Inc."
#define ORGANIZATION_DOMAIN     "softus.org"

#define PRODUCT_FULL_NAME       "Beryllium"
#define PRODUCT_SHORT_NAME      "beryllium" // lowercase, no spaces

#define PRODUCT_VERSION         0x010400
#define PRODUCT_VERSION_STR     "1.4.0"

#define PRODUCT_SITE_URL        "http://" ORGANIZATION_DOMAIN "/projects/" PRODUCT_SHORT_NAME "/"
#define PRODUCT_NAMESPACE       "org.softus." PRODUCT_SHORT_NAME

#define SITE_UID_ROOT           "1.2.643.2.66"

#define AUX_STR_EXP(__A)        #__A
#define AUX_STR(__A)            AUX_STR_EXP(__A)

#ifdef Q_OS_WIN
#define DATA_FOLDER qApp->applicationDirPath()
#else
#define DATA_FOLDER qApp->applicationDirPath() + "/../share/" PRODUCT_SHORT_NAME
#endif
#define TRANSLATIONS_FOLDER DATA_FOLDER + "/translations/"
#define SOUND_FOLDER        DATA_FOLDER + "/sound/"

#endif // PRODUCT_H
