/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "passport/passport_panel_edit_scans.h"

#include "passport/passport_panel_controller.h"
#include "passport/passport_panel_details_row.h"
#include "info/profile/info_profile_button.h"
#include "info/profile/info_profile_values.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/fade_wrap.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/text_options.h"
#include "core/file_utilities.h"
#include "lang/lang_keys.h"
#include "boxes/abstract_box.h"
#include "styles/style_passport.h"

namespace Passport {

class ScanButton : public Ui::AbstractButton {
public:
	ScanButton(
		QWidget *parent,
		const style::PassportScanRow &st,
		const QString &name,
		const QString &status,
		bool deleted);

	void setImage(const QImage &image);
	void setStatus(const QString &status);
	void setDeleted(bool deleted);

	rpl::producer<> deleteClicks() const {
		return _delete->entity()->clicks();
	}
	rpl::producer<> restoreClicks() const {
		return _restore->entity()->clicks();
	}

protected:
	int resizeGetHeight(int newWidth) override;

	void paintEvent(QPaintEvent *e) override;

private:
	int countAvailableWidth() const;

	const style::PassportScanRow &_st;
	Text _name;
	Text _status;
	int _nameHeight = 0;
	int _statusHeight = 0;
	QImage _image;
	object_ptr<Ui::FadeWrapScaled<Ui::IconButton>> _delete;
	object_ptr<Ui::FadeWrapScaled<Ui::RoundButton>> _restore;

};

ScanButton::ScanButton(
	QWidget *parent,
	const style::PassportScanRow &st,
	const QString &name,
	const QString &status,
	bool deleted)
: AbstractButton(parent)
, _st(st)
, _name(
	st::passportScanNameStyle,
	name,
	Ui::NameTextOptions())
, _status(
	st::defaultTextStyle,
	status,
	Ui::NameTextOptions())
, _delete(this, object_ptr<Ui::IconButton>(this, _st.remove))
, _restore(
	this,
	object_ptr<Ui::RoundButton>(
		this,
		langFactory(lng_passport_delete_scan_undo),
		_st.restore)) {
	_delete->toggle(!deleted, anim::type::instant);
	_restore->toggle(deleted, anim::type::instant);
}

void ScanButton::setImage(const QImage &image) {
	_image = image;
	update();
}

void ScanButton::setStatus(const QString &status) {
	_status.setText(
		st::defaultTextStyle,
		status,
		Ui::NameTextOptions());
	update();
}

void ScanButton::setDeleted(bool deleted) {
	_delete->toggle(!deleted, anim::type::instant);
	_restore->toggle(deleted, anim::type::instant);
	update();
}

int ScanButton::resizeGetHeight(int newWidth) {
	_nameHeight = st::semiboldFont->height;
	_statusHeight = st::normalFont->height;
	const auto result = _st.padding.top() + _st.size + _st.padding.bottom();
	const auto right = _st.padding.right();
	_delete->moveToRight(
		right,
		(result - _delete->height()) / 2,
		newWidth);
	_restore->moveToRight(
		right,
		(result - _restore->height()) / 2,
		newWidth);
	return result + st::lineWidth;
}

int ScanButton::countAvailableWidth() const {
	return width()
		- _st.padding.left()
		- _st.textLeft
		- _st.padding.right()
		- std::max(_delete->width(), _restore->width());
}

void ScanButton::paintEvent(QPaintEvent *e) {
	Painter p(this);

	const auto left = _st.padding.left();
	const auto top = _st.padding.top();
	p.fillRect(
		left,
		height() - _st.border,
		width() - left,
		_st.border,
		_st.borderFg);

	if (_restore->toggled()) {
		p.setOpacity(st::passportScanDeletedOpacity);
	}

	if (_image.isNull()) {
		p.fillRect(left, top, _st.size, _st.size, Qt::black);
	} else {
		PainterHighQualityEnabler hq(p);
		const auto fromRect = [&] {
			if (_image.width() > _image.height()) {
				const auto shift = (_image.width() - _image.height()) / 2;
				return QRect(shift, 0, _image.height(), _image.height());
			} else {
				const auto shift = (_image.height() - _image.width()) / 2;
				return QRect(0, shift, _image.width(), _image.width());
			}
		}();
		p.drawImage(QRect(left, top, _st.size, _st.size), _image, fromRect);
	}
	const auto availableWidth = countAvailableWidth();

	p.setPen(st::windowFg);
	_name.drawLeftElided(
		p,
		left + _st.textLeft,
		top + _st.nameTop,
		availableWidth,
		width());
	p.setPen(st::windowSubTextFg);
	_status.drawLeftElided(
		p,
		left + _st.textLeft,
		top + _st.statusTop,
		availableWidth,
		width());
}

EditScans::EditScans(
	QWidget *parent,
	not_null<PanelController*> controller,
	const QString &header,
	std::vector<ScanInfo> &&files,
	std::unique_ptr<ScanInfo> &&selfie)
: RpWidget(parent)
, _controller(controller)
, _files(std::move(files))
, _selfie(std::move(selfie))
, _content(this) {
	setupContent(header);
}

base::optional<int> EditScans::validateGetErrorTop() {
	const auto exists = ranges::find(
		_files,
		false,
		[](const ScanInfo &file) { return file.deleted; }) != end(_files);
	if (!exists) {
		toggleError(true);
		return (_files.size() > 5) ? _upload->y() : _header->y();
	}
	if (_selfie && (!_selfie->key.id || _selfie->deleted)) {
		toggleSelfieError(true);
		return _selfieHeader->y();
	}
	return base::none;
}

void EditScans::setupContent(const QString &header) {
	const auto inner = _content.data();
	inner->move(0, 0);

	_divider = inner->add(
		object_ptr<Ui::SlideWrap<BoxContentDivider>>(
			inner,
			object_ptr<BoxContentDivider>(
				inner,
				st::passportFormDividerHeight)));
	_divider->toggle(_files.empty(), anim::type::instant);

	_header = inner->add(
		object_ptr<Ui::SlideWrap<Ui::FlatLabel>>(
			inner,
			object_ptr<Ui::FlatLabel>(
				inner,
				header,
				Ui::FlatLabel::InitType::Simple,
				st::passportFormHeader),
			st::passportUploadHeaderPadding));
	_header->toggle(!_files.empty(), anim::type::instant);

	_wrap = inner->add(object_ptr<Ui::VerticalLayout>(inner));
	for (const auto &scan : _files) {
		pushScan(scan);
		_rows.back()->show(anim::type::instant);
	}

	_upload = inner->add(
		object_ptr<Info::Profile::Button>(
			inner,
			_uploadTexts.events_starting_with(
				uploadButtonText()
			) | rpl::flatten_latest(),
			st::passportUploadButton),
		st::passportUploadButtonPadding);
	_upload->addClickHandler([=] {
		chooseScan();
	});

	inner->add(object_ptr<BoxContentDivider>(
		inner,
		st::passportFormDividerHeight));

	if (_selfie) {
		_selfieHeader = inner->add(
			object_ptr<Ui::SlideWrap<Ui::FlatLabel>>(
				inner,
				object_ptr<Ui::FlatLabel>(
					inner,
					lang(lng_passport_selfie_title),
					Ui::FlatLabel::InitType::Simple,
					st::passportFormHeader),
				st::passportUploadHeaderPadding));
		_selfieHeader->toggle(_selfie->key.id != 0, anim::type::instant);
		_selfieWrap = inner->add(object_ptr<Ui::VerticalLayout>(inner));
		if (_selfie->key.id) {
			createSelfieRow(*_selfie);
		}
		_selfieUpload = inner->add(
			object_ptr<Info::Profile::Button>(
				inner,
				Lang::Viewer(
					lng_passport_upload_selfie
				) | Info::Profile::ToUpperValue(),
				st::passportUploadButton),
			st::passportUploadButtonPadding);
		_selfieUpload->addClickHandler([=] {
			chooseSelfie();
		});

		inner->add(object_ptr<PanelLabel>(
			inner,
			object_ptr<Ui::FlatLabel>(
				_content,
				lang(lng_passport_selfie_description),
				Ui::FlatLabel::InitType::Simple,
				st::passportFormLabel),
			st::passportFormLabelPadding));
	}

	_controller->scanUpdated(
	) | rpl::start_with_next([=](ScanInfo &&info) {
		updateScan(std::move(info));
	}, lifetime());

	widthValue(
	) | rpl::start_with_next([=](int width) {
		_content->resizeToWidth(width);
	}, _content->lifetime());

	_content->heightValue(
	) | rpl::start_with_next([=](int height) {
		resize(width(), height);
	}, _content->lifetime());
}

void EditScans::updateScan(ScanInfo &&info) {
	const auto updateRow = [&](
			not_null<ScanButton*> button,
			const ScanInfo &info) {
		button->setStatus(info.status);
		button->setImage(info.thumb);
		button->setDeleted(info.deleted);
	};
	if (info.selfie) {
		Assert(info.key.id != 0);
		Assert(_selfie != nullptr);
		if (_selfie->key.id) {
			updateRow(_selfieRow->entity(), info);
			if (!info.deleted) {
				hideSelfieError();
			}
		} else {
			createSelfieRow(info);
			_selfieWrap->resizeToWidth(width());
			_selfieRow->show(anim::type::normal);
			_selfieHeader->show(anim::type::normal);
		}
		*_selfie = std::move(info);
		return;
	}
	const auto i = ranges::find(_files, info.key, [](const ScanInfo &file) {
		return file.key;
	});
	if (i != _files.end()) {
		*i = std::move(info);
		const auto scan = _rows[i - _files.begin()]->entity();
		scan->setStatus(i->status);
		scan->setImage(i->thumb);
		scan->setDeleted(i->deleted);
		if (!i->deleted) {
			hideError();
		}
	} else {
		_files.push_back(std::move(info));
		pushScan(_files.back());
		_wrap->resizeToWidth(width());
		_rows.back()->show(anim::type::normal);
		_divider->hide(anim::type::normal);
		_header->show(anim::type::normal);
		_uploadTexts.fire(uploadButtonText());
	}
}

void EditScans::createSelfieRow(const ScanInfo &info) {
	_selfieRow = createScan(
		_selfieWrap,
		info,
		lang(lng_passport_selfie_name));
	const auto row = _selfieRow->entity();

	row->deleteClicks(
	) | rpl::start_with_next([=] {
		_controller->deleteSelfie();
	}, row->lifetime());

	row->restoreClicks(
	) | rpl::start_with_next([=] {
		_controller->restoreSelfie();
	}, row->lifetime());

	hideSelfieError();
}

void EditScans::pushScan(const ScanInfo &info) {
	const auto index = _rows.size();
	_rows.push_back(createScan(
		_wrap,
		info,
		lng_passport_scan_index(lt_index, QString::number(index + 1))));
	_rows.back()->hide(anim::type::instant);

	const auto scan = _rows.back()->entity();

	scan->deleteClicks(
	) | rpl::start_with_next([=] {
		_controller->deleteScan(index);
	}, scan->lifetime());

	scan->restoreClicks(
	) | rpl::start_with_next([=] {
		_controller->restoreScan(index);
	}, scan->lifetime());

	hideError();
}

base::unique_qptr<Ui::SlideWrap<ScanButton>> EditScans::createScan(
		not_null<Ui::VerticalLayout*> parent,
		const ScanInfo &info,
		const QString &name) {
	auto result = base::unique_qptr<Ui::SlideWrap<ScanButton>>(
		parent->add(object_ptr<Ui::SlideWrap<ScanButton>>(
			parent,
			object_ptr<ScanButton>(
				parent,
				st::passportScanRow,
				name,
				info.status,
				info.deleted))));
	result->entity()->setImage(info.thumb);
	return result;
}

void EditScans::chooseScan() {
	if (!_controller->canAddScan()) {
		_controller->showToast(lang(lng_passport_scans_limit_reached));
		return;
	}
	ChooseScan(this, [=](QByteArray &&content) {
		_controller->uploadScan(std::move(content));
	});
}

void EditScans::chooseSelfie() {
	ChooseScan(this, [=](QByteArray &&content) {
		_controller->uploadSelfie(std::move(content));
	});
}

void EditScans::ChooseScan(
		QPointer<QWidget> parent,
		base::lambda<void(QByteArray&&)> callback) {
	Expects(parent != nullptr);

	const auto filter = FileDialog::AllFilesFilter()
		+ qsl(";;Image files (*")
		+ cImgExtensions().join(qsl(" *"))
		+ qsl(")");
	const auto guardedCallback = base::lambda_guarded(parent, callback);
	const auto processFile = [=](FileDialog::OpenResult &&result) {
		if (result.paths.size() == 1) {
			auto content = [&] {
				QFile f(result.paths.front());
				if (!f.open(QIODevice::ReadOnly)) {
					return QByteArray();
				}
				return f.readAll();
			}();
			if (!content.isEmpty()) {
				guardedCallback(std::move(content));
			}
		} else if (!result.remoteContent.isEmpty()) {
			guardedCallback(std::move(result.remoteContent));
		}
	};
	FileDialog::GetOpenPath(
		parent,
		lang(lng_passport_choose_image),
		filter,
		processFile);
}

rpl::producer<QString> EditScans::uploadButtonText() const {
	return Lang::Viewer(_files.empty()
		? lng_passport_upload_scans
		: lng_passport_upload_more) | Info::Profile::ToUpperValue();
}

void EditScans::hideError() {
	toggleError(false);
}

void EditScans::toggleError(bool shown) {
	if (_errorShown != shown) {
		_errorShown = shown;
		_errorAnimation.start(
			[=] { errorAnimationCallback(); },
			_errorShown ? 0. : 1.,
			_errorShown ? 1. : 0.,
			st::passportDetailsField.duration);
	}
}

void EditScans::errorAnimationCallback() {
	const auto error = _errorAnimation.current(_errorShown ? 1. : 0.);
	if (error == 0.) {
		_upload->setColorOverride(base::none);
	} else {
		_upload->setColorOverride(anim::color(
			st::passportUploadButton.textFg,
			st::boxTextFgError,
			error));
	}
}

void EditScans::hideSelfieError() {
	toggleSelfieError(false);
}

void EditScans::toggleSelfieError(bool shown) {
	if (_selfieErrorShown != shown) {
		_selfieErrorShown = shown;
		_selfieErrorAnimation.start(
			[=] { selfieErrorAnimationCallback(); },
			_selfieErrorShown ? 0. : 1.,
			_selfieErrorShown ? 1. : 0.,
			st::passportDetailsField.duration);
	}
}

void EditScans::selfieErrorAnimationCallback() {
	const auto error = _selfieErrorAnimation.current(
		_selfieErrorShown ? 1. : 0.);
	if (error == 0.) {
		_selfieUpload->setColorOverride(base::none);
	} else {
		_selfieUpload->setColorOverride(anim::color(
			st::passportUploadButton.textFg,
			st::boxTextFgError,
			error));
	}
}

} // namespace Passport