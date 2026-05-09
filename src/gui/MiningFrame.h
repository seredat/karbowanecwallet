// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2015-2016 XDN developers
// Copyright (c) 2016-2022 The Karbowanec developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QDateTime>
#include <QFrame>
#include <QList>
#include <QStringList>
#include <QTimer>
#include "qcustomplot.h"
#include "Miner.h"
#include <Logging/LoggerMessage.h>

class QAbstractButton;
class QEvent;

namespace Ui {
class MiningFrame;
}

class Miner;

namespace WalletGui {

class LogFileWatcher;

class MiningFrame : public QFrame {
  Q_OBJECT

public:
  MiningFrame(QWidget* _parent);
  ~MiningFrame();

  void addPoint(double x, double y);
  void plot();

  bool isSoloRunning() const;
  void stopMiningForShutdown();

protected:
  void changeEvent(QEvent* _event) override;
  void timerEvent(QTimerEvent* _event) Q_DECL_OVERRIDE;

private:
  QScopedPointer<Ui::MiningFrame> m_ui;
  int m_soloHashRateTimerId;
  int m_minerRoutineTimerId;
  QVector<double> m_hX, m_hY, m_averageHashRateY;
  QVector<double> m_difficultyX, m_difficultyY;
  QList<QCPItemLine*> m_hashRateEventMarkers;
  QList<bool> m_hashRateEventMarkerHighlights;
  std::unique_ptr<Miner> m_miner;
  LogFileWatcher* m_coreLogWatcher = nullptr;
  QString m_miner_log;
  QStringList m_event_log;
  QTimer m_threadResizeTimer;
  int m_pendingMiningThreads = 0;

  void initCpuCoreList();
  void startSolo();
  void stopSolo();

  bool m_wallet_closed = false;
  bool m_solo_mining = false;
  bool m_sychronized = false;
  bool m_mining_was_stopped = false;
  QDateTime m_sessionStartedAt;
  double m_sessionTotalHashes = 0;
  double m_roundHashes = 0;
  double m_sessionPeakHashRate = 0;
  double m_lastAnnouncedPeakHashRate = 0;
  double m_lastHashRate = 0;
  double m_currentDifficulty = 0;
  quint64 m_sessionBlocksFound = 0;

  void applyChartPalette();
  void initDifficultyChart();
  void initHashRateChartItems();
  void updateDifficulty(quint64 _difficulty);
  void plotDifficulty();
  void addHashRateEventMarker(bool _highlight);
  void appendMiningEvent(const QString& _kind, const QString& _message);
  void showBlockFound(quint64 _height);
  void appendRawLogLine(const QString& _line);
  void resetSessionStats();
  void updateSessionStats();
  void updateCpuIntensity();
  void applyCpuPreset(double _fraction);
  void setMiningStatusBadge(const QString& _text, const QString& _backgroundColor, const QString& _textColor);
  void scheduleMiningThreadsChange(int _threads);
  void applyPendingMiningThreads();
  void walletOpened();
  void walletClosed();
  quint32 getHashRate() const;
  double m_maxHr = 10;
  QCPItemStraightLine* m_peakHashRateLine = nullptr;

  Q_SLOT void startStopSoloClicked(QAbstractButton* _button);
  Q_SLOT void enableSolo();
  Q_SLOT void setMiningThreads();
  Q_SLOT void onBlockHeightUpdated(quint64 _height);
  Q_SLOT void onSynchronizationCompleted();
  Q_SLOT void updateBalance(quint64 _balance);
  Q_SLOT void updatePendingBalance(quint64 _balance);
  Q_SLOT void updateMinerLog(const QString& _message);
  Q_SLOT void updateCoreLog(const QString& _message);
  Q_SLOT void onMinerStarted(quint32 _threads, quint64 _difficulty);
  Q_SLOT void onMinerStopped(quint32 _threads);
  Q_SLOT void onMinerThreadsChanged(quint32 _threads);
  Q_SLOT void onMinerTemplateUpdated(quint64 _height, quint64 _difficulty);
  Q_SLOT void onBlockFound(const QString& _hash, quint64 _height, quint64 _difficulty, const QString& _pow);
  Q_SLOT void onMinerError(const QString& _message);
  Q_SLOT void coreDealTurned(int _cores);
  Q_SLOT void poolChanged();
};

}
