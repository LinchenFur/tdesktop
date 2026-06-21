/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "teleqq/teleqq_napcat_client.h"

#include "teleqq/teleqq_onebot.h"

#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRandomGenerator>
#include <QtCore/QTimer>
#include <QtCore/QUrlQuery>
#include <QtCore/qglobal.h>

#include <utility>

namespace TeleQQ {
namespace {

constexpr auto kEndpointEnv = "TELEQQ_NAPCAT_WS";
constexpr auto kTokenEnv = "TELEQQ_NAPCAT_TOKEN";

[[nodiscard]] QString NewEcho(const QString &action) {
	return action
		+ u":"_q
		+ QString::number(QDateTime::currentMSecsSinceEpoch())
		+ u":"_q
		+ QString::number(QRandomGenerator::global()->generate(), 16);
}

[[nodiscard]] ApiResponse ParseResponse(const QJsonObject &object) {
	const auto status = object.value(u"status"_q).toString();
	const auto retcode = object.value(u"retcode"_q).toInt(-1);
	const auto message = object.value(u"message"_q).toString();
	const auto wording = object.value(u"wording"_q).toString();
	return {
		.ok = (status == u"ok"_q && retcode == 0),
		.retcode = retcode,
		.data = object.value(u"data"_q),
		.error = !message.isEmpty() ? message : wording,
		.echo = object.value(u"echo"_q).toString(),
	};
}

} // namespace

NapcatClient::NapcatClient(QObject *parent)
: QObject(parent)
, _socket(this) {
	_socket.setStatusCallback([=](QString status) {
		handleStatus(std::move(status));
	});
	_socket.setTextCallback([=](QByteArray bytes) {
		handleTextFrame(bytes);
	});
}

std::optional<NapcatClient::Options> NapcatClient::OptionsFromEnvironment() {
	const auto endpoint = qEnvironmentVariable(kEndpointEnv);
	if (endpoint.isEmpty()) {
		return std::nullopt;
	}
	return Options{
		.endpoint = QUrl(endpoint),
		.token = qEnvironmentVariable(kTokenEnv),
		.reconnect = true,
	};
}

void NapcatClient::connectTo(const Options &options) {
	_options = options;
	_hasOptions = true;
	_manualDisconnect = false;
	_socket.connectTo(endpointWithToken());
}

void NapcatClient::disconnectFromHost() {
	_manualDisconnect = true;
	_pending.clear();
	_socket.disconnectFromHost();
}

void NapcatClient::call(
		const QString &action,
		const QJsonObject &params,
		ResponseCallback callback) {
	const auto echo = NewEcho(action);
	const auto payload = QJsonObject{
		{ u"action"_q, action },
		{ u"params"_q, params },
		{ u"echo"_q, echo },
	};
	const auto json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
	if (!_socket.sendText(QString::fromUtf8(json))) {
		if (callback) {
			callback({
				.ok = false,
				.retcode = -1,
				.error = u"NapCat WebSocket is not connected."_q,
				.echo = echo,
			});
		}
		return;
	}
	if (callback) {
		_pending.emplace(echo, std::move(callback));
	}
}

void NapcatClient::sendTextMessage(
		ChatKind kind,
		const QString &peerId,
		const QString &text,
		ResponseCallback callback) {
	call(
		(kind == ChatKind::Group) ? u"send_group_msg"_q : u"send_private_msg"_q,
		OneBot::SendTextParams(kind, peerId, text),
		std::move(callback));
}

void NapcatClient::requestFriendList(RosterCallback callback) {
	call(u"get_friend_list"_q, QJsonObject(), [callback = std::move(callback)](
			ApiResponse response) mutable {
		auto result = RosterResult{ .response = response };
		if (response.ok && response.data.isArray()) {
			for (const auto &entry : response.data.toArray()) {
				if (entry.isObject()) {
					result.chats.push_back(OneBot::ChatFromFriend(
						entry.toObject()));
				}
			}
		}
		if (callback) {
			callback(std::move(result));
		}
	});
}

void NapcatClient::requestGroupList(RosterCallback callback) {
	call(u"get_group_list"_q, QJsonObject(), [callback = std::move(callback)](
			ApiResponse response) mutable {
		auto result = RosterResult{ .response = response };
		if (response.ok && response.data.isArray()) {
			for (const auto &entry : response.data.toArray()) {
				if (entry.isObject()) {
					result.chats.push_back(OneBot::ChatFromGroup(
						entry.toObject()));
				}
			}
		}
		if (callback) {
			callback(std::move(result));
		}
	});
}

void NapcatClient::requestHistory(
		Chat chat,
		int count,
		HistoryCallback callback) {
	auto params = QJsonObject{
		{ u"message_seq"_q, u"0"_q },
		{ u"count"_q, count },
	};
	if (chat.kind == ChatKind::Group) {
		params.insert(u"group_id"_q, chat.peerId);
	} else {
		params.insert(u"user_id"_q, chat.peerId);
	}
	call(
		(chat.kind == ChatKind::Group)
			? u"get_group_msg_history"_q
			: u"get_friend_msg_history"_q,
		params,
		[chat = std::move(chat),
		 selfId = _selfId,
		 callback = std::move(callback)](
				ApiResponse response) mutable {
			auto result = HistoryResult{
				.chat = chat,
				.response = response,
			};
			const auto data = response.data.toObject();
			const auto messages = data.value(u"messages"_q).toArray();
			if (response.ok) {
				result.messages.reserve(messages.size());
				for (const auto &entry : messages) {
					if (entry.isObject()) {
						auto message = OneBot::MessageFromHistory(
							chat.kind,
							chat.peerId,
							entry.toObject(),
							selfId);
						message.historical = true;
						result.messages.push_back(std::move(message));
					}
				}
			}
			if (callback) {
				callback(std::move(result));
			}
		});
}

void NapcatClient::setStatusCallback(StatusCallback callback) {
	_statusCallback = std::move(callback);
}

void NapcatClient::addStatusCallback(StatusCallback callback) {
	_statusCallbacks.push_back(std::move(callback));
}

void NapcatClient::setEventCallback(EventCallback callback) {
	_eventCallback = std::move(callback);
}

void NapcatClient::setMessageCallback(MessageCallback callback) {
	_messageCallback = std::move(callback);
}

void NapcatClient::handleStatus(const QString &status) {
	if (_statusCallback) {
		_statusCallback(status);
	}
	for (const auto &callback : _statusCallbacks) {
		if (callback) {
			callback(status);
		}
	}
	if (_manualDisconnect || !_hasOptions || !_options.reconnect) {
		return;
	}
	if (status == u"disconnected"_q || status.startsWith(u"error:"_q)) {
		scheduleReconnect();
	}
}

void NapcatClient::handleTextFrame(const QByteArray &bytes) {
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(bytes, &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		if (_statusCallback) {
			_statusCallback(u"invalid-json: "_q + error.errorString());
		}
		return;
	}

	const auto object = document.object();
	const auto echo = object.value(u"echo"_q).toString();
	if (!echo.isEmpty()) {
		if (const auto i = _pending.find(echo); i != _pending.end()) {
			const auto callback = std::move(i->second);
			_pending.erase(i);
			if (callback) {
				callback(ParseResponse(object));
			}
			return;
		}
	}

	if (const auto selfId = object.value(u"self_id"_q); !selfId.isUndefined()) {
		_selfId = selfId.isString()
			? selfId.toString()
			: QString::number(qint64(selfId.toDouble()));
	}

	if (_eventCallback) {
		_eventCallback(object);
	}

	const auto postType = object.value(u"post_type"_q).toString();
	if ((postType == u"message"_q || postType == u"message_sent"_q)
		&& _messageCallback) {
		_messageCallback(
			OneBot::ChatFromMessageEvent(object),
			OneBot::MessageFromEvent(object, _selfId));
	}
}

void NapcatClient::scheduleReconnect() {
	QTimer::singleShot(3000, this, [=] {
		if (!_manualDisconnect && _hasOptions) {
			_socket.connectTo(endpointWithToken());
		}
	});
}

QUrl NapcatClient::endpointWithToken() const {
	auto result = _options.endpoint;
	if (!_options.token.isEmpty()) {
		auto query = QUrlQuery(result);
		query.addQueryItem(u"access_token"_q, _options.token);
		result.setQuery(query);
	}
	return result;
}

} // namespace TeleQQ
