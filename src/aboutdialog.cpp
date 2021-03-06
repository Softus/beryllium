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

#include "aboutdialog.h"
#include "product.h"

#include <QApplication>
#include <QBoxLayout>
#include <QLabel>
#include <QProxyStyle>
#include <QPushButton>
#include <qxtglobal.h>
#include <gst/gst.h>
#include <QGlib/Value>
#include <QGst/Buffer>

#ifdef WITH_DICOM
#define HAVE_CONFIG_H
#include <dcmtk/dcmdata/dcuid.h>

#if defined(UNICODE) || defined (_UNICODE)
  #include <MediaInfo/MediaInfo.h>
#else
  // MediaInfo was built with utf-16 under linux, but we a going to compile with utf-8,
  // so we define it, include the header, then undefine it back.
  //
  #define UNICODE
  #include <MediaInfo/MediaInfo.h>
  #undef UNICODE
#endif
#endif

#define QT_GST_VERSION_STR "1.2"

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(tr("About %1").arg(tr("Beryllium")/*PRODUCT_FULL_NAME*/));

    auto layoutMain = new QHBoxLayout;
    layoutMain->setContentsMargins(16,16,16,16);
    auto icon = new QLabel();
    if (qApp->style()->inherits("QProxyStyle"))
    {
        // Since the app icon is already inverted, reset the label style to the default.
        //
        icon->setStyle(static_cast<QProxyStyle*>(qApp->style())->baseStyle());
    }
    icon->setPixmap(qApp->windowIcon().pixmap(100));
    layoutMain->addWidget(icon, 1, Qt::AlignTop);
    auto layoutText = new QVBoxLayout;
    layoutText->setContentsMargins(16,0,16,0);

    auto const& str = QString(tr(PRODUCT_FULL_NAME)).append(" ").append(PRODUCT_VERSION_STR);
    auto lblTitle = new QLabel(str);
    auto titleFont = lblTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() * 2);
    lblTitle->setFont(titleFont);
    layoutText->addWidget(lblTitle);
    layoutText->addSpacing(16);

    //
    // Third party libraries
    //

    layoutText->addWidget(new QLabel(tr("Based on:")));
#ifdef WITH_DICOM
    auto mediaInfoVer = QString::fromStdWString(
        MediaInfoLib::MediaInfo::Option_Static(__T("Info_Version")));
    auto lblMediaInfo = new QLabel(tr("<a href=\"https://mediaarea.net/ru/MediaInfo\">")
        .append(mediaInfoVer.replace("- v", "")).append("</a>"));
    lblMediaInfo->setOpenExternalLinks(true);
    layoutText->addWidget(lblMediaInfo);

    auto lblDcmtk = new QLabel(tr("<a href=\"http://dcmtk.org/\">DCMTK ")
        .append(OFFIS_DCMTK_VERSION_STRING).append("</a>"));
    lblDcmtk->setOpenExternalLinks(true);
    layoutText->addWidget(lblDcmtk);
#endif

    auto lblGstreamer = new QLabel(tr("<a href=\"https://gstreamer.freedesktop.org/\">")
        .append(gst_version_string()).append("</a>"));
    lblGstreamer->setOpenExternalLinks(true);
    layoutText->addWidget(lblGstreamer);

    auto lblQtGstreamer = new QLabel(
        tr("<a href=\"https://gstreamer.freedesktop.org/modules/qt-gstreamer.html\">QtGStreamer ")
            .append(QT_GST_VERSION_STR).append("</a>"));
    lblQtGstreamer->setOpenExternalLinks(true);
    layoutText->addWidget(lblQtGstreamer);

    auto lblQt = new QLabel(tr("<a href=\"https://www.qt.io/\">Qt ")
        .append(QT_VERSION_STR).append("</a>"));
    lblQt->setOpenExternalLinks(true);
    layoutText->addWidget(lblQt);

    auto lblQxt = new QLabel(tr("<a href=\"https://bitbucket.org/libqxt/libqxt/wiki/Home\">LibQxt ")
        .append(QXT_VERSION_STR).append("</a>"));
    lblQxt->setOpenExternalLinks(true);
    layoutText->addWidget(lblQxt);

    //
    // Media
    //

    auto lblIconsWin8 = new QLabel(
        tr("<a href=\"https://icons8.com/\">Icons8 icon set by VisualPharm</a>"));
    lblIconsWin8->setOpenExternalLinks(true);
    layoutText->addWidget(lblIconsWin8);
    layoutText->addSpacing(16);

    //
    // Copyright & warranty
    //

    auto lblCopyright = new QLabel(
        tr("<p>Copyright (C) 2013-2018 <a href=\"%1\">%2</a>. All rights reserved.</p>")
            .arg(PRODUCT_SITE_URL, tr("Softus Inc.")/*ORGANIZATION_FULL_NAME*/));
    lblCopyright->setOpenExternalLinks(true);
    layoutText->addWidget(lblCopyright);
    layoutText->addSpacing(16);

    auto lblWarranty = new QLabel(tr("The program is provided AS IS with NO WARRANTY OF ANY KIND,\n"
                                     "INCLUDING THE WARRANTY OF DESIGN,\n"
                                     "MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE."));
    layoutText->addWidget(lblWarranty);

    //
    // Footer
    //

    if (qApp->keyboardModifiers().testFlag(Qt::ShiftModifier))
    {
        auto const& buildInfo = "Built by " AUX_STR(USERNAME) " on " \
          AUX_STR(OS_DISTRO) " " AUX_STR(OS_REVISION) " at " __DATE__ " " __TIME__;
        layoutText->addWidget(new QLabel(buildInfo));
    }

    layoutText->addSpacing(16);
    auto btnClose = new QPushButton(tr("OK"));
    connect(btnClose, SIGNAL(clicked()), this, SLOT(accept()));
    layoutText->addWidget(btnClose, 1, Qt::AlignRight);
    layoutMain->addLayout(layoutText);
    setLayout(layoutMain);
}
