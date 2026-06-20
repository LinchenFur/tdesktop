/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "teleqq/teleqq_panel.h"

#include "teleqq/teleqq_napcat_client.h"
#include "logs.h"

#include <QtCore/QDateTime>
#include <QtCore/QPointer>
#include <QtCore/QSettings>
#include <QtCore/QUrl>
#include <QtCore/QVariant>
#include <QtGui/QTextCursor>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QListWidgetItem>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

namespace TeleQQ {
namespace {

constexpr auto kChatIdRole = Qt::UserRole + 1;
constexpr auto kSettingsGroup = "TeleQQ/NapCat";
constexpr auto kEndpointKey = "endpoint";
constexpr auto kTokenKey = "token";
constexpr auto kReconnectKey = "reconnect";
constexpr auto kDefaultEndpoint = "ws://127.0.0.1:3001";

[[nodiscard]] QString EscapeLine(QString text) {
	return text
		.replace(u"&"_q, u"&amp;"_q)
		.replace(u"<"_q, u"&lt;"_q)
		.replace(u">"_q, u"&gt;"_q);
}

} // namespace

Panel::Panel(
	not_null<Store*> store,
	not_null<NapcatClient*> client,
	QWidget *parent)
: QWidget(parent)
, _store(store)
, _client(client) {
	setupUi();
	loadSavedOptions();
	const auto weak = QPointer<Panel>(this);
	_client->addStatusCallback([=](QString status) {
		if (weak) {
			weak->setStatusText(status);
		}
	});
	if (const auto options = NapcatClient::OptionsFromEnvironment()) {
		setOptions(*options);
		connectCurrent();
	}
	refreshChats();

	_store->addChatUpdatedCallback([=](Chat chat) {
		if (weak) {
			weak->upsertChatRow(chat);
		}
	});
	_store->addMessageAddedCallback([=](Message message) {
		if (weak && message.chatId == weak->_selectedChatId) {
			weak->refreshMessages();
		}
	});
}

void Panel::refreshChats() {
	_chats->clear();
	for (const auto &chat : _store->chats()) {
		upsertChatRow(chat);
	}
	if (_selectedChatId.isEmpty() && _chats->count() > 0) {
		_chats->setCurrentRow(0);
		selectChat(currentChatId());
	}
}

void Panel::refreshMessages() {
	_messages->clear();
	if (_selectedChatId.isEmpty()) {
		return;
	}
	for (const auto &message : _store->messages(_selectedChatId)) {
		_messages->append(renderMessageLine(message));
	}
	_messages->moveCursor(QTextCursor::End);
}

void Panel::setupUi() {
	setWindowTitle(u"TeleQQ - NapCat"_q);
	resize(960, 640);

	const auto root = new QVBoxLayout(this);

	const auto config = new QWidget(this);
	const auto configLayout = new QGridLayout(config);
	configLayout->setContentsMargins(0, 0, 0, 0);
	_endpoint = new QLineEdit(config);
	_endpoint->setPlaceholderText(QString::fromLatin1(kDefaultEndpoint));
	_token = new QLineEdit(config);
	_token->setEchoMode(QLineEdit::Password);
	_token->setPlaceholderText(u"NapCat access_token，可留空"_q);
	_reconnect = new QCheckBox(u"自动重连"_q, config);
	_reconnect->setChecked(true);
	_connect = new QPushButton(u"连接 NapCat"_q, config);
	_disconnect = new QPushButton(u"断开"_q, config);
	_status = new QLabel(u"状态：未连接"_q, config);

	configLayout->addWidget(new QLabel(u"WebSocket"_q, config), 0, 0);
	configLayout->addWidget(_endpoint, 0, 1);
	configLayout->addWidget(new QLabel(u"Token"_q, config), 0, 2);
	configLayout->addWidget(_token, 0, 3);
	configLayout->addWidget(_reconnect, 0, 4);
	configLayout->addWidget(_connect, 0, 5);
	configLayout->addWidget(_disconnect, 0, 6);
	configLayout->addWidget(_status, 1, 0, 1, 7);
	configLayout->setColumnStretch(1, 2);
	configLayout->setColumnStretch(3, 1);
	root->addWidget(config);

	const auto splitter = new QSplitter(this);
	root->addWidget(splitter, 1);

	_chats = new QListWidget(splitter);
	_chats->setMinimumWidth(260);
	_chats->setUniformItemSizes(false);

	const auto right = new QWidget(splitter);
	const auto rightLayout = new QVBoxLayout(right);
	rightLayout->setContentsMargins(8, 8, 8, 8);

	_messages = new QTextEdit(right);
	_messages->setReadOnly(true);
	rightLayout->addWidget(_messages, 1);

	const auto composerRow = new QHBoxLayout();
	_composer = new QLineEdit(right);
	_composer->setPlaceholderText(u"输入消息"_q);
	_send = new QPushButton(u"发送"_q, right);
	composerRow->addWidget(_composer, 1);
	composerRow->addWidget(_send);
	rightLayout->addLayout(composerRow);

	splitter->addWidget(_chats);
	splitter->addWidget(right);
	splitter->setStretchFactor(0, 0);
	splitter->setStretchFactor(1, 1);

	QObject::connect(_chats, &QListWidget::currentItemChanged, this, [=] {
		selectChat(currentChatId());
	});
	QObject::connect(_send, &QPushButton::clicked, this, [=] {
		sendCurrentText();
	});
	QObject::connect(_composer, &QLineEdit::returnPressed, this, [=] {
		sendCurrentText();
	});
	QObject::connect(_connect, &QPushButton::clicked, this, [=] {
		connectCurrent();
	});
	QObject::connect(_disconnect, &QPushButton::clicked, this, [=] {
		_client->disconnectFromHost();
		setStatusText(u"disconnected"_q);
	});
}

void Panel::loadSavedOptions() {
	auto settings = QSettings();
	settings.beginGroup(QString::fromLatin1(kSettingsGroup));
	_endpoint->setText(settings.value(
		QString::fromLatin1(kEndpointKey),
		QString::fromLatin1(kDefaultEndpoint)).toString());
	_token->setText(settings.value(QString::fromLatin1(kTokenKey)).toString());
	_reconnect->setChecked(settings.value(
		QString::fromLatin1(kReconnectKey),
		true).toBool());
	settings.endGroup();
}

void Panel::saveOptions(const NapcatClient::Options &options) const {
	auto settings = QSettings();
	settings.beginGroup(QString::fromLatin1(kSettingsGroup));
	settings.setValue(QString::fromLatin1(kEndpointKey), options.endpoint.toString());
	settings.setValue(QString::fromLatin1(kTokenKey), options.token);
	settings.setValue(QString::fromLatin1(kReconnectKey), options.reconnect);
	settings.endGroup();
}

NapcatClient::Options Panel::currentOptions() const {
	auto endpoint = _endpoint->text().trimmed();
	if (!endpoint.contains(u"://"_q)) {
		endpoint = u"ws://"_q + endpoint;
	}
	return {
		.endpoint = QUrl(endpoint),
		.token = _token->text().trimmed(),
		.reconnect = _reconnect->isChecked(),
	};
}

void Panel::setOptions(const NapcatClient::Options &options) {
	_endpoint->setText(options.endpoint.toString());
	_token->setText(options.token);
	_reconnect->setChecked(options.reconnect);
}

void Panel::setStatusText(const QString &status) {
	if (_status) {
		_status->setText(u"状态："_q + status);
	}
}

void Panel::connectCurrent() {
	const auto options = currentOptions();
	setOptions(options);
	saveOptions(options);
	setStatusText(u"connecting"_q);
	_client->disconnectFromHost();
	_client->connectTo(options);
}

void Panel::selectChat(const QString &chatId) {
	if (chatId.isEmpty()) {
		return;
	}
	_selectedChatId = chatId;
	_store->markRead(chatId);
	refreshMessages();
}

void Panel::sendCurrentText() {
	const auto text = _composer->text().trimmed();
	if (text.isEmpty() || _selectedChatId.isEmpty()) {
		return;
	}

	const auto chat = _store->chat(_selectedChatId);
	if (!chat) {
		return;
	}

	_composer->clear();
	_store->addMessage(*chat, {
		.id = u"local:"_q
			+ QString::number(QDateTime::currentMSecsSinceEpoch()),
		.chatId = chat->id,
		.author = u"我"_q,
		.text = text,
		.time = QDateTime::currentSecsSinceEpoch(),
		.outgoing = true,
	});
	_client->sendTextMessage(chat->kind, chat->peerId, text, [](ApiResponse response) {
		if (!response.ok) {
			LOG(("TeleQQ: send failed %1").arg(response.error));
		}
	});
}

void Panel::upsertChatRow(const Chat &chat) {
	for (auto i = 0; i != _chats->count(); ++i) {
		const auto item = _chats->item(i);
		if (item->data(kChatIdRole).toString() == chat.id) {
			item->setText(renderChatRow(chat));
			return;
		}
	}
	const auto item = new QListWidgetItem(renderChatRow(chat), _chats);
	item->setData(kChatIdRole, chat.id);
	if (_selectedChatId.isEmpty()) {
		_chats->setCurrentItem(item);
		selectChat(chat.id);
	}
}

QString Panel::currentChatId() const {
	const auto item = _chats->currentItem();
	return item ? item->data(kChatIdRole).toString() : QString();
}

QString Panel::renderChatRow(const Chat &chat) const {
	const auto unread = chat.unread > 0
		? (u"  ["_q + QString::number(chat.unread) + u"]"_q)
		: QString();
	const auto preview = chat.lastMessage.isEmpty()
		? chat.subtitle
		: chat.lastMessage;
	return chat.title + unread + u"\n"_q + preview;
}

QString Panel::renderMessageLine(const Message &message) const {
	const auto time = QDateTime::fromSecsSinceEpoch(message.time).toString(
		u"HH:mm"_q);
	const auto name = message.outgoing ? u"我"_q : message.author;
	return u"<p><b>["_q
		+ EscapeLine(time)
		+ u"] "_q
		+ EscapeLine(name)
		+ u":</b> "_q
		+ EscapeLine(message.text).replace(u"\n"_q, u"<br>"_q)
		+ u"</p>"_q;
}

} // namespace TeleQQ
