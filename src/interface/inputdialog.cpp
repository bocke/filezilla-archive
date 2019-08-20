#include <filezilla.h>
#include "inputdialog.h"
#include "textctrlex.h"

BEGIN_EVENT_TABLE(CInputDialog, wxDialogEx)
EVT_TEXT(XRCID("ID_STRING"), CInputDialog::OnValueChanged)
END_EVENT_TABLE()

bool CInputDialog::Create(wxWindow* parent, wxString const& title, wxString const& text, int max_len, bool password)
{
	SetParent(parent);

	if (!wxDialogEx::Create(parent, -1, title)) {
		return false;
	}

	auto& lay = layout();
	auto * main = lay.createMain(this, 1);

	main->Add(new wxStaticText(this, -1, text));

	m_pTextCtrl = new wxTextCtrlEx(this, XRCID("ID_STRING"), wxString(), wxDefaultPosition, wxDefaultSize, password ? wxTE_PASSWORD : 0);
	main->Add(m_pTextCtrl, lay.grow);
	if (max_len != -1) {
		m_pTextCtrl->SetMaxLength(max_len);
	}

	auto * buttons = lay.createButtonSizer(this, main, true);

	auto ok = new wxButton(this, wxID_OK, _("&OK"));
	ok->SetDefault();
	buttons->AddButton(ok);

	auto cancel = new wxButton(this, wxID_CANCEL, _("&Cancel"));
	buttons->AddButton(cancel);
	buttons->Realize();

	Bind(wxEVT_BUTTON, [this](wxEvent& evt) {EndModal(evt.GetId()); });

	WrapRecursive(this, 2.0);

	m_pTextCtrl->SetFocus();
	ok->Disable();

	return true;
}

void CInputDialog::AllowEmpty(bool allowEmpty)
{
	m_allowEmpty = allowEmpty;
	XRCCTRL(*this, "wxID_OK", wxButton)->Enable(m_allowEmpty ? true : (!m_pTextCtrl->GetValue().empty()));
}

void CInputDialog::OnValueChanged(wxCommandEvent&)
{
	wxString value = m_pTextCtrl->GetValue();
	XRCCTRL(*this, "wxID_OK", wxButton)->Enable(m_allowEmpty ? true : !value.empty());
}

void CInputDialog::SetValue(wxString const& value)
{
	m_pTextCtrl->SetValue(value);
}

wxString CInputDialog::GetValue() const
{
	return m_pTextCtrl->GetValue();
}

bool CInputDialog::SelectText(int start, int end)
{
#ifdef __WXGTK__
	Show();
#endif
	m_pTextCtrl->SetFocus();
	m_pTextCtrl->SetSelection(start, end);
	return true;
}

bool CInputDialog::SetPasswordMode(bool password)
{
	if (password) {
		m_pTextCtrl = XRCCTRL(*this, "ID_STRING_PW", wxTextCtrl);
		m_pTextCtrl->Show();
		XRCCTRL(*this, "ID_STRING", wxTextCtrl)->Hide();
	}
	else {
		m_pTextCtrl = XRCCTRL(*this, "ID_STRING", wxTextCtrl);
		m_pTextCtrl->Show();
		XRCCTRL(*this, "ID_STRING_PW", wxTextCtrl)->Hide();
	}
	return true;
}
