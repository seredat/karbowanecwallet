// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2025, The Karbo developers
//
// This file is part of Karbo.
//
// Karbo is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Karbo is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Karbo.  If not, see <http://www.gnu.org/licenses/>.

#include "Miner.h"

#include <chrono>
#include <future>
#include <iterator>
#include <numeric>
#include <sstream>
#include <thread>
#include <QDebug>
#include <QTime>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem.hpp>
#include <boost/limits.hpp>
#include <boost/utility/value_init.hpp>

#include <CryptoNoteConfig.h>

#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "crypto/random.h"
#include "Common/CommandLine.h"
#include "Common/Math.h"
#include "Common/StringTools.h"
#include "Serialization/SerializationTools.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/TransactionExtra.h"

#include "CurrencyAdapter.h"
#include "Wallet/WalletRpcServerCommandsDefinitions.h"

#include "NodeAdapter.h"

#include <QThread>
#include <QTimerEvent>

#undef ERROR

using namespace Logging;
using namespace CryptoNote;

namespace WalletGui
{

  Miner::Miner(QObject* _parent, Logging::ILogger &log) :
    QObject(_parent),
    m_logger(log, "Miner"),
    m_stop_mining(true),
    m_template(boost::value_initialized<Block>()),
    m_template_no(0),
    m_diffic(0),
    m_pausers_count(0),
    m_threads_total(0),
    m_starter_nonce(0),
    m_last_hr_merge_time(0),
    m_hashes(0),
    m_do_mining(false),
    m_current_hash_rate(0),
    m_hash_rate(0),
    m_update_block_template_interval(240),
    m_update_merge_hr_interval(2) {
  }
  //-----------------------------------------------------------------------------------------------------
  Miner::~Miner() {
    stop();
  }
  //-----------------------------------------------------------------------------------------------------
  bool Miner::set_block_template(const Block& bl, const difficulty_type& di) {
    std::lock_guard<decltype(m_template_lock)> lk(m_template_lock);

    m_template = bl;

    if (m_template.majorVersion == BLOCK_MAJOR_VERSION_2 || m_template.majorVersion == BLOCK_MAJOR_VERSION_3) {
      CryptoNote::TransactionExtraMergeMiningTag mm_tag;
      mm_tag.depth = 0;
      if (!CryptoNote::get_aux_block_header_hash(m_template, mm_tag.merkleRoot)) {
        return false;
      }

      m_template.parentBlock.baseTransaction.extra.clear();
      if (!CryptoNote::appendMergeMiningTagToExtra(m_template.parentBlock.baseTransaction.extra, mm_tag)) {
        return false;
      }
    }

    m_starter_nonce = Random::randomValue<uint32_t>();
    m_diffic = di;
    ++m_template_no;
    return true;
  }

