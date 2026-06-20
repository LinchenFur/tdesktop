/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "teleqq/teleqq_types.h"

#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace TeleQQ {

class Store final {
public:
	using ChatUpdatedCallback = std::function<void(Chat)>;
	using MessageAddedCallback = std::function<void(Message)>;

	void upsertChat(Chat chat);
	void upsertChats(std::vector<Chat> chats);
	void addMessage(Chat chat, Message message);
	void markRead(const QString &chatId);

	[[nodiscard]] std::vector<Chat> chats() const;
	[[nodiscard]] std::vector<Message> messages(const QString &chatId) const;
	[[nodiscard]] std::optional<Chat> chat(const QString &chatId) const;

	void setChatUpdatedCallback(ChatUpdatedCallback callback);
	void setMessageAddedCallback(MessageAddedCallback callback);
	void addChatUpdatedCallback(ChatUpdatedCallback callback);
	void addMessageAddedCallback(MessageAddedCallback callback);

private:
	void emitChatUpdated(const Chat &chat) const;
	void emitMessageAdded(const Message &message) const;

	std::map<QString, Chat> _chats;
	std::map<QString, std::vector<Message>> _messages;
	std::vector<ChatUpdatedCallback> _chatUpdatedCallbacks;
	std::vector<MessageAddedCallback> _messageAddedCallbacks;
};

} // namespace TeleQQ
