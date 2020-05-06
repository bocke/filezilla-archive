#include <filezilla.h>
#include "../Options.h"
#include "settingsdialog.h"
#include "optionspage.h"
#include "optionspage_connection_ftp.h"
#include "../netconfwizard.h"

BEGIN_EVENT_TABLE(COptionsPageConnectionFTP, COptionsPage)
EVT_BUTTON(XRCID("ID_RUNWIZARD"), COptionsPageConnectionFTP::OnWizard)
END_EVENT_TABLE()

bool COptionsPageConnectionFTP::LoadPage()
{
	bool failure = false;

	const bool use_pasv = m_pOptions->GetOptionVal(OPTION_USEPASV) != 0;
	SetRCheck(XRCID("ID_PASSIVE"), use_pasv, failure);
	SetRCheck(XRCID("ID_ACTIVE"), !use_pasv, failure);
	SetCheckFromOption(XRCID("ID_FALLBACK"), OPTION_ALLOW_TRANSFERMODEFALLBACK, failure);
	SetCheckFromOption(XRCID("ID_USEKEEPALIVE"), OPTION_FTP_SENDKEEPALIVE, failure);
	return !failure;
}

bool COptionsPageConnectionFTP::SavePage()
{
	m_pOptions->SetOption(OPTION_USEPASV, GetRCheck(XRCID("ID_PASSIVE")) ? 1 : 0);
	SetOptionFromCheck(XRCID("ID_FALLBACK"), OPTION_ALLOW_TRANSFERMODEFALLBACK);
	SetOptionFromCheck(XRCID("ID_USEKEEPALIVE"), OPTION_FTP_SENDKEEPALIVE);
	return true;
}

void COptionsPageConnectionFTP::OnWizard(wxCommandEvent&)
{
	CNetConfWizard wizard(GetParent(), m_pOptions, m_pOwner->GetEngineContext());
	if (!wizard.Load()) {
		return;
	}
	if (wizard.Run()) {
		ReloadSettings();
	}
}