  //-----------------------------------------------------------------------------------------------------
  void Miner::reset_nonce_sequence() {
    std::lock_guard<decltype(m_template_lock)> lk(m_template_lock);
    m_starter_nonce = Random::randomValue<uint32_t>();
    ++m_template_no;
  }
  //-----------------------------------------------------------------------------------------------------
  bool Miner::on_block_chain_update() {
    if (!is_mining()) {
      return true;
    }

    //pause();

    if (request_block_template()) {
      //resume();
      return true;
    }
    else {
      stop();
    }

    return false;
  }
  //-----------------------------------------------------------------------------------------------------
  bool Miner::request_block_template() {
    QDateTime date = QDateTime::currentDateTime();
    QString formattedTime = date.toString("dd.MM.yyyy hh:mm:ss");
    qDebug() << formattedTime << "Requesting block template";

    Block bl = boost::value_initialized<Block>();
    CryptoNote::difficulty_type di = 0;
    uint32_t height;
    CryptoNote::BinaryArray extra_nonce;

    if (!NodeAdapter::instance().getBlockTemplate(bl, m_account, extra_nonce, di, height)) {
      m_logger(Logging::ERROR) << "Failed to get_block_template(), stopping mining";
      const QString errorMessage = tr("Failed to get block template");
      Q_EMIT minerMessageSignal(errorMessage);
      Q_EMIT miningErrorSignal(errorMessage);
      return false;
    }

    if (!set_block_template(bl, di)) {
      const QString errorMessage = tr("Failed to set block template");
      m_logger(Logging::ERROR) << errorMessage.toStdString();
      Q_EMIT minerMessageSignal(errorMessage);
      Q_EMIT miningErrorSignal(errorMessage);
      return false;
    }

    Q_EMIT minerTemplateUpdatedSignal(height, static_cast<quint64>(di));

    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool Miner::on_idle()
  {
    m_update_block_template_interval.call([&](){
      if (is_mining())
        request_block_template();
      return true;
    });

    m_update_merge_hr_interval.call([&](){
      merge_hr();
      return true;
    });

    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  uint64_t millisecondsSinceEpoch() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
  }

  //-----------------------------------------------------------------------------------------------------
  void Miner::merge_hr()
  {
    const uint64_t now = millisecondsSinceEpoch();
    const uint64_t hashes = m_hashes.exchange(0);
    if(m_last_hr_merge_time && is_mining()) {
      m_current_hash_rate = hashes * 1000 / (now - m_last_hr_merge_time + 1);
      std::lock_guard<std::mutex> lk(m_last_hash_rates_lock);
      m_last_hash_rates.push_back(m_current_hash_rate);
      if(m_last_hash_rates.size() > 19)
        m_last_hash_rates.pop_front();

      uint64_t total_hr = std::accumulate(m_last_hash_rates.begin(), m_last_hash_rates.end(), static_cast<uint64_t>(0));
      m_hash_rate = static_cast<float>(total_hr) / static_cast<float>(m_last_hash_rates.size());
      //qDebug() << "Hashrate: " << m_hash_rate << " H/s";
    }
    
    m_last_hr_merge_time = now;
  }

  //-----------------------------------------------------------------------------------------------------
  bool Miner::is_mining()
  {
    return !m_stop_mining;
  }
  //-----------------------------------------------------------------------------------------------------
  bool Miner::start(size_t threads_count)
  {   
    if (!m_stop_mining) {
      m_logger(Logging::DEBUGGING) << "Starting miner but it's already started";
      Q_EMIT miningErrorSignal(tr("Miner is already running"));
      return false;
    }

    std::lock_guard<std::mutex> lk(m_threads_lock);

    if(!m_threads.empty()) {
      m_logger(Logging::DEBUGGING) << "Unable to start miner because there are active mining threads";
      Q_EMIT miningErrorSignal(tr("Unable to start miner because there are active mining threads"));
      return false;
    }

    if (!WalletAdapter::instance().getAccountKeys(m_account)) {
      m_logger(Logging::ERROR) << "Unable to start miner because couldn't get account keys";
      Q_EMIT miningErrorSignal(tr("Unable to start miner because account keys are unavailable"));
      return false;
    }

    m_threads_total = static_cast<uint32_t>(threads_count);
    reset_nonce_sequence();
    m_hashes = 0;
    m_current_hash_rate = 0;
    m_hash_rate = 0;
    m_last_hr_merge_time = millisecondsSinceEpoch();
    {
      std::lock_guard<std::mutex> hashRatesLock(m_last_hash_rates_lock);
      m_last_hash_rates.clear();
    }

    // always request block template on start
    if (!request_block_template()) {
      m_logger(Logging::ERROR) << "Unable to start miner because block template request was unsuccessful";
      return false;
    }

    m_stop_mining = false;
    m_pausers_count = 0; // in case mining wasn't resumed after pause

    for (uint32_t i = 0; i != threads_count; i++) {
      std::shared_ptr<std::atomic<bool>> stopSignal(new std::atomic<bool>(false));
      m_threads.emplace_back(i, stopSignal, std::thread(std::bind(&Miner::worker_thread, this, i, stopSignal)));
    }

    QDateTime date = QDateTime::currentDateTime();
    QString formattedTime = date.toString("dd.MM.yyyy hh:mm:ss");

    m_logger(Logging::INFO) << "Mining has started with " << threads_count << " thread(s), at difficulty " << m_diffic << " good luck!";
    Q_EMIT minerMessageSignal(
        tr("%1 Mining has started with %n thread(s) at difficulty %2, good luck!", nullptr, threads_count)
            .arg(formattedTime)
            .arg(m_diffic)
        );
    Q_EMIT minerStartedSignal(static_cast<quint32>(threads_count), static_cast<quint64>(m_diffic));
    return true;
  }

  //-----------------------------------------------------------------------------------------------------
  bool Miner::set_thread_count(size_t threads_count) {
    if (threads_count == 0) {
      threads_count = 1;
    }

    if (!is_mining()) {
      m_threads_total = static_cast<uint32_t>(threads_count);
      return true;
    }

    std::list<MiningThread> threadsToJoin;
    uint32_t oldThreadsCount = 0;

    {
      std::lock_guard<std::mutex> lk(m_threads_lock);
      oldThreadsCount = static_cast<uint32_t>(m_threads.size());
      if (threads_count == oldThreadsCount) {
        return true;
      }

      m_threads_total = static_cast<uint32_t>(threads_count);

      if (threads_count > oldThreadsCount) {
        for (uint32_t i = oldThreadsCount; i != threads_count; ++i) {
          std::shared_ptr<std::atomic<bool>> stopSignal(new std::atomic<bool>(false));
          m_threads.emplace_back(i, stopSignal, std::thread(std::bind(&Miner::worker_thread, this, i, stopSignal)));
        }
      } else {
        while (m_threads.size() > threads_count) {
          auto threadIt = std::prev(m_threads.end());
          threadIt->stop_signal->store(true);
          threadsToJoin.splice(threadsToJoin.end(), m_threads, threadIt);
        }
      }
    }

    reset_nonce_sequence();

    for (auto& miningThread : threadsToJoin) {
      if (miningThread.thread.joinable()) {
        miningThread.thread.join();
      }
    }

    m_logger(Logging::INFO) << "Mining thread count changed from " << oldThreadsCount << " to " << threads_count;
    Q_EMIT minerMessageSignal(tr("Mining thread count changed to %n thread(s)", nullptr, static_cast<int>(threads_count)));
    Q_EMIT minerThreadsChangedSignal(static_cast<quint32>(threads_count));
    return true;
  }
  
  //-----------------------------------------------------------------------------------------------------
  double Miner::get_speed()
  {
    if(is_mining())
      return m_hash_rate.load();
    else
      return 0;
  }
  
  //-----------------------------------------------------------------------------------------------------
  void Miner::send_stop_signal() 
  {
    m_stop_mining = true;
  }

  //-----------------------------------------------------------------------------------------------------
  bool Miner::stop()
  {
    const bool wasMining = !m_stop_mining.exchange(true);
    int threadsCount = 0;
    std::list<MiningThread> threadsToJoin;

    {
      std::lock_guard<std::mutex> lk(m_threads_lock);
      threadsCount = static_cast<int>(m_threads.size());
      threadsToJoin.splice(threadsToJoin.end(), m_threads);
    }

    const std::thread::id currentThreadId = std::this_thread::get_id();
    for (auto& miningThread : threadsToJoin) {
      if (!miningThread.thread.joinable()) {
        continue;
      }
      if (miningThread.thread.get_id() == currentThreadId) {
        miningThread.thread.detach();
      } else {
        miningThread.thread.join();
      }
    }

    m_hashes = 0;
    m_current_hash_rate = 0;
    m_hash_rate = 0;
    m_last_hr_merge_time = 0;
    {
      std::lock_guard<std::mutex> hashRatesLock(m_last_hash_rates_lock);
      m_last_hash_rates.clear();
    }

    if (!wasMining && threadsCount == 0) {
      return true;
    }

    m_logger(Logging::INFO) << "Mining stopped, " << threadsCount << " threads finished" ;
    Q_EMIT minerMessageSignal(tr("Mining stopped, %n thread(s) finished", nullptr, threadsCount));
    Q_EMIT minerStoppedSignal(static_cast<quint32>(threadsCount));

    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  void Miner::on_synchronized()
  {
    if(m_do_mining) {
      start(m_threads_total);
    }
  }
  //-----------------------------------------------------------------------------------------------------
  void Miner::pause()
  {
    std::lock_guard<std::mutex> lk(m_miners_count_lock);
    ++m_pausers_count;
    if(m_pausers_count == 1 && is_mining())
      qDebug() << "MINING PAUSED";
  }
  //-----------------------------------------------------------------------------------------------------
  void Miner::resume()
  {
    std::lock_guard<std::mutex> lk(m_miners_count_lock);
    --m_pausers_count;
    if(m_pausers_count < 0)
    {
      m_pausers_count = 0;
      m_logger(Logging::DEBUGGING) << "Unexpected Miner::resume() called";
      //Q_EMIT minerMessageSignal(QString("Unexpected Miner::resume() called"));
    }
    if(!m_pausers_count && is_mining())
      qDebug() << "MINING RESUMED";
      //Q_EMIT minerMessageSignal(QString("MINING RESUMED"));
  }
  //-----------------------------------------------------------------------------------------------------
  bool Miner::worker_thread(uint32_t th_local_index, std::shared_ptr<std::atomic<bool>> _thread_stop)
  {
    m_logger(Logging::DEBUGGING) << "Miner thread was started ["<< th_local_index << "]";
    uint32_t nonce = m_starter_nonce.load() + th_local_index;
    difficulty_type local_diff = 0;
    uint32_t local_template_ver = 0;
    Crypto::cn_context context;
    Block b;

    while(!m_stop_mining.load() && !_thread_stop->load())
    {
      if(m_pausers_count) //anti split workaround
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      const uint32_t observed_template_ver = m_template_no.load();
      if(local_template_ver != observed_template_ver) {
        std::unique_lock<std::mutex> lk(m_template_lock);
        b = m_template;
        local_diff = m_diffic;
        local_template_ver = m_template_no.load();
        nonce = m_starter_nonce.load() + th_local_index;
      }

      if(!local_template_ver) //no any set_block_template call
      {
        m_logger(Logging::DEBUGGING) << "Block template not set yet";
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        continue;
      }

      b.nonce = nonce;

      // step 1: sing the block
      if (b.majorVersion >= CryptoNote::BLOCK_MAJOR_VERSION_5) {
        BinaryArray ba;
        if (!get_block_hashing_blob(b, ba)) {
          m_logger(Logging::ERROR) << "get_block_hashing_blob for signature failed.";
          const QString errorMessage = QStringLiteral("get_block_hashing_blob for signature failed");
          Q_EMIT minerMessageSignal(errorMessage);
          Q_EMIT miningErrorSignal(errorMessage);
          m_stop_mining = true;
        }

        Crypto::Hash h = Crypto::cn_fast_hash(ba.data(), ba.size());
        try {
          Crypto::PublicKey txPublicKey = getTransactionPublicKeyFromExtra(b.baseTransaction.extra);
          Crypto::KeyDerivation derivation;
          if (!Crypto::generate_key_derivation(txPublicKey, m_account.viewSecretKey, derivation)) {
            m_logger(Logging::ERROR) << "Failed to generate_key_derivation for block signature";
            const QString errorMessage = QStringLiteral("Failed to generate_key_derivation for block signature");
            Q_EMIT minerMessageSignal(errorMessage);
            Q_EMIT miningErrorSignal(errorMessage);
            m_stop_mining = true;
          }
          Crypto::SecretKey ephSecKey;
          Crypto::derive_secret_key(derivation, 0, m_account.spendSecretKey, ephSecKey);
          Crypto::PublicKey ephPubKey = boost::get<KeyOutput>(b.baseTransaction.outputs[0].target).key;

          Crypto::generate_signature(h, ephPubKey, ephSecKey, b.signature);
        }
        catch (std::exception& e) {
          m_logger(Logging::ERROR) << "Signing block failed: " << e.what();
          const QString errorMessage = QString(tr("Signing block failed")) + QString(e.what());
          Q_EMIT minerMessageSignal(errorMessage);
          Q_EMIT miningErrorSignal(errorMessage);
          m_stop_mining = true;
        }
      }

      // step 2: get long hash
      Crypto::Hash pow;

      if (!m_stop_mining.load()) {
        if (!NodeAdapter::instance().getBlockLongHash(context, b, pow)) {
          m_logger(Logging::ERROR) << "getBlockLongHash failed.";
          const QString errorMessage = tr("getBlockLongHash failed");
          Q_EMIT minerMessageSignal(errorMessage);
          Q_EMIT miningErrorSignal(errorMessage);
          m_stop_mining = true;
        }
      }

      if (!m_stop_mining.load() && check_hash(pow, local_diff))
      {
        // we lucky!

        //pause();

        Crypto::Hash id;
        if (!get_block_hash(b, id)) {
          m_logger(Logging::ERROR) << "Failed to get block hash.";
          const QString errorMessage = QStringLiteral("Failed to get block hash");
          Q_EMIT minerMessageSignal(errorMessage);
          Q_EMIT miningErrorSignal(errorMessage);
          m_stop_mining = true;
        }
        uint32_t bh = boost::get<BaseInput>(b.baseTransaction.inputs[0]).blockIndex;

        QDateTime date = QDateTime::currentDateTime();
        QString formattedTime = date.toString("dd.MM.yyyy hh:mm:ss");

        const QString blockHash = QString::fromStdString(Common::podToHex(id));
        const QString powHash = QString::fromStdString(Common::podToHex(pow));
        m_logger(Logging::INFO) << "Found block " << Common::podToHex(id) << " at height " << bh << " for difficulty: " << local_diff << ", POW " << Common::podToHex(pow);
        Q_EMIT minerMessageSignal(QString(tr("%1 Found block %2 at height %3 for difficulty %4, POW %5")).arg(formattedTime).arg(blockHash).arg(bh).arg(local_diff).arg(powHash));
        Q_EMIT blockFoundSignal(blockHash, bh, static_cast<quint64>(local_diff), powHash);

        if(!NodeAdapter::instance().handleBlockFound(b)) {
          m_logger(Logging::ERROR) << "Failed to submit block to the main chain";
          const QString errorMessage = tr("Failed to submit block to the main chain");
          Q_EMIT minerMessageSignal(errorMessage);
          Q_EMIT miningErrorSignal(errorMessage);
        } else {
          // yay!
        }
      }

      nonce += m_threads_total.load();
      ++m_hashes;
    }
    m_logger(Logging::DEBUGGING) << "Miner thread stopped ["<< th_local_index << "]";
    return true;
  }
}
