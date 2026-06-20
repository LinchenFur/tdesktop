/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "intro/intro_start.h"

#include "core/application.h"
#include "teleqq/teleqq_napcat_client.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/fields/password_input.h"
#include "window/window_controller.h"
#include "styles/style_intro.h"

#include <QtCore/QSettings>
#include <QtCore/QUrl>

namespace Intro {
namespace details {
namespace {

constexpr auto kSettingsGroup = "TeleQQ/NapCat";
constexpr auto kEndpointKey = "endpoint";
constexpr auto kTokenKey = "token";
constexpr auto kDefaultEndpoint = "ws://127.0.0.1:3001";

[[nodiscard]] QString NormalizeEndpoint(QString endpoint) {
	endpoint = endpoint.trimmed();
	if (!endpoint.contains(u"://"_q)) {
		endpoint = u"ws://"_q + endpoint;
	}
	return endpoint;
}

} // namespace

StartWidget::StartWidget(
	QWidget *parent,
	not_null<Main::Account*> account,
	not_null<Data*> data)
: Step(parent, account, data, true)
, _endpoint(
	this,
	st::introCountry,
	rpl::single(u"NapCat WebSocket"_q))
, _token(
	this,
	st::introPassword,
	rpl::single(u"NapCat access_token（可留空）"_q)) {
	setMouseTracking(true);
	setTitleText(rpl::single(u"TeleQQ"_q));
	setDescriptionText(u"使用 Telegram Desktop 界面连接 NapCat 后端。"_q);
	setErrorCentered(true);

	_endpoint->changes(
	) | rpl::on_next([=] {
		hideError();
	}, _endpoint->lifetime());
	connect(_token, &Ui::PasswordInput::changed, [=] {
		hideError();
	});

	setTabOrder(_endpoint, _token);
	loadSavedOptions();
	show();
}

void StartWidget::submit() {
	if (_connecting) {
		return;
	}

	auto endpoint = NormalizeEndpoint(_endpoint->getLastText());
	if (endpoint.isEmpty() || endpoint == u"ws://"_q) {
		_endpoint->showError();
		showError(rpl::single(u"请输入 NapCat WebSocket 地址。"_q));
		_endpoint->setFocusFast();
		return;
	}

	const auto url = QUrl(endpoint);
	const auto scheme = url.scheme();
	if (!url.isValid()
		|| scheme != u"ws"_q
		|| url.host().isEmpty()) {
		_endpoint->showError();
		showError(rpl::single(u"WebSocket 地址需要是 ws://。"_q));
		_endpoint->setFocusFast();
		return;
	}

	_endpoint->setText(url.toString());
	saveCurrentOptions();

	if (const auto client = Core::App().teleqqClient()) {
		_connecting = true;
		showError(rpl::single(u"正在连接 NapCat..."_q));
		client->addStatusCallback([=](QString status) {
			handleStatus(status);
		});
		client->disconnectFromHost();
		client->connectTo({
			.endpoint = url,
			.token = _token->getLastText().trimmed(),
			.reconnect = true,
		});
	}
}

rpl::producer<QString> StartWidget::nextButtonText() const {
	return rpl::single(u"连接 NapCat"_q);
}

rpl::producer<> StartWidget::nextButtonFocusRequests() const {
	return _nextButtonFocusRequests.events();
}

void StartWidget::activate() {
	Step::activate();
	_endpoint->show();
	_token->show();
	setInnerFocus();
}

void StartWidget::setInnerFocus() {
	_endpoint->setFocusFast();
}

void StartWidget::resizeEvent(QResizeEvent *e) {
	Step::resizeEvent(e);
	updateControlsGeometry();
}

void StartWidget::updateControlsGeometry() {
	const auto firstTop = contentTop() + st::introStepFieldTop;
	_endpoint->moveToLeft(contentLeft(), firstTop);
	_token->moveToLeft(
		contentLeft(),
		firstTop + _endpoint->height() + st::introPhoneTop);
}

void StartWidget::handleStatus(const QString &status) {
	if (!_connecting) {
		return;
	}
	if (status == u"connected"_q) {
		_connecting = false;
		hideError();
		if (const auto window = Core::App().activePrimaryWindow()) {
			window->widget()->setupTeleqq({});
		}
	} else if (status.startsWith(u"error:"_q)) {
		_connecting = false;
		showError(rpl::single(u"NapCat 连接失败："_q + status));
		_endpoint->setFocusFast();
	}
}

void StartWidget::loadSavedOptions() {
	if (const auto env = TeleQQ::NapcatClient::OptionsFromEnvironment()) {
		_endpoint->setText(env->endpoint.toString());
		_token->setText(env->token);
		return;
	}

	auto settings = QSettings();
	settings.beginGroup(QString::fromLatin1(kSettingsGroup));
	_endpoint->setText(settings.value(
		QString::fromLatin1(kEndpointKey),
		QString::fromLatin1(kDefaultEndpoint)).toString());
	_token->setText(settings.value(QString::fromLatin1(kTokenKey)).toString());
	settings.endGroup();
}

void StartWidget::saveCurrentOptions() const {
	auto settings = QSettings();
	settings.beginGroup(QString::fromLatin1(kSettingsGroup));
	settings.setValue(
		QString::fromLatin1(kEndpointKey),
		NormalizeEndpoint(_endpoint->getLastText()));
	settings.setValue(
		QString::fromLatin1(kTokenKey),
		_token->getLastText().trimmed());
	settings.setValue(QString::fromLatin1("reconnect"), true);
	settings.endGroup();
}

} // namespace details
} // namespace Intro
