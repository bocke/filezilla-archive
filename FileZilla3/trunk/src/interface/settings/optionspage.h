#ifndef FILEZILLA_INTERFACE_OPTIONSPAGE_HEADER
#define FILEZILLA_INTERFACE_OPTIONSPAGE_HEADER

#include <wx/panel.h>

#include "../Options.h"

class CSettingsDialog;
class COptionsPage : public wxPanel
{
public:
	virtual bool CreatePage(COptions* pOptions, CSettingsDialog* pOwner, wxWindow* parent, wxSize& maxSize);

	void UpdateMaxPageSize(wxSize& maxSize);

	virtual bool LoadPage() = 0;
	virtual bool SavePage() = 0;
	virtual bool Validate() { return true; }

	void SetRCheck(int id, bool checked, bool& failure);

	// The GetXXX functions do never return an error since the controls were
	// checked to exist while loading the dialog.
	bool GetRCheck(int id) const;

	void ReloadSettings();

	// Always returns false
	bool DisplayError(wxString const& controlToFocus, wxString const& error);
	bool DisplayError(wxWindow* pWnd, wxString const& error);

	bool Display();

	virtual bool OnDisplayedFirstTime();

protected:
	virtual wxString GetResourceName() const {
		return wxString();
	}

	virtual bool CreateControls(wxWindow* parent);

	COptions* m_pOptions{};
	CSettingsDialog* m_pOwner{};

	bool m_was_selected{};
};

#endif
