#include <filezilla.h>
#include "sitemanager_site.h"

#include "filezillaapp.h"
#include "fzputtygen_interface.h"
#include "Options.h"
#if USE_MAC_SANDBOX
#include "osx_sandbox_userdirs.h"
#endif
#include "sitemanager_controls.h"
#include "sitemanager_dialog.h"
#if ENABLE_STORJ
#include "storj_key_interface.h"
#endif
#include "textctrlex.h"
#include "xrc_helper.h"

#include <s3sse.h>

#include <libfilezilla/translate.hpp>

#include <wx/dcclient.h>
#include <wx/gbsizer.h>
#include <wx/hyperlink.h>
#include <wx/statline.h>

#ifdef __WXMSW__
#include "commctrl.h"
#endif

#include <array>

BEGIN_EVENT_TABLE(CSiteManagerSite, wxNotebook)
EVT_CHOICE(XRCID("ID_PROTOCOL"), CSiteManagerSite::OnProtocolSelChanged)
EVT_CHOICE(XRCID("ID_LOGONTYPE"), CSiteManagerSite::OnLogontypeSelChanged)
EVT_BUTTON(XRCID("ID_BROWSE"), CSiteManagerSite::OnRemoteDirBrowse)
EVT_BUTTON(XRCID("ID_KEYFILE_BROWSE"), CSiteManagerSite::OnKeyFileBrowse)
EVT_BUTTON(XRCID("ID_ENCRYPTIONKEY_GENERATE"), CSiteManagerSite::OnGenerateEncryptionKey)
END_EVENT_TABLE()

namespace {
struct ProtocolGroup {
	std::wstring name;
	std::vector<std::pair<ServerProtocol, std::wstring>> protocols;
};

std::array<ProtocolGroup, 2> const& protocolGroups()
{
	static auto const groups = std::array<ProtocolGroup, 2>{{
		{
			fztranslate("FTP - File Transfer Protocol"), {
				{ FTP, fztranslate("Use explicit FTP over TLS if available") },
				{ FTPES, fztranslate("Require explicit FTP over TLS") },
				{ FTPS, fztranslate("Require implicit FTP over TLS") },
				{ INSECURE_FTP, fztranslate("Only use plain FTP (insecure)") }
			}
		},
		{
			fztranslate("WebDAV"), {
				{ WEBDAV, fztranslate("Using secure HTTPS") },
				{ INSECURE_WEBDAV, fztranslate("Using insecure HTTP") }
			}
		}
	}};
	return groups;
}

std::pair<std::array<ProtocolGroup, 2>::const_iterator, std::vector<std::pair<ServerProtocol, std::wstring>>::const_iterator> findGroup(ServerProtocol protocol)
{
	auto const& groups = protocolGroups();
	for (auto group = groups.cbegin(); group != groups.cend(); ++group) {
		for (auto entry = group->protocols.cbegin(); entry != group->protocols.cend(); ++entry) {
			if (entry->first == protocol) {
				return std::make_pair(group, entry);
			}
		}
	}

	return std::make_pair(groups.cend(), std::vector<std::pair<ServerProtocol, std::wstring>>::const_iterator());
}
}

CSiteManagerSite::CSiteManagerSite(CSiteManagerDialog &sitemanager)
    : sitemanager_(sitemanager)
{
}

