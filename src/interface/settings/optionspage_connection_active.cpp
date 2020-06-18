#include "../filezilla.h"
#include "../Options.h"
#include "settingsdialog.h"
#include "optionspage.h"
#include "optionspage_connection_active.h"
#include "xrc_helper.h"

#include <libfilezilla/iputils.hpp>

BEGIN_EVENT_TABLE(COptionsPageConnectionActive, COptionsPage)
EVT_CHECKBOX(XRCID("ID_LIMITPORTS"), COptionsPageConnectionActive::OnRadioOrCheckEvent)
EVT_RADIOBUTTON(XRCID("ID_ACTIVEMODE1"), COptionsPageConnectionActive::OnRadioOrCheckEvent)
EVT_RADIOBUTTON(XRCID("ID_ACTIVEMODE2"), COptionsPageConnectionActive::OnRadioOrCheckEvent)
EVT_RADIOBUTTON(XRCID("ID_ACTIVEMODE3"), COptionsPageConnectionActive::OnRadioOrCheckEvent)
END_EVENT_TABLE()

bool COptionsPageConnectionActive::LoadPage()
{
	bool failure = false;
	xrc_call(*this, "ID_LIMITPORTS", &wxCheckBox::SetValue, m_pOptions->get_bool(OPTION_LIMITPORTS));
	xrc_call(*this, "ID_LOWESTPORT", &wxTextCtrl::ChangeValue, m_pOptions->get_string(OPTION_LIMITPORTS_LOW));
	xrc_call(*this, "ID_HIGHESTPORT", &wxTextCtrl::ChangeValue, m_pOptions->get_string(OPTION_LIMITPORTS_HIGH));

	SetRCheck(XRCID("ID_ACTIVEMODE1"), m_pOptions->get_int(OPTION_EXTERNALIPMODE) == 0, failure);
	SetRCheck(XRCID("ID_ACTIVEMODE2"), m_pOptions->get_int(OPTION_EXTERNALIPMODE) == 1, failure);
	SetRCheck(XRCID("ID_ACTIVEMODE3"), m_pOptions->get_int(OPTION_EXTERNALIPMODE) == 2, failure);

	xrc_call(*this, "ID_ACTIVEIP", &wxTextCtrl::ChangeValue, m_pOptions->get_string(OPTION_EXTERNALIP));
	xrc_call(*this, "ID_ACTIVERESOLVER", &wxTextCtrl::ChangeValue, m_pOptions->get_string(OPTION_EXTERNALIPRESOLVER));
	xrc_call(*this, "ID_NOEXTERNALONLOCAL", &wxCheckBox::SetValue, m_pOptions->get_bool(OPTION_NOEXTERNALONLOCAL));

	if (!failure) {
		SetCtrlState();
	}

	return !failure;
}

bool COptionsPageConnectionActive::SavePage()
{
	m_pOptions->set(OPTION_LIMITPORTS, xrc_call(*this, "ID_LIMITPORTS", &wxCheckBox::GetValue) ? 1 : 0);

	m_pOptions->set(OPTION_LIMITPORTS_LOW, xrc_call(*this, "ID_LOWESTPORT", &wxTextCtrl::GetValue).ToStdWstring());
	m_pOptions->set(OPTION_LIMITPORTS_HIGH, xrc_call(*this, "ID_HIGHESTPORT", &wxTextCtrl::GetValue).ToStdWstring());

	int mode;
	if (GetRCheck(XRCID("ID_ACTIVEMODE1"))) {
		mode = 0;
	}
	else {
		mode = GetRCheck(XRCID("ID_ACTIVEMODE2")) ? 1 : 2;
	}
	m_pOptions->set(OPTION_EXTERNALIPMODE, mode);

	if (mode == 1) {
		m_pOptions->set(OPTION_EXTERNALIP, xrc_call(*this, "ID_ACTIVEIP", &wxTextCtrl::GetValue).ToStdWstring());
	}
	else if (mode == 2) {
		m_pOptions->set(OPTION_EXTERNALIPRESOLVER, xrc_call(*this, "ID_ACTIVERESOLVER", &wxTextCtrl::GetValue).ToStdWstring());
	}
	m_pOptions->set(OPTION_NOEXTERNALONLOCAL, xrc_call(*this, "ID_NOEXTERNALONLOCAL", &wxCheckBox::GetValue) ? 1 : 0);

	return true;
}

bool COptionsPageConnectionActive::Validate()
{
	// Validate port limiting settings
	if (GetCheck(XRCID("ID_LIMITPORTS"))) {
		wxTextCtrl* pLow = XRCCTRL(*this, "ID_LOWESTPORT", wxTextCtrl);

		long low;
		if (!pLow->GetValue().ToLong(&low) || low < 1024 || low > 65535) {
			return DisplayError(pLow, _("Lowest available port has to be a number between 1024 and 65535."));
		}

		wxTextCtrl* pHigh = XRCCTRL(*this, "ID_LOWESTPORT", wxTextCtrl);

		long high;
		if (!pHigh->GetValue().ToLong(&high) || high < 1024 || high > 65535) {
			return DisplayError(pHigh, _("Highest available port has to be a number between 1024 and 65535."));
		}

		if (low > high) {
			return DisplayError(pLow, _("The lowest available port has to be less or equal than the highest available port."));
		}
	}

	int mode;
	if (GetRCheck(XRCID("ID_ACTIVEMODE1"))) {
		mode = 0;
	}
	else {
		mode = GetRCheck(XRCID("ID_ACTIVEMODE2")) ? 1 : 2;
	}

	if (mode == 1) {
		wxTextCtrl* pActiveIP = XRCCTRL(*this, "ID_ACTIVEIP", wxTextCtrl);
		wxString const ip = pActiveIP->GetValue();
		if (fz::get_address_type(ip.ToStdWstring()) != fz::address_type::ipv4) {
			return DisplayError(pActiveIP, _("You have to enter a valid IPv4 address."));
		}
	}

	return true;
}

void COptionsPageConnectionActive::SetCtrlState()
{
	FindWindow(XRCID("ID_LOWESTPORT"))->Enable(GetCheck(XRCID("ID_LIMITPORTS")));
	FindWindow(XRCID("ID_HIGHESTPORT"))->Enable(GetCheck(XRCID("ID_LIMITPORTS")));

	int mode;
	if (GetRCheck(XRCID("ID_ACTIVEMODE1"))) {
		mode = 0;
	}
	else {
		mode = GetRCheck(XRCID("ID_ACTIVEMODE2")) ? 1 : 2;
	}
	FindWindow(XRCID("ID_ACTIVEIP"))->Enable(mode == 1);
	FindWindow(XRCID("ID_ACTIVERESOLVER"))->Enable(mode == 2);

	FindWindow(XRCID("ID_NOEXTERNALONLOCAL"))->Enable(mode != 0);
}

void COptionsPageConnectionActive::OnRadioOrCheckEvent(wxCommandEvent&)
{
	SetCtrlState();
}
