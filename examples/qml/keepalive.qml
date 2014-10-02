/* ------------------------------------------------------------------------- *
 * Copyright (C) 2014 Jolla Ltd.
 * Contact: Martin Jones <martin.jones@jollamobile.com>
 * License: BSD
 * ------------------------------------------------------------------------- */

import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.keepalive 1.1

ApplicationWindow {
    property var startTime
    property var currentTime

    initialPage: Component {
        Page {
            KeepAlive {
                id: keepAlive
            }

            Timer {
                id: timer
                property int count
                interval: 1000
                repeat: true
                onTriggered: {
                    ++count
                    currentTime = new Date()
                }
            }

            Column {
                width: parent.width
                spacing: Theme.paddingLarge
                anchors.centerIn: parent
                TextSwitch {
                    text: "Keep Alive"
                    anchors.horizontalCenter: parent.horizontalCenter
                    onCheckedChanged: {
                        startTime = new Date()
                        currentTime = new Date()
                        timer.count = 0
                        timer.restart()
                        keepAlive.enabled = checked
                    }
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Duration: " + Math.round((currentTime.getTime() - startTime.getTime())/1000)
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Timer triggered: " + timer.count
                }
            }

            Component.onCompleted: {
                startTime = new Date()
                currentTime = new Date()
                timer.start()
            }
        }
    }
}