bool CSiteManagerSite::Load(wxWindow* parent)
{
	Create(parent, -1);

	DialogLayout lay(static_cast<wxTopLevelWindow*>(wxGetTopLevelParent(parent)));

	{
		wxPanel* generalPage = new wxPanel(this);
		AddPage(generalPage, _("General"));

		auto* main = lay.createMain(generalPage, 1);
		main->AddGrowableCol(0);

		auto * bag = lay.createGridBag(2);
		bag->AddGrowableCol(1);
		main->Add(bag, 0, wxGROW);

		lay.gbNewRow(bag);
		lay.gbAdd(bag, new wxStaticText(generalPage, -1, _("Pro&tocol:")), lay.valign);
		lay.gbAdd(bag, new wxChoice(generalPage, XRCID("ID_PROTOCOL")), lay.valigng);

		lay.gbNewRow(bag);
		lay.gbAdd(bag, new wxStaticText(generalPage, XRCID("ID_HOST_DESC"), _("&Host:")), lay.valign);
		auto * row = lay.createFlex(0, 1);
		row->AddGrowableCol(0);
		lay.gbAdd(bag, row, lay.valigng);
		row->Add(new wxTextCtrlEx(generalPage, XRCID("ID_HOST")), lay.valigng);
		row->Add(new wxStaticText(generalPage, -1, _("&Port:")), lay.valign);
		auto* port = new wxTextCtrlEx(generalPage, XRCID("ID_PORT"), wxString(), wxDefaultPosition, wxSize(lay.dlgUnits(27), -1));
		port->SetMaxLength(5);
		row->Add(port, lay.valign);

		lay.gbNewRow(bag);
		lay.gbAdd(bag, new wxStaticText(generalPage, XRCID("ID_ENCRYPTION_DESC"), _("&Encryption:")), lay.valign);
		auto brow = new wxBoxSizer(wxHORIZONTAL);
		lay.gbAdd(bag, brow, lay.valigng);
		brow->Add(new wxChoice(generalPage, XRCID("ID_ENCRYPTION")), 1);
		brow->Add(new wxHyperlinkCtrl(generalPage, XRCID("ID_SIGNUP"), _("Signup"), L"https://app.storj.io/#/signup"), lay.valign)->Show(false);
		brow->AddSpacer(0);

		lay.gbNewRow(bag);
		lay.gbAdd(bag, new wxStaticText(generalPage, XRCID("ID_EXTRA_HOST_DESC"), L""), lay.valign)->Show(false);
		lay.gbAdd(bag, new wxTextCtrlEx(generalPage, XRCID("ID_EXTRA_HOST")), lay.valigng)->Show(false);

		lay.gbAddRow(bag, new wxStaticLine(generalPage), lay.grow);

		lay.gbNewRow(bag);
		lay.gbAdd(bag, new wxStaticText(generalPage, -1, _("&Logon Type:")), lay.valign);
		lay.gbAdd(bag, new wxChoice(generalPage, XRCID("ID_LOGONTYPE")), lay.valigng);

		lay.gbNewRow(bag);
		lay.gbAdd(bag, new wxStaticText(generalPage, XRCID("ID_USER_DESC"), _("&User:")), lay.valign);
		lay.gbAdd(bag, new wxTextCtrlEx(generalPage, XRCID("ID_USER")), lay.valigng);

		lay.gbNewRow(bag);
		lay.gbAdd(bag, new wxStaticText(generalPage, XRCID("ID_EXTRA_USER_DESC"), L""), lay.valign)->Show(false);
		lay.gbAdd(bag, new wxTextCtrlEx(generalPage, XRCID("ID_EXTRA_USER")), lay.valigng)->Show(false);

		lay.gbNewRow(bag);
		lay.gbAdd(bag, new wxStaticText(generalPage, XRCID("ID_PASS_DESC"), _("Pass&word:")), lay.valign);
		lay.gbAdd(bag, new wxTextCtrlEx(generalPage, XRCID("ID_PASS"), L"", wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD), lay.valigng);

		lay.gbNewRow(bag);
		lay.gbAdd(bag, new wxStaticText(generalPage, XRCID("ID_ACCOUNT_DESC"), _("&Account:")), lay.valign);
		lay.gbAdd(bag, new wxTextCtrlEx(generalPage, XRCID("ID_ACCOUNT")), lay.valigng);

		lay.gbNewRow(bag);
		lay.gbAdd(bag, new wxStaticText(generalPage, XRCID("ID_KEYFILE_DESC"), _("&Key file:")), lay.valign)->Show(false);
		row = lay.createFlex(0, 1);
		row->AddGrowableCol(0);
		lay.gbAdd(bag, row, lay.valigng);
		row->Add(new wxTextCtrlEx(generalPage, XRCID("ID_KEYFILE")), lay.valigng)->Show(false);
		row->Add(new wxButton(generalPage, XRCID("ID_KEYFILE_BROWSE"), _("Browse...")), lay.valign)->Show(false);

		lay.gbNewRow(bag);
		lay.gbAdd(bag, new wxStaticText(generalPage, XRCID("ID_ENCRYPTIONKEY_DESC"), _("Encryption &key:")), lay.valign)->Show(false);
		row = lay.createFlex(0, 1);
		row->AddGrowableCol(0);
		lay.gbAdd(bag, row, lay.valigng);
		row->Add(new wxTextCtrlEx(generalPage, XRCID("ID_ENCRYPTIONKEY")), lay.valigng)->Show(false);
		row->Add(new wxButton(generalPage, XRCID("ID_ENCRYPTIONKEY_GENERATE"), _("Generate...")), lay.valign)->Show(false);

		lay.gbNewRow(bag);
		lay.gbAdd(bag, new wxStaticText(generalPage, XRCID("ID_EXTRA_CREDENTIALS_DESC"), L""), lay.valign)->Show(false);
		lay.gbAdd(bag, new wxTextCtrlEx(generalPage, XRCID("ID_EXTRA_CREDENTIALS"), L"", wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD), lay.valigng)->Show(false);

		lay.gbNewRow(bag);
		lay.gbAdd(bag, new wxStaticText(generalPage, XRCID("ID_EXTRA_EXTRA_DESC"), L""), lay.valign)->Show(false);
		lay.gbAdd(bag, new wxTextCtrlEx(generalPage, XRCID("ID_EXTRA_EXTRA")), lay.valigng)->Show(false);

		main->Add(new wxStaticLine(generalPage), lay.grow);

		row = lay.createFlex(0, 1);
		main->Add(row);
		row->Add(new wxStaticText(generalPage, -1, _("&Background color:")), lay.valign);
		row->Add(new wxChoice(generalPage, XRCID("ID_COLOR")), lay.valign);

		main->Add(new wxStaticText(generalPage, -1, _("Co&mments:")));
		main->Add(new wxTextCtrlEx(generalPage, XRCID("ID_COMMENTS"), L"", wxDefaultPosition, wxSize(-1, lay.dlgUnits(43)), wxTE_MULTILINE), 1, wxGROW);
		main->AddGrowableRow(main->GetEffectiveRowsCount() - 1);
	}

#if 1
	{
		advancedPage_ = new wxPanel(this);
		AddPage(advancedPage_, _("Advanced"));

		auto * main = lay.createMain(advancedPage_, 1);
		main->AddGrowableCol(0);
		auto* row = lay.createFlex(0, 1);
		main->Add(row);

		row->Add(new wxStaticText(advancedPage_, XRCID("ID_SERVERTYPE_LABEL"), _("Server &type:")), lay.valign);
		row->Add(new wxChoice(advancedPage_, XRCID("ID_SERVERTYPE")), lay.valign);
		main->AddSpacer(0);
		main->Add(new wxCheckBox(advancedPage_, XRCID("ID_BYPASSPROXY"), _("B&ypass proxy")));

		main->Add(new wxStaticLine(advancedPage_), lay.grow);

		main->Add(new wxStaticText(advancedPage_, -1, _("Default &local directory:")));

		row = lay.createFlex(0, 1);
		main->Add(row, lay.grow);
		row->AddGrowableCol(0);
		row->Add(new wxTextCtrlEx(advancedPage_, XRCID("ID_LOCALDIR")), lay.valigng);
		row->Add(new wxButton(advancedPage_, XRCID("ID_BROWSE"), _("&Browse...")), lay.valign);
		main->AddSpacer(0);
		main->Add(new wxStaticText(advancedPage_, -1, _("Default r&emote directory:")));
		main->Add(new wxTextCtrlEx(advancedPage_, XRCID("ID_REMOTEDIR")), lay.grow);
		main->AddSpacer(0);
		main->Add(new wxCheckBox(advancedPage_, XRCID("ID_SYNC"), _("&Use synchronized browsing")));
		main->Add(new wxCheckBox(advancedPage_, XRCID("ID_COMPARISON"), _("Directory comparison")));

		main->Add(new wxStaticLine(advancedPage_), lay.grow);

		main->Add(new wxStaticText(advancedPage_, -1, _("&Adjust server time, offset by:")));
		row = lay.createFlex(0, 1);
		main->Add(row);
		auto* hours = new wxSpinCtrl(advancedPage_, XRCID("ID_TIMEZONE_HOURS"), wxString(), wxDefaultPosition, wxSize(lay.dlgUnits(26), -1));
		hours->SetRange(-24, 24);
		row->Add(hours, lay.valign);
		row->Add(new wxStaticText(advancedPage_, -1, _("Hours,")), lay.valign);
		auto* minutes = new wxSpinCtrl(advancedPage_, XRCID("ID_TIMEZONE_MINUTES"), wxString(), wxDefaultPosition, wxSize(lay.dlgUnits(26), -1));
		minutes->SetRange(-59, 59);
		row->Add(minutes, lay.valign);
		row->Add(new wxStaticText(advancedPage_, -1, _("Minutes")), lay.valign);
	}

	{
		transferPage_ = new wxPanel(this);
		AddPage(transferPage_, _("Transfer Settings"));
		auto * main = lay.createMain(transferPage_, 1);
		controls_.emplace_back(std::make_unique<TransferSettingsSiteControls>(*transferPage_, lay, *main));
	}

	{
		charsetPage_ = new wxPanel(this);
		AddPage(charsetPage_, _("Charset"));
		auto * main = lay.createMain(charsetPage_, 1);
		controls_.emplace_back(std::make_unique<CharsetSiteControls>(*charsetPage_, lay, *main));

		int const charsetPageIndex = FindPage(charsetPage_);
		m_charsetPageText = GetPageText(charsetPageIndex);
		wxGetApp().GetWrapEngine()->WrapRecursive(charsetPage_, 1.3);
	}

	{
		s3Page_ = new wxPanel(this);
		AddPage(s3Page_, L"S3");
		auto * main = lay.createMain(s3Page_, 1);
		controls_.emplace_back(std::make_unique<S3SiteControls>(*s3Page_, lay, *main));
	}
#endif

	extraParameters_[ParameterSection::host].emplace_back(XRCCTRL(*this, "ID_EXTRA_HOST_DESC", wxStaticText), XRCCTRL(*this, "ID_EXTRA_HOST", wxTextCtrl));
	extraParameters_[ParameterSection::user].emplace_back(XRCCTRL(*this, "ID_EXTRA_USER_DESC", wxStaticText), XRCCTRL(*this, "ID_EXTRA_USER", wxTextCtrl));
	extraParameters_[ParameterSection::credentials].emplace_back(XRCCTRL(*this, "ID_EXTRA_CREDENTIALS_DESC", wxStaticText), XRCCTRL(*this, "ID_EXTRA_CREDENTIALS", wxTextCtrl));
	extraParameters_[ParameterSection::extra].emplace_back(XRCCTRL(*this, "ID_EXTRA_EXTRA_DESC", wxStaticText), XRCCTRL(*this, "ID_EXTRA_EXTRA", wxTextCtrl));

	InitProtocols();

	m_totalPages = GetPageCount();

	auto generalSizer = static_cast<wxGridBagSizer*>(xrc_call(*this, "ID_PROTOCOL", &wxWindow::GetContainingSizer));
	generalSizer->SetEmptyCellSize(wxSize(-generalSizer->GetHGap(), -generalSizer->GetVGap()));

	GetPage(0)->GetSizer()->Fit(GetPage(0));

#ifdef __WXMSW__
	// Make pages at least wide enough to fit all tabs
	HWND hWnd = (HWND)GetHandle();

	int width = 4;
	for (unsigned int i = 0; i < GetPageCount(); ++i) {
		RECT tab_rect{};
		if (TabCtrl_GetItemRect(hWnd, i, &tab_rect)) {
			width += tab_rect.right - tab_rect.left;
		}
	}
#else
	// Make pages at least wide enough to fit all tabs
	int width = 10; // Guessed
	wxClientDC dc(this);
	for (unsigned int i = 0; i < GetPageCount(); ++i) {
		wxCoord w, h;
		dc.GetTextExtent(GetPageText(i), &w, &h);

		width += w;
#ifdef __WXMAC__
		width += 20; // Guessed
#else
		width += 20;
#endif
	}
#endif

	wxSize const descSize = XRCCTRL(*this, "ID_ENCRYPTION_DESC", wxWindow)->GetSize();
	wxSize const encSize = XRCCTRL(*this, "ID_ENCRYPTION", wxWindow)->GetSize();

	int dataWidth = std::max(encSize.GetWidth(), XRCCTRL(*this, "ID_PROTOCOL", wxWindow)->GetSize().GetWidth());

	width = std::max(width, static_cast<int>(descSize.GetWidth() * 2 + dataWidth + generalSizer->GetHGap() * 3));

	wxSize page_min_size = GetPage(0)->GetSizer()->GetMinSize();
	if (page_min_size.x < width) {
		page_min_size.x = width;
		GetPage(0)->GetSizer()->SetMinSize(page_min_size);
	}

	// Set min height of general page sizer
	generalSizer->SetMinSize(generalSizer->GetMinSize());

	// Set min height of encryption row
	auto encSizer = xrc_call(*this, "ID_ENCRYPTION", &wxWindow::GetContainingSizer);
	encSizer->GetItem(encSizer->GetItemCount() - 1)->SetMinSize(0, std::max(descSize.GetHeight(), encSize.GetHeight()));

	return true;
}

