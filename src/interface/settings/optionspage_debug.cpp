#include "../filezilla.h"
#include "../Options.h"
#include "settingsdialog.h"
#include "optionspage.h"
#include "optionspage_debug.h"
#include "../xrc_helper.h"

bool COptionsPageDebug::LoadPage()
{
	bool failure = false;

	SetCheckFromOption(XRCID("ID_DEBUGMENU"), OPTION_DEBUG_MENU, failure);
	xrc_call(*this, "ID_RAWLISTING", &wxCheckBox::SetValue, m_pOptions->get_bool(OPTION_LOGGING_RAWLISTING));
	SetChoice(XRCID("ID_DEBUGLEVEL"), m_pOptions->get_int(OPTION_LOGGING_DEBUGLEVEL), failure);

	return !failure;
}

bool COptionsPageDebug::SavePage()
{
	SetOptionFromCheck(XRCID("ID_DEBUGMENU"), OPTION_DEBUG_MENU);
	m_pOptions->set(OPTION_LOGGING_RAWLISTING, xrc_call(*this, "ID_RAWLISTING", &wxCheckBox::GetValue));
	m_pOptions->set(OPTION_LOGGING_DEBUGLEVEL, GetChoice(XRCID("ID_DEBUGLEVEL")));

	return true;
}

bool COptionsPageDebug::Validate()
{
	return true;
}
