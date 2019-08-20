#include <filezilla.h>

#include "textctrlex.h"

#ifdef __WXMAC__

wxTextCtrlEx::wxTextCtrlEx(wxWindow* parent, int id, wxString const& value, wxPoint const& pos, wxSize const& size, long style)
	: wxTextCtrl(parent, id, value, pos, size, style)
{
}

static wxTextAttr DoGetDefaultStyle(wxTextCtrl* ctrl)
{
	wxTextAttr style;
	style.SetFont(ctrl->GetFont());
	return style;
}

const wxTextAttr& GetDefaultTextCtrlStyle(wxTextCtrl* ctrl)
{
	static const wxTextAttr style = DoGetDefaultStyle(ctrl);
	return style;
}

void wxTextCtrlEx::Paste()
{
	wxTextCtrl::Paste();
	SetStyle(0, GetLastPosition(), GetDefaultTextCtrlStyle(this));
}

#endif