void CSiteManagerSite::InitProtocols()
{
	wxChoice *pProtocol = XRCCTRL(*this, "ID_PROTOCOL", wxChoice);
	if (!pProtocol) {
		return;
	}

	for (auto const& proto : CServer::GetDefaultProtocols()) {
		auto const entry = findGroup(proto);
		if (entry.first != protocolGroups().cend()) {
			if (entry.second == entry.first->protocols.cbegin()) {
				mainProtocolListIndex_[proto] = pProtocol->Append(entry.first->name);
			}
			else {
				mainProtocolListIndex_[proto] = mainProtocolListIndex_[entry.first->protocols.front().first];
			}
		}
		else {
			mainProtocolListIndex_[proto] = pProtocol->Append(CServer::GetProtocolName(proto));
		}
	}

	wxChoice *pChoice = dynamic_cast<wxChoice*>(FindWindow(XRCID("ID_SERVERTYPE")));
	if (pChoice) {
		for (int i = 0; i < SERVERTYPE_MAX; ++i) {
			pChoice->Append(CServer::GetNameFromServerType(static_cast<ServerType>(i)));
		}

		pChoice = XRCCTRL(*this, "ID_LOGONTYPE", wxChoice);
		wxASSERT(pChoice);
		for (int i = 0; i < static_cast<int>(LogonType::count); ++i) {
			pChoice->Append(GetNameFromLogonType(static_cast<LogonType>(i)));
		}
	}

	wxChoice *pColors = dynamic_cast<wxChoice*>(FindWindow(XRCID("ID_COLOR")));
	if (pColors) {
		for (int i = 0; ; ++i) {
			wxString name = CSiteManager::GetColourName(i);
			if (name.empty()) {
				break;
			}
			pColors->AppendString(wxGetTranslation(name));
		}
	}
}

void CSiteManagerSite::SetProtocol(ServerProtocol protocol)
{
	wxChoice* pProtocol = XRCCTRL(*this, "ID_PROTOCOL", wxChoice);
	wxChoice* pEncryption = XRCCTRL(*this, "ID_ENCRYPTION", wxChoice);
	wxStaticText* pEncryptionDesc = XRCCTRL(*this, "ID_ENCRYPTION_DESC", wxStaticText);

	auto const entry = findGroup(protocol);
	if (entry.first != protocolGroups().cend()) {
		pEncryption->Clear();
		for (auto const& prot : entry.first->protocols) {
			std::wstring name = prot.second;
			if (!CServer::ProtocolHasFeature(prot.first, ProtocolFeature::Security)) {
				name += ' ';
				name += 0x26a0; // Unicode's warning emoji
				name += 0xfe0f; // Variant selector, makes it colorful
			}
			pEncryption->AppendString(name);
		}
		pEncryption->Show();
		pEncryptionDesc->Show();
		pEncryption->SetSelection(entry.second - entry.first->protocols.cbegin());
	}
	else {
		pEncryption->Hide();
		pEncryptionDesc->Hide();
	}

	auto const protoIt = mainProtocolListIndex_.find(protocol);
	if (protoIt != mainProtocolListIndex_.cend()) {
		pProtocol->SetSelection(protoIt->second);
	}
	else if (protocol != ServerProtocol::UNKNOWN) {
		auto const entry = findGroup(protocol);
		if (entry.first != protocolGroups().cend()) {
			mainProtocolListIndex_[protocol] = pProtocol->Append(entry.first->name);
			for (auto const& sub : entry.first->protocols) {
				mainProtocolListIndex_[sub.first] = mainProtocolListIndex_[protocol];
			}
		}
		else {
			mainProtocolListIndex_[protocol] = pProtocol->Append(CServer::GetProtocolName(protocol));
		}

		pProtocol->SetSelection(mainProtocolListIndex_[protocol]);
	}
	else {
		pProtocol->SetSelection(mainProtocolListIndex_[FTP]);
	}
	UpdateHostFromDefaults(GetProtocol());

	previousProtocol_ = protocol;
}

ServerProtocol CSiteManagerSite::GetProtocol() const
{
	int const sel = xrc_call(*this, "ID_PROTOCOL", &wxChoice::GetSelection);

	ServerProtocol protocol = UNKNOWN;
	for (auto const it : mainProtocolListIndex_) {
		if (it.second == sel) {
			protocol = it.first;
			break;
		}
	}

	auto const group = findGroup(protocol);
	if (group.first != protocolGroups().cend()) {
		int encSel = xrc_call(*this, "ID_ENCRYPTION", &wxChoice::GetSelection);
		if (encSel < 0 || encSel >= static_cast<int>(group.first->protocols.size())) {
			encSel = 0;
		}
		protocol = group.first->protocols[encSel].first;
	}

	return protocol;
}

