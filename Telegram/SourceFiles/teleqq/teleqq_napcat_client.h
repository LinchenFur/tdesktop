/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "teleqq/teleqq_types.h"
#include "teleqq/teleqq_websocket.h"

#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace TeleQQ {

class NapcatClient final : public QObject {
public:
	struct Options {
		QUrl endpoint;
		QString token;
		bool reconnect = true;
	};

	using StatusCallback = std::function<void(QString)>;
	using EventCallback = std::function<void(QJsonObject)>;
	using MessageCallback = std::function<void(Chat, Message)>;
	using ResponseCallback = std::function<void(ApiResponse)>;
	using RosterCallback = std::function<void(RosterResult)>;
	using HistoryCallback = std::function<void(HistoryResult)>;

	explicit NapcatClient(QObject *parent = nullptr);

	[[nodiscard]] static std::optional<Options> OptionsFromEnvironment();

	void connectTo(const Options &options);
	void disconnectFromHost();

	void call(
		const QString &action,
		const QJsonObject &params,
		ResponseCallback callback = nullptr);
	void sendTextMessage(
		ChatKind kind,
		const QString &peerId,
		const QString &text,
		ResponseCallback callback = nullptr);
	void requestFriendList(RosterCallback callback);
	void requestGroupList(RosterCallback callback);
	void requestHistory(
		Chat chat,
		int count,
		HistoryCallback callback);

	void setStatusCallback(StatusCallback callback);
	void addStatusCallback(StatusCallback callback);
	void setEventCallback(EventCallback callback);
	void setMessageCallback(MessageCallback callback);

private:
	void handleStatus(const QString &status);
	void handleTextFrame(const QByteArray &bytes);
	void scheduleReconnect();
	[[nodiscard]] QUrl endpointWithToken() const;

	WebSocketClient _socket;
	Options _options;
	bool _hasOptions = false;
	bool _manualDisconnect = false;
	QString _selfId;
	std::map<QString, ResponseCallback> _pending;
	StatusCallback _statusCallback;
	std::vector<StatusCallback> _statusCallbacks;
	EventCallback _eventCallback;
	MessageCallback _messageCallback;
};

} // namespace TeleQQ
