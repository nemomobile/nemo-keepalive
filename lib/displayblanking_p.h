/****************************************************************************************
**
** Copyright (C) 2014 Jolla Ltd.
** Contact: Simo Piiroinen <simo.piiroinen@jollamobile.com>
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

#ifndef DISPLAYBLANKING_P_H_
# define DISPLAYBLANKING_P_H_

# include "displayblanking.h"
# include "mceiface.h"

class QTimer;

class DisplayBlankingPrivate: public QObject
{
  Q_OBJECT

private:
  // Block the default copy-constructor
  DisplayBlankingPrivate(const DisplayBlankingPrivate &that);

public:
  explicit DisplayBlankingPrivate(DisplayBlanking *parent);

  DisplayBlanking::Status displayStatus() const;

signals:
  void displayStatusChanged();

private:
  QTimer *keepaliveTimer(void);
  void startKeepalive(void);
  void stopKeepalive(void);

private slots:
  void renewKeepalive(void);
  void updateDisplayStatus(const QString &status);
  void getDisplayStatusComplete(QDBusPendingCallWatcher *call);

private:
  bool    m_preventBlanking;
  int     m_renew_period;
  QTimer *m_renew_timer;
  DisplayBlanking::Status m_displayStatus;

  ComNokiaMceRequestInterface *m_mce_req_iface;
  ComNokiaMceSignalInterface *m_mce_signal_iface;

  friend class DisplayBlanking;
};

#endif // DISPLAYBLANKING_P_H_
