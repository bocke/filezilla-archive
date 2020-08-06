#include "../filezilla.h"
#include "../Options.h"
#include "settingsdialog.h"
#include "optionspage.h"

bool COptionsPage::CreatePage(COptions* pOptions, CSettingsDialog* pOwner, wxWindow* parent, wxSize& maxSize)
{
	m_pOwner = pOwner;
	m_pOptions = pOptions;

	if (!CreateControls(parent)) {
		return false;
	}

	UpdateMaxPageSize(maxSize);

	return true;
}

bool COptionsPage::CreateControls(wxWindow* parent)
{
	return wxXmlResource::Get()->LoadPanel(this, parent, GetResourceName());
}

void COptionsPage::UpdateMaxPageSize(wxSize& maxSize)
{
	wxSize size = GetSize();

#ifdef __WXGTK__
	// wxStaticBox draws its own border coords -1.
	// Adjust this window so that the left border is fully visible.
	Move(1, 0);
	size.x += 1;
#endif

	if (size.GetWidth() > maxSize.GetWidth()) {
		maxSize.SetWidth(size.GetWidth());
	}
	if (size.GetHeight() > maxSize.GetHeight()) {
		maxSize.SetHeight(size.GetHeight());
	}
}

void COptionsPage::SetRCheck(int id, bool checked, bool& failure)
{
	auto pRadioButton = dynamic_cast<wxRadioButton*>(FindWindow(id));
	if (!pRadioButton) {
		failure = true;
		return;
	}

	pRadioButton->SetValue(checked);
}

bool COptionsPage::GetRCheck(int id) const
{
	auto pRadioButton = dynamic_cast<wxRadioButton*>(FindWindow(id));
	wxASSERT(pRadioButton);

	return pRadioButton ? pRadioButton->GetValue() : false;
}

void COptionsPage::ReloadSettings()
{
	m_pOwner->LoadSettings();
}

bool COptionsPage::DisplayError(wxString const& controlToFocus, wxString const& error)
{
	int id = wxXmlResource::GetXRCID(controlToFocus);
	if (id == -1) {
		DisplayError(0, error);
	}
	else {
		DisplayError(FindWindow(id), error);
	}

	return false;
}

bool COptionsPage::DisplayError(wxWindow* pWnd, wxString const& error)
{
	if (pWnd) {
		pWnd->SetFocus();
	}

	wxMessageBoxEx(error, _("Failed to validate settings"), wxICON_EXCLAMATION, this);

	return false;
}

bool COptionsPage::Display()
{
	if (!m_was_selected) {
		if (!OnDisplayedFirstTime()) {
			return false;
		}
		m_was_selected = true;
	}
	Show();

	return true;
}

bool COptionsPage::OnDisplayedFirstTime()
{
	return true;
}
