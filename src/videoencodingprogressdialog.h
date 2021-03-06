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

#ifndef VIDEOENCODINGPROGRESSDIALOG_H
#define VIDEOENCODINGPROGRESSDIALOG_H

#include <QProgressDialog>

#include <QGst/Message>
#include <QGst/Pipeline>

class VideoEncodingProgressDialog : public QProgressDialog
{
    Q_OBJECT
public:
    VideoEncodingProgressDialog
        ( const QGst::PipelinePtr& pipeline
        , qint64 duration
        , QWidget* parent = nullptr
        );

signals:

public slots:
    void onPositionChanged();

private:
    void onBusMessage(const QGst::MessagePtr& message);
    void onStateChange(const QGst::StateChangedMessagePtr& message);

private:
    const QGst::PipelinePtr& pipeline;
    qint64                   duration;
    QTimer*                  positionTimer;
};

#endif // VIDEOENCODINGPROGRESSDIALOG_H
