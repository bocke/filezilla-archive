#include <filezilla.h>
#include "sitemanager_controls.h"

#include "dialogex.h"
#include "textctrlex.h"
#include "xrc_helper.h"

#include <s3sse.h>

#include <wx/spinctrl.h>
#include <wx/statline.h>

AdvancedSiteControls::AdvancedSiteControls(wxWindow & parent, DialogLayout const& lay, wxFlexGridSizer & sizer)
    : SiteControls(parent)
{
	sizer.AddGrowableCol(0);
	auto* row = lay.createFlex(0, 1);
	sizer.Add(row);

	row->Add(new wxStaticText(&parent, XRCID("ID_SERVERTYPE_LABEL"), _("Server &type:")), lay.valign);

	auto types = new wxChoice(&parent, XRCID("ID_SERVERTYPE"));
	row->Add(types, lay.valign);

	for (int i = 0; i < SERVERTYPE_MAX; ++i) {
		types->Append(CServer::GetNameFromServerType(static_cast<ServerType>(i)));
	}

	sizer.AddSpacer(0);
	sizer.Add(new wxCheckBox(&parent, XRCID("ID_BYPASSPROXY"), _("B&ypass proxy")));

	sizer.Add(new wxStaticLine(&parent), lay.grow);

	sizer.Add(new wxStaticText(&parent, -1, _("Default &local directory:")));

	row = lay.createFlex(0, 1);
	sizer.Add(row, lay.grow);
	row->AddGrowableCol(0);
	auto localDir = new wxTextCtrlEx(&parent, XRCID("ID_LOCALDIR"));
	row->Add(localDir, lay.valigng);
	auto browse = new wxButton(&parent, XRCID("ID_BROWSE"), _("&Browse..."));
	row->Add(browse, lay.valign);

	browse->Bind(wxEVT_BUTTON, [localDir, p = &parent](wxEvent const&) {
		wxDirDialog dlg(wxGetTopLevelParent(p), _("Choose the default local directory"), localDir->GetValue(), wxDD_NEW_DIR_BUTTON);
		if (dlg.ShowModal() == wxID_OK) {
			localDir->ChangeValue(dlg.GetPath());
		}
	});

	sizer.AddSpacer(0);
	sizer.Add(new wxStaticText(&parent, -1, _("Default r&emote directory:")));
	sizer.Add(new wxTextCtrlEx(&parent, XRCID("ID_REMOTEDIR")), lay.grow);
	sizer.AddSpacer(0);
	sizer.Add(new wxCheckBox(&parent, XRCID("ID_SYNC"), _("&Use synchronized browsing")));
	sizer.Add(new wxCheckBox(&parent, XRCID("ID_COMPARISON"), _("Directory comparison")));

	sizer.Add(new wxStaticLine(&parent), lay.grow);

	sizer.Add(new wxStaticText(&parent, -1, _("&Adjust server time, offset by:")));
	row = lay.createFlex(0, 1);
	sizer.Add(row);
	auto* hours = new wxSpinCtrl(&parent, XRCID("ID_TIMEZONE_HOURS"), wxString(), wxDefaultPosition, wxSize(lay.dlgUnits(26), -1));
	hours->SetRange(-24, 24);
	row->Add(hours, lay.valign);
	row->Add(new wxStaticText(&parent, -1, _("Hours,")), lay.valign);
	auto* minutes = new wxSpinCtrl(&parent, XRCID("ID_TIMEZONE_MINUTES"), wxString(), wxDefaultPosition, wxSize(lay.dlgUnits(26), -1));
	minutes->SetRange(-59, 59);
	row->Add(minutes, lay.valign);
	row->Add(new wxStaticText(&parent, -1, _("Minutes")), lay.valign);
}

