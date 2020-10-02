#include "../filezilla.h"

#include "../directorycache.h"
#include "file_transfer.h"

#include <libfilezilla/local_filesys.hpp>

enum FileTransferStates
{
	filetransfer_init,
	filetransfer_checkfileexists,
	filetransfer_waitfileexists,
	filetransfer_delete,
	filetransfer_transfer
};

int CStorjFileTransferOpData::Send()
{
	switch (opState) {
	case filetransfer_init:
	{
		if (localFile_.empty()) {
			if (!download()) {
				return FZ_REPLY_CRITICALERROR | FZ_REPLY_NOTSUPPORTED;
			}
			else {
				return FZ_REPLY_SYNTAXERROR;
			}
		}

		if (!remotePath_.SegmentCount()) {
			if (!download()) {
				log(logmsg::error, _("You cannot upload files into the root directory."));
			}
			return FZ_REPLY_CRITICALERROR | FZ_REPLY_NOTSUPPORTED;
		}

		if (download()) {
			std::wstring filename = remotePath_.FormatFilename(remoteFile_);
			log(logmsg::status, _("Starting download of %s"), filename);
		}
		else {
			log(logmsg::status, _("Starting upload of %s"), localFile_);
		}

		int64_t size;
		bool isLink;
		if (fz::local_filesys::get_file_info(fz::to_native(localFile_), isLink, &size, 0, 0) == fz::local_filesys::file) {
			localFileSize_ = size;
		}

		if (remotePath_.GetType() == DEFAULT) {
			remotePath_.SetType(currentServer_.GetType());
		}

		bool needs_listing = false;

		// Get information about remote file
		CDirentry entry;
		bool dirDidExist;
		bool matchedCase;
		bool found = engine_.GetDirectoryCache().LookupFile(entry, currentServer_, remotePath_, remoteFile_, dirDidExist, matchedCase);
		if (found) {
			if (entry.is_unsure()) {
				needs_listing = true;
			}
			else {
				if (matchedCase) {
					remoteFileSize_ = entry.size;
					if (entry.has_date()) {
						fileTime_ = entry.time;
					}
				}
			}
		}
		else {
			if (!dirDidExist) {
				needs_listing = true;
			}
		}

		if (needs_listing) {
			controlSocket_.List(remotePath_, L"", LIST_FLAG_REFRESH);
			return FZ_REPLY_CONTINUE;
		}

		opState = filetransfer_checkfileexists;
		return FZ_REPLY_CONTINUE;
	}
	case filetransfer_checkfileexists:
		{
			int res = controlSocket_.CheckOverwriteFile();
			if (res != FZ_REPLY_OK) {
				opState = filetransfer_waitfileexists;
				return res;
			}

			opState = filetransfer_transfer;
		}
		return FZ_REPLY_CONTINUE;

	case filetransfer_waitfileexists:
//		if (!download() && !fileId_.empty()) {
//			controlSocket_.Delete(remotePath_, std::vector<std::wstring>{remoteFile_});
//			opState = filetransfer_delete;
//		}
//		else {
			opState = filetransfer_transfer;
//		}
		return FZ_REPLY_CONTINUE;
	case filetransfer_transfer:

		if (download()) {
			if (!resume_) {
				controlSocket_.CreateLocalDir(localFile_);
			}
			engine_.transfer_status_.Init(remoteFileSize_, 0, false);
		}
		else {
			engine_.transfer_status_.Init(localFileSize_, 0, false);
		}

		engine_.transfer_status_.SetStartTime();
		transferInitiated_ = true;
		if (download()) {
			return controlSocket_.SendCommand(L"get " + controlSocket_.QuoteFilename(remotePath_.FormatFilename(remoteFile_)) + L" " + controlSocket_.QuoteFilename(localFile_));
		}
		else {
			return controlSocket_.SendCommand(L"put " + controlSocket_.QuoteFilename(localFile_) + L" " + controlSocket_.QuoteFilename(remotePath_.FormatFilename(remoteFile_)));
		}

		return FZ_REPLY_WOULDBLOCK;
	}

	log(logmsg::debug_warning, L"Unknown opState in CStorjFileTransferOpData::Send()");
	return FZ_REPLY_INTERNALERROR;
}

int CStorjFileTransferOpData::ParseResponse()
{
	if (opState == filetransfer_transfer) {
		return controlSocket_.result_;
	}

	log(logmsg::debug_warning, L"CStorjFileTransferOpData::ParseResponse called at improper time: %d", opState);
	return FZ_REPLY_INTERNALERROR;
}

int CStorjFileTransferOpData::SubcommandResult(int prevResult, COpData const&)
{
	switch (opState) {
	case filetransfer_init:
		if (prevResult == FZ_REPLY_OK) {
			// Get information about remote file
			CDirentry entry;
			bool dirDidExist;
			bool matchedCase;
			bool found = engine_.GetDirectoryCache().LookupFile(entry, currentServer_, remotePath_, remoteFile_, dirDidExist, matchedCase);
			if (found) {
				if (matchedCase) {
					remoteFileSize_ = entry.size;
					if (entry.has_date()) {
						fileTime_ = entry.time;
					}
				}
			}
		}

		opState = filetransfer_checkfileexists;
		return FZ_REPLY_CONTINUE;
	case filetransfer_delete:
		opState = filetransfer_transfer;
		return FZ_REPLY_CONTINUE;
	}

	log(logmsg::debug_warning, L"Unknown opState in CStorjFileTransferOpData::SubcommandResult()");
	return FZ_REPLY_INTERNALERROR;
}

