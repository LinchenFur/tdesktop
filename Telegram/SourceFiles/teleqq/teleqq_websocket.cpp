/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "teleqq/teleqq_websocket.h"

#include <QtCore/QRandomGenerator>
#include <QtNetwork/QAbstractSocket>

#include <limits>
#include <utility>

namespace TeleQQ {
namespace {

[[nodiscard]] QByteArray RandomKey() {
	auto raw = QByteArray(16, Qt::Uninitialized);
	for (auto i = 0; i != raw.size(); ++i) {
		raw[i] = char(QRandomGenerator::global()->bounded(256));
	}
	return raw.toBase64();
}

[[nodiscard]] quint16 Read16(const QByteArray &bytes, int offset) {
	return (quint16(quint8(bytes[offset])) << 8)
		| quint16(quint8(bytes[offset + 1]));
}

[[nodiscard]] quint64 Read64(const QByteArray &bytes, int offset) {
	auto result = quint64(0);
	for (auto i = 0; i != 8; ++i) {
		result = (result << 8) | quint8(bytes[offset + i]);
	}
	return result;
}

void Append16(QByteArray &bytes, quint16 value) {
	bytes.append(char((value >> 8) & 0xFF));
	bytes.append(char(value & 0xFF));
}

void Append64(QByteArray &bytes, quint64 value) {
	for (auto shift = 56; shift >= 0; shift -= 8) {
		bytes.append(char((value >> shift) & 0xFF));
	}
}

} // namespace

WebSocketClient::WebSocketClient(QObject *parent)
: QObject(parent) {
	QObject::connect(&_socket, &QTcpSocket::connected, this, [=] {
		handleConnected();
	});
	QObject::connect(&_socket, &QTcpSocket::readyRead, this, [=] {
		handleReadyRead();
	});
	QObject::connect(&_socket, &QTcpSocket::disconnected, this, [=] {
		handleDisconnected();
	});
	QObject::connect(
		&_socket,
		&QAbstractSocket::errorOccurred,
		this,
		[=] {
			if (!_manualDisconnect) {
				fail(_socket.errorString());
			}
		});
}

void WebSocketClient::connectTo(const QUrl &url) {
	_manualDisconnect = true;
	_socket.abort();

	if (url.scheme() != u"ws"_q) {
		fail(u"Only ws:// NapCat endpoints are supported by TeleQQ for now."_q);
		return;
	}

	_url = url;
	_key = RandomKey();
	_buffer.clear();
	_handshakeComplete = false;
	_manualDisconnect = false;

	emitStatus(u"connecting"_q);
	_socket.connectToHost(_url.host(), quint16(_url.port(80)));
}

void WebSocketClient::disconnectFromHost() {
	_manualDisconnect = true;
	_buffer.clear();
	_handshakeComplete = false;
	if (_socket.state() != QAbstractSocket::UnconnectedState) {
		_socket.disconnectFromHost();
	}
}

bool WebSocketClient::sendText(const QString &text) {
	if (!_handshakeComplete
		|| _socket.state() != QAbstractSocket::ConnectedState) {
		return false;
	}
	sendFrame(0x1, text.toUtf8());
	return true;
}

void WebSocketClient::setTextCallback(TextCallback callback) {
	_textCallback = std::move(callback);
}

void WebSocketClient::setStatusCallback(StatusCallback callback) {
	_statusCallback = std::move(callback);
}

void WebSocketClient::handleConnected() {
	const auto port = _url.port(80);
	const auto defaultPort = (port == 80);
	const auto host = defaultPort
		? _url.host().toUtf8()
		: (_url.host() + u":"_q + QString::number(port)).toUtf8();
	const auto target = (_url.path().isEmpty() ? u"/"_q : _url.path())
		+ (_url.query().isEmpty() ? QString() : (u"?"_q + _url.query()));
	const auto request = QByteArray()
		+ "GET " + target.toUtf8() + " HTTP/1.1\r\n"
		+ "Host: " + host + "\r\n"
		+ "Upgrade: websocket\r\n"
		+ "Connection: Upgrade\r\n"
		+ "Sec-WebSocket-Key: " + _key + "\r\n"
		+ "Sec-WebSocket-Version: 13\r\n"
		+ "User-Agent: TeleQQ/tdesktop\r\n"
		+ "\r\n";
	_socket.write(request);
}

void WebSocketClient::handleReadyRead() {
	_buffer.append(_socket.readAll());

	if (!_handshakeComplete && !consumeHandshake()) {
		return;
	}
	while (consumeFrame()) {
	}
}

void WebSocketClient::handleDisconnected() {
	if (!_manualDisconnect) {
		emitStatus(u"disconnected"_q);
	}
}

void WebSocketClient::fail(const QString &message) {
	emitStatus(u"error: "_q + message);
	if (_socket.state() != QAbstractSocket::UnconnectedState) {
		_socket.abort();
	}
}

bool WebSocketClient::consumeHandshake() {
	const auto split = _buffer.indexOf("\r\n\r\n");
	if (split < 0) {
		return false;
	}
	const auto header = _buffer.left(split);
	_buffer.remove(0, split + 4);
	if (!header.startsWith("HTTP/1.1 101")
		&& !header.startsWith("HTTP/1.0 101")) {
		fail(u"NapCat WebSocket handshake failed."_q);
		return false;
	}
	_handshakeComplete = true;
	emitStatus(u"connected"_q);
	return true;
}

bool WebSocketClient::consumeFrame() {
	if (_buffer.size() < 2) {
		return false;
	}

	const auto first = quint8(_buffer[0]);
	const auto second = quint8(_buffer[1]);
	const auto opcode = first & 0x0F;
	const auto masked = (second & 0x80) != 0;
	auto length = quint64(second & 0x7F);
	auto offset = 2;

	if (length == 126) {
		if (_buffer.size() < offset + 2) {
			return false;
		}
		length = Read16(_buffer, offset);
		offset += 2;
	} else if (length == 127) {
		if (_buffer.size() < offset + 8) {
			return false;
		}
		length = Read64(_buffer, offset);
		offset += 8;
	}

	if (length > quint64(std::numeric_limits<int>::max())) {
		fail(u"NapCat WebSocket frame is too large."_q);
		return false;
	}

	auto mask = QByteArray();
	if (masked) {
		if (_buffer.size() < offset + 4) {
			return false;
		}
		mask = _buffer.mid(offset, 4);
		offset += 4;
	}

	const auto full = offset + int(length);
	if (_buffer.size() < full) {
		return false;
	}

	auto payload = _buffer.mid(offset, int(length));
	_buffer.remove(0, full);

	if (masked) {
		for (auto i = 0; i != payload.size(); ++i) {
			payload[i] = char(payload[i] ^ mask[i % 4]);
		}
	}

	if (opcode == 0x1) {
		if (_textCallback) {
			_textCallback(std::move(payload));
		}
	} else if (opcode == 0x8) {
		_socket.disconnectFromHost();
	} else if (opcode == 0x9) {
		sendFrame(0xA, payload);
	}
	return true;
}

void WebSocketClient::sendFrame(quint8 opcode, const QByteArray &payload) {
	auto frame = QByteArray();
	frame.append(char(0x80 | opcode));
	const auto size = quint64(payload.size());
	if (size <= 125) {
		frame.append(char(0x80 | quint8(size)));
	} else if (size <= 0xFFFF) {
		frame.append(char(0x80 | 126));
		Append16(frame, quint16(size));
	} else {
		frame.append(char(0x80 | 127));
		Append64(frame, size);
	}

	auto mask = QByteArray(4, Qt::Uninitialized);
	for (auto i = 0; i != mask.size(); ++i) {
		mask[i] = char(QRandomGenerator::global()->bounded(256));
	}
	frame.append(mask);

	auto masked = payload;
	for (auto i = 0; i != masked.size(); ++i) {
		masked[i] = char(masked[i] ^ mask[i % 4]);
	}
	frame.append(masked);
	_socket.write(frame);
}

void WebSocketClient::emitStatus(const QString &status) {
	if (_statusCallback) {
		_statusCallback(status);
	}
}

} // namespace TeleQQ
