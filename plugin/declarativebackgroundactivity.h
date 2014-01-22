/****************************************************************************************
**
** Copyright (C) 2014 Jolla Ltd.
** Contact: Martin Jones <martin.jones@jollamobile.com>
** All rights reserved.
**
** This file is part of nemo keepalive package.
**
** You may use this file under the terms of the GNU Lesser General
** Public License version 2.1 as published by the Free Software Foundation
** and appearing in the file license.lgpl included in the packaging
** of this file.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file license.lgpl included in the packaging
** of this file.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Lesser General Public License for more details.
**
****************************************************************************************/

#ifndef DECLARATIVEBACKGROUNDACTIVITY_H_
# define DECLARATIVEBACKGROUNDACTIVITY_H_

# include <QObject>
# include <QBasicTimer>
# include <qqml.h>
# include "backgroundactivity.h"

class DeclarativeKeepAlive : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    DeclarativeKeepAlive(QObject *parent=0);

    bool enabled() const;
    void setEnabled(bool enabled);

signals:
    void enabledChanged();

private:
    bool mEnabled;
    BackgroundActivity *mBackgroundActivity;
};

QML_DECLARE_TYPE(DeclarativeKeepAlive)


class DeclarativeBackgroundJob : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(Frequency frequency READ frequency WRITE setFrequency NOTIFY frequencyChanged)
    Q_PROPERTY(int minimumWait READ minimumWait WRITE setMinimumWait NOTIFY minimumWaitChanged)
    Q_PROPERTY(int maximumWait READ maximumWait WRITE setMaximumWait NOTIFY maximumWaitChanged)
    Q_ENUMS(Frequency)

public:
    DeclarativeBackgroundJob(QObject *parent=0);

    enum Frequency
    {
        Range             = BackgroundActivity::Range,
        ThirtySeconds     = BackgroundActivity::ThirtySeconds,
        TwoAndHalfMinutes = BackgroundActivity::TwoAndHalfMinutes,
        FiveMinutes       = BackgroundActivity::FiveMinutes,
        TenMinutes        = BackgroundActivity::TenMinutes,
        FifteenMinutes    = BackgroundActivity::FifteenMinutes,
        ThirtyMinutes     = BackgroundActivity::ThirtyMinutes,
        OneHour           = BackgroundActivity::OneHour,
        TwoHours          = BackgroundActivity::TwoHours,
        FourHours         = BackgroundActivity::FourHours,
        EightHours        = BackgroundActivity::EightHours,
        TenHours          = BackgroundActivity::TenHours,
        TwelveHours       = BackgroundActivity::TwelveHours,
        MaximumFrequency  = BackgroundActivity::MaximumFrequency
    };

    bool enabled() const;
    void setEnabled(bool enabled);

    bool running() const;

    Frequency frequency() const;
    void setFrequency(Frequency frequency);

    int minimumWait() const;
    void setMinimumWait(int minimum);

    int maximumWait() const;
    void setMaximumWait(int maximum);

    void classBegin();
    void componentComplete();

signals:
    void enabledChanged();
    void runningChanged();
    void frequencyChanged();
    void minimumWaitChanged();
    void maximumWaitChanged();

signals:
    void triggered();

public slots:
    void begin();
    void finished();

protected:
    bool event(QEvent *event);

private slots:
    void stateChanged();
    void update();

private:
    void scheduleUpdate();

    BackgroundActivity *mBackgroundActivity;
    QBasicTimer mTimer;
    Frequency mFrequency;
    BackgroundActivity::State mPreviousState;
    int mMinimum;
    int mMaximum;
    bool mEnabled;
    bool mComplete;
};

QML_DECLARE_TYPE(DeclarativeBackgroundJob)


#endif /* DECLARATIVEBACKGROUNDACTIVITY_H_ */
