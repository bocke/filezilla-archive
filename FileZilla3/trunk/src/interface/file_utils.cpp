#include "filezilla.h"

#include "file_utils.h"

#include <wx/stdpaths.h>
#ifdef FZ_WINDOWS
#include <knownfolders.h>
#include <shlobj.h>
#else
#include <wx/textfile.h>
#include <wordexp.h>
#endif

#include "Options.h"

std::wstring GetAsURL(std::wstring const& dir)
{
	// Cheap URL encode
	std::string utf8 = fz::to_utf8(dir);

	std::wstring encoded;
	encoded.reserve(utf8.size());

	char const* p = utf8.c_str();
	while (*p) {
		// List of characters that don't need to be escaped taken
		// from the BNF grammar in RFC 1738
		// Again attention seeking Windows wants special treatment...
		unsigned char const c = static_cast<unsigned char>(*p++);
		if ((c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') ||
			c == '$' ||
			c == '_' ||
			c == '-' ||
			c == '.' ||
			c == '+' ||
			c == '!' ||
			c == '*' ||
#ifndef __WXMSW__
			c == '\'' ||
#endif
			c == '(' ||
			c == ')' ||
			c == ',' ||
			c == '?' ||
			c == ':' ||
			c == '@' ||
			c == '&' ||
			c == '=' ||
			c == '/')
		{
			encoded += c;
		}
#ifdef __WXMSW__
		else if (c == '\\') {
			encoded += '/';
		}
#endif
		else {
			encoded += fz::sprintf(L"%%%x", c);
		}
	}
#ifdef __WXMSW__
	if (fz::starts_with(encoded, std::wstring(L"//"))) {
		// UNC path
		encoded = encoded.substr(2);
	}
	else {
		encoded = L"/" + encoded;
	}
#endif
	return L"file://" + encoded;
}

bool OpenInFileManager(std::wstring const& dir)
{
	bool ret = false;
#ifdef __WXMSW__
	// Unfortunately under Windows, UTF-8 encoded file:// URLs don't work, so use native paths.
	// Unfortunatelier, we cannot use this for UNC paths, have to use file:// here
	// Unfortunateliest, we again have a problem with UTF-8 characters which we cannot fix...
	if (dir.substr(0, 2) != L"\\\\" && dir != L"/") {
		ret = wxLaunchDefaultBrowser(dir);
	}
	else
#endif
	{
		std::wstring url = GetAsURL(dir);
		if (!url.empty()) {
			ret = wxLaunchDefaultBrowser(url);
		}
	}


	if (!ret) {
		wxBell();
	}

	return ret;
}

std::wstring GetSystemOpenCommand(std::wstring file, bool &program_exists)
{
	// Disallowed on MSW. On other platforms wxWidgets doens't escape properly.
	// For now don't support these files until we can replace wx with something sane.
	if (file.find('"') != std::wstring::npos) {
		return std::wstring();
	}
#ifndef __WXMSW__
	// wxWidgets doens't escape backslashes properly.
	// For now don't support these files until we can replace wx with something sane.
	if (file.find('\\') != std::wstring::npos) {
		return std::wstring();
	}
#endif

	wxFileName fn(file);

	const wxString& ext = fn.GetExt();
	if (ext.empty()) {
		return std::wstring();
	}

#ifdef __WXGTK__
	for (;;)
#endif
	{
		wxFileType* pType = wxTheMimeTypesManager->GetFileTypeFromExtension(ext);
		if (!pType) {
			return std::wstring();
		}

		std::wstring cmd = pType->GetOpenCommand(file).ToStdWstring();
		delete pType;
		if (cmd.empty()) {
			return std::wstring();
		}

		program_exists = false;

		std::wstring editor;
		bool is_dde = false;
#ifdef __WXMSW__
		if (cmd.substr(0, 7) == L"WX_DDE#") {
			// See wxWidget's wxExecute in src/msw/utilsexc.cpp
			// WX_DDE#<command>#DDE_SERVER#DDE_TOPIC#DDE_COMMAND
			editor = cmd.substr(7);
			size_t pos = editor.find('#');
			if (!pos || pos == std::wstring::npos) {
				return cmd;
			}
			editor = editor.substr(0, pos);
			is_dde = true;
		}
		else
#endif
		{
			editor = cmd;
		}

		std::wstring args;
		if (!UnquoteCommand(editor, args, is_dde) || editor.empty()) {
			return cmd;
		}

		if (!PathExpand(editor)) {
			return cmd;
		}

		if (ProgramExists(editor)) {
			program_exists = true;
		}

#ifdef __WXGTK__
		size_t pos = args.find(file);
		if (pos != std::wstring::npos && file.find(' ') != std::wstring::npos && file[0] != '\'' && file[0] != '"') {
			// Might need to quote filename, wxWidgets doesn't do it
			if ((!pos || (args[pos - 1] != '\'' && args[pos - 1] != '"')) &&
				(pos + file.size() >= args.size() || (args[pos + file.size()] != '\'' && args[pos + file.size()] != '"')))
			{
				// Filename in command arguments isn't quoted. Repeat with quoted filename
				file = L"\"" + file + L"\"";
				continue;
			}
		}
#endif
		return cmd;
	}

	return std::wstring();
}

bool UnquoteCommand(std::wstring & command, std::wstring & arguments, bool is_dde)
{
	arguments.clear();

	if (command.empty()) {
		return true;
	}

	wchar_t inQuotes = 0;
	std::wstring file;
	for (size_t i = 0; i < command.size(); i++) {
		wchar_t const& c = command[i];
		if (c == '"' || c == '\'') {
			if (!inQuotes) {
				inQuotes = c;
			}
			else if (c != inQuotes) {
				file += c;
			}
			else if (command[i + 1] == c) {
				file += c;
				i++;
			}
			else {
				inQuotes = false;
			}
		}
		else if (c == ' ' && !inQuotes) {
			arguments = fz::ltrimmed(command.substr(i + 1));
			break;
		}
		else if (is_dde && !inQuotes && (c == ',' || c == '#')) {
			arguments = fz::ltrimmed(command.substr(i + 1));
			break;
		}
		else {
			file += c;
		}
	}
	if (inQuotes) {
		return false;
	}

	command = file;

	return true;
}

bool ProgramExists(std::wstring const& editor)
{
	if (wxFileName::FileExists(editor)) {
		return true;
	}

#ifdef __WXMAC__
	if (editor.size() > 4 && editor.substr(editor.size() - 4) == L".app" && wxFileName::DirExists(editor)) {
		return true;
	}
#endif

	return false;
}

bool PathExpand(std::wstring & cmd)
{
	if (cmd.empty()) {
		return false;
	}
#ifndef __WXMSW__
	if (!cmd.empty() && cmd[0] == '/') {
		return true;
	}
#else
	if (!cmd.empty() && cmd[0] == '\\') {
		// UNC or root of current working dir, whatever that is
		return true;
	}
	if (cmd.size() > 2 && cmd[1] == ':') {
		// Absolute path
		return true;
	}
#endif

	// Need to search for program in $PATH
	std::wstring path = GetEnv("PATH");
	if (path.empty()) {
		return false;
	}

	wxString full_cmd;
	bool found = wxFindFileInPath(&full_cmd, path, cmd);
#ifdef __WXMSW__
	if (!found && !fz::equal_insensitive_ascii(cmd.substr(cmd.size() - 1), L".exe")) {
		cmd += L".exe";
		found = wxFindFileInPath(&full_cmd, path, cmd);
	}
#endif

	if (!found) {
		return false;
	}

	cmd = full_cmd;
	return true;
}

bool RenameFile(wxWindow* parent, wxString dir, wxString from, wxString to)
{
	if (dir.Right(1) != _T("\\") && dir.Right(1) != _T("/")) {
		dir += wxFileName::GetPathSeparator();
	}

#ifdef __WXMSW__
	to = to.Left(255);

	if ((to.Find('/') != -1) ||
		(to.Find('\\') != -1) ||
		(to.Find(':') != -1) ||
		(to.Find('*') != -1) ||
		(to.Find('?') != -1) ||
		(to.Find('"') != -1) ||
		(to.Find('<') != -1) ||
		(to.Find('>') != -1) ||
		(to.Find('|') != -1))
	{
		wxMessageBoxEx(_("Filenames may not contain any of the following characters: / \\ : * ? \" < > |"), _("Invalid filename"), wxICON_EXCLAMATION, parent);
		return false;
	}

	SHFILEOPSTRUCT op;
	memset(&op, 0, sizeof(op));

	from = dir + from + _T(" ");
	from.SetChar(from.Length() - 1, '\0');
	op.pFrom = from.wc_str();
	to = dir + to + _T(" ");
	to.SetChar(to.Length()-1, '\0');
	op.pTo = to.wc_str();
	op.hwnd = (HWND)parent->GetHandle();
	op.wFunc = FO_RENAME;
	op.fFlags = FOF_ALLOWUNDO;

	wxWindow * focused = wxWindow::FindFocus();

	bool res = SHFileOperation(&op) == 0;

	if (focused) {
		// Microsoft introduced a bug in Windows 10 1803: Calling SHFileOperation resets focus.
		focused->SetFocus();
	}
	
	return res;
#else
	if ((to.Find('/') != -1) ||
		(to.Find('*') != -1) ||
		(to.Find('?') != -1) ||
		(to.Find('<') != -1) ||
		(to.Find('>') != -1) ||
		(to.Find('|') != -1))
	{
		wxMessageBoxEx(_("Filenames may not contain any of the following characters: / * ? < > |"), _("Invalid filename"), wxICON_EXCLAMATION, parent);
		return false;
	}

	return wxRename(dir + from, dir + to) == 0;
#endif
}

#if defined __WXMAC__
char const* GetDownloadDirImpl();
#elif !defined(__WXMSW__)
wxString ShellUnescape(wxString const& path)
{
	wxString ret;

	const wxWX2MBbuf buf = path.mb_str();
	if (buf && *buf) {
		wordexp_t p;
		int res = wordexp(buf, &p, WRDE_NOCMD);
		if (!res && p.we_wordc == 1 && p.we_wordv) {
			ret = wxString(p.we_wordv[0], wxConvLocal);
		}
		wordfree(&p);
	}
	return ret;
}
#endif

CLocalPath GetDownloadDir()
{
#ifdef __WXMSW__
	PWSTR path;
	HRESULT result = SHGetKnownFolderPath(FOLDERID_Downloads, 0, 0, &path);
	if(result == S_OK) {
		std::wstring dir = path;
		CoTaskMemFree(path);
		return CLocalPath(dir);
	}
#elif defined(__WXMAC__)
	CLocalPath ret;
	char const* url = GetDownloadDirImpl();
	ret.SetPath(fz::to_wstring_from_utf8(url));
	return ret;
#else
	// Code copied from wx, but for downloads directory.
	// Also, directory is now unescaped.
	{
		wxLogNull logNull;
		wxString homeDir = wxFileName::GetHomeDir();
		wxString configPath;
		if (wxGetenv(wxT("XDG_CONFIG_HOME"))) {
			configPath = wxGetenv(wxT("XDG_CONFIG_HOME"));
		}
		else {
			configPath = homeDir + wxT("/.config");
		}
		wxString dirsFile = configPath + wxT("/user-dirs.dirs");
		if (wxFileExists(dirsFile)) {
			wxTextFile textFile;
			if (textFile.Open(dirsFile)) {
				size_t i;
				for (i = 0; i < textFile.GetLineCount(); ++i) {
					wxString line(textFile[i]);
					int pos = line.Find(wxT("XDG_DOWNLOAD_DIR"));
					if (pos != wxNOT_FOUND) {
						wxString value = line.AfterFirst(wxT('='));
						value = ShellUnescape(value);
						if (!value.empty() && wxDirExists(value)) {
							return CLocalPath(value.ToStdWstring());
						}
						else {
							break;
						}
					}
				}
			}
		}
	}
#endif
	return CLocalPath(wxStandardPaths::Get().GetDocumentsDir().ToStdWstring());
}

std::wstring GetExtension(std::wstring_view file)
{
	// Strip path if any
#ifdef FZ_WINDOWS
	size_t pos = file.find_last_of(L"/\\");
#else
	size_t pos = file.find_last_of(L"/");
#endif
	if (pos != std::wstring::npos) {
		file = file.substr(pos + 1);
	}

	// Find extension
	pos = file.find_last_of('.');
	if (!pos) {
		return std::wstring(L".");
	}
	else if (pos != std::wstring::npos) {
		return std::wstring(file.substr(pos + 1));
	}

	return std::wstring();
}

bool IsInvalidChar(wchar_t c, bool includeQuotes)
{
	switch (c)
	{
	case '/':
#ifdef __WXMSW__
	case '\\':
	case ':':
	case '*':
	case '?':
	case '"':
	case '<':
	case '>':
	case '|':
#endif
		return true;


	case '\'':
#ifndef __WXMSW__
	case '"':
	case '\\':
#endif
		return includeQuotes;

	default:
#ifdef __WXMSW__
		if (c < 0x20) {
			return true;
		}
#endif
		return false;
	}
}
