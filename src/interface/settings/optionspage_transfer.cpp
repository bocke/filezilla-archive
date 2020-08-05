#include "../filezilla.h"
#include "../Options.h"
#include "../sizeformatting.h"
#include "settingsdialog.h"
#include "optionspage.h"
#include "optionspage_transfer.h"
#include "../textctrlex.h"
#include "../wxext/spinctrlex.h"
#include "../xrc_helper.h"

#include <wx/statbox.h>

struct COptionsPageTransfer::impl final
{
	wxSpinCtrlEx* transfers_{};
	wxSpinCtrlEx* downloads_{};
	wxSpinCtrlEx* uploads_{};

	wxChoice* burst_tolerance_{};
};

COptionsPageTransfer::COptionsPageTransfer()
	: impl_(std::make_unique<impl>())
{
}

COptionsPageTransfer::~COptionsPageTransfer()
{
}

bool COptionsPageTransfer::CreateControls(wxWindow* parent)
{
	auto const& lay = m_pOwner->layout();

	Create(parent);
	auto main = lay.createFlex(1);
	main->AddGrowableCol(0);
	SetSizer(main);

	{
		auto [box, inner] = lay.createStatBox(main, _("Concurrent transfers"), 3);
		inner->Add(new wxStaticText(box, nullID, _("Maximum simultaneous &transfers:")), lay.valign);
		impl_->transfers_ = new wxSpinCtrlEx(box, nullID, wxString(), wxDefaultPosition, wxSize(lay.dlgUnits(26), -1));
		impl_->transfers_->SetRange(1, 10);
		impl_->transfers_->SetMaxLength(2);
		inner->Add(impl_->transfers_, lay.valign);
		inner->Add(new wxStaticText(box, nullID, _("(1-10)")), lay.valign);
		inner->Add(new wxStaticText(box, nullID, _("Limit for concurrent &downloads:")), lay.valign);
		impl_->downloads_ = new wxSpinCtrlEx(box, nullID, wxString(), wxDefaultPosition, wxSize(lay.dlgUnits(26), -1));
		impl_->downloads_->SetRange(0, 10);
		impl_->downloads_->SetMaxLength(2);
		inner->Add(impl_->downloads_, lay.valign);
		inner->Add(new wxStaticText(box, nullID, _("(0 for no limit)")), lay.valign);
		inner->Add(new wxStaticText(box, nullID, _("Limit for concurrent &uploads:")), lay.valign);
		impl_->uploads_ = new wxSpinCtrlEx(box, nullID, wxString(), wxDefaultPosition, wxSize(lay.dlgUnits(26), -1));
		impl_->uploads_->SetRange(0, 10);
		impl_->uploads_->SetMaxLength(2);
		inner->Add(impl_->uploads_, lay.valign);
		inner->Add(new wxStaticText(box, nullID, _("(0 for no limit)")), lay.valign);
	}

	{
		auto [box, inner] = lay.createStatBox(main, _("Speed limits"), 1);

		auto enable = new wxCheckBox(box, XRCID("ID_ENABLE_SPEEDLIMITS"), _("&Enable speed limits"));
		inner->Add(enable);

		auto innermost = lay.createFlex(2);
		inner->Add(innermost);
		innermost->Add(new wxStaticText(box, nullID, _("Download &limit:")), lay.valign);
		auto row = lay.createFlex(2);
		innermost->Add(row, lay.valign);
		auto dllimit = new wxTextCtrlEx(box, XRCID("ID_DOWNLOADLIMIT"), wxString(), wxDefaultPosition, wxSize(lay.dlgUnits(40), -1));
		dllimit->SetMaxLength(9);
		row->Add(dllimit, lay.valign);
		row->Add(new wxStaticText(box, nullID, wxString::Format(_("(in %s/s)"), CSizeFormat::GetUnitWithBase(CSizeFormat::kilo, 1024))), lay.valign);

		innermost->Add(new wxStaticText(box, nullID, _("Upload &limit:")), lay.valign);
		row = lay.createFlex(2);
		innermost->Add(row, lay.valign);
		auto ullimit = new wxTextCtrlEx(box, XRCID("ID_UPLOADLIMIT"), wxString(), wxDefaultPosition, wxSize(lay.dlgUnits(40), -1));
		ullimit->SetMaxLength(9);
		row->Add(ullimit, lay.valign);
		row->Add(new wxStaticText(box, nullID, wxString::Format(_("(in %s/s)"), CSizeFormat::GetUnitWithBase(CSizeFormat::kilo, 1024))), lay.valign);

		innermost->Add(new wxStaticText(box, nullID, _("&Burst tolerance:")), lay.valign);
		impl_->burst_tolerance_ = new wxChoice(box, nullID);
		impl_->burst_tolerance_->AppendString(_("Normal"));
		impl_->burst_tolerance_->AppendString(_("High"));
		impl_->burst_tolerance_->AppendString(_("Very high"));
		innermost->Add(impl_->burst_tolerance_, lay.valign);

		enable->Bind(wxEVT_CHECKBOX, [dllimit, ullimit, this](wxCommandEvent const& ev) {
			dllimit->Enable(ev.IsChecked());
			ullimit->Enable(ev.IsChecked());
			impl_->burst_tolerance_->Enable(ev.IsChecked());
		});
	}
	
	{
		auto [box, inner] = lay.createStatBox(main, _("Filter invalid characters in filenames"), 1);
		inner->Add(new wxCheckBox(box, XRCID("ID_ENABLE_REPLACE"), _("Enable invalid character &filtering")));
		inner->Add(new wxStaticText(box, nullID, _("When enabled, characters that are not supported by the local operating system in filenames are replaced if downloading such a file.")));
		auto innermost = lay.createFlex(2);
		inner->Add(innermost);
		innermost->Add(new wxStaticText(box, nullID, _("&Replace invalid characters with:")), lay.valign);
		auto replace = new wxTextCtrlEx(box, XRCID("ID_REPLACE"), wxString(), wxDefaultPosition, wxSize(lay.dlgUnits(10), -1));
		replace->SetMaxLength(1);
		innermost->Add(replace, lay.valign);
#ifdef __WXMSW__
		wxString invalid = _T("\\ / : * ? \" < > |");
		wxString filtered = wxString::Format(_("The following characters will be replaced: %s"), invalid);
#else
		wxString invalid = _T("/");
		wxString filtered = wxString::Format(_("The following character will be replaced: %s"), invalid);
#endif
		inner->Add(new wxStaticText(box, nullID, filtered));
	}

	{
		auto [box, inner] = lay.createStatBox(main, _("Preallocation"), 1);
		inner->Add(new wxCheckBox(box, XRCID("ID_PREALLOCATE"), _("Pre&allocate space before downloading")));
	}

	GetSizer()->Fit(this);

	return true;
}

