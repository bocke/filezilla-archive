#include "../filezilla.h"

#include "../Options.h"
#include "settingsdialog.h"
#include "optionspage.h"
#include "optionspage_ftpproxy.h"
#include "../xrc_helper.h"

BEGIN_EVENT_TABLE(COptionsPageFtpProxy, COptionsPageFtpProxy::COptionsPage)
EVT_RADIOBUTTON(XRCID("ID_PROXYTYPE_NONE"), COptionsPageFtpProxy::OnProxyTypeChanged)
EVT_RADIOBUTTON(XRCID("ID_PROXYTYPE_USER"), COptionsPageFtpProxy::OnProxyTypeChanged)
EVT_RADIOBUTTON(XRCID("ID_PROXYTYPE_SITE"), COptionsPageFtpProxy::OnProxyTypeChanged)
EVT_RADIOBUTTON(XRCID("ID_PROXYTYPE_OPEN"), COptionsPageFtpProxy::OnProxyTypeChanged)
EVT_RADIOBUTTON(XRCID("ID_PROXYTYPE_CUSTOM"), COptionsPageFtpProxy::OnProxyTypeChanged)
EVT_TEXT(XRCID("ID_LOGINSEQUENCE"), COptionsPageFtpProxy::OnLoginSequenceChanged)
END_EVENT_TABLE()

bool COptionsPageFtpProxy::LoadPage()
{
	bool failure = false;

	xrc_call(*this, "ID_PROXY_HOST", &wxTextCtrl::ChangeValue, m_pOptions->get_string(OPTION_FTP_PROXY_HOST));
	xrc_call(*this, "ID_PROXY_USER", &wxTextCtrl::ChangeValue, m_pOptions->get_string(OPTION_FTP_PROXY_USER));
	xrc_call(*this, "ID_PROXY_PASS", &wxTextCtrl::ChangeValue, m_pOptions->get_string(OPTION_FTP_PROXY_PASS));

	int type = m_pOptions->get_int(OPTION_FTP_PROXY_TYPE);
	switch (type)
	{
	default:
	case 0:
		SetRCheck(XRCID("ID_PROXYTYPE_NONE"), true, failure);
		break;
	case 1:
		SetRCheck(XRCID("ID_PROXYTYPE_USER"), true, failure);
		break;
	case 2:
		SetRCheck(XRCID("ID_PROXYTYPE_SITE"), true, failure);
		break;
	case 3:
		SetRCheck(XRCID("ID_PROXYTYPE_OPEN"), true, failure);
		break;
	case 4:
		SetRCheck(XRCID("ID_PROXYTYPE_CUSTOM"), true, failure);
		xrc_call(*this, "ID_LOGINSEQUENCE", &wxTextCtrl::ChangeValue, m_pOptions->get_string(OPTION_FTP_PROXY_CUSTOMLOGINSEQUENCE));
		break;
	}

	if (!failure) {
		SetCtrlState();
	}

	return !failure;
}

bool COptionsPageFtpProxy::SavePage()
{
	m_pOptions->set(OPTION_FTP_PROXY_HOST, xrc_call(*this, "ID_PROXY_HOST", &wxTextCtrl::GetValue).ToStdWstring());
	m_pOptions->set(OPTION_FTP_PROXY_USER, xrc_call(*this, "ID_PROXY_USER", &wxTextCtrl::GetValue).ToStdWstring());
	m_pOptions->set(OPTION_FTP_PROXY_PASS, xrc_call(*this, "ID_PROXY_PASS", &wxTextCtrl::GetValue).ToStdWstring());

	int type = 0;
	if (GetRCheck(XRCID("ID_PROXYTYPE_USER"))) {
		type = 1;
	}
	else if (GetRCheck(XRCID("ID_PROXYTYPE_SITE"))) {
		type = 2;
	}
	else if (GetRCheck(XRCID("ID_PROXYTYPE_OPEN"))) {
		type = 3;
	}
	else if (GetRCheck(XRCID("ID_PROXYTYPE_CUSTOM"))) {
		m_pOptions->set(OPTION_FTP_PROXY_CUSTOMLOGINSEQUENCE, xrc_call(*this, "ID_LOGINSEQUENCE", &wxTextCtrl::GetValue).ToStdWstring());
		type = 4;
	}
	m_pOptions->set(OPTION_FTP_PROXY_TYPE, type);

	return true;
}

