/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "teleqq/teleqq_store.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace TeleQQ {
namespace {

[[nodiscard]] bool PlaceholderTitle(const Chat &chat, const QString &title) {
	const auto prefix = (chat.kind == ChatKind::Group) ? u"群 "_q : u"QQ "_q;
	return title == (prefix + chat.peerId);
}

[[nodiscard]] Chat MergeChat(Chat current, const Chat &next) {
	if (!next.id.isEmpty()) {
		current.id = next.id;
	}
	if (!next.peerId.isEmpty()) {
		current.peerId = next.peerId;
	}
	if (!next.title.isEmpty()
		&& (current.title.isEmpty()
			|| !PlaceholderTitle(next, next.title)
			|| PlaceholderTitle(current, current.title))) {
		current.title = next.title;
	}
	if (!next.subtitle.isEmpty()) {
		current.subtitle = next.subtitle;
	}
	if (!next.lastMessage.isEmpty()) {
		current.lastMessage = next.lastMessage;
	}
	if (!next.avatarUrl.isEmpty()) {
		current.avatarUrl = next.avatarUrl;
	}
	if (next.updatedAt > 0) {
		current.updatedAt = next.updatedAt;
	}
	if (next.unread > 0) {
		current.unread = next.unread;
	}
	current.kind = next.kind;
	return current;
}

[[nodiscard]] Chat PlaceholderChat(const QString &chatId) {
	const auto group = chatId.startsWith(u"group:"_q);
	const auto peerId = chatId.section(u":"_q, 1);
	return {
		.kind = group ? ChatKind::Group : ChatKind::Private,
		.id = chatId,
		.peerId = peerId,
		.title = group ? (u"群 "_q + peerId) : (u"QQ "_q + peerId),
		.subtitle = group ? u"群聊"_q : peerId,
	};
}

} // namespace

void Store::upsertChat(Chat chat) {
	if (chat.id.isEmpty()) {
		return;
	}
	const auto key = chat.id;
	const auto i = _chats.find(key);
	if (i == _chats.end()) {
		const auto [inserted, ok] = _chats.emplace(key, std::move(chat));
		if (ok) {
			emitChatUpdated(inserted->second);
		}
		return;
	}
	i->second = MergeChat(i->second, chat);
	emitChatUpdated(i->second);
}

void Store::upsertChats(std::vector<Chat> chats) {
	for (auto &chat : chats) {
		upsertChat(std::move(chat));
	}
}

void Store::addMessage(Chat chat, Message message) {
	if (message.chatId.isEmpty()) {
		return;
	}
	if (chat.id.isEmpty()) {
		chat = PlaceholderChat(message.chatId);
	}
	chat.lastMessage = message.text;
	chat.updatedAt = message.time;
	if (!message.outgoing && !message.historical) {
		const auto existing = _chats.find(chat.id);
		chat.unread = (existing == _chats.end()) ? 1 : (existing->second.unread + 1);
	}
	upsertChat(std::move(chat));

	auto &list = _messages[message.chatId];
	if (!message.id.isEmpty()) {
		const auto duplicate = std::find_if(
			begin(list),
			end(list),
			[&](const Message &existing) {
				return existing.id == message.id;
			});
		if (duplicate != end(list)) {
			return;
		}
	}
	list.push_back(message);
	emitMessageAdded(list.back());
}

void Store::markRead(const QString &chatId) {
	const auto i = _chats.find(chatId);
	if (i == _chats.end()) {
		return;
	}
	i->second.unread = 0;
	emitChatUpdated(i->second);
}

std::vector<Chat> Store::chats() const {
	auto result = std::vector<Chat>();
	result.reserve(_chats.size());
	for (const auto &[id, chat] : _chats) {
		result.push_back(chat);
	}
	std::sort(result.begin(), result.end(), [](const Chat &a, const Chat &b) {
		if (a.updatedAt != b.updatedAt) {
			return a.updatedAt > b.updatedAt;
		}
		return a.title.localeAwareCompare(b.title) < 0;
	});
	return result;
}

std::vector<Message> Store::messages(const QString &chatId) const {
	const auto i = _messages.find(chatId);
	return (i == _messages.end()) ? std::vector<Message>() : i->second;
}

std::optional<Chat> Store::chat(const QString &chatId) const {
	const auto i = _chats.find(chatId);
	if (i == _chats.end()) {
		return std::nullopt;
	}
	return i->second;
}

void Store::setChatUpdatedCallback(ChatUpdatedCallback callback) {
	_chatUpdatedCallbacks.clear();
	addChatUpdatedCallback(std::move(callback));
}

void Store::setMessageAddedCallback(MessageAddedCallback callback) {
	_messageAddedCallbacks.clear();
	addMessageAddedCallback(std::move(callback));
}

void Store::addChatUpdatedCallback(ChatUpdatedCallback callback) {
	if (callback) {
		_chatUpdatedCallbacks.push_back(std::move(callback));
	}
}

void Store::addMessageAddedCallback(MessageAddedCallback callback) {
	if (callback) {
		_messageAddedCallbacks.push_back(std::move(callback));
	}
}

void Store::emitChatUpdated(const Chat &chat) const {
	for (const auto &callback : _chatUpdatedCallbacks) {
		callback(chat);
	}
}

void Store::emitMessageAdded(const Message &message) const {
	for (const auto &callback : _messageAddedCallbacks) {
		callback(message);
	}
}

} // namespace TeleQQ
