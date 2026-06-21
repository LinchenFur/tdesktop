/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <vector>

namespace TeleQQ {

enum class ChatKind {
	Private,
	Group,
};

struct Chat {
	ChatKind kind = ChatKind::Private;
	QString id;
	QString peerId;
	QString title;
	QString subtitle;
	QString lastMessage;
	QString avatarUrl;
	qint64 updatedAt = 0;
	int unread = 0;
};

struct Message {
	QString id;
	QString chatId;
	QString author;
	QString text;
	QJsonArray segments;
	qint64 time = 0;
	bool outgoing = false;
	bool historical = false;
};

struct ApiResponse {
	bool ok = false;
	int retcode = -1;
	QJsonValue data;
	QString error;
	QString echo;
};

struct RosterResult {
	std::vector<Chat> chats;
	ApiResponse response;
};

struct HistoryResult {
	Chat chat;
	std::vector<Message> messages;
	ApiResponse response;
};

} // namespace TeleQQ
