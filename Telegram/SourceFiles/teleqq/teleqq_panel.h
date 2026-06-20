/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "teleqq/teleqq_napcat_client.h"
#include "teleqq/teleqq_store.h"
#include "base/basic_types.h"

#include <QtWidgets/QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTextEdit;

namespace TeleQQ {

class Panel final : public QWidget {
public:
	Panel(
		not_null<Store*> store,
		not_null<NapcatClient*> client,
		QWidget *parent = nullptr);

	void refreshChats();
	void refreshMessages();
	void setOptions(const NapcatClient::Options &options);
	void setStatusText(const QString &status);
	void connectCurrent();

private:
	void setupUi();
	void loadSavedOptions();
	void saveOptions(const NapcatClient::Options &options) const;
	[[nodiscard]] NapcatClient::Options currentOptions() const;
	void selectChat(const QString &chatId);
	void sendCurrentText();
	void upsertChatRow(const Chat &chat);
	[[nodiscard]] QString currentChatId() const;
	[[nodiscard]] QString renderChatRow(const Chat &chat) const;
	[[nodiscard]] QString renderMessageLine(const Message &message) const;

	not_null<Store*> _store;
	not_null<NapcatClient*> _client;
	QLineEdit *_endpoint = nullptr;
	QLineEdit *_token = nullptr;
	QCheckBox *_reconnect = nullptr;
	QLabel *_status = nullptr;
	QPushButton *_connect = nullptr;
	QPushButton *_disconnect = nullptr;
	QListWidget *_chats = nullptr;
	QTextEdit *_messages = nullptr;
	QLineEdit *_composer = nullptr;
	QPushButton *_send = nullptr;
	QString _selectedChatId;
};

} // namespace TeleQQ
