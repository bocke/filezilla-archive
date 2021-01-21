#ifndef FILEZILLA_COMMONUI_FZ_PATHS_HEADER
#define FILEZILLA_COMMONUI_FZ_PATHS_HEADER

#include "../include/local_path.h"

#include "visibility.h"

#include <vector>

struct FZCUI_PUBLIC_SYMBOL app_paths final {
	CLocalPath settings_path;
	CLocalPath defaults_path;

	std::wstring settings_file(std::wstring const& name) const {
		return settings_path.GetPath() + name + L".xml";
	}
};

CLocalPath FZCUI_PUBLIC_SYMBOL GetUnadjustedSettingsDir();

// If non-empty, always terminated by a separator
std::wstring FZCUI_PUBLIC_SYMBOL GetOwnExecutableDir();

CLocalPath FZCUI_PUBLIC_SYMBOL GetFZDataDir(std::vector<std::wstring> const& fileToFind, std::wstring const& prefixSub, bool searchSelfDir = true);
CLocalPath FZCUI_PUBLIC_SYMBOL GetDefaultsDir();
CLocalPath FZCUI_PUBLIC_SYMBOL GetSettingsDir();
CLocalPath FZCUI_PUBLIC_SYMBOL GetHomeDir();

std::string FZCUI_PUBLIC_SYMBOL ExpandPath(std::string const& dir);
std::wstring FZCUI_PUBLIC_SYMBOL ExpandPath(std::wstring const& dir);

#endif
