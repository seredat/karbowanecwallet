// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2015-2016 XDN developers
// Copyright (c) 2016-2018 The Karbowanec developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <CryptoNote.h>
#include "RestoreFromMnemonicSeedDialog.h"
#include "Mnemonics/electrum-words.h"
#include "ui_restorefrommnemonicseeddialog.h"

namespace WalletGui {

RestoreFromMnemonicSeedDialog::RestoreFromMnemonicSeedDialog(QWidget* _parent) : QDialog(_parent), m_ui(new Ui::RestoreFromMnemonicSeedDialog) {
  m_ui->setupUi(this);
}

RestoreFromMnemonicSeedDialog::~RestoreFromMnemonicSeedDialog() {
}

QString RestoreFromMnemonicSeedDialog::getSeedString() const {
  return m_ui->m_seedEdit->toPlainText().trimmed();
}

QString RestoreFromMnemonicSeedDialog::getFilePath() const {
  return m_ui->m_pathEdit->text().trimmed();
}

quint32 RestoreFromMnemonicSeedDialog::getSyncHeight() const {
  return m_ui->m_syncHeight->value();
}

void RestoreFromMnemonicSeedDialog::selectPathClicked() {
  QString filePath = QFileDialog::getSaveFileName(this, tr("Wallet file"),
#ifdef Q_OS_WIN
    //QApplication::applicationDirPath(),
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
#else
    QDir::homePath(),
#endif
    tr("Wallets (*.wallet)")
    );

  if (!filePath.isEmpty() && !filePath.endsWith(".wallet")) {
    filePath.append(".wallet");
  }

  m_ui->m_pathEdit->setText(filePath);
}

void RestoreFromMnemonicSeedDialog::onTextChanged() {
  wordCount = m_ui->m_seedEdit->toPlainText().split(QRegExp("(\\s|\\n|\\r)+")
                                                  , QString::SkipEmptyParts).count();
  if(wordCount != 25) {
    m_ui->m_okButton->setEnabled(false);
    m_ui->m_errorLabel->setText(QString::number(wordCount));
    m_ui->m_errorLabel->setStyleSheet("QLabel { color : red; }");
  }
  if(wordCount == 25) {
    m_ui->m_okButton->setEnabled(true);
    m_ui->m_errorLabel->setText("OK");
    m_ui->m_errorLabel->setStyleSheet("QLabel { color : green; }");
  }
}

void RestoreFromMnemonicSeedDialog::onAccept() {
  CryptoNote::AccountKeys keys;
  std::string seed_language = "English";
  if (!Crypto::ElectrumWords::words_to_bytes(getSeedString().toStdString(), keys.spendSecretKey, seed_language)) {
    QMessageBox::critical(nullptr, tr("Mnemonic seed is not correct"), tr("There must be an error in mnemonic seed. Make sure you entered it correctly."), QMessageBox::Ok);
  } else if (getFilePath().isEmpty()) {
    QMessageBox::critical(nullptr, tr("File path is empty"), tr("Please enter the path where to save the wallet file and its name."), QMessageBox::Ok);
  } else {
    accept();
  }
}

}
