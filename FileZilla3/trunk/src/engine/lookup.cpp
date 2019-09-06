#include <filezilla.h>

#include "directorycache.h"
#include "lookup.h"

enum {
	lookup_init = 0,
	lookup_list
};

LookupOpData::LookupOpData(CControlSocket &controlSocket, CServerPath const &path, std::wstring const &file, CDirentry * entry)
    : COpData(Command::lookup, L"LookupOpData")
    , CProtocolOpData(controlSocket)
	, path_(path)
	, file_(file)
	, entry_(entry)
{
	if (!entry) {
		internal_entry_ = std::make_unique<CDirentry>();
		entry_ = internal_entry_.get();
	}
	entry_->clear();
}

int LookupOpData::Send()
{
	switch (opState) {
	case lookup_init:
	case lookup_list:
		if (path_.empty() || file_.empty()) {
			return FZ_REPLY_INTERNALERROR;
		}
		else {
			log(logmsg::debug_info, L"Looking for '%s' in '%s'", file_, path_.GetPath());

			// look at the cache first

			LookupFlags flags{};
			if (opState == lookup_list) {
					// Can only make a difference if running on a literal potato, or if the developer
					// stepping through this code in a debugger late at night fell asleep
					flags |= LookupFlags::allow_outdated;
			}
			auto [results, entry] = engine_.GetDirectoryCache().LookupFile(currentServer_, path_, file_, flags);

			if (results & LookupResults::found) {
				if (!entry || entry.is_unsure()) {
					log(logmsg::debug_info, L"Found unsure entry for '%s': %d", file_, entry.flags);
				}
				else {
					*entry_ = std::move(entry);
					log(logmsg::debug_info, L"Found valid entry for '%s'", file_);
					return FZ_REPLY_OK;
				}
			}
			else if (results & LookupResults::direxists) {
				log(logmsg::debug_info, L"'%s' does not appear to exist", file_);
				return FZ_REPLY_OK;
			}

			if (opState == lookup_init) {
				opState = lookup_list;
				controlSocket_.List(path_, std::wstring(), LIST_FLAG_REFRESH);
				return FZ_REPLY_CONTINUE;
			}
			else {
				log(logmsg::debug_info, L"Directory %s not in cache after a successful listing", path_.GetPath());
				return FZ_REPLY_ERROR;
			}
		}
		break;
	}

	log(logmsg::debug_warning, L"Unknown opState in LookupOpData::Send()");
	return FZ_REPLY_INTERNALERROR;
}

int LookupOpData::SubcommandResult(int prevResult, COpData const&)
{
	switch (opState) {
	case lookup_list:
		if (prevResult == FZ_REPLY_OK) {
			return FZ_REPLY_CONTINUE;
		}
		return prevResult;
	}

	log(logmsg::debug_warning, L"Unknown opState in LookupOpData::SubcommandResult()");
	return FZ_REPLY_INTERNALERROR;
}

/*
int LookupManyOpData::Send()
{
	switch (opState) {
	case resolve_init
:
		ids_.clear();

		if (path_.empty() || files_.empty()) {
			return FZ_REPLY_INTERNALERROR;
		}
		else {
			CDirectoryListing listing;

			bool outdated{};
			bool found = engine_.GetDirectoryCache().Lookup(listing, currentServer_, path_, false, outdated);
			if (found && !outdated) {
				bool unsure{};

				for (auto const& file : files_) {
					size_t pos = listing.FindFile_CmpCase(file);
					if (pos != std::string::npos) {
						if (listing[pos].is_unsure()) {
							log(logmsg::debug_info, L"File %s has id %s, but is unsure, requesting new listing", path_.FormatFilename(file), *listing[pos].ownerGroup);
							unsure = true;
							ids_.clear();
							break;
						}
						log(logmsg::debug_info, L"File %s has id %s", path_.FormatFilename(file), *listing[pos].ownerGroup);
						ids_.emplace_back(*listing[pos].ownerGroup);
					}
					else {
						ids_.emplace_back();
					}
				}
				if (!unsure) {
					return FZ_REPLY_OK;
				}
			}

			opState = resolve_waitlist;
			controlSocket_.List(path_, std::wstring(), 0);
			return FZ_REPLY_CONTINUE;
		}
		break;
	}

	log(logmsg::debug_warning, L"Unknown opState in LookupManyOpData::Send()");
	return FZ_REPLY_INTERNALERROR;
}

int LookupManyOpData::SubcommandResult(int prevResult, COpData const&)
{
	if (prevResult != FZ_REPLY_OK) {
		return prevResult;
	}

	switch (opState) {
	case resolve_waitlist: {
			CDirectoryListing listing;

			bool outdated{};
			bool found = engine_.GetDirectoryCache().Lookup(listing, currentServer_, path_, false, outdated);
			if (found && !outdated) {
				for (auto const& file : files_) {
					size_t pos = listing.FindFile_CmpCase(file);
					if (pos != std::string::npos) {
						log(logmsg::debug_info, L"File %s has id %s", path_.FormatFilename(file), *listing[pos].ownerGroup);
						ids_.emplace_back(*listing[pos].ownerGroup);
					}
					else {
						ids_.emplace_back();
					}
				}
				return FZ_REPLY_OK;
			}
		}
		log(logmsg::error, _("Files not found"));
		return FZ_REPLY_ERROR;
	}

	log(logmsg::debug_warning, L"Unknown opState in LookupManyOpData::SubcommandResult()");
	return FZ_REPLY_INTERNALERROR;
}
*/
