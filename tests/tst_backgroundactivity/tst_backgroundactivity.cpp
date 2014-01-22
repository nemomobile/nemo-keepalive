/*
 * Copyright (C) 2014 Jolla Ltd. <martin.jones@jollamobile.com>
 *
 * You may use this file under the terms of the LGPLv2.1
 *
 */

#include <QObject>
#include <QtTest>

#include "backgroundactivity.h"


class tst_BackgroundActivity : public QObject
{
    Q_OBJECT

private slots:
    void frequency();
};

void tst_BackgroundActivity::frequency()
{
    QScopedPointer<BackgroundActivity> activity(new BackgroundActivity);
    QSignalSpy waitingSpy(activity.data(), SIGNAL(waiting()));
    QSignalSpy runningSpy(activity.data(), SIGNAL(running()));
    QSignalSpy stoppedSpy(activity.data(), SIGNAL(stopped()));
    QCOMPARE(activity->isStopped(), true);
    QCOMPARE(activity->isRunning(), false);
    QCOMPARE(activity->isWaiting(), false);

    activity->setWakeupFrequency(BackgroundActivity::ThirtySeconds);
    QCOMPARE(activity->wakeupFrequency(), BackgroundActivity::ThirtySeconds);

    activity->wait();
    QCOMPARE(runningSpy.count(), 0);
    QCOMPARE(activity->isStopped(), false);
    QCOMPARE(activity->isRunning(), false);
    QCOMPARE(activity->isWaiting(), true);

    // The actual time varies, but we should get an event within around
    // 30s +/- 10s
    QTest::qWait(20000);
    QTRY_COMPARE_WITH_TIMEOUT(runningSpy.count(), 1, 20000);

    activity->stop();
    QCOMPARE(activity->isStopped(), true);
    QCOMPARE(activity->isRunning(), false);
    QCOMPARE(activity->isWaiting(), false);
}


#include "tst_backgroundactivity.moc"
QTEST_MAIN(tst_BackgroundActivity)