void CSiteManagerSite::SetControlVisibility(ServerProtocol protocol, LogonType type)
{
	auto const group = findGroup(protocol);
	bool const isFtp = group.first != protocolGroups().cend() && group.first->protocols.front().first == FTP;

	xrc_call(*this, "ID_ENCRYPTION_DESC", &wxStaticText::Show, group.first != protocolGroups().cend());
	xrc_call(*this, "ID_ENCRYPTION", &wxChoice::Show, group.first != protocolGroups().cend());

	xrc_call(*this, "ID_SIGNUP", &wxControl::Show, protocol == STORJ);

	auto const supportedlogonTypes = GetSupportedLogonTypes(protocol);
	assert(!supportedlogonTypes.empty());

	auto choice = XRCCTRL(*this, "ID_LOGONTYPE", wxChoice);
	choice->Clear();

	if (std::find(supportedlogonTypes.cbegin(), supportedlogonTypes.cend(), type) == supportedlogonTypes.cend()) {
		type = supportedlogonTypes.front();
	}

	for (auto const supportedLogonType : supportedlogonTypes) {
		choice->Append(GetNameFromLogonType(supportedLogonType));
		if (supportedLogonType == type) {
			choice->SetSelection(choice->GetCount() - 1);
		}
	}

	bool const hasUser = ProtocolHasUser(protocol) && type != LogonType::anonymous;

	xrc_call(*this, "ID_USER_DESC", &wxStaticText::Show, hasUser);
	xrc_call(*this, "ID_USER", &wxTextCtrl::Show, hasUser);
	xrc_call(*this, "ID_PASS_DESC", &wxStaticText::Show, type != LogonType::anonymous && type != LogonType::interactive  && (protocol != SFTP || type != LogonType::key));
	xrc_call(*this, "ID_PASS", &wxTextCtrl::Show, type != LogonType::anonymous && type != LogonType::interactive && (protocol != SFTP || type != LogonType::key));
	xrc_call(*this, "ID_ACCOUNT_DESC", &wxStaticText::Show, isFtp && type == LogonType::account);
	xrc_call(*this, "ID_ACCOUNT", &wxTextCtrl::Show, isFtp && type == LogonType::account);
	xrc_call(*this, "ID_KEYFILE_DESC", &wxStaticText::Show, protocol == SFTP && type == LogonType::key);
	xrc_call(*this, "ID_KEYFILE", &wxTextCtrl::Show, protocol == SFTP && type == LogonType::key);
	xrc_call(*this, "ID_KEYFILE_BROWSE", &wxButton::Show, protocol == SFTP && type == LogonType::key);

	xrc_call(*this, "ID_ENCRYPTIONKEY_DESC", &wxStaticText::Show, protocol == STORJ);
	xrc_call(*this, "ID_ENCRYPTIONKEY", &wxTextCtrl::Show, protocol == STORJ);
	xrc_call(*this, "ID_ENCRYPTIONKEY_GENERATE", &wxButton::Show, protocol == STORJ);

	wxString hostLabel = _("&Host:");
	wxString hostHint;
	wxString userHint;
	wxString userLabel = _("&User:");
	wxString passLabel = _("Pass&word:");
	switch (protocol) {
	case S3:
		// @translator: Keep short
		userLabel = _("&Access key ID:");
		// @translator: Keep short
		passLabel = _("Secret Access &Key:");
		break;
	case AZURE_FILE:
	case AZURE_BLOB:
		// @translator: Keep short
		userLabel = _("Storage &account:");
		passLabel = _("Access &Key:");
		break;
	case GOOGLE_CLOUD:
		userLabel = _("Pro&ject ID:");
		break;
	case SWIFT:
		// @translator: Keep short
		hostLabel = _("Identity &host:");
		// @translator: Keep short
		hostHint = _("Host name of identity service");
		userLabel = _("Pro&ject:");
		// @translator: Keep short
		userHint = _("Project (or tenant) name or ID");
		break;
	case B2:
		// @translator: Keep short
		userLabel = _("&Account ID:");
		// @translator: Keep short
		passLabel = _("Application &Key:");
		break;
	default:
		break;
	}
	xrc_call(*this, "ID_HOST_DESC", &wxStaticText::SetLabel, hostLabel);
	xrc_call(*this, "ID_HOST", &wxTextCtrl::SetHint, hostHint);
	xrc_call(*this, "ID_USER_DESC", &wxStaticText::SetLabel, userLabel);
	xrc_call(*this, "ID_PASS_DESC", &wxStaticText::SetLabel, passLabel);
	xrc_call(*this, "ID_USER", &wxTextCtrl::SetHint, userHint);

	auto InsertRow = [this](std::vector<std::pair<wxStaticText*, wxTextCtrl*>> & rows, bool password) {

		if (rows.empty()) {
			return rows.end();
		}

		wxGridBagSizer* sizer = dynamic_cast<wxGridBagSizer*>(rows.back().first->GetContainingSizer());
		if (!sizer) {
			return rows.end();
		}
		auto pos = sizer->GetItemPosition(rows.back().first);

		for (int row = sizer->GetRows() - 1; row > pos.GetRow(); --row) {
			auto left = sizer->FindItemAtPosition(wxGBPosition(row, 0));
			auto right = sizer->FindItemAtPosition(wxGBPosition(row, 1));
			if (!left) {
				break;
			}
			left->SetPos(wxGBPosition(row + 1, 0));
			if (right) {
				right->SetPos(wxGBPosition(row + 1, 1));
			}
		}
		auto label = new wxStaticText(rows.back().first->GetParent(), wxID_ANY, L"");
		auto text = new wxTextCtrlEx(rows.back().first->GetParent(), wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, password ? wxTE_PASSWORD : 0);
		sizer->Add(label, wxGBPosition(pos.GetRow() + 1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
		sizer->Add(text, wxGBPosition(pos.GetRow() + 1, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL|wxGROW);

		rows.emplace_back(label, text);
		return rows.end() - 1;
	};

	auto SetLabel = [](wxStaticText & label, ServerProtocol const, std::string const& name) {
		if (name == "login_hint") {
			label.SetLabel(_("Login (optional):"));
		}
		else if (name == "identpath") {
			// @translator: Keep short
			label.SetLabel(_("Identity service path:"));
		}
		else if (name == "identuser") {
			label.SetLabel(_("&User:"));
		}
		else {
			label.SetLabel(name);
		}
	};

	std::vector<std::pair<wxStaticText*, wxTextCtrl*>>::iterator paramIt[ParameterSection::section_count];
	for (int i = 0; i < ParameterSection::section_count; ++i) {
		paramIt[i] = extraParameters_[i].begin();
	}

	std::vector<ParameterTraits> const& parameterTraits = ExtraServerParameterTraits(protocol);
	for (auto const& trait : parameterTraits) {
		if (trait.section_ == ParameterSection::custom) {
			continue;
		}
		auto & parameters = extraParameters_[trait.section_];
		auto & it = paramIt[trait.section_];

		if (it == parameters.cend()) {
			it = InsertRow(parameters, trait.section_ == ParameterSection::credentials);
		}

		if (it == parameters.cend()) {
			continue;
		}
		it->first->Show();
		it->second->Show();
		SetLabel(*it->first, protocol, trait.name_);
		it->second->SetHint(trait.hint_);

		++it;
	}

	auto encSizer = xrc_call(*this, "ID_ENCRYPTION", &wxWindow::GetContainingSizer);
	encSizer->Show(encSizer->GetItemCount() - 1, paramIt[ParameterSection::host] == extraParameters_[ParameterSection::host].cbegin());

	for (int i = 0; i < ParameterSection::section_count; ++i) {
		for (; paramIt[i] != extraParameters_[i].cend(); ++paramIt[i]) {
			paramIt[i]->first->Hide();
			paramIt[i]->second->Hide();
		}
	}

	auto keyfileSizer = xrc_call(*this, "ID_KEYFILE_DESC", &wxStaticText::GetContainingSizer);
	if (keyfileSizer) {
		keyfileSizer->CalcMin();
		keyfileSizer->Layout();
	}

	auto encryptionkeySizer = xrc_call(*this, "ID_ENCRYPTIONKEY_DESC", &wxStaticText::GetContainingSizer);
	if (encryptionkeySizer) {
		encryptionkeySizer->CalcMin();
		encryptionkeySizer->Layout();
	}

	bool const hasServerType = CServer::ProtocolHasFeature(protocol, ProtocolFeature::ServerType);
	xrc_call(*this, "ID_SERVERTYPE_LABEL", &wxWindow::Show, hasServerType);
	xrc_call(*this, "ID_SERVERTYPE", &wxWindow::Show, hasServerType);
	auto * serverTypeSizer = xrc_call(*this, "ID_SERVERTYPE_LABEL", &wxWindow::GetContainingSizer)->GetContainingWindow()->GetSizer();
	serverTypeSizer->CalcMin();
	serverTypeSizer->Layout();

	for (auto & controls : controls_) {
		controls->SetControlVisibility(protocol, type);
	}

	if (charsetPage_) {
		if (CServer::ProtocolHasFeature(protocol, ProtocolFeature::Charset)) {
			if (FindPage(charsetPage_) == wxNOT_FOUND) {
				AddPage(charsetPage_, m_charsetPageText);
			}
		}
		else {
			int const charsetPageIndex = FindPage(charsetPage_);
			if (charsetPageIndex != wxNOT_FOUND) {
				RemovePage(charsetPageIndex);
			}
		}
	}

	if (s3Page_) {
		if (protocol == S3) {
			if (FindPage(s3Page_) == wxNOT_FOUND) {
				AddPage(s3Page_, L"S3");
			}
		}
		else {
			int const s3pageIndex = FindPage(s3Page_);
			if (s3pageIndex != wxNOT_FOUND) {
				RemovePage(s3pageIndex);
			}
		}
	}

	GetPage(0)->GetSizer()->Fit(GetPage(0));
}


void CSiteManagerSite::SetLogonTypeCtrlState()
{
	LogonType const t = GetLogonType();
	xrc_call(*this, "ID_USER", &wxTextCtrl::Enable, !predefined_ && t != LogonType::anonymous);
	xrc_call(*this, "ID_PASS", &wxTextCtrl::Enable, !predefined_ && (t == LogonType::normal || t == LogonType::account));
	xrc_call(*this, "ID_ACCOUNT", &wxTextCtrl::Enable, !predefined_ && t == LogonType::account);
	xrc_call(*this, "ID_KEYFILE", &wxTextCtrl::Enable, !predefined_ && t == LogonType::key);
	xrc_call(*this, "ID_KEYFILE_BROWSE", &wxButton::Enable, !predefined_ && t == LogonType::key);
	xrc_call(*this, "ID_ENCRYPTIONKEY", &wxTextCtrl::Enable, !predefined_ && t == LogonType::normal);
	xrc_call(*this, "ID_ENCRYPTIONKEY_GENERATE", &wxButton::Enable, !predefined_ && t == LogonType::normal);

	for (int i = 0; i < ParameterSection::section_count; ++i) {
		for (auto & pair : extraParameters_[i]) {
			pair.second->Enable(!predefined_);
		}
	}
}

LogonType CSiteManagerSite::GetLogonType() const
{
	return GetLogonTypeFromName(xrc_call(*this, "ID_LOGONTYPE", &wxChoice::GetStringSelection).ToStdWstring());
}

bool CSiteManagerSite::Verify(bool predefined)
{
	std::wstring const host = xrc_call(*this, "ID_HOST", &wxTextCtrl::GetValue).ToStdWstring();
	if (host.empty()) {
		XRCCTRL(*this, "ID_HOST", wxTextCtrl)->SetFocus();
		wxMessageBoxEx(_("You have to enter a hostname."), _("Site Manager - Invalid data"), wxICON_EXCLAMATION, this);
		return false;
	}

	auto logon_type = GetLogonType();

	ServerProtocol protocol = GetProtocol();
	wxASSERT(protocol != UNKNOWN);

	if (protocol == SFTP &&
	        logon_type == LogonType::account)
	{
		XRCCTRL(*this, "ID_LOGONTYPE", wxChoice)->SetFocus();
		wxMessageBoxEx(_("'Account' logontype not supported by selected protocol"), _("Site Manager - Invalid data"), wxICON_EXCLAMATION, this);
		return false;
	}

	if (COptions::Get()->GetOptionVal(OPTION_DEFAULT_KIOSKMODE) != 0 &&
	        !predefined &&
	        (logon_type == LogonType::account || logon_type == LogonType::normal))
	{
		XRCCTRL(*this, "ID_LOGONTYPE", wxChoice)->SetFocus();
		wxString msg;
		if (COptions::Get()->OptionFromFzDefaultsXml(OPTION_DEFAULT_KIOSKMODE) && COptions::Get()->GetOptionVal(OPTION_DEFAULT_KIOSKMODE) != 0) {
			msg = _("Saving of password has been disabled by your system administrator.");
		}
		else {
			msg = _("Saving of passwords has been disabled by you.");
		}
		msg += _T("\n");
		msg += _("'Normal' and 'Account' logontypes are not available. Your entry has been changed to 'Ask for password'.");
		XRCCTRL(*this, "ID_LOGONTYPE", wxChoice)->SetStringSelection(GetNameFromLogonType(LogonType::ask));
		XRCCTRL(*this, "ID_PASS", wxTextCtrl)->ChangeValue(wxString());
		logon_type = LogonType::ask;
		wxMessageBoxEx(msg, _("Site Manager - Cannot remember password"), wxICON_INFORMATION, this);
	}

	// Set selected type
	Site site;
	site.SetLogonType(logon_type);
	site.server.SetProtocol(protocol);

	std::wstring port = xrc_call(*this, "ID_PORT", &wxTextCtrl::GetValue).ToStdWstring();
	CServerPath path;
	std::wstring error;
	if (!site.ParseUrl(host, port, std::wstring(), std::wstring(), error, path, protocol)) {
		XRCCTRL(*this, "ID_HOST", wxTextCtrl)->SetFocus();
		wxMessageBoxEx(error, _("Site Manager - Invalid data"), wxICON_EXCLAMATION, this);
		return false;
	}

	XRCCTRL(*this, "ID_HOST", wxTextCtrl)->ChangeValue(site.Format(ServerFormat::host_only));
	if (site.server.GetPort() != CServer::GetDefaultPort(site.server.GetProtocol())) {
		XRCCTRL(*this, "ID_PORT", wxTextCtrl)->ChangeValue(wxString::Format(_T("%d"), site.server.GetPort()));
	}
	else {
		XRCCTRL(*this, "ID_PORT", wxTextCtrl)->ChangeValue(wxString());
	}

	SetProtocol(site.server.GetProtocol());

	for (auto & controls : controls_) {
		if (!controls->Verify(predefined)) {
			return false;
		}
	}

	// Require username for non-anonymous, non-ask logon type
	const wxString user = XRCCTRL(*this, "ID_USER", wxTextCtrl)->GetValue();
	if (logon_type != LogonType::anonymous &&
	        logon_type != LogonType::ask &&
	        logon_type != LogonType::interactive &&
	        user.empty())
	{
		XRCCTRL(*this, "ID_USER", wxTextCtrl)->SetFocus();
		wxMessageBoxEx(_("You have to specify a user name"), _("Site Manager - Invalid data"), wxICON_EXCLAMATION, this);
		return false;
	}

	// We don't allow username of only spaces, confuses both users and XML libraries
	if (!user.empty()) {
		bool space_only = true;
		for (unsigned int i = 0; i < user.Len(); ++i) {
			if (user[i] != ' ') {
				space_only = false;
				break;
			}
		}
		if (space_only) {
			XRCCTRL(*this, "ID_USER", wxTextCtrl)->SetFocus();
			wxMessageBoxEx(_("Username cannot be a series of spaces"), _("Site Manager - Invalid data"), wxICON_EXCLAMATION, this);
			return false;
		}
	}

	// Require account for account logon type
	if (logon_type == LogonType::account &&
	        XRCCTRL(*this, "ID_ACCOUNT", wxTextCtrl)->GetValue().empty())
	{
		XRCCTRL(*this, "ID_ACCOUNT", wxTextCtrl)->SetFocus();
		wxMessageBoxEx(_("You have to enter an account name"), _("Site Manager - Invalid data"), wxICON_EXCLAMATION, this);
		return false;
	}

	// In key file logon type, check that the provided key file exists
	if (logon_type == LogonType::key) {
		std::wstring keyFile = xrc_call(*this, "ID_KEYFILE", &wxTextCtrl::GetValue).ToStdWstring();
		if (keyFile.empty()) {
			wxMessageBoxEx(_("You have to enter a key file path"), _("Site Manager - Invalid data"), wxICON_EXCLAMATION, this);
			xrc_call(*this, "ID_KEYFILE", &wxWindow::SetFocus);
			return false;
		}

		// Check (again) that the key file is in the correct format since it might have been introduced manually
		CFZPuttyGenInterface cfzg(this);

		std::wstring keyFileComment, keyFileData;
		if (cfzg.LoadKeyFile(keyFile, false, keyFileComment, keyFileData)) {
			xrc_call(*this, "ID_KEYFILE", &wxTextCtrl::ChangeValue, keyFile);
		}
		else {
			xrc_call(*this, "ID_KEYFILE", &wxWindow::SetFocus);
			return false;
		}
	}

	if (protocol == STORJ && logon_type == LogonType::normal) {
		std::wstring pw = xrc_call(*this, "ID_PASS", &wxTextCtrl::GetValue).ToStdWstring();
		std::wstring encryptionKey = xrc_call(*this, "ID_ENCRYPTIONKEY", &wxTextCtrl::GetValue).ToStdWstring();

		bool encrypted = !xrc_call(*this, "ID_PASS", &wxTextCtrl::GetHint).empty();
		if (encrypted) {
			if (pw.empty() != encryptionKey.empty()) {
				wxMessageBoxEx(_("You cannot change password and encryption key individually if using a master password."), _("Site Manager - Invalid data"), wxICON_EXCLAMATION, this);
				xrc_call(*this, "ID_ENCRYPTIONKEY", &wxWindow::SetFocus);
				return false;
			}
		}
#if ENABLE_STORJ
		if (!encryptionKey.empty() || !encrypted) {
			CStorjKeyInterface validator(this);
			if (!validator.ValidateKey(encryptionKey, false)) {
				wxMessageBoxEx(_("You have to enter a valid encryption key"), _("Site Manager - Invalid data"), wxICON_EXCLAMATION, this);
				xrc_call(*this, "ID_ENCRYPTIONKEY", &wxWindow::SetFocus);
				return false;
			}
		}
#endif
	}

	std::wstring const remotePathRaw = XRCCTRL(*this, "ID_REMOTEDIR", wxTextCtrl)->GetValue().ToStdWstring();
	if (!remotePathRaw.empty()) {
		std::wstring serverType = XRCCTRL(*this, "ID_SERVERTYPE", wxChoice)->GetStringSelection().ToStdWstring();

		CServerPath remotePath;
		remotePath.SetType(CServer::GetServerTypeFromName(serverType));
		if (!remotePath.SetPath(remotePathRaw)) {
			XRCCTRL(*this, "ID_REMOTEDIR", wxTextCtrl)->SetFocus();
			wxMessageBoxEx(_("Default remote path cannot be parsed. Make sure it is a valid absolute path for the selected server type."), _("Site Manager - Invalid data"), wxICON_EXCLAMATION, this);
			return false;
		}
	}

	std::wstring const localPath = XRCCTRL(*this, "ID_LOCALDIR", wxTextCtrl)->GetValue().ToStdWstring();
	if (XRCCTRL(*this, "ID_SYNC", wxCheckBox)->GetValue()) {
		if (remotePathRaw.empty() || localPath.empty()) {
			XRCCTRL(*this, "ID_SYNC", wxCheckBox)->SetFocus();
			wxMessageBoxEx(_("You need to enter both a local and a remote path to enable synchronized browsing for this site."), _("Site Manager - Invalid data"), wxICON_EXCLAMATION, this);
			return false;
		}
	}

	std::vector<std::pair<wxStaticText*, wxTextCtrl*>>::iterator paramIt[ParameterSection::section_count];
	for (int i = 0; i < ParameterSection::section_count; ++i) {
		paramIt[i] = extraParameters_[i].begin();
	}

	std::vector<ParameterTraits> const& parameterTraits = ExtraServerParameterTraits(protocol);
	for (auto const& trait : parameterTraits) {
		if (trait.section_ == ParameterSection::custom) {
			continue;
		}
		assert(paramIt[trait.section_] != extraParameters_[trait.section_].cend());

		if (!(trait.flags_ & ParameterTraits::optional)) {
			auto & controls = *paramIt[trait.section_];
			if (controls.second->GetValue().empty()) {
				controls.second->SetFocus();
				wxMessageBoxEx(_("You need to enter a value."), _("Site Manager - Invalid data"), wxICON_EXCLAMATION, this);
				return false;
			}
		}

		++paramIt[trait.section_];
	}

	return true;
}

void CSiteManagerSite::UpdateSite(Site &site)
{
	ServerProtocol const protocol = GetProtocol();
	wxASSERT(protocol != UNKNOWN);
	site.server.SetProtocol(protocol);

	unsigned long port;
	if (!xrc_call(*this, "ID_PORT", &wxTextCtrl::GetValue).ToULong(&port) || !port || port > 65535) {
		port = CServer::GetDefaultPort(protocol);
	}
	std::wstring host = xrc_call(*this, "ID_HOST", &wxTextCtrl::GetValue).ToStdWstring();
	// SetHost does not accept URL syntax
	if (!host.empty() && host[0] == '[') {
		host = host.substr(1, host.size() - 2);
	}
	site.server.SetHost(host, port);

	auto logon_type = GetLogonType();
	site.SetLogonType(logon_type);

	site.SetUser(xrc_call(*this, "ID_USER", &wxTextCtrl::GetValue).ToStdWstring());
	auto pw = xrc_call(*this, "ID_PASS", &wxTextCtrl::GetValue).ToStdWstring();

	if (protocol == STORJ && logon_type == LogonType::normal && (!pw.empty() || !site.credentials.encrypted_)) {
		pw += '|';
		pw += xrc_call(*this, "ID_ENCRYPTIONKEY", &wxTextCtrl::GetValue).ToStdWstring();
	}

	if (site.credentials.encrypted_) {
		if (!pw.empty()) {
			site.credentials.encrypted_ = fz::public_key();
			site.credentials.SetPass(pw);
		}
	}
	else {
		site.credentials.SetPass(pw);
	}
	site.credentials.account_ = xrc_call(*this, "ID_ACCOUNT", &wxTextCtrl::GetValue).ToStdWstring();

	site.credentials.keyFile_ = xrc_call(*this, "ID_KEYFILE", &wxTextCtrl::GetValue).ToStdWstring();

	site.comments_ = xrc_call(*this, "ID_COMMENTS", &wxTextCtrl::GetValue).ToStdWstring();
	site.m_colour = CSiteManager::GetColourFromIndex(xrc_call(*this, "ID_COLOR", &wxChoice::GetSelection));

	std::wstring const serverType = xrc_call(*this, "ID_SERVERTYPE", &wxChoice::GetStringSelection).ToStdWstring();
	site.server.SetType(CServer::GetServerTypeFromName(serverType));

	site.m_default_bookmark.m_localDir = xrc_call(*this, "ID_LOCALDIR", &wxTextCtrl::GetValue).ToStdWstring();
	site.m_default_bookmark.m_remoteDir = CServerPath();
	site.m_default_bookmark.m_remoteDir.SetType(site.server.GetType());
	site.m_default_bookmark.m_remoteDir.SetPath(xrc_call(*this, "ID_REMOTEDIR", &wxTextCtrl::GetValue).ToStdWstring());
	site.m_default_bookmark.m_sync = xrc_call(*this, "ID_SYNC", &wxCheckBox::GetValue);
	site.m_default_bookmark.m_comparison = xrc_call(*this, "ID_COMPARISON", &wxCheckBox::GetValue);

	int hours = xrc_call(*this, "ID_TIMEZONE_HOURS", &wxSpinCtrl::GetValue);
	int minutes = xrc_call(*this, "ID_TIMEZONE_MINUTES", &wxSpinCtrl::GetValue);

	site.server.SetTimezoneOffset(hours * 60 + minutes);

	UpdateExtraParameters(site.server);

	for (auto & controls : controls_) {
		controls->UpdateSite(site);
	}

	if (xrc_call(*this, "ID_BYPASSPROXY", &wxCheckBox::GetValue)) {
		site.server.SetBypassProxy(true);
	}
	else {
		site.server.SetBypassProxy(false);
	}
}

void CSiteManagerSite::UpdateExtraParameters(CServer & server)
{
	server.ClearExtraParameters();

	std::vector<std::pair<wxStaticText*, wxTextCtrl*>>::iterator paramIt[ParameterSection::section_count];
	for (int i = 0; i < ParameterSection::section_count; ++i) {
		paramIt[i] = extraParameters_[i].begin();
	}
	auto const& traits = ExtraServerParameterTraits(server.GetProtocol());
	for (auto const& trait : traits) {
		if (trait.section_ == ParameterSection::credentials || trait.section_ == ParameterSection::custom) {
			continue;
		}

		server.SetExtraParameter(trait.name_, paramIt[trait.section_]->second->GetValue().ToStdWstring());
		++paramIt[trait.section_];
	}
}

void CSiteManagerSite::SetSite(Site const& site, bool predefined)
{
	predefined_ = predefined;

	xrc_call(*this, "ID_HOST", &wxWindow::Enable, !predefined);
	xrc_call(*this, "ID_PORT", &wxWindow::Enable, !predefined);
	xrc_call(*this, "ID_PROTOCOL", &wxWindow::Enable, !predefined);
	xrc_call(*this, "ID_ENCRYPTION", &wxWindow::Enable, !predefined);
	xrc_call(*this, "ID_LOGONTYPE", &wxWindow::Enable, !predefined);
	xrc_call(*this, "ID_COLOR", &wxWindow::Enable, !predefined);
	xrc_call(*this, "ID_COMMENTS", &wxWindow::Enable, !predefined);
	if (advancedPage_) {
		xrc_call(*this, "ID_SERVERTYPE", &wxWindow::Enable, !predefined);
		xrc_call(*this, "ID_BYPASSPROXY", &wxWindow::Enable, !predefined);
		xrc_call(*this, "ID_SYNC", &wxWindow::Enable, !predefined);
		xrc_call(*this, "ID_COMPARISON", &wxCheckBox::Enable, !predefined);
		xrc_call(*this, "ID_LOCALDIR", &wxWindow::Enable, !predefined);
		xrc_call(*this, "ID_BROWSE", &wxWindow::Enable, !predefined);
		xrc_call(*this, "ID_REMOTEDIR", &wxWindow::Enable, !predefined);
		xrc_call(*this, "ID_TIMEZONE_HOURS", &wxWindow::Enable, !predefined);
		xrc_call(*this, "ID_TIMEZONE_MINUTES", &wxWindow::Enable, !predefined);
	}
	for (auto & controls : controls_) {
		controls->SetSite(site, predefined);
	}

	if (!site) {
		// Empty all site information
		xrc_call(*this, "ID_HOST", &wxTextCtrl::ChangeValue, wxString());
		xrc_call(*this, "ID_PORT", &wxTextCtrl::ChangeValue, wxString());
		SetProtocol(FTP);
		xrc_call(*this, "ID_BYPASSPROXY", &wxCheckBox::SetValue, false);
		bool const kiosk_mode = COptions::Get()->GetOptionVal(OPTION_DEFAULT_KIOSKMODE) != 0;
		auto const logonType = kiosk_mode ? LogonType::ask : LogonType::normal;
		xrc_call(*this, "ID_LOGONTYPE", &wxChoice::SetStringSelection, GetNameFromLogonType(logonType));
		xrc_call(*this, "ID_USER", &wxTextCtrl::ChangeValue, wxString());
		xrc_call(*this, "ID_PASS", &wxTextCtrl::ChangeValue, wxString());
		xrc_call(*this, "ID_PASS", &wxTextCtrl::SetHint, wxString());
		xrc_call(*this, "ID_ACCOUNT", &wxTextCtrl::ChangeValue, wxString());
		xrc_call(*this, "ID_KEYFILE", &wxTextCtrl::ChangeValue, wxString());
		xrc_call(*this, "ID_ENCRYPTIONKEY", &wxTextCtrl::ChangeValue, wxString());
		xrc_call(*this, "ID_COMMENTS", &wxTextCtrl::ChangeValue, wxString());
		xrc_call(*this, "ID_COLOR", &wxChoice::Select, 0);

		SetControlVisibility(FTP, logonType);
		SetLogonTypeCtrlState();

		xrc_call(*this, "ID_SERVERTYPE", &wxChoice::SetSelection, 0);
		xrc_call(*this, "ID_LOCALDIR", &wxTextCtrl::ChangeValue, wxString());
		xrc_call(*this, "ID_REMOTEDIR", &wxTextCtrl::ChangeValue, wxString());
		xrc_call(*this, "ID_SYNC", &wxCheckBox::SetValue, false);
		xrc_call<wxSpinCtrl, int>(*this, "ID_TIMEZONE_HOURS", &wxSpinCtrl::SetValue, 0);
		xrc_call<wxSpinCtrl, int>(*this, "ID_TIMEZONE_MINUTES", &wxSpinCtrl::SetValue, 0);
	}
	else {
		xrc_call(*this, "ID_HOST", &wxTextCtrl::ChangeValue, site.Format(ServerFormat::host_only));
		unsigned int port = site.server.GetPort();

		if (port != CServer::GetDefaultPort(site.server.GetProtocol())) {
			xrc_call(*this, "ID_PORT", &wxTextCtrl::ChangeValue, wxString::Format(_T("%d"), port));
		}
		else {
			xrc_call(*this, "ID_PORT", &wxTextCtrl::ChangeValue, wxString());
		}

		ServerProtocol protocol = site.server.GetProtocol();
		SetProtocol(protocol);
		xrc_call(*this, "ID_BYPASSPROXY", &wxCheckBox::SetValue, site.server.GetBypassProxy());

		LogonType const logonType = site.credentials.logonType_;
		xrc_call(*this, "ID_LOGONTYPE", &wxChoice::SetStringSelection, GetNameFromLogonType(logonType));

		SetControlVisibility(protocol, logonType);
		SetLogonTypeCtrlState();

		xrc_call(*this, "ID_USER", &wxTextCtrl::ChangeValue, site.server.GetUser());
		xrc_call(*this, "ID_ACCOUNT", &wxTextCtrl::ChangeValue, site.credentials.account_);

		std::wstring pass = site.credentials.GetPass();
		std::wstring encryptionKey;
		if (protocol == STORJ) {
			size_t pos = pass.rfind('|');
			if (pos != std::wstring::npos) {
				encryptionKey = pass.substr(pos + 1);
				pass = pass.substr(0, pos);
			}
		}

		if (site.credentials.encrypted_) {
			xrc_call(*this, "ID_PASS", &wxTextCtrl::ChangeValue, wxString());
			xrc_call(*this, "ID_ENCRYPTIONKEY", &wxTextCtrl::ChangeValue, wxString());

			// @translator: Keep this string as short as possible
			xrc_call(*this, "ID_PASS", &wxTextCtrl::SetHint, _("Leave empty to keep existing password."));
			for (auto & control : extraParameters_[ParameterSection::credentials]) {
				control.second->SetHint(_("Leave empty to keep existing data."));
			}
		}
		else {
			xrc_call(*this, "ID_PASS", &wxTextCtrl::ChangeValue, pass);
			xrc_call(*this, "ID_PASS", &wxTextCtrl::SetHint, wxString());
			xrc_call(*this, "ID_ENCRYPTIONKEY", &wxTextCtrl::ChangeValue, encryptionKey);

			auto it = extraParameters_[ParameterSection::credentials].begin();

			auto const& traits = ExtraServerParameterTraits(protocol);
			for (auto const& trait : traits) {
				if (trait.section_ != ParameterSection::credentials) {
					continue;
				}

				it->second->ChangeValue(site.credentials.GetExtraParameter(trait.name_));
				++it;
			}
		}

		SetExtraParameters(site.server);

		xrc_call(*this, "ID_KEYFILE", &wxTextCtrl::ChangeValue, site.credentials.keyFile_);
		xrc_call(*this, "ID_COMMENTS", &wxTextCtrl::ChangeValue, site.comments_);
		xrc_call(*this, "ID_COLOR", &wxChoice::Select, CSiteManager::GetColourIndex(site.m_colour));

		xrc_call(*this, "ID_SERVERTYPE", &wxChoice::SetSelection, site.server.GetType());
		xrc_call(*this, "ID_LOCALDIR", &wxTextCtrl::ChangeValue, site.m_default_bookmark.m_localDir);
		xrc_call(*this, "ID_REMOTEDIR", &wxTextCtrl::ChangeValue, site.m_default_bookmark.m_remoteDir.GetPath());
		xrc_call(*this, "ID_SYNC", &wxCheckBox::SetValue, site.m_default_bookmark.m_sync);
		xrc_call(*this, "ID_COMPARISON", &wxCheckBox::SetValue, site.m_default_bookmark.m_comparison);
		xrc_call<wxSpinCtrl, int>(*this, "ID_TIMEZONE_HOURS", &wxSpinCtrl::SetValue, site.server.GetTimezoneOffset() / 60);
		xrc_call<wxSpinCtrl, int>(*this, "ID_TIMEZONE_MINUTES", &wxSpinCtrl::SetValue, site.server.GetTimezoneOffset() % 60);
	}
}

void CSiteManagerSite::SetExtraParameters(CServer const& server)
{
	std::vector<std::pair<wxStaticText*, wxTextCtrl*>>::iterator paramIt[ParameterSection::section_count];
	for (int i = 0; i < ParameterSection::section_count; ++i) {
		paramIt[i] = extraParameters_[i].begin();
	}
	auto const& traits = ExtraServerParameterTraits(server.GetProtocol());
	for (auto const& trait : traits) {
		if (trait.section_ == ParameterSection::credentials || trait.section_ == ParameterSection::custom) {
			continue;
		}

		std::wstring value = server.GetExtraParameter(trait.name_);
		paramIt[trait.section_]->second->ChangeValue(value.empty() ? trait.default_ : value);
		++paramIt[trait.section_];
	}
}

void CSiteManagerSite::OnProtocolSelChanged(wxCommandEvent&)
{
	auto const protocol = GetProtocol();
	UpdateHostFromDefaults(protocol);

	CServer server;
	if (previousProtocol_ != UNKNOWN) {
		server.SetProtocol(previousProtocol_);
		UpdateExtraParameters(server);
	}
	server.SetProtocol(protocol);
	SetExtraParameters(server);

	auto const logonType = GetLogonType();
	SetControlVisibility(protocol, logonType);
	SetLogonTypeCtrlState();

	SetProtocol(protocol);
}

void CSiteManagerSite::OnLogontypeSelChanged(wxCommandEvent&)
{
	LogonType const t = GetLogonType();
	SetControlVisibility(GetProtocol(), t);
	SetLogonTypeCtrlState();
}

void CSiteManagerSite::OnRemoteDirBrowse(wxCommandEvent&)
{
	wxDirDialog dlg(this, _("Choose the default local directory"), XRCCTRL(*this, "ID_LOCALDIR", wxTextCtrl)->GetValue(), wxDD_NEW_DIR_BUTTON);
	if (dlg.ShowModal() == wxID_OK) {
		XRCCTRL(*this, "ID_LOCALDIR", wxTextCtrl)->ChangeValue(dlg.GetPath());
	}
}

void CSiteManagerSite::OnKeyFileBrowse(wxCommandEvent&)
{
	wxString wildcards(_T("PPK files|*.ppk|PEM files|*.pem|All files|*.*"));
	wxFileDialog dlg(this, _("Choose a key file"), wxString(), wxString(), wildcards, wxFD_OPEN|wxFD_FILE_MUST_EXIST);

	if (dlg.ShowModal() == wxID_OK) {
		std::wstring keyFilePath = dlg.GetPath().ToStdWstring();
		// If the selected file was a PEM file, LoadKeyFile() will automatically convert it to PPK
		// and tell us the new location.
		CFZPuttyGenInterface fzpg(this);

		std::wstring keyFileComment, keyFileData;
		if (fzpg.LoadKeyFile(keyFilePath, false, keyFileComment, keyFileData)) {
			XRCCTRL(*this, "ID_KEYFILE", wxTextCtrl)->ChangeValue(keyFilePath);
#if USE_MAC_SANDBOX
			OSXSandboxUserdirs::Get().AddFile(keyFilePath);
#endif

		}
		else {
			xrc_call(*this, "ID_KEYFILE", &wxWindow::SetFocus);
		}
	}
}

void CSiteManagerSite::OnGenerateEncryptionKey(wxCommandEvent&)
{
#if ENABLE_STORJ
	CStorjKeyInterface generator(this);
	std::wstring key = generator.GenerateKey();
	if (!key.empty()) {
		xrc_call(*this, "ID_ENCRYPTIONKEY", &wxTextCtrl::ChangeValue, wxString(key));
		xrc_call(*this, "ID_ENCRYPTIONKEY", &wxWindow::SetFocus);

		wxDialogEx dlg;
		if (dlg.Load(this, "ID_STORJ_GENERATED_KEY")) {
			dlg.WrapRecursive(&dlg, 2.5);
			dlg.GetSizer()->Fit(&dlg);
			dlg.GetSizer()->SetSizeHints(&dlg);
			xrc_call(dlg, "ID_KEY", &wxTextCtrl::ChangeValue, wxString(key));
			dlg.ShowModal();
		}
	}
#endif
}

void CSiteManagerSite::UpdateHostFromDefaults(ServerProtocol const protocol)
{
	if (protocol != previousProtocol_) {
		auto const oldDefault = std::get<0>(GetDefaultHost(previousProtocol_));
		auto const newDefault = GetDefaultHost(protocol);

		std::wstring const host = xrc_call(*this, "ID_HOST", &wxTextCtrl::GetValue).ToStdWstring();
		if (host.empty() || host == oldDefault) {
			xrc_call(*this, "ID_HOST", &wxTextCtrl::ChangeValue, std::get<0>(newDefault));
		}
		xrc_call(*this, "ID_HOST", &wxTextCtrl::SetHint, std::get<1>(newDefault));
	}
}
