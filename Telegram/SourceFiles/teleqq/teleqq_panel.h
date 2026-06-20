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

#include <QtCore/QRect>
#include <QtWidgets/QWidget>

#include <vector>

class QLineEdit;
class QMouseEvent;
class QPaintEvent;
class QPushButton;
class QResizeEvent;

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

protected:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;

private:
	void setupUi();
	void loadSavedOptions();
	void saveOptions(const NapcatClient::Options &options) const;
	[[nodiscard]] NapcatClient::Options currentOptions() const;
	void selectChat(const QString &chatId);
	void sendCurrentText();
	void updateLayout();
	[[nodiscard]] QString selectedChatTitle() const;
	[[nodiscard]] QRect chatRowRect(int index) const;
	[[nodiscard]] int chatIndexAt(const QPoint &position) const;

	not_null<Store*> _store;
	not_null<NapcatClient*> _client;
	QLineEdit *_composer = nullptr;
	QPushButton *_send = nullptr;
	QString _endpoint;
	QString _token;
	bool _reconnect = true;
	QString _status = u"connected"_q;
	QString _selectedChatId;
	std::vector<Chat> _chats;
	QRect _dialogsRect;
	QRect _historyRect;
	QRect _topBarRect;
	QRect _composerRect;
};

} // namespace TeleQQ
