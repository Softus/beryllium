/*
 * Copyright (C) 2013-2015 Irkutsk Diagnostic Center.
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

#define ORGANIZATION_FULL_NAME  "Irkutsk Diagnostic Center"
#define ORGANIZATION_DOMAIN     "dc.baikal.ru"

#define PRODUCT_FULL_NAME       "Beryllium"
#define PRODUCT_SHORT_NAME      "beryllium" // lowercase, no spaces

#define PRODUCT_VERSION         0x010304
#define PRODUCT_VERSION_STR     "1.3.4"

#define PRODUCT_SITE_URL        "http://" ORGANIZATION_DOMAIN "/projects/" PRODUCT_SHORT_NAME "/"
#define PRODUCT_NAMESPACE       "ru.baikal.dc." PRODUCT_SHORT_NAME

#define SITE_UID_ROOT           "1.2.643.2.66"

#define AUX_STR_EXP(__A)        #__A
#define AUX_STR(__A)            AUX_STR_EXP(__A)

#endif // PRODUCT_H
