/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtNetwork/QTcpSocket>

#include <functional>

namespace TeleQQ {

class WebSocketClient final : public QObject {
public:
	using TextCallback = std::function<void(QByteArray)>;
	using StatusCallback = std::function<void(QString)>;

	explicit WebSocketClient(QObject *parent = nullptr);

	void connectTo(const QUrl &url);
	void disconnectFromHost();
	[[nodiscard]] bool sendText(const QString &text);

	void setTextCallback(TextCallback callback);
	void setStatusCallback(StatusCallback callback);

private:
	void handleConnected();
	void handleReadyRead();
	void handleDisconnected();
	void fail(const QString &message);
	[[nodiscard]] bool consumeHandshake();
	[[nodiscard]] bool consumeFrame();
	void sendFrame(quint8 opcode, const QByteArray &payload);
	void emitStatus(const QString &status);

	QTcpSocket _socket;
	QUrl _url;
	QByteArray _key;
	QByteArray _buffer;
	bool _handshakeComplete = false;
	bool _manualDisconnect = false;
	TextCallback _textCallback;
	StatusCallback _statusCallback;
};

} // namespace TeleQQ

