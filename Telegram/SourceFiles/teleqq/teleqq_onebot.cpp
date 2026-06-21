/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "teleqq/teleqq_onebot.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QUrl>

namespace TeleQQ::OneBot {
namespace {

[[nodiscard]] QString StringField(
		const QJsonObject &object,
		const QString &key) {
	const auto value = object.value(key);
	if (value.isString()) {
		return value.toString();
	} else if (value.isDouble()) {
		return QString::number(qint64(value.toDouble()));
	}
	return QString();
}

[[nodiscard]] bool LooksLikeUrl(const QString &value) {
	return value.startsWith(u"http://"_q, Qt::CaseInsensitive)
		|| value.startsWith(u"https://"_q, Qt::CaseInsensitive);
}

[[nodiscard]] QString FirstUrl(const QJsonObject &data) {
	for (const auto &key : { u"url"_q, u"file"_q }) {
		const auto value = data.value(key).toString();
		if (LooksLikeUrl(value)) {
			return value;
		}
	}
	return QString();
}

[[nodiscard]] QString FirstFileName(
		const QJsonObject &data,
		const QString &url,
		const QString &fallback) {
	for (const auto &key : { u"name"_q, u"file_name"_q, u"filename"_q }) {
		const auto value = data.value(key).toString();
		if (!value.isEmpty()) {
			return value;
		}
	}
	const auto fromUrl = QUrl(url).fileName();
	return fromUrl.isEmpty() ? fallback : fromUrl;
}

[[nodiscard]] QString MimeFromName(
		const QString &name,
		AttachmentKind kind) {
	const auto lower = name.toLower();
	if (lower.endsWith(u".png"_q)) {
		return u"image/png"_q;
	} else if (lower.endsWith(u".gif"_q)) {
		return u"image/gif"_q;
	} else if (lower.endsWith(u".webp"_q)) {
		return u"image/webp"_q;
	} else if (lower.endsWith(u".jpg"_q) || lower.endsWith(u".jpeg"_q)) {
		return u"image/jpeg"_q;
	} else if (lower.endsWith(u".mp4"_q)) {
		return u"video/mp4"_q;
	} else if (lower.endsWith(u".pdf"_q)) {
		return u"application/pdf"_q;
	} else if (lower.endsWith(u".zip"_q)) {
		return u"application/zip"_q;
	}
	return (kind == AttachmentKind::Image)
		? u"image/jpeg"_q
		: u"application/octet-stream"_q;
}

[[nodiscard]] qint64 SizeField(const QJsonObject &data) {
	for (const auto &key : { u"size"_q, u"file_size"_q }) {
		const auto value = data.value(key);
		if (value.isDouble()) {
			return qint64(value.toDouble());
		}
	}
	return 0;
}

[[nodiscard]] QString RenderSegment(const QJsonObject &segment) {
	const auto type = segment.value(u"type"_q).toString();
	const auto data = segment.value(u"data"_q).toObject();
	if (type == u"text"_q) {
		return data.value(u"text"_q).toString();
	} else if (type == u"at"_q) {
		return u"@"_q + StringField(data, u"qq"_q);
	} else if (type == u"image"_q) {
		return u"[图片]"_q;
	} else if (type == u"record"_q) {
		return u"[语音]"_q;
	} else if (type == u"video"_q) {
		return u"[视频]"_q;
	} else if (type == u"file"_q) {
		const auto name = FirstFileName(data, FirstUrl(data), QString());
		return name.isEmpty() ? u"[文件]"_q : (u"[文件 "_q + name + u"]"_q);
	} else if (type == u"face"_q) {
		const auto id = StringField(data, u"id"_q);
		return id.isEmpty() ? u"[QQ表情]"_q : (u"[QQ表情 "_q + id + u"]"_q);
	} else if (type == u"reply"_q) {
		return u"[回复]"_q;
	}
	return u"["_q + type + u"]"_q;
}

[[nodiscard]] QString BestSenderName(const QJsonObject &sender) {
	const auto card = sender.value(u"card"_q).toString();
	if (!card.isEmpty()) {
		return card;
	}
	const auto nickname = sender.value(u"nickname"_q).toString();
	if (!nickname.isEmpty()) {
		return nickname;
	}
	return StringField(sender, u"user_id"_q);
}

[[nodiscard]] QString PrivateAvatarUrl(const QString &userId) {
	return u"https://q1.qlogo.cn/g?b=qq&nk=%1&s=100"_q.arg(userId);
}

[[nodiscard]] QString GroupAvatarUrl(const QString &groupId) {
	return u"https://p.qlogo.cn/gh/%1/%1/100"_q.arg(groupId);
}

[[nodiscard]] std::vector<Attachment> Attachments(const QJsonArray &segments) {
	auto result = std::vector<Attachment>();
	for (const auto &value : segments) {
		if (!value.isObject()) {
			continue;
		}
		const auto segment = value.toObject();
		const auto type = segment.value(u"type"_q).toString();
		if (type != u"image"_q && type != u"file"_q) {
			continue;
		}
		const auto kind = (type == u"image"_q)
			? AttachmentKind::Image
			: AttachmentKind::File;
		const auto data = segment.value(u"data"_q).toObject();
		const auto url = FirstUrl(data);
		if (url.isEmpty()) {
			continue;
		}
		const auto name = FirstFileName(
			data,
			url,
			(kind == AttachmentKind::Image) ? u"image.jpg"_q : u"file"_q);
		result.push_back({
			.kind = kind,
			.url = url,
			.name = name,
			.mime = MimeFromName(name, kind),
			.size = SizeField(data),
		});
	}
	return result;
}

[[nodiscard]] QStringList ImageUrls(const QJsonArray &segments) {
	auto result = QStringList();
	for (const auto &value : segments) {
		if (!value.isObject()) {
			continue;
		}
		const auto segment = value.toObject();
		if (segment.value(u"type"_q).toString() != u"image"_q) {
			continue;
		}
		const auto data = segment.value(u"data"_q).toObject();
		const auto url = data.value(u"url"_q).toString();
		const auto file = data.value(u"file"_q).toString();
		if (!url.isEmpty()) {
			result.push_back(url);
		} else if (!file.isEmpty()) {
			result.push_back(file);
		}
	}
	return result;
}

[[nodiscard]] Message MessageFromObject(
		const Chat &chat,
		const QJsonObject &object,
		const QString &selfId) {
	const auto sender = object.value(u"sender"_q).toObject();
	const auto userId = StringField(object, u"user_id"_q);
	const auto raw = object.value(u"raw_message"_q).toString();
	const auto text = RenderMessage(object.value(u"message"_q));
	const auto message = object.value(u"message"_q);
	const auto segments = message.isArray() ? message.toArray() : TextSegments(raw);
	const auto sentBySelf = (object.value(u"post_type"_q).toString()
			== u"message_sent"_q)
		|| (object.value(u"message_sent_type"_q).toString() == u"self"_q)
		|| (!selfId.isEmpty() && userId == selfId);
	return {
		.id = StringField(object, u"message_id"_q),
		.chatId = chat.id,
		.authorId = userId,
		.author = BestSenderName(sender),
		.text = !text.isEmpty() ? text : raw,
		.imageUrls = ImageUrls(segments),
		.attachments = Attachments(segments),
		.segments = segments,
		.time = qint64(object.value(u"time"_q).toDouble()),
		.outgoing = sentBySelf,
	};
}

} // namespace

QString PrivateChatId(const QString &userId) {
	return u"private:"_q + userId;
}

QString GroupChatId(const QString &groupId) {
	return u"group:"_q + groupId;
}

QJsonArray TextSegments(const QString &text) {
	return QJsonArray{
		QJsonObject{
			{ u"type"_q, u"text"_q },
			{ u"data"_q, QJsonObject{ { u"text"_q, text } } },
		},
	};
}

QString RenderMessage(const QJsonValue &message) {
	if (message.isString()) {
		return message.toString();
	} else if (message.isObject()) {
		return RenderSegment(message.toObject());
	} else if (!message.isArray()) {
		return QString();
	}
	auto result = QString();
	for (const auto &segment : message.toArray()) {
		if (segment.isObject()) {
			result += RenderSegment(segment.toObject());
		}
	}
	return result.trimmed();
}

Chat ChatFromFriend(const QJsonObject &object) {
	const auto peerId = StringField(object, u"user_id"_q);
	const auto remark = object.value(u"remark"_q).toString();
	const auto nickname = object.value(u"nickname"_q).toString();
	const auto title = !remark.isEmpty()
		? remark
		: (!nickname.isEmpty() ? nickname : (u"QQ "_q + peerId));
	return {
		.kind = ChatKind::Private,
		.id = PrivateChatId(peerId),
		.peerId = peerId,
		.title = title,
		.subtitle = peerId,
		.avatarUrl = PrivateAvatarUrl(peerId),
	};
}

Chat ChatFromGroup(const QJsonObject &object) {
	const auto peerId = StringField(object, u"group_id"_q);
	const auto title = object.value(u"group_name"_q).toString(
		u"群 "_q + peerId);
	const auto count = object.value(u"member_count"_q);
	return {
		.kind = ChatKind::Group,
		.id = GroupChatId(peerId),
		.peerId = peerId,
		.title = title,
		.subtitle = count.isDouble()
			? QString::number(qint64(count.toDouble())) + u" 人"_q
			: u"群聊"_q,
		.avatarUrl = GroupAvatarUrl(peerId),
	};
}

Chat ChatFromMessageEvent(const QJsonObject &object) {
	const auto isGroup = (object.value(u"message_type"_q).toString()
		== u"group"_q);
	const auto peerId = isGroup
		? StringField(object, u"group_id"_q)
		: (object.value(u"post_type"_q).toString() == u"message_sent"_q
			&& !StringField(object, u"target_id"_q).isEmpty())
		? StringField(object, u"target_id"_q)
		: StringField(object, u"user_id"_q);
	const auto sender = object.value(u"sender"_q).toObject();
	const auto senderName = BestSenderName(sender);
	const auto groupName = object.value(u"group_name"_q).toString();
	return {
		.kind = isGroup ? ChatKind::Group : ChatKind::Private,
		.id = isGroup ? GroupChatId(peerId) : PrivateChatId(peerId),
		.peerId = peerId,
		.title = isGroup
			? groupName
			: (!senderName.isEmpty() ? senderName : (u"QQ "_q + peerId)),
		.subtitle = isGroup ? senderName : peerId,
		.avatarUrl = isGroup ? GroupAvatarUrl(peerId) : PrivateAvatarUrl(peerId),
	};
}

Message MessageFromEvent(
		const QJsonObject &object,
		const QString &selfId) {
	const auto chat = ChatFromMessageEvent(object);
	return MessageFromObject(chat, object, selfId);
}

Message MessageFromHistory(
		ChatKind kind,
		const QString &peerId,
		const QJsonObject &object,
		const QString &selfId) {
	auto chat = Chat{
		.kind = kind,
		.id = (kind == ChatKind::Group)
			? GroupChatId(peerId)
			: PrivateChatId(peerId),
		.peerId = peerId,
		.title = (kind == ChatKind::Group)
			? (u"群 "_q + peerId)
			: (u"QQ "_q + peerId),
		.avatarUrl = (kind == ChatKind::Group)
			? GroupAvatarUrl(peerId)
			: PrivateAvatarUrl(peerId),
	};
	return MessageFromObject(chat, object, selfId);
}

QJsonObject SendTextParams(
		ChatKind kind,
		const QString &peerId,
		const QString &text) {
	auto result = QJsonObject{
		{ u"message"_q, TextSegments(text) },
	};
	if (kind == ChatKind::Group) {
		result.insert(u"group_id"_q, peerId);
	} else {
		result.insert(u"user_id"_q, peerId);
	}
	return result;
}

} // namespace TeleQQ::OneBot
