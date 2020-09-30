#ifndef FILEZILLA_INTERFACE_LED_HEADER
#define FILEZILLA_INTERFACE_LED_HEADER

#include <wx/event.h>
#include <wx/timer.h>

wxDECLARE_EVENT(fzEVT_UPDATE_LED_TOOLTIP, wxCommandEvent);

class CLed final : public wxWindow
{
public:
	CLed(wxWindow *parent, unsigned int index);

	void Ping();

protected:
	void Set();
	void Unset();

	int const m_index;
	int m_ledState;

	wxBitmap m_leds[2];
	bool m_loaded{};

	wxTimer m_timer;

	DECLARE_EVENT_TABLE()
	void OnPaint(wxPaintEvent& event);
	void OnTimer(wxTimerEvent& event);
	void OnEnterWindow(wxMouseEvent& event);
};

#endif
