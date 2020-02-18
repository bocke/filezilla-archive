#include <filezilla.h>
#include "spinctrlex.h"

#define DEFAULT_LENGTH_LIMIT 26 // Big enough for all 64bit values with thousands separator

wxSpinCtrlEx::wxSpinCtrlEx(wxWindow* parent, wxWindowID id, wxString const& value, wxPoint const& pos, wxSize const& size, long style, int min, int max, int initial)
	: wxSpinCtrl(parent, id, value, pos, size, style, min, max, initial)
{
	SetMaxLength(DEFAULT_LENGTH_LIMIT);
}

bool wxSpinCtrlEx::Create(wxWindow* parent, wxWindowID id, wxString const& value, wxPoint const& pos, wxSize const& size, long style, int min, int max, int initial, wxString const& name)
{
	bool ret = wxSpinCtrl::Create(parent, id, value, pos, size, style, min, max, initial, name);
	if (ret) {
		SetMaxLength(DEFAULT_LENGTH_LIMIT);
	}
	return ret;
}

void wxSpinCtrlEx::SetMaxLength(unsigned long len)
{
#ifdef __WXMSW__
	::SendMessage(m_hwndBuddy, EM_LIMITTEXT, len, 0);
#endif
}
