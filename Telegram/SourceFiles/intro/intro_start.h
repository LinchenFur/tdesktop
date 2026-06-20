/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "intro/intro_step.h"

namespace Ui {
class FlatLabel;
class InputField;
class LinkButton;
class PasswordInput;
class RoundButton;
} // namespace Ui

namespace Intro {
namespace details {

class StartWidget : public Step {
public:
	StartWidget(
		QWidget *parent,
		not_null<Main::Account*> account,
		not_null<Data*> data);

	void submit() override;
	rpl::producer<QString> nextButtonText() const override;
	rpl::producer<> nextButtonFocusRequests() const override;
	void activate() override;
	void setInnerFocus() override;

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	void loadSavedOptions();
	void saveCurrentOptions() const;
	void updateControlsGeometry();
	void handleStatus(const QString &status);

	object_ptr<Ui::InputField> _endpoint;
	object_ptr<Ui::PasswordInput> _token;
	rpl::event_stream<> _nextButtonFocusRequests;
	bool _connecting = false;

};

} // namespace details
} // namespace Intro
