#include "../filezilla.h"

#include "../Options.h"
#include "settingsdialog.h"
#include "optionspage.h"
#include "optionspage_proxy.h"
#include "../xrc_helper.h"

BEGIN_EVENT_TABLE(COptionsPageProxy, COptionsPageProxy::COptionsPage)
EVT_RADIOBUTTON(XRCID("ID_PROXYTYPE_NONE"), COptionsPageProxy::OnProxyTypeChanged)
EVT_RADIOBUTTON(XRCID("ID_PROXYTYPE_HTTP"), COptionsPageProxy::OnProxyTypeChanged)
EVT_RADIOBUTTON(XRCID("ID_PROXYTYPE_SOCKS4"), COptionsPageProxy::OnProxyTypeChanged)
EVT_RADIOBUTTON(XRCID("ID_PROXYTYPE_SOCKS5"), COptionsPageProxy::OnProxyTypeChanged)
END_EVENT_TABLE()

bool COptionsPageProxy::LoadPage()
{
	bool failure = false;

	xrc_call(*this, "ID_PROXY_HOST", &wxTextCtrl::ChangeValue, m_pOptions->get_string(OPTION_PROXY_HOST));
	xrc_call(*this, "ID_PROXY_PORT", &wxTextCtrl::ChangeValue, m_pOptions->get_string(OPTION_PROXY_PORT));
	xrc_call(*this, "ID_PROXY_USER", &wxTextCtrl::ChangeValue, m_pOptions->get_string(OPTION_PROXY_USER));
	xrc_call(*this, "ID_PROXY_PASS", &wxTextCtrl::ChangeValue, m_pOptions->get_string(OPTION_PROXY_PASS));

	int type = m_pOptions->get_int(OPTION_PROXY_TYPE);
	switch (type)
	{
	default:
	case 0:
		SetRCheck(XRCID("ID_PROXYTYPE_NONE"), true, failure);
		break;
	case 1:
		SetRCheck(XRCID("ID_PROXYTYPE_HTTP"), true, failure);
		break;
	case 2:
		SetRCheck(XRCID("ID_PROXYTYPE_SOCKS5"), true, failure);
		break;
	case 3:
		SetRCheck(XRCID("ID_PROXYTYPE_SOCKS4"), true, failure);
		break;
	}

	if (!failure) {
		SetCtrlState();
	}

	return !failure;
}

bool COptionsPageProxy::SavePage()
{
	m_pOptions->set(OPTION_PROXY_HOST, xrc_call(*this, "ID_PROXY_HOST", &wxTextCtrl::GetValue).ToStdWstring());
	m_pOptions->set(OPTION_PROXY_PORT, xrc_call(*this, "ID_PROXY_PORT", &wxTextCtrl::GetValue).ToStdWstring());
	m_pOptions->set(OPTION_PROXY_USER, xrc_call(*this, "ID_PROXY_USER", &wxTextCtrl::GetValue).ToStdWstring());
	m_pOptions->set(OPTION_PROXY_PASS, xrc_call(*this, "ID_PROXY_PASS", &wxTextCtrl::GetValue).ToStdWstring());

	int type;
	if (GetRCheck(XRCID("ID_PROXYTYPE_HTTP"))) {
		type = 1;
	}
	else if (GetRCheck(XRCID("ID_PROXYTYPE_SOCKS5"))) {
		type = 2;
	}
	else if (GetRCheck(XRCID("ID_PROXYTYPE_SOCKS4"))) {
		type = 3;
	}
	else {
		type = 0;
	}
	m_pOptions->set(OPTION_PROXY_TYPE, type);

	return true;
}

bool COptionsPageProxy::Validate()
{
	if (XRCCTRL(*this, "ID_PROXYTYPE_NONE", wxRadioButton)->GetValue()) {
		return true;
	}

	wxTextCtrl* pTextCtrl = XRCCTRL(*this, "ID_PROXY_HOST", wxTextCtrl);
	wxString host = pTextCtrl->GetValue();
	host.Trim(false);
	host.Trim(true);
	if (host.empty()) {
		return DisplayError(_T("ID_PROXY_HOST"), _("You need to enter a proxy host."));
	}
	else {
		pTextCtrl->ChangeValue(host);
	}

	pTextCtrl = XRCCTRL(*this, "ID_PROXY_PORT", wxTextCtrl);
	unsigned long port;
	if (!pTextCtrl->GetValue().ToULong(&port) || port < 1 || port > 65536) {
		return DisplayError(_T("ID_PROXY_PORT"), _("You need to enter a proxy port in the range from 1 to 65535"));
	}

	return true;
}

void COptionsPageProxy::SetCtrlState()
{
	bool enabled = XRCCTRL(*this, "ID_PROXYTYPE_NONE", wxRadioButton)->GetValue() == 0;
	bool enabled_auth = XRCCTRL(*this, "ID_PROXYTYPE_SOCKS4", wxRadioButton)->GetValue() == 0;

	XRCCTRL(*this, "ID_PROXY_HOST", wxTextCtrl)->Enable(enabled);
	XRCCTRL(*this, "ID_PROXY_PORT", wxTextCtrl)->Enable(enabled);
	XRCCTRL(*this, "ID_PROXY_USER", wxTextCtrl)->Enable(enabled && enabled_auth);
	XRCCTRL(*this, "ID_PROXY_PASS", wxTextCtrl)->Enable(enabled && enabled_auth);
}

void COptionsPageProxy::OnProxyTypeChanged(wxCommandEvent&)
{
	SetCtrlState();
}
