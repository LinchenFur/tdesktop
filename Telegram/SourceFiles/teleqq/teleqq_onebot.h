/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "teleqq/teleqq_types.h"

namespace TeleQQ {
namespace OneBot {

[[nodiscard]] QString PrivateChatId(const QString &userId);
[[nodiscard]] QString GroupChatId(const QString &groupId);
[[nodiscard]] QJsonArray TextSegments(const QString &text);
[[nodiscard]] QString RenderMessage(const QJsonValue &message);
[[nodiscard]] Chat ChatFromFriend(const QJsonObject &object);
[[nodiscard]] Chat ChatFromGroup(const QJsonObject &object);
[[nodiscard]] Chat ChatFromMessageEvent(const QJsonObject &object);
[[nodiscard]] Message MessageFromEvent(
	const QJsonObject &object,
	const QString &selfId = QString());
[[nodiscard]] Message MessageFromHistory(
	ChatKind kind,
	const QString &peerId,
	const QJsonObject &object,
	const QString &selfId = QString());
[[nodiscard]] QJsonObject SendTextParams(
	ChatKind kind,
	const QString &peerId,
	const QString &text);

} // namespace OneBot
} // namespace TeleQQ
