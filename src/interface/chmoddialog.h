#ifndef FILEZILLA_INTERFACE_CHMODDIALOG_HEADER
#define FILEZILLA_INTERFACE_CHMODDIALOG_HEADER

#include "dialogex.h"
#include "../commonui/chmod_data.h"

class CChmodDialog final : public wxDialogEx
{
public:
	CChmodDialog(ChmodData & data);

	bool Create(wxWindow* parent, int fileCount, int dirCount,
				const wxString& name, const char permissions[9]);

	bool Recursive() const ;

protected:

	DECLARE_EVENT_TABLE()
	void OnOK(wxCommandEvent&);
	void OnCancel(wxCommandEvent&);
	void OnRecurseChanged(wxCommandEvent&);

	void OnCheckboxClick(wxCommandEvent&);
	void OnNumericChanged(wxCommandEvent&);

	ChmodData & data_;

	wxCheckBox* m_checkBoxes[9];

	bool m_noUserTextChange{};
	wxString oldNumeric;
	bool lastChangedNumeric{};

	bool m_recursive{};
};

#endif