bool COptionsPageFtpProxy::Validate()
{
	if (!XRCCTRL(*this, "ID_PROXYTYPE_NONE", wxRadioButton)->GetValue()) {
		wxTextCtrl* pTextCtrl = XRCCTRL(*this, "ID_PROXY_HOST", wxTextCtrl);
		if (pTextCtrl->GetValue().empty()) {
			return DisplayError(_T("ID_PROXY_HOST"), _("You need to enter a proxy host."));
		}
	}

	if (XRCCTRL(*this, "ID_PROXYTYPE_CUSTOM", wxRadioButton)->GetValue()) {
		wxTextCtrl* pTextCtrl = XRCCTRL(*this, "ID_LOGINSEQUENCE", wxTextCtrl);
		if (pTextCtrl->GetValue().empty()) {
			return DisplayError(_T("ID_LOGINSEQUENCE"), _("The custom login sequence cannot be empty."));
		}
	}

	return true;
}

void COptionsPageFtpProxy::SetCtrlState()
{
	wxTextCtrl* pTextCtrl = XRCCTRL(*this, "ID_LOGINSEQUENCE", wxTextCtrl);
	if (!pTextCtrl) {
		return;
	}

	if (XRCCTRL(*this, "ID_PROXYTYPE_NONE", wxRadioButton)->GetValue()) {
		pTextCtrl->ChangeValue(wxString());
		pTextCtrl->Enable(false);
		pTextCtrl->SetEditable(false);
#ifdef __WXMSW__
		pTextCtrl->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
#endif

		XRCCTRL(*this, "ID_PROXY_HOST", wxTextCtrl)->Enable(false);
		XRCCTRL(*this, "ID_PROXY_USER", wxTextCtrl)->Enable(false);
		XRCCTRL(*this, "ID_PROXY_PASS", wxTextCtrl)->Enable(false);
		return;
	}

	pTextCtrl->Enable(true);
	pTextCtrl->SetEditable(true);
#ifdef __WXMSW__
	pTextCtrl->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
#endif

	XRCCTRL(*this, "ID_PROXY_HOST", wxTextCtrl)->Enable(true);
	XRCCTRL(*this, "ID_PROXY_USER", wxTextCtrl)->Enable(true);
	XRCCTRL(*this, "ID_PROXY_PASS", wxTextCtrl)->Enable(true);

	if (XRCCTRL(*this, "ID_PROXYTYPE_CUSTOM", wxRadioButton)->GetValue()) {
		return;
	}

	wxString loginSequence = _T("USER %s\nPASS %w\n");

	if (XRCCTRL(*this, "ID_PROXYTYPE_USER", wxRadioButton)->GetValue()) {
		loginSequence += _T("USER %u@%h\n");
	}
	else {
		if (XRCCTRL(*this, "ID_PROXYTYPE_SITE", wxRadioButton)->GetValue()) {
			loginSequence += _T("SITE %h\n");
		}
		else {
			loginSequence += _T("OPEN %h\n");
		}
		loginSequence += _T("USER %u\n");
	}

	loginSequence += _T("PASS %p\nACCT %a");

	pTextCtrl->ChangeValue(loginSequence);
}

void COptionsPageFtpProxy::OnProxyTypeChanged(wxCommandEvent&)
{
	SetCtrlState();
}

void COptionsPageFtpProxy::OnLoginSequenceChanged(wxCommandEvent&)
{
	XRCCTRL(*this, "ID_PROXYTYPE_CUSTOM", wxRadioButton)->SetValue(true);
}
