#include <filezilla.h>

#include "../Options.h"
#include "file_utils.h"
#include "settingsdialog.h"
#include "optionspage.h"
#include "optionspage_edit.h"

#include <wx/filedlg.h>

BEGIN_EVENT_TABLE(COptionsPageEdit, COptionsPage)
EVT_BUTTON(XRCID("ID_BROWSE"), COptionsPageEdit::OnBrowseEditor)
EVT_RADIOBUTTON(wxID_ANY, COptionsPageEdit::OnRadioButton)
END_EVENT_TABLE()

bool COptionsPageEdit::LoadPage()
{
	bool failure = false;

	COptions* pOptions = COptions::Get();

	std::wstring editor = pOptions->GetOption(OPTION_EDIT_DEFAULTEDITOR);
	if (editor.empty() || editor[0] == '0') {
		SetRCheck(XRCID("ID_DEFAULT_NONE"), true, failure);
	}
	else if (editor[0] == '1') {
		SetRCheck(XRCID("ID_DEFAULT_TEXT"), true, failure);
	}
	else {
		if (editor[0] == '2') {
			editor = editor.substr(1);
		}

		SetRCheck(XRCID("ID_DEFAULT_CUSTOM"), true, failure);
		SetText(XRCID("ID_EDITOR"), editor, failure);
	}

	if (pOptions->GetOptionVal(OPTION_EDIT_ALWAYSDEFAULT)) {
		SetRCheck(XRCID("ID_USEDEFAULT"), true, failure);
	}
	else {
		SetRCheck(XRCID("ID_USEASSOCIATIONS"), true, failure);
	}

	SetCheckFromOption(XRCID("ID_EDIT_TRACK_LOCAL"), OPTION_EDIT_TRACK_LOCAL, failure);

	if (!failure) {
		SetCtrlState();
	}

	return !failure;
}

bool COptionsPageEdit::SavePage()
{
	COptions* pOptions = COptions::Get();

	if (GetRCheck(XRCID("ID_DEFAULT_CUSTOM"))) {
		pOptions->SetOption(OPTION_EDIT_DEFAULTEDITOR, _T("2") + GetText(XRCID("ID_EDITOR")));
	}
	else {
		pOptions->SetOption(OPTION_EDIT_DEFAULTEDITOR, GetRCheck(XRCID("ID_DEFAULT_TEXT")) ? _T("1") : _T("0"));
	}

	if (GetRCheck(XRCID("ID_USEDEFAULT"))) {
		pOptions->SetOption(OPTION_EDIT_ALWAYSDEFAULT, 1);
	}
	else {
		pOptions->SetOption(OPTION_EDIT_ALWAYSDEFAULT, 0);
	}

	SetOptionFromCheck(XRCID("ID_EDIT_TRACK_LOCAL"), OPTION_EDIT_TRACK_LOCAL);

	return true;
}

bool COptionsPageEdit::Validate()
{
	const bool custom = GetRCheck(XRCID("ID_DEFAULT_CUSTOM"));
	std::wstring editor;
	if (custom) {
		bool failure = false;

		editor = fz::trimmed(GetText(XRCID("ID_EDITOR")));
		SetText(XRCID("EDITOR"), editor, failure);

		if (!editor.empty()) {
			auto cmd_with_args = UnquoteCommand(editor);
			if (cmd_with_args.empty()) {
				return DisplayError(_T("ID_EDITOR"), _("Default editor not properly quoted."));
			}

			if (!ProgramExists(cmd_with_args[0])) {
				return DisplayError(_T("ID_EDITOR"), _("The file selected as default editor does not exist."));
			}

			SetText(XRCID("EDITOR"), QuoteCommand(cmd_with_args), failure);
		}
	}

	if (GetRCheck(XRCID("ID_USEDEFAULT"))) {
		if (GetRCheck(XRCID("ID_DEFAULT_NONE")) ||
			(custom && editor.empty()))
		{
			return DisplayError(_T("ID_EDITOR"), _("A default editor needs to be set."));
		}
	}

	return true;
}

void COptionsPageEdit::OnBrowseEditor(wxCommandEvent&)
{
	wxFileDialog dlg(this, _("Select default editor"), wxString(), wxString(),
#ifdef __WXMSW__
		_T("Executable file (*.exe)|*.exe"),
#elif __WXMAC__
		_T("Applications (*.app)|*.app"),
#else
		wxFileSelectorDefaultWildcardStr,
#endif
		wxFD_OPEN | wxFD_FILE_MUST_EXIST);

	if (dlg.ShowModal() != wxID_OK) {
		return;
	}

	std::wstring editor = dlg.GetPath().ToStdWstring();
	if (editor.empty()) {
		return;
	}

	if (!ProgramExists(editor)) {
		XRCCTRL(*this, "ID_EDITOR", wxWindow)->SetFocus();
		wxMessageBoxEx(_("Selected editor does not exist."), _("File not found"), wxICON_EXCLAMATION, this);
		return;
	}

	if (editor.find(' ') != std::wstring::npos) {
		editor = L"\"" + editor + L"\"";
	}

	bool tmp;
	SetText(XRCID("ID_EDITOR"), editor, tmp);
}

void COptionsPageEdit::SetCtrlState()
{
	bool custom = GetRCheck(XRCID("ID_DEFAULT_CUSTOM"));

	XRCCTRL(*this, "ID_EDITOR", wxTextCtrl)->Enable(custom);
	XRCCTRL(*this, "ID_BROWSE", wxButton)->Enable(custom);

	XRCCTRL(*this, "ID_USEDEFAULT", wxRadioButton)->Enable(!GetRCheck(XRCID("ID_DEFAULT_NONE")) || GetRCheck(XRCID("ID_USEDEFAULT")));
}

void COptionsPageEdit::OnRadioButton(wxCommandEvent&)
{
	SetCtrlState();
}