bool COptionsPageTransfer::LoadPage()
{
	bool failure = false;

	bool enable_speedlimits = m_pOptions->get_int(OPTION_SPEEDLIMIT_ENABLE) != 0;
	SetCheck(XRCID("ID_ENABLE_SPEEDLIMITS"), enable_speedlimits, failure);

	wxTextCtrl* pTextCtrl = XRCCTRL(*this, "ID_DOWNLOADLIMIT", wxTextCtrl);
	if (!pTextCtrl) {
		return false;
	}
	pTextCtrl->ChangeValue(m_pOptions->get_string(OPTION_SPEEDLIMIT_INBOUND));
	pTextCtrl->Enable(enable_speedlimits);

	pTextCtrl = XRCCTRL(*this, "ID_UPLOADLIMIT", wxTextCtrl);
	if (!pTextCtrl) {
		return false;
	}
	pTextCtrl->ChangeValue(m_pOptions->get_string(OPTION_SPEEDLIMIT_OUTBOUND));
	pTextCtrl->Enable(enable_speedlimits);

	impl_->transfers_->SetValue(m_pOptions->get_int(OPTION_NUMTRANSFERS));
	impl_->downloads_->SetValue(m_pOptions->get_int(OPTION_CONCURRENTDOWNLOADLIMIT));
	impl_->uploads_->SetValue(m_pOptions->get_int(OPTION_CONCURRENTUPLOADLIMIT));

	impl_->burst_tolerance_->SetSelection(m_pOptions->get_int(OPTION_SPEEDLIMIT_BURSTTOLERANCE));
	impl_->burst_tolerance_->Enable(enable_speedlimits);

	pTextCtrl = XRCCTRL(*this, "ID_REPLACE", wxTextCtrl);
	pTextCtrl->ChangeValue(m_pOptions->get_string(OPTION_INVALID_CHAR_REPLACE));

	SetCheckFromOption(XRCID("ID_ENABLE_REPLACE"), OPTION_INVALID_CHAR_REPLACE_ENABLE, failure);

	xrc_call(*this, "ID_PREALLOCATE", &wxCheckBox::SetValue, m_pOptions->get_bool(OPTION_PREALLOCATE_SPACE));

	return !failure;
}