void AdvancedSiteControls::SetSite(Site const& site, bool predefined)
{
	xrc_call(parent_, "ID_SERVERTYPE", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_BYPASSPROXY", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_SYNC", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_COMPARISON", &wxCheckBox::Enable, !predefined);
	xrc_call(parent_, "ID_LOCALDIR", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_BROWSE", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_REMOTEDIR", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_TIMEZONE_HOURS", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_TIMEZONE_MINUTES", &wxWindow::Enable, !predefined);

	if (site) {
		xrc_call(parent_, "ID_SERVERTYPE", &wxChoice::SetSelection, site.server.GetType());
		xrc_call(parent_, "ID_LOCALDIR", &wxTextCtrl::ChangeValue, site.m_default_bookmark.m_localDir);
		xrc_call(parent_, "ID_REMOTEDIR", &wxTextCtrl::ChangeValue, site.m_default_bookmark.m_remoteDir.GetPath());
		xrc_call(parent_, "ID_SYNC", &wxCheckBox::SetValue, site.m_default_bookmark.m_sync);
		xrc_call(parent_, "ID_COMPARISON", &wxCheckBox::SetValue, site.m_default_bookmark.m_comparison);
		xrc_call<wxSpinCtrl, int>(parent_, "ID_TIMEZONE_HOURS", &wxSpinCtrl::SetValue, site.server.GetTimezoneOffset() / 60);
		xrc_call<wxSpinCtrl, int>(parent_, "ID_TIMEZONE_MINUTES", &wxSpinCtrl::SetValue, site.server.GetTimezoneOffset() % 60);
	}
	else {
		xrc_call(parent_, "ID_SERVERTYPE", &wxChoice::SetSelection, 0);
		xrc_call(parent_, "ID_BYPASSPROXY", &wxCheckBox::SetValue, false);
		xrc_call(parent_, "ID_SYNC", &wxCheckBox::SetValue, false);
		xrc_call(parent_, "ID_COMPARISON", &wxCheckBox::SetValue, false);
		xrc_call(parent_, "ID_LOCALDIR", &wxTextCtrl::ChangeValue, wxString());
		xrc_call(parent_, "ID_REMOTEDIR", &wxTextCtrl::ChangeValue, wxString());
		xrc_call<wxSpinCtrl, int>(parent_, "ID_TIMEZONE_HOURS", &wxSpinCtrl::SetValue, 0);
		xrc_call<wxSpinCtrl, int>(parent_, "ID_TIMEZONE_MINUTES", &wxSpinCtrl::SetValue, 0);
	}
}

bool AdvancedSiteControls::Verify(bool)
{
	std::wstring const remotePathRaw = XRCCTRL(parent_, "ID_REMOTEDIR", wxTextCtrl)->GetValue().ToStdWstring();
	if (!remotePathRaw.empty()) {
		std::wstring serverType = XRCCTRL(parent_, "ID_SERVERTYPE", wxChoice)->GetStringSelection().ToStdWstring();

		CServerPath remotePath;
		remotePath.SetType(CServer::GetServerTypeFromName(serverType));
		if (!remotePath.SetPath(remotePathRaw)) {
			XRCCTRL(parent_, "ID_REMOTEDIR", wxTextCtrl)->SetFocus();
			wxMessageBoxEx(_("Default remote path cannot be parsed. Make sure it is a valid absolute path for the selected server type."), _("Site Manager - Invalid data"), wxICON_EXCLAMATION, wxGetTopLevelParent(&parent_));
			return false;
		}
	}

	std::wstring const localPath = XRCCTRL(parent_, "ID_LOCALDIR", wxTextCtrl)->GetValue().ToStdWstring();
	if (XRCCTRL(parent_, "ID_SYNC", wxCheckBox)->GetValue()) {
		if (remotePathRaw.empty() || localPath.empty()) {
			XRCCTRL(parent_, "ID_SYNC", wxCheckBox)->SetFocus();
			wxMessageBoxEx(_("You need to enter both a local and a remote path to enable synchronized browsing for this site."), _("Site Manager - Invalid data"), wxICON_EXCLAMATION, wxGetTopLevelParent(&parent_));
			return false;
		}
	}

	return true;
}

void AdvancedSiteControls::SetControlVisibility(ServerProtocol protocol, LogonType)
{
	bool const hasServerType = CServer::ProtocolHasFeature(protocol, ProtocolFeature::ServerType);
	xrc_call(parent_, "ID_SERVERTYPE_LABEL", &wxWindow::Show, hasServerType);
	xrc_call(parent_, "ID_SERVERTYPE", &wxWindow::Show, hasServerType);
	auto * serverTypeSizer = xrc_call(parent_, "ID_SERVERTYPE_LABEL", &wxWindow::GetContainingSizer)->GetContainingWindow()->GetSizer();
	serverTypeSizer->CalcMin();
	serverTypeSizer->Layout();
}

void AdvancedSiteControls::UpdateSite(Site & site)
{
	std::wstring const serverType = xrc_call(parent_, "ID_SERVERTYPE", &wxChoice::GetStringSelection).ToStdWstring();
	site.server.SetType(CServer::GetServerTypeFromName(serverType));

	if (xrc_call(parent_, "ID_BYPASSPROXY", &wxCheckBox::GetValue)) {
		site.server.SetBypassProxy(true);
	}
	else {
		site.server.SetBypassProxy(false);
	}

	site.m_default_bookmark.m_localDir = xrc_call(parent_, "ID_LOCALDIR", &wxTextCtrl::GetValue).ToStdWstring();
	site.m_default_bookmark.m_remoteDir = CServerPath();
	site.m_default_bookmark.m_remoteDir.SetType(site.server.GetType());
	site.m_default_bookmark.m_remoteDir.SetPath(xrc_call(parent_, "ID_REMOTEDIR", &wxTextCtrl::GetValue).ToStdWstring());
	site.m_default_bookmark.m_sync = xrc_call(parent_, "ID_SYNC", &wxCheckBox::GetValue);
	site.m_default_bookmark.m_comparison = xrc_call(parent_, "ID_COMPARISON", &wxCheckBox::GetValue);

	int hours = xrc_call(parent_, "ID_TIMEZONE_HOURS", &wxSpinCtrl::GetValue);
	int minutes = xrc_call(parent_, "ID_TIMEZONE_MINUTES", &wxSpinCtrl::GetValue);

	site.server.SetTimezoneOffset(hours * 60 + minutes);
}

CharsetSiteControls::CharsetSiteControls(wxWindow & parent, DialogLayout const& lay, wxFlexGridSizer & sizer)
    : SiteControls(parent)
{
	sizer.Add(new wxStaticText(&parent, -1, _("The server uses following charset encoding for filenames:")));
	auto rbAuto = new wxRadioButton(&parent, XRCID("ID_CHARSET_AUTO"), _("&Autodetect"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	sizer.Add(rbAuto);
	sizer.Add(new wxStaticText(&parent, -1, _("Uses UTF-8 if the server supports it, else uses local charset.")), 0, wxLEFT, 18);

	auto rbUtf8 = new wxRadioButton(&parent, XRCID("ID_CHARSET_UTF8"), _("Force &UTF-8"));
	sizer.Add(rbUtf8);
	auto rbCustom = new wxRadioButton(&parent, XRCID("ID_CHARSET_CUSTOM"), _("Use &custom charset"));
	sizer.Add(rbCustom);

	auto * row = lay.createFlex(0, 1);
	row->Add(new wxStaticText(&parent, -1, _("&Encoding:")), lay.valign);
	auto * encoding = new wxTextCtrlEx(&parent, XRCID("ID_ENCODING"));
	row->Add(encoding, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, 18);
	sizer.Add(row);
	sizer.AddSpacer(lay.dlgUnits(6));
	sizer.Add(new wxStaticText(&parent, -1, _("Using the wrong charset can result in filenames not displaying properly.")));

	rbAuto->Bind(wxEVT_RADIOBUTTON, [encoding](wxEvent const&){ encoding->Disable(); });
	rbUtf8->Bind(wxEVT_RADIOBUTTON, [encoding](wxEvent const&){ encoding->Disable(); });
	rbCustom->Bind(wxEVT_RADIOBUTTON, [encoding](wxEvent const&){ encoding->Enable(); });
}

void CharsetSiteControls::SetSite(Site const& site, bool predefined)
{
	xrc_call(parent_, "ID_CHARSET_AUTO", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_CHARSET_UTF8", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_CHARSET_CUSTOM", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_ENCODING", &wxWindow::Enable, !predefined);

	if (!site) {
		xrc_call(parent_, "ID_CHARSET_AUTO", &wxRadioButton::SetValue, true);
		xrc_call(parent_, "ID_ENCODING", &wxTextCtrl::ChangeValue, wxString());
		xrc_call(parent_, "ID_ENCODING", &wxTextCtrl::Enable, false);
	}
	else {
		switch (site.server.GetEncodingType()) {
		default:
		case ENCODING_AUTO:
			xrc_call(parent_, "ID_CHARSET_AUTO", &wxRadioButton::SetValue, true);
			break;
		case ENCODING_UTF8:
			xrc_call(parent_, "ID_CHARSET_UTF8", &wxRadioButton::SetValue, true);
			break;
		case ENCODING_CUSTOM:
			xrc_call(parent_, "ID_CHARSET_CUSTOM", &wxRadioButton::SetValue, true);
			break;
		}
		xrc_call(parent_, "ID_ENCODING", &wxTextCtrl::Enable, !predefined && site.server.GetEncodingType() == ENCODING_CUSTOM);
		xrc_call(parent_, "ID_ENCODING", &wxTextCtrl::ChangeValue, site.server.GetCustomEncoding());
	}
}

bool CharsetSiteControls::Verify(bool)
{
	if (XRCCTRL(parent_, "ID_CHARSET_CUSTOM", wxRadioButton)->GetValue()) {
		if (XRCCTRL(parent_, "ID_ENCODING", wxTextCtrl)->GetValue().empty()) {
			XRCCTRL(parent_, "ID_ENCODING", wxTextCtrl)->SetFocus();
			wxMessageBoxEx(_("Need to specify a character encoding"), _("Site Manager - Invalid data"), wxICON_EXCLAMATION, wxGetTopLevelParent(&parent_));
			return false;
		}
	}

	return true;
}

void CharsetSiteControls::UpdateSite(Site & site)
{
	if (xrc_call(parent_, "ID_CHARSET_UTF8", &wxRadioButton::GetValue)) {
		site.server.SetEncodingType(ENCODING_UTF8);
	}
	else if (xrc_call(parent_, "ID_CHARSET_CUSTOM", &wxRadioButton::GetValue)) {
		std::wstring encoding = xrc_call(parent_, "ID_ENCODING", &wxTextCtrl::GetValue).ToStdWstring();
		site.server.SetEncodingType(ENCODING_CUSTOM, encoding);
	}
	else {
		site.server.SetEncodingType(ENCODING_AUTO);
	}
}

TransferSettingsSiteControls::TransferSettingsSiteControls(wxWindow & parent, DialogLayout const& lay, wxFlexGridSizer & sizer)
    : SiteControls(parent)
{
	sizer.Add(new wxStaticText(&parent, XRCID("ID_TRANSFERMODE_LABEL"), _("&Transfer mode:")));
	auto * row = lay.createFlex(0, 1);
	sizer.Add(row);
	row->Add(new wxRadioButton(&parent, XRCID("ID_TRANSFERMODE_DEFAULT"), _("D&efault"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP), lay.valign);
	row->Add(new wxRadioButton(&parent, XRCID("ID_TRANSFERMODE_ACTIVE"), _("&Active")), lay.valign);
	row->Add(new wxRadioButton(&parent, XRCID("ID_TRANSFERMODE_PASSIVE"), _("&Passive")), lay.valign);
	sizer.AddSpacer(0);

	auto limit = new wxCheckBox(&parent, XRCID("ID_LIMITMULTIPLE"), _("&Limit number of simultaneous connections"));
	sizer.Add(limit);
	row = lay.createFlex(0, 1);
	sizer.Add(row, 0, wxLEFT, lay.dlgUnits(10));
	row->Add(new wxStaticText(&parent, -1, _("&Maximum number of connections:")), lay.valign);
	auto * spin = new wxSpinCtrl(&parent, XRCID("ID_MAXMULTIPLE"), wxString(), wxDefaultPosition, wxSize(lay.dlgUnits(26), -1));
	spin->SetRange(1, 10);
	row->Add(spin, lay.valign);

	limit->Bind(wxEVT_CHECKBOX, [spin](wxCommandEvent const& ev){ spin->Enable(ev.IsChecked()); });
}

void TransferSettingsSiteControls::SetSite(Site const& site, bool predefined)
{
	xrc_call(parent_, "ID_TRANSFERMODE_DEFAULT", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_TRANSFERMODE_ACTIVE", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_TRANSFERMODE_PASSIVE", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_LIMITMULTIPLE", &wxWindow::Enable, !predefined);

	if (site) {
		xrc_call(parent_, "ID_TRANSFERMODE_DEFAULT", &wxRadioButton::SetValue, true);
		xrc_call(parent_, "ID_LIMITMULTIPLE", &wxCheckBox::SetValue, false);
		xrc_call(parent_, "ID_MAXMULTIPLE", &wxSpinCtrl::Enable, false);
		xrc_call<wxSpinCtrl, int>(parent_, "ID_MAXMULTIPLE", &wxSpinCtrl::SetValue, 1);
	}
	else {
		if (CServer::ProtocolHasFeature(site.server.GetProtocol(), ProtocolFeature::TransferMode)) {
			PasvMode pasvMode = site.server.GetPasvMode();
			if (pasvMode == MODE_ACTIVE) {
				xrc_call(parent_, "ID_TRANSFERMODE_ACTIVE", &wxRadioButton::SetValue, true);
			}
			else if (pasvMode == MODE_PASSIVE) {
				xrc_call(parent_, "ID_TRANSFERMODE_PASSIVE", &wxRadioButton::SetValue, true);
			}
			else {
				xrc_call(parent_, "ID_TRANSFERMODE_DEFAULT", &wxRadioButton::SetValue, true);
			}
		}

		int const maxMultiple = site.server.MaximumMultipleConnections();
		xrc_call(parent_, "ID_LIMITMULTIPLE", &wxCheckBox::SetValue, maxMultiple != 0);
		if (maxMultiple != 0) {
			xrc_call(parent_, "ID_MAXMULTIPLE", &wxSpinCtrl::Enable, !predefined);
			xrc_call<wxSpinCtrl, int>(parent_, "ID_MAXMULTIPLE", &wxSpinCtrl::SetValue, maxMultiple);
		}
		else {
			xrc_call(parent_, "ID_MAXMULTIPLE", &wxSpinCtrl::Enable, false);
			xrc_call<wxSpinCtrl, int>(parent_, "ID_MAXMULTIPLE", &wxSpinCtrl::SetValue, 1);
		}

	}
}

void TransferSettingsSiteControls::UpdateSite(Site & site)
{
	if (xrc_call(parent_, "ID_TRANSFERMODE_ACTIVE", &wxRadioButton::GetValue)) {
		site.server.SetPasvMode(MODE_ACTIVE);
	}
	else if (xrc_call(parent_, "ID_TRANSFERMODE_PASSIVE", &wxRadioButton::GetValue)) {
		site.server.SetPasvMode(MODE_PASSIVE);
	}
	else {
		site.server.SetPasvMode(MODE_DEFAULT);
	}

	if (xrc_call(parent_, "ID_LIMITMULTIPLE", &wxCheckBox::GetValue)) {
		site.server.MaximumMultipleConnections(xrc_call(parent_, "ID_MAXMULTIPLE", &wxSpinCtrl::GetValue));
	}
	else {
		site.server.MaximumMultipleConnections(0);
	}
}

void TransferSettingsSiteControls::SetControlVisibility(ServerProtocol protocol, LogonType)
{
	bool const hasTransferMode = CServer::ProtocolHasFeature(protocol, ProtocolFeature::TransferMode);
	xrc_call(parent_, "ID_TRANSFERMODE_DEFAULT", &wxWindow::Show, hasTransferMode);
	xrc_call(parent_, "ID_TRANSFERMODE_ACTIVE", &wxWindow::Show, hasTransferMode);
	xrc_call(parent_, "ID_TRANSFERMODE_PASSIVE", &wxWindow::Show, hasTransferMode);
	auto* transferModeLabel = XRCCTRL(parent_, "ID_TRANSFERMODE_LABEL", wxStaticText);
	transferModeLabel->Show(hasTransferMode);
	transferModeLabel->GetContainingSizer()->CalcMin();
	transferModeLabel->GetContainingSizer()->Layout();
}


S3SiteControls::S3SiteControls(wxWindow & parent, DialogLayout const& lay, wxFlexGridSizer & sizer)
    : SiteControls(parent)
{
	sizer.AddGrowableCol(0);
	sizer.Add(new wxStaticText(&parent, -1, _("Server Side Encryption:")));

	sizer.Add(new wxRadioButton(&parent, XRCID("ID_S3_NOENCRYPTION"), _("N&o encryption"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP));

	sizer.Add(new wxRadioButton(&parent, XRCID("ID_S3_AES256"), _("&AWS S3 encryption")));

	sizer.Add(new wxRadioButton(&parent, XRCID("ID_S3_AWSKMS"), _("AWS &KMS encryption")));
	auto * row = lay.createFlex(2);
	row->AddGrowableCol(1);
	sizer.Add(row, 0, wxLEFT|wxGROW, lay.dlgUnits(10));
	row->Add(new wxStaticText(&parent, -1, _("&Select a key:")), lay.valign);
	auto * choice = new wxChoice(&parent, XRCID("ID_S3_KMSKEY"));
	choice->Append(_("Default (AWS/S3)"));
	choice->Append(_("Custom KMS ARN"));
	row->Add(choice, lay.valigng);
	row->Add(new wxStaticText(&parent, -1, _("C&ustom KMS ARN:")), lay.valign);
	row->Add(new wxTextCtrlEx(&parent, XRCID("ID_S3_CUSTOM_KMS")), lay.valigng);

	sizer.Add(new wxRadioButton(&parent, XRCID("ID_S3_CUSTOMER_ENCRYPTION"), _("Cu&stomer encryption")));
	row = lay.createFlex(2);
	row->AddGrowableCol(1);
	sizer.Add(row, 0, wxLEFT | wxGROW, lay.dlgUnits(10));
	row->Add(new wxStaticText(&parent, -1, _("Cus&tomer Key:")), lay.valign);
	row->Add(new wxTextCtrlEx(&parent, XRCID("ID_S3_CUSTOMER_KEY")), lay.valigng);

}

void S3SiteControls::SetSite(Site const& site, bool predefined)
{
	xrc_call(parent_, "ID_S3_KMSKEY", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_S3_NOENCRYPTION", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_S3_AES256", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_S3_AWSKMS", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_S3_KMSKEY", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_S3_CUSTOM_KMS", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_S3_CUSTOMER_ENCRYPTION", &wxWindow::Enable, !predefined);
	xrc_call(parent_, "ID_S3_CUSTOMER_KEY", &wxWindow::Enable, !predefined);

	xrc_call(parent_, "ID_S3_KMSKEY", &wxChoice::SetSelection, static_cast<int>(s3_sse::KmsKey::DEFAULT));
	auto ssealgorithm = site.server.GetExtraParameter("ssealgorithm");
	if (ssealgorithm.empty()) {
		xrc_call(parent_, "ID_S3_NOENCRYPTION", &wxRadioButton::SetValue, true);
	}
	else if (ssealgorithm == "AES256") {
		xrc_call(parent_, "ID_S3_AES256", &wxRadioButton::SetValue, true);
	}
	else if (ssealgorithm == "aws:kms") {
		xrc_call(parent_, "ID_S3_AWSKMS", &wxRadioButton::SetValue, true);
		auto sseKmsKey = site.server.GetExtraParameter("ssekmskey");
		if (!sseKmsKey.empty()) {
			xrc_call(parent_, "ID_S3_KMSKEY", &wxChoice::SetSelection, static_cast<int>(s3_sse::KmsKey::CUSTOM));
			xrc_call(parent_, "ID_S3_CUSTOM_KMS", &wxTextCtrl::ChangeValue, sseKmsKey);
		}
	}
	else if (ssealgorithm == "customer") {
		xrc_call(parent_, "ID_S3_CUSTOMER_ENCRYPTION", &wxRadioButton::SetValue, true);
		auto customerKey = site.server.GetExtraParameter("ssecustomerkey");
		xrc_call(parent_, "ID_S3_CUSTOMER_KEY", &wxTextCtrl::ChangeValue, customerKey);
	}
}

void S3SiteControls::UpdateSite(Site & site)
{
	CServer & server = site.server;
	if (server.GetProtocol() == S3) {
		if (xrc_call(parent_, "ID_S3_NOENCRYPTION", &wxRadioButton::GetValue)) {
			server.ClearExtraParameter("ssealgorithm");
		}
		else if (xrc_call(parent_, "ID_S3_AES256", &wxRadioButton::GetValue)) {
			server.SetExtraParameter("ssealgorithm", L"AES256");
		}
		else if (xrc_call(parent_, "ID_S3_AWSKMS", &wxRadioButton::GetValue)) {
			server.SetExtraParameter("ssealgorithm", L"aws:kms");
			if (xrc_call(parent_, "ID_S3_KMSKEY", &wxChoice::GetSelection) == static_cast<int>(s3_sse::KmsKey::CUSTOM)) {
				server.SetExtraParameter("ssekmskey", fz::to_wstring(xrc_call(parent_, "ID_S3_CUSTOM_KMS", &wxTextCtrl::GetValue)));
			}
		}
		else if (xrc_call(parent_, "ID_S3_CUSTOMER_ENCRYPTION", &wxRadioButton::GetValue)) {
			server.SetExtraParameter("ssealgorithm", L"customer");
			server.SetExtraParameter("ssecustomerkey", fz::to_wstring(xrc_call(parent_, "ID_S3_CUSTOMER_KEY", &wxTextCtrl::GetValue)));
		}
	}
}
