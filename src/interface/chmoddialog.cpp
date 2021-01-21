#include "filezilla.h"
#include "chmoddialog.h"
#include "xrc_helper.h"

BEGIN_EVENT_TABLE(CChmodDialog, wxDialogEx)
EVT_BUTTON(XRCID("wxID_OK"), CChmodDialog::OnOK)
EVT_BUTTON(XRCID("wxID_CANCEL"), CChmodDialog::OnCancel)
EVT_TEXT(XRCID("ID_NUMERIC"), CChmodDialog::OnNumericChanged)
EVT_CHECKBOX(XRCID("ID_RECURSE"), CChmodDialog::OnRecurseChanged)
END_EVENT_TABLE()

CChmodDialog::CChmodDialog(ChmodData & data)
	: data_(data)
{
	for (int i = 0; i < 9; ++i) {
		m_checkBoxes[i] = 0;
	}
}

bool CChmodDialog::Create(wxWindow* parent, int fileCount, int dirCount,
						  const wxString& name, const char permissions[9])
{
	m_noUserTextChange = false;
	lastChangedNumeric = false;

	memcpy(data_.permissions_, permissions, 9);

	SetExtraStyle(wxWS_EX_BLOCK_EVENTS);
	SetParent(parent);

	wxString title;
	if (!dirCount) {
		if (fileCount == 1) {
			title = wxString::Format(_("Please select the new attributes for the file \"%s\"."), name);
		}
		else {
			title = _("Please select the new attributes for the selected files.");
		}
	}
	else {
		if (!fileCount) {
			if (dirCount == 1) {
				title = wxString::Format(_("Please select the new attributes for the directory \"%s\"."), name);
			}
			else {
				title = _("Please select the new attributes for the selected directories.");
			}
		}
		else {
			title = _("Please select the new attributes for the selected files and directories.");
		}
	}

	if (!Load(parent, _T("ID_CHMODDIALOG"))) {
		return false;
	}

	if (!SetChildLabel(XRCID("ID_DESC"), title, 300)) {
		wxFAIL_MSG(_T("Could not set ID_DESC"));
	}

	if (!XRCCTRL(*this, "wxID_OK", wxButton)) {
		return false;
	}

	if (!XRCCTRL(*this, "wxID_CANCEL", wxButton)) {
		return false;
	}

	if (!XRCCTRL(*this, "ID_NUMERIC", wxTextCtrl)) {
		return false;
	}

	if (!WrapText(this, XRCID("ID_NUMERICTEXT"), 300)) {
		wxFAIL_MSG(_T("Wrapping of ID_NUMERICTEXT failed"));
	}

	wxCheckBox* pRecurse = XRCCTRL(*this, "ID_RECURSE", wxCheckBox);
	wxRadioButton* pApplyAll = XRCCTRL(*this, "ID_APPLYALL", wxRadioButton);
	wxRadioButton* pApplyFiles = XRCCTRL(*this, "ID_APPLYFILES", wxRadioButton);
	wxRadioButton* pApplyDirs = XRCCTRL(*this, "ID_APPLYDIRS", wxRadioButton);
	if (!pRecurse || !pApplyAll || !pApplyFiles || !pApplyDirs) {
		return false;
	}

	if (!dirCount) {
		pRecurse->Hide();
		pApplyAll->Hide();
		pApplyFiles->Hide();
		pApplyDirs->Hide();
	}

	pApplyAll->Enable(false);
	pApplyFiles->Enable(false);
	pApplyDirs->Enable(false);

	const wxChar* IDs[9] = { _T("ID_OWNERREAD"), _T("ID_OWNERWRITE"), _T("ID_OWNEREXECUTE"),
						   _T("ID_GROUPREAD"), _T("ID_GROUPWRITE"), _T("ID_GROUPEXECUTE"),
						   _T("ID_PUBLICREAD"), _T("ID_PUBLICWRITE"), _T("ID_PUBLICEXECUTE")
						 };

	for (int i = 0; i < 9; ++i) {
		int id = wxXmlResource::GetXRCID(IDs[i]);
		m_checkBoxes[i] = dynamic_cast<wxCheckBox*>(FindWindow(id));

		if (!m_checkBoxes[i]) {
			return false;
		}

		Connect(id, wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(CChmodDialog::OnCheckboxClick));

		switch (permissions[i])
		{
		default:
		case 0:
			m_checkBoxes[i]->Set3StateValue(wxCHK_UNDETERMINED);
			break;
		case 1:
			m_checkBoxes[i]->Set3StateValue(wxCHK_UNCHECKED);
			break;
		case 2:
			m_checkBoxes[i]->Set3StateValue(wxCHK_CHECKED);
			break;
		}
	}

	GetSizer()->Fit(this);
	GetSizer()->SetSizeHints(this);

	wxCommandEvent evt;
	OnCheckboxClick(evt);

	return true;
}

