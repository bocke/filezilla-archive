#include "../filezilla.h"
#include "../Options.h"
#include "settingsdialog.h"
#include "optionspage_interface.h"
#include "../Mainfrm.h"
#include "../power_management.h"
#include "../xrc_helper.h"
#include <libfilezilla/util.hpp>

#include <wx/statbox.h>

struct COptionsPageInterface::impl
{
	wxChoice* filepane_layout_{};
	wxChoice* messagelog_pos_{};
	wxCheckBox* swap_{};

	wxChoice* newconn_action_{};
};

COptionsPageInterface::COptionsPageInterface()
	: impl_(std::make_unique<impl>())
{}

COptionsPageInterface::~COptionsPageInterface()
{
}

bool COptionsPageInterface::CreateControls(wxWindow* parent)
{
	auto const& lay = m_pOwner->layout();

	Create(parent);
	auto main = lay.createFlex(1);
	main->AddGrowableCol(0);
	SetSizer(main);

	{
		auto [box, inner] = lay.createStatBox(main, _("Layout"), 1);

		auto rows = lay.createFlex(2);
		inner->Add(rows);
		rows->Add(new wxStaticText(box, nullID, _("&Layout of file and directory panes:")), lay.valign);
		impl_->filepane_layout_ = new wxChoice(box, nullID);
		impl_->filepane_layout_->Append(_("Classic"));
		impl_->filepane_layout_->Append(_("Explorer"));
		impl_->filepane_layout_->Append(_("Widescreen"));
		impl_->filepane_layout_->Append(_("Blackboard"));
		rows->Add(impl_->filepane_layout_, lay.valign);
		rows->Add(new wxStaticText(box, nullID, _("Message log positio&n:")), lay.valign);
		impl_->messagelog_pos_ = new wxChoice(box, nullID);
		impl_->messagelog_pos_->Append(_("Above the file lists"));
		impl_->messagelog_pos_->Append(_("Next to the transfer queue"));
		impl_->messagelog_pos_->Append(_("As tab in the transfer queue pane"));
		rows->Add(impl_->messagelog_pos_, lay.valign);
		impl_->swap_ = new wxCheckBox(box, nullID, _("&Swap local and remote panes"));
		inner->Add(impl_->swap_);

		impl_->filepane_layout_->Bind(wxEVT_CHOICE, &COptionsPageInterface::OnLayoutChange, this);
		impl_->messagelog_pos_->Bind(wxEVT_CHOICE, &COptionsPageInterface::OnLayoutChange, this);
		impl_->swap_->Bind(wxEVT_CHECKBOX, &COptionsPageInterface::OnLayoutChange, this);
	}

	{
		auto [box, inner] = lay.createStatBox(main, _("Behaviour"), 1);
		
#ifndef __WXMAC__
		inner->Add(new wxCheckBox(box, XRCID("ID_MINIMIZE_TRAY"), _("&Minimize to tray")));
#endif
		inner->Add(new wxCheckBox(box, XRCID("ID_PREVENT_IDLESLEEP"), _("P&revent system from entering idle sleep during transfers and other operations")));
		inner->AddSpacer(0);
		inner->Add(new wxStaticText(box, nullID, _("On startup of FileZilla:")));
		inner->Add(new wxRadioButton(box, XRCID("ID_INTERFACE_STARTUP_NORMAL"), _("S&tart normally"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP));
		inner->Add(new wxRadioButton(box, XRCID("ID_INTERFACE_STARTUP_SITEMANAGER"), _("S&how the Site Manager on startup")));
		inner->Add(new wxRadioButton(box, XRCID("ID_INTERFACE_STARTUP_RESTORE"), _("Restore ta&bs and reconnect")));
		inner->AddSpacer(0);
		inner->Add(new wxStaticText(box, nullID, _("When st&arting a new connection while already connected:")));
		impl_->newconn_action_ = new wxChoice(box, nullID);
		impl_->newconn_action_->Append(_("Ask for action"));
		impl_->newconn_action_->Append(_("Connect in new tab"));
		impl_->newconn_action_->Append(_("Connect in current tab"));
		inner->Add(impl_->newconn_action_);
	}

	{
		auto [box, inner] = lay.createStatBox(main, _("Transfer Queue"), 1);
		inner->Add(new wxCheckBox(box, XRCID("ID_SPEED_DISPLAY"), _("&Display momentary transfer speed instead of average speed")));
	}

	return true;
}

bool COptionsPageInterface::LoadPage()
{
	bool failure = false;

	impl_->filepane_layout_->SetSelection(m_pOptions->get_int(OPTION_FILEPANE_LAYOUT));
	impl_->messagelog_pos_->SetSelection(m_pOptions->get_int(OPTION_MESSAGELOG_POSITION));
	impl_->swap_->SetValue(m_pOptions->get_bool(OPTION_FILEPANE_SWAP));
	
#ifndef __WXMAC__
	SetCheckFromOption(XRCID("ID_MINIMIZE_TRAY"), OPTION_MINIMIZE_TRAY, failure);
#endif

	SetCheckFromOption(XRCID("ID_PREVENT_IDLESLEEP"), OPTION_PREVENT_IDLESLEEP, failure);

	SetCheckFromOption(XRCID("ID_SPEED_DISPLAY"), OPTION_SPEED_DISPLAY, failure);

	if (!CPowerManagement::IsSupported()) {
		XRCCTRL(*this, "ID_PREVENT_IDLESLEEP", wxCheckBox)->Hide();
	}

	int const startupAction = m_pOptions->get_int(OPTION_STARTUP_ACTION);
	switch (startupAction) {
	default:
		xrc_call(*this, "ID_INTERFACE_STARTUP_NORMAL", &wxRadioButton::SetValue, true);
		break;
	case 1:
		xrc_call(*this, "ID_INTERFACE_STARTUP_SITEMANAGER", &wxRadioButton::SetValue, true);
		break;
	case 2:
		xrc_call(*this, "ID_INTERFACE_STARTUP_RESTORE", &wxRadioButton::SetValue, true);
		break;
	}

	int action = m_pOptions->get_int(OPTION_ALREADYCONNECTED_CHOICE);
	if (action & 2) {
		action = 1 + (action & 1);
	}
	else {
		action = 0;
	}
	impl_->newconn_action_->SetSelection(action);

	m_pOwner->RememberOldValue(OPTION_MESSAGELOG_POSITION);
	m_pOwner->RememberOldValue(OPTION_FILEPANE_LAYOUT);
	m_pOwner->RememberOldValue(OPTION_FILEPANE_SWAP);

	return !failure;
}

bool COptionsPageInterface::SavePage()
{
	m_pOptions->set(OPTION_FILEPANE_LAYOUT, impl_->filepane_layout_->GetSelection());
	m_pOptions->set(OPTION_MESSAGELOG_POSITION, impl_->messagelog_pos_->GetSelection());
	m_pOptions->set(OPTION_FILEPANE_SWAP, impl_->swap_->GetValue());

#ifndef __WXMAC__
	SetOptionFromCheck(XRCID("ID_MINIMIZE_TRAY"), OPTION_MINIMIZE_TRAY);
#endif

	SetOptionFromCheck(XRCID("ID_PREVENT_IDLESLEEP"), OPTION_PREVENT_IDLESLEEP);

	SetOptionFromCheck(XRCID("ID_SPEED_DISPLAY"), OPTION_SPEED_DISPLAY);

	int startupAction = 0;
	if (xrc_call(*this, "ID_INTERFACE_STARTUP_SITEMANAGER", &wxRadioButton::GetValue)) {
		startupAction = 1;
	}
	else if (xrc_call(*this, "ID_INTERFACE_STARTUP_RESTORE", &wxRadioButton::GetValue)) {
		startupAction = 2;
	}
	m_pOptions->set(OPTION_STARTUP_ACTION, startupAction);

	int action = impl_->newconn_action_->GetSelection();
	if (!action) {
		action = m_pOptions->get_int(OPTION_ALREADYCONNECTED_CHOICE) & 1;
	}
	else {
		action += 1;
	}
	m_pOptions->set(OPTION_ALREADYCONNECTED_CHOICE, action);

	return true;
}

void COptionsPageInterface::OnLayoutChange(wxCommandEvent&)
{
	m_pOptions->set(OPTION_FILEPANE_LAYOUT, impl_->filepane_layout_->GetSelection());
	m_pOptions->set(OPTION_MESSAGELOG_POSITION, impl_->messagelog_pos_->GetSelection());
	m_pOptions->set(OPTION_FILEPANE_SWAP, impl_->swap_->GetValue());
}