bool COptionsPageTransfer::SavePage()
{
	m_pOptions->set(OPTION_SPEEDLIMIT_ENABLE, xrc_call(*this, "ID_ENABLE_SPEEDLIMITS", &wxCheckBox::GetValue));

	m_pOptions->set(OPTION_NUMTRANSFERS, impl_->transfers_->GetValue());
	m_pOptions->set(OPTION_CONCURRENTDOWNLOADLIMIT,	impl_->downloads_->GetValue());
	m_pOptions->set(OPTION_CONCURRENTUPLOADLIMIT, impl_->uploads_->GetValue());

	m_pOptions->set(OPTION_SPEEDLIMIT_INBOUND, xrc_call(*this, "ID_DOWNLOADLIMIT", &wxTextCtrl::GetValue).ToStdWstring());
	m_pOptions->set(OPTION_SPEEDLIMIT_OUTBOUND, xrc_call(*this, "ID_UPLOADLIMIT", &wxTextCtrl::GetValue).ToStdWstring());
	m_pOptions->set(OPTION_SPEEDLIMIT_BURSTTOLERANCE, impl_->burst_tolerance_->GetSelection());
	m_pOptions->set(OPTION_INVALID_CHAR_REPLACE, xrc_call(*this, "ID_REPLACE", &wxTextCtrlEx::GetValue).ToStdWstring());
	SetOptionFromCheck(XRCID("ID_ENABLE_REPLACE"), OPTION_INVALID_CHAR_REPLACE_ENABLE);

	m_pOptions->set(OPTION_PREALLOCATE_SPACE, xrc_call(*this, "ID_PREALLOCATE", &wxCheckBox::GetValue));

	return true;
}

bool COptionsPageTransfer::Validate()
{
	if (impl_->transfers_->GetValue() < 1 || impl_->transfers_->GetValue() > 10) {
		return DisplayError(impl_->transfers_, _("Please enter a number between 1 and 10 for the number of concurrent transfers."));
	}

	if (impl_->downloads_->GetValue() < 0 || impl_->downloads_->GetValue() > 10) {
		return DisplayError(impl_->downloads_, _("Please enter a number between 0 and 10 for the number of concurrent downloads."));
	}

	if (impl_->uploads_->GetValue() < 0 || impl_->uploads_->GetValue() > 10) {
		return DisplayError(impl_->uploads_, _("Please enter a number between 0 and 10 for the number of concurrent uploads."));
	}

	long tmp{};
	auto pCtrl = XRCCTRL(*this, "ID_DOWNLOADLIMIT", wxTextCtrl);
	if (!pCtrl->GetValue().ToLong(&tmp) || (tmp < 0)) {
		const wxString unit = CSizeFormat::GetUnitWithBase(CSizeFormat::kilo, 1024);
		return DisplayError(pCtrl, wxString::Format(_("Please enter a download speed limit greater or equal to 0 %s/s."), unit));
	}

	pCtrl = XRCCTRL(*this, "ID_UPLOADLIMIT", wxTextCtrl);
	if (!pCtrl->GetValue().ToLong(&tmp) || (tmp < 0)) {
		const wxString unit = CSizeFormat::GetUnitWithBase(CSizeFormat::kilo, 1024);
		return DisplayError(pCtrl, wxString::Format(_("Please enter an upload speed limit greater or equal to 0 %s/s."), unit));
	}

	pCtrl = XRCCTRL(*this, "ID_REPLACE", wxTextCtrl);
	wxString replace = pCtrl->GetValue();
#ifdef __WXMSW__
	if (replace == _T("\\") ||
		replace == _T("/") ||
		replace == _T(":") ||
		replace == _T("*") ||
		replace == _T("?") ||
		replace == _T("\"") ||
		replace == _T("<") ||
		replace == _T(">") ||
		replace == _T("|"))
#else
	if (replace == _T("/"))
#endif
	{
		return DisplayError(pCtrl, _("You cannot replace an invalid character with another invalid character. Please enter a character that is allowed in filenames."));
	}

	return true;
}