void CChmodDialog::OnOK(wxCommandEvent&)
{
	wxCheckBox* pRecurse = XRCCTRL(*this, "ID_RECURSE", wxCheckBox);
	m_recursive = pRecurse->GetValue();
	wxRadioButton* pApplyFiles = XRCCTRL(*this, "ID_APPLYFILES", wxRadioButton);
	wxRadioButton* pApplyDirs = XRCCTRL(*this, "ID_APPLYDIRS", wxRadioButton);
	if (pApplyFiles->GetValue()) {
		data_.applyType_ = 1;
	}
	else if (pApplyDirs->GetValue()) {
		data_.applyType_ = 2;
	}
	else {
		data_.applyType_ = 0;
	}
	data_.numeric_ = xrc_call(*this, "ID_NUMERIC", &wxTextCtrl::GetValue).ToStdWstring();
	EndModal(wxID_OK);
}

void CChmodDialog::OnCancel(wxCommandEvent&)
{
	EndModal(wxID_CANCEL);
}

void CChmodDialog::OnCheckboxClick(wxCommandEvent&)
{
	lastChangedNumeric = false;
	for (int i = 0; i < 9; ++i) {
		wxCheckBoxState state = m_checkBoxes[i]->Get3StateValue();
		switch (state)
		{
		default:
		case wxCHK_UNDETERMINED:
			data_.permissions_[i] = 0;
			break;
		case wxCHK_UNCHECKED:
			data_.permissions_[i] = 1;
			break;
		case wxCHK_CHECKED:
			data_.permissions_[i] = 2;
			break;
		}
	}

	wxString numericValue;
	for (int i = 0; i < 3; ++i) {
		if (!data_.permissions_[i * 3] || !data_.permissions_[i * 3 + 1] || !data_.permissions_[i * 3 + 2]) {
			numericValue += 'x';
			continue;
		}

		numericValue += wxString::Format(_T("%d"), (data_.permissions_[i * 3] - 1) * 4 + (data_.permissions_[i * 3 + 1] - 1) * 2 + (data_.permissions_[i * 3 + 2] - 1) * 1);
	}

	wxTextCtrl *pTextCtrl = XRCCTRL(*this, "ID_NUMERIC", wxTextCtrl);
	wxString oldValue = pTextCtrl->GetValue();

	m_noUserTextChange = true;
	pTextCtrl->SetValue(oldValue.Left(oldValue.size() - 3) + numericValue);
	m_noUserTextChange = false;
	oldNumeric = numericValue;
}

void CChmodDialog::OnNumericChanged(wxCommandEvent&)
{
	if (m_noUserTextChange) {
		return;
	}

	lastChangedNumeric = true;

	wxTextCtrl *pTextCtrl = XRCCTRL(*this, "ID_NUMERIC", wxTextCtrl);
	wxString numeric = pTextCtrl->GetValue();
	if (numeric.size() < 3) {
		return;
	}

	numeric = numeric.Right(3);
	for (int i = 0; i < 3; ++i) {
		if ((numeric[i] < '0' || numeric[i] > '9') && numeric[i] != 'x') {
			return;
		}
	}
	for (int i = 0; i < 3; ++i) {
		if (!oldNumeric.empty() && numeric[i] == oldNumeric[i]) {
			continue;
		}
		if (numeric[i] == 'x') {
			data_.permissions_[i * 3] = 0;
			data_.permissions_[i * 3 + 1] = 0;
			data_.permissions_[i * 3 + 2] = 0;
		}
		else {
			int value = numeric[i] - '0';
			data_.permissions_[i * 3] = (value & 4) ? 2 : 1;
			data_.permissions_[i * 3 + 1] = (value & 2) ? 2 : 1;
			data_.permissions_[i * 3 + 2] = (value & 1) ? 2 : 1;
		}
	}

	oldNumeric = numeric;

	for (int i = 0; i < 9; ++i) {
		switch (data_.permissions_[i])
		{
		default:
		case 0:
			m_checkBoxes[i]->Set3StateValue(wxCHK_UNDETERMINED);
			break;
		case 1:
			m_checkBoxes[i]->Set3StateValue(wxCHK_UNCHECKED);
			break;
		case 2:
			m_checkBoxes[i]->Set3StateValue(wxCHK_CHECKED);
			break;
		}
	}
}

bool CChmodDialog::Recursive() const
{
	return m_recursive;
}

void CChmodDialog::OnRecurseChanged(wxCommandEvent&)
{
	wxCheckBox* pRecurse = XRCCTRL(*this, "ID_RECURSE", wxCheckBox);
	wxRadioButton* pApplyAll = XRCCTRL(*this, "ID_APPLYALL", wxRadioButton);
	wxRadioButton* pApplyFiles = XRCCTRL(*this, "ID_APPLYFILES", wxRadioButton);
	wxRadioButton* pApplyDirs = XRCCTRL(*this, "ID_APPLYDIRS", wxRadioButton);
	pApplyAll->Enable(pRecurse->GetValue());
	pApplyFiles->Enable(pRecurse->GetValue());
	pApplyDirs->Enable(pRecurse->GetValue());
}
