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
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QTextOption>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>

namespace TeleQQ {
namespace {

constexpr auto kSettingsGroup = "TeleQQ/NapCat";
constexpr auto kEndpointKey = "endpoint";
constexpr auto kTokenKey = "token";
constexpr auto kReconnectKey = "reconnect";
constexpr auto kDefaultEndpoint = "ws://127.0.0.1:3001";
constexpr auto kDialogsWidth = 340;
constexpr auto kTopBarHeight = 56;
constexpr auto kComposerHeight = 58;
constexpr auto kRowHeight = 72;
constexpr auto kPadding = 14;

[[nodiscard]] QString Elide(
		QPainter &p,
		const QString &text,
		int width) {
	return p.fontMetrics().elidedText(text, Qt::ElideRight, width);
}

[[nodiscard]] QString DisplayTime(qint64 seconds) {
	return seconds > 0
		? QDateTime::fromSecsSinceEpoch(seconds).toString(u"HH:mm"_q)
		: QString();
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

	_store->addChatUpdatedCallback([=](Chat) {
		if (weak) {
			weak->refreshChats();
		}
	});
	_store->addMessageAddedCallback([=](Message message) {
		if (weak && message.chatId == weak->_selectedChatId) {
			weak->refreshMessages();
		}
	});
}

void Panel::refreshChats() {
	_chats = _store->chats();
	if (_selectedChatId.isEmpty() && !_chats.empty()) {
		selectChat(_chats.front().id);
	}
	update();
}

void Panel::refreshMessages() {
	update();
}

void Panel::setupUi() {
	setWindowTitle(u"TeleQQ"_q);
	setMouseTracking(true);
	setAutoFillBackground(false);

	_composer = new QLineEdit(this);
	_composer->setPlaceholderText(u"输入消息"_q);
	_composer->setFrame(false);
	_composer->setClearButtonEnabled(true);

	_send = new QPushButton(u"发送"_q, this);
	_send->setFlat(true);
	_send->setCursor(Qt::PointingHandCursor);

	QObject::connect(_send, &QPushButton::clicked, this, [=] {
		sendCurrentText();
	});
	QObject::connect(_composer, &QLineEdit::returnPressed, this, [=] {
		sendCurrentText();
	});
}

void Panel::loadSavedOptions() {
	auto settings = QSettings();
	settings.beginGroup(QString::fromLatin1(kSettingsGroup));
	_endpoint = settings.value(
		QString::fromLatin1(kEndpointKey),
		QString::fromLatin1(kDefaultEndpoint)).toString();
	_token = settings.value(QString::fromLatin1(kTokenKey)).toString();
	_reconnect = settings.value(
		QString::fromLatin1(kReconnectKey),
		true).toBool();
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
	auto endpoint = _endpoint.trimmed();
	if (!endpoint.contains(u"://"_q)) {
		endpoint = u"ws://"_q + endpoint;
	}
	return {
		.endpoint = QUrl(endpoint),
		.token = _token.trimmed(),
		.reconnect = _reconnect,
	};
}

void Panel::setOptions(const NapcatClient::Options &options) {
	_endpoint = options.endpoint.toString();
	_token = options.token;
	_reconnect = options.reconnect;
}

void Panel::setStatusText(const QString &status) {
	_status = status;
	update(_topBarRect);
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
	update();
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

void Panel::paintEvent(QPaintEvent*) {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	p.fillRect(rect(), QColor(255, 255, 255));

	p.fillRect(_dialogsRect, QColor(248, 249, 250));
	p.fillRect(QRect(_dialogsRect.right(), 0, 1, height()), QColor(221, 225, 229));

	p.setPen(Qt::NoPen);
	p.setBrush(QColor(255, 255, 255));
	p.drawRect(_topBarRect);
	p.fillRect(QRect(_topBarRect.left(), _topBarRect.bottom(), _topBarRect.width(), 1), QColor(221, 225, 229));

	p.setPen(QColor(32, 42, 54));
	auto titleFont = p.font();
	titleFont.setPixelSize(15);
	titleFont.setBold(true);
	p.setFont(titleFont);
	p.drawText(
		_topBarRect.adjusted(kPadding, 8, -kPadding, -26),
		Qt::AlignLeft | Qt::AlignVCenter,
		selectedChatTitle().isEmpty() ? u"TeleQQ"_q : selectedChatTitle());

	auto smallFont = p.font();
	smallFont.setPixelSize(12);
	smallFont.setBold(false);
	p.setFont(smallFont);
	p.setPen(_status == u"connected"_q ? QColor(78, 164, 95) : QColor(115, 128, 144));
	p.drawText(
		_topBarRect.adjusted(kPadding, 28, -kPadding, -6),
		Qt::AlignLeft | Qt::AlignVCenter,
		u"NapCat "_q + _status);

	p.setFont(titleFont);
	p.setPen(QColor(32, 42, 54));
	p.drawText(QRect(kPadding, 10, _dialogsRect.width() - 2 * kPadding, 28),
		Qt::AlignLeft | Qt::AlignVCenter,
		u"TeleQQ"_q);
	p.setFont(smallFont);
	p.setPen(QColor(115, 128, 144));
	p.drawText(QRect(kPadding, 34, _dialogsRect.width() - 2 * kPadding, 20),
		Qt::AlignLeft | Qt::AlignVCenter,
		u"Telegram Desktop UI / NapCat"_q);

	for (auto i = 0; i != int(_chats.size()); ++i) {
		const auto &chat = _chats[i];
		const auto row = chatRowRect(i);
		const auto selected = (chat.id == _selectedChatId);
		if (selected) {
			p.setPen(Qt::NoPen);
			p.setBrush(QColor(51, 144, 236));
			p.drawRoundedRect(row.adjusted(8, 4, -8, -4), 8, 8);
		}

		const auto avatar = QRect(row.left() + 18, row.top() + 14, 44, 44);
		p.setBrush(selected ? QColor(255, 255, 255, 60) : QColor(91, 156, 214));
		p.setPen(Qt::NoPen);
		p.drawEllipse(avatar);
		p.setPen(Qt::white);
		p.drawText(avatar, Qt::AlignCenter, chat.title.left(1).toUpper());

		const auto textLeft = avatar.right() + 12;
		const auto textWidth = row.right() - textLeft - 18;
		p.setFont(titleFont);
		p.setPen(selected ? Qt::white : QColor(32, 42, 54));
		p.drawText(QRect(textLeft, row.top() + 13, textWidth, 22),
			Qt::AlignLeft | Qt::AlignVCenter,
			Elide(p, chat.title, textWidth));

		p.setFont(smallFont);
		p.setPen(selected ? QColor(230, 242, 255) : QColor(115, 128, 144));
		const auto preview = chat.lastMessage.isEmpty()
			? chat.subtitle
			: chat.lastMessage;
		p.drawText(QRect(textLeft, row.top() + 37, textWidth, 20),
			Qt::AlignLeft | Qt::AlignVCenter,
			Elide(p, preview, textWidth));
	}

	p.fillRect(_composerRect.adjusted(0, -1, 0, 0), QColor(221, 225, 229));
	p.fillRect(_composerRect, QColor(255, 255, 255));

	const auto messages = _store->messages(_selectedChatId);
	auto y = _composerRect.top() - kPadding;
	for (auto i = int(messages.size()); i-- > 0;) {
		const auto &message = messages[i];
		const auto maxWidth = qMax(160, qMin(520, _historyRect.width() - 80));
		QTextOption option;
		option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
		QRect textBounds(0, 0, maxWidth - 28, 2000);
		const auto textRect = p.fontMetrics().boundingRect(
			textBounds,
			Qt::TextWordWrap,
			message.text);
		const auto bubbleHeight = qMax(38, textRect.height() + 22);
		const auto bubbleWidth = qMax(96, qMin(maxWidth, textRect.width() + 28));
		y -= bubbleHeight;
		if (y < _topBarRect.bottom() + kPadding) {
			break;
		}
		const auto left = message.outgoing
			? _historyRect.right() - bubbleWidth - 28
			: _historyRect.left() + 28;
		const auto bubble = QRect(left, y, bubbleWidth, bubbleHeight);
		p.setPen(Qt::NoPen);
		p.setBrush(message.outgoing ? QColor(238, 255, 222) : QColor(255, 255, 255));
		p.drawRoundedRect(bubble, 10, 10);
		p.setPen(QColor(32, 42, 54));
		p.drawText(bubble.adjusted(14, 8, -14, -12), message.text, option);
		p.setPen(QColor(115, 128, 144));
		p.drawText(bubble.adjusted(14, bubble.height() - 18, -10, -2),
			Qt::AlignRight | Qt::AlignVCenter,
			DisplayTime(message.time));
		y -= 8;
	}

	if (_selectedChatId.isEmpty()) {
		p.setPen(QColor(115, 128, 144));
		p.drawText(_historyRect, Qt::AlignCenter, u"连接后会话会显示在这里"_q);
	}
}

void Panel::resizeEvent(QResizeEvent*) {
	updateLayout();
}

void Panel::mousePressEvent(QMouseEvent *e) {
	const auto index = chatIndexAt(e->pos());
	if (index >= 0 && index < int(_chats.size())) {
		selectChat(_chats[index].id);
	}
}

void Panel::updateLayout() {
	_dialogsRect = QRect(0, 0, qMin(kDialogsWidth, qMax(260, width() / 3)), height());
	_historyRect = QRect(_dialogsRect.right() + 1, 0, width() - _dialogsRect.width() - 1, height());
	_topBarRect = QRect(_historyRect.left(), 0, _historyRect.width(), kTopBarHeight);
	_composerRect = QRect(_historyRect.left(), height() - kComposerHeight, _historyRect.width(), kComposerHeight);
	const auto sendWidth = 70;
	_send->setGeometry(
		_composerRect.right() - sendWidth - 12,
		_composerRect.top() + 12,
		sendWidth,
		34);
	_composer->setGeometry(
		_composerRect.left() + 18,
		_composerRect.top() + 12,
		_composerRect.width() - sendWidth - 42,
		34);
}

QString Panel::selectedChatTitle() const {
	const auto chat = _store->chat(_selectedChatId);
	return chat ? chat->title : QString();
}

QRect Panel::chatRowRect(int index) const {
	return QRect(0, 62 + index * kRowHeight, _dialogsRect.width(), kRowHeight);
}

int Panel::chatIndexAt(const QPoint &position) const {
	if (!_dialogsRect.contains(position) || position.y() < 62) {
		return -1;
	}
	return (position.y() - 62) / kRowHeight;
}

} // namespace TeleQQ
