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

#include "declarativebackgroundactivity.h"
#include <QDebug>

DeclarativeKeepAlive::DeclarativeKeepAlive(QObject *parent)
    : QObject(parent), mEnabled(false), mBackgroundActivity(0)
{
}

bool DeclarativeKeepAlive::enabled() const
{
    return mEnabled;
}

void DeclarativeKeepAlive::setEnabled(bool enabled)
{
    if (enabled != mEnabled) {
        if (!mBackgroundActivity)
            mBackgroundActivity = new BackgroundActivity(this);
        mEnabled = enabled;
        if (mEnabled)
            mBackgroundActivity->run();
        else
            mBackgroundActivity->stop();
        emit enabledChanged();
    }
}

//==============================


DeclarativeBackgroundJob::DeclarativeBackgroundJob(QObject *parent)
    : QObject(parent), mBackgroundActivity(0), mFrequency(OneHour), mPreviousState(BackgroundActivity::Stopped)
    , mMinimum(0), mMaximum(0), mEnabled(false), mComplete(false)
{
    mBackgroundActivity = new BackgroundActivity(this);
    connect(mBackgroundActivity, SIGNAL(stateChanged()), this, SLOT(stateChanged()));
}

bool DeclarativeBackgroundJob::enabled() const
{
    return mEnabled;
}

void DeclarativeBackgroundJob::setEnabled(bool enabled)
{
    if (enabled != mEnabled) {
        mEnabled = enabled;
        emit enabledChanged();
        scheduleUpdate();
    }
}

bool DeclarativeBackgroundJob::running() const
{
    return mBackgroundActivity->isRunning();
}

DeclarativeBackgroundJob::Frequency DeclarativeBackgroundJob::frequency() const
{
    return mFrequency;
}

void DeclarativeBackgroundJob::setFrequency(Frequency frequency)
{
    if (frequency != mFrequency) {
        mFrequency = frequency;
        emit frequencyChanged();
        scheduleUpdate();
    }
}

int DeclarativeBackgroundJob::minimumWait() const
{
    return mMinimum;
}

void DeclarativeBackgroundJob::setMinimumWait(int minimum)
{
    if (minimum != mMinimum) {
        mMinimum = minimum;
        emit minimumWaitChanged();
        scheduleUpdate();
    }
}

int DeclarativeBackgroundJob::maximumWait() const
{
    return mMaximum;
}

void DeclarativeBackgroundJob::setMaximumWait(int maximum)
{
    if (maximum != mMaximum) {
        mMaximum = maximum;
        emit maximumWaitChanged();
        scheduleUpdate();
    }
}

void DeclarativeBackgroundJob::begin()
{
    if (!mComplete || !mEnabled)
        return;

    mTimer.stop();
    mBackgroundActivity->setState(BackgroundActivity::Running);
}

void DeclarativeBackgroundJob::finished()
{
    if (!mComplete || !mEnabled)
        return;

    mTimer.stop();
    mBackgroundActivity->setState(BackgroundActivity::Waiting);
}

bool DeclarativeBackgroundJob::event(QEvent *event)
{
    if (event->type() == QEvent::Timer) {
        QTimerEvent *te = static_cast<QTimerEvent*>(event);
        if (te->timerId() == mTimer.timerId()) {
            mTimer.stop();
            update();
        }
    }

    return QObject::event(event);
}

void DeclarativeBackgroundJob::update()
{
    if (!mComplete)
        return;

    if (!mEnabled) {
        mBackgroundActivity->stop();
    } else {
        if (mFrequency == Range)
            mBackgroundActivity->setWakeupRange(mMinimum, mMaximum);
        else
            mBackgroundActivity->setWakeupFrequency(static_cast<BackgroundActivity::Frequency>(mFrequency));
        mBackgroundActivity->run();
    }
}

void DeclarativeBackgroundJob::scheduleUpdate()
{
    mTimer.start(0, this);
}

void DeclarativeBackgroundJob::classBegin()
{
}

void DeclarativeBackgroundJob::stateChanged()
{
    if (mBackgroundActivity->isRunning()) {
        emit triggered();
        emit runningChanged();
    }

    if (mPreviousState == BackgroundActivity::Running)
        emit runningChanged();

    mPreviousState = mBackgroundActivity->state();
}

void DeclarativeBackgroundJob::componentComplete()
{
    mComplete = true;
    update();
}
