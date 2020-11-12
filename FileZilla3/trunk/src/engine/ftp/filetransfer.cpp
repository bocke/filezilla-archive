#include "../filezilla.h"

#include "filetransfer.h"
#include "transfersocket.h"

#include "../directorycache.h"
#include "../servercapabilities.h"
#include "../../include/engine_options.h"

#include <libfilezilla/file.hpp>
#include <libfilezilla/local_filesys.hpp>

#include <assert.h>

CFtpFileTransferOpData::CFtpFileTransferOpData(CFtpControlSocket& controlSocket, std::wstring const& local_file, std::wstring const& remote_file, CServerPath const& remote_path, transfer_flags const& flags)
	: CFileTransferOpData(L"CFtpFileTransferOpData", local_file, remote_file, remote_path, flags)
	, CFtpOpData(controlSocket)
{
	binary = !(flags & ftp_transfer_flags::ascii);
}

int CFtpFileTransferOpData::Send()
{
	std::wstring cmd;
	switch (opState)
	{
	case filetransfer_init:
		if (localFile_.empty()) {
			if (!download()) {
				return FZ_REPLY_CRITICALERROR | FZ_REPLY_NOTSUPPORTED;
			}
			else {
				return FZ_REPLY_SYNTAXERROR;
			}
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
		if (fz::local_filesys::get_file_info(fz::to_native(localFile_), isLink, &size, nullptr, nullptr) == fz::local_filesys::file) {
			localFileSize_ = size;
		}

		opState = filetransfer_waitcwd;

		if (remotePath_.GetType() == DEFAULT) {
			remotePath_.SetType(currentServer_.GetType());
		}

		controlSocket_.ChangeDir(remotePath_);
		return FZ_REPLY_CONTINUE;
	case filetransfer_size:
		cmd = L"SIZE ";
		cmd += remotePath_.FormatFilename(remoteFile_, !tryAbsolutePath_);
		break;
	case filetransfer_mdtm:
		cmd = L"MDTM ";
		cmd += remotePath_.FormatFilename(remoteFile_, !tryAbsolutePath_);
		break;
	case filetransfer_resumetest:
	case filetransfer_transfer:
		if (controlSocket_.m_pTransferSocket) {
			log(logmsg::debug_verbose, L"m_pTransferSocket != 0");
			controlSocket_.m_pTransferSocket.reset();
		}

		{
			int64_t startOffset{};
			if (download()) {
				// Potentially racy
				bool didExist = writer_factory_.size() != writer_base::npos;
				fileDidExist_ = didExist;

				if (resume_) {
					startOffset = didExist ? writer_factory_.size() : 0;
					localFileSize_ = startOffset;

					// Check resume capabilities
					if (opState == filetransfer_resumetest) {
						int res = TestResumeCapability();
						if (res != FZ_REPLY_CONTINUE || opState != filetransfer_resumetest) {
							return res;
						}
					}
				}
				else {
					localFileSize_ = 0;
				}

				resumeOffset = resume_ ? localFileSize_ : 0;
				startOffset = resumeOffset;

				engine_.transfer_status_.Init(remoteFileSize_, startOffset, false);

				/* TODO
				if (engine_.GetOptions().get_int(OPTION_PREALLOCATE_SPACE)) {
					// Try to preallocate the file in order to reduce fragmentation
					int64_t sizeToPreallocate = remoteFileSize_ - startOffset;
					if (sizeToPreallocate > 0) {
						log(logmsg::debug_info, L"Preallocating %d bytes for the file \"%s\"", sizeToPreallocate, localFile_);
						auto oldPos = pFile->seek(0, fz::file::current);
						if (oldPos >= 0) {
							if (pFile->seek(sizeToPreallocate, fz::file::end) == remoteFileSize_) {
								if (!pFile->truncate()) {
									log(logmsg::debug_warning, L"Could not preallocate the file");
								}
							}
							if (pFile->seek(oldPos, fz::file::begin) != oldPos) {
								log(logmsg::error, _("Could not seek to offset %d within file"), oldPos);
								return FZ_REPLY_ERROR;
							}
						}
					}
				}
				*/
			}
			else {
				if (resume_) {
					if (remoteFileSize_ > 0) {
						startOffset = remoteFileSize_;

						if (startOffset == localFileSize_ && binary) {
							log(logmsg::debug_info, L"No need to resume, remote file size matches local file size.");

							if (engine_.GetOptions().get_int(OPTION_PRESERVE_TIMESTAMPS) &&
								CServerCapabilities::GetCapability(currentServer_, mfmt_command) == yes)
							{
								fz::datetime mtime = fz::local_filesys::get_modification_time(fz::to_native(localFile_));
								if (!mtime.empty()) {
									fileTime_ = mtime;
									opState = filetransfer_mfmt;
									return FZ_REPLY_CONTINUE;
								}
							}
							return FZ_REPLY_OK;
						}
					}
					else {
						startOffset = 0;
					}
				}
				else {
					startOffset = 0;
				}

				if (CServerCapabilities::GetCapability(currentServer_, rest_stream) == yes) {
					// Use REST + STOR if resuming
					resumeOffset = startOffset;
				}
				else {
					// Play it safe, use APPE if resuming
					resumeOffset = 0;
				}

				engine_.transfer_status_.Init(reader_factory_.size(), startOffset, false);
			}

			controlSocket_.m_pTransferSocket = std::make_unique<CTransferSocket>(engine_, controlSocket_, download() ? TransferMode::download : TransferMode::upload);
			controlSocket_.m_pTransferSocket->m_binaryMode = binary;
			if (download()) {
				auto writer = writer_factory_.open(startOffset, *controlSocket_.m_pTransferSocket.get(), false);
				if (!writer) {
					// TODO: Handle different errors
					log(logmsg::error, _("Failed to open \"%s\" for writing"), localFile_);
					return FZ_REPLY_ERROR;
				}
				controlSocket_.m_pTransferSocket->set_writer(std::move(writer));
			}
			else {
				auto reader = reader_factory_.open(startOffset, *controlSocket_.m_pTransferSocket.get(), false);
				if (!reader) {
					// TODO: Handle different errors
					log(logmsg::error, _("Failed to open \"%s\" for reading"), localFile_);
					return FZ_REPLY_ERROR;
				}
				controlSocket_.m_pTransferSocket->set_reader(std::move(reader));
			}
		}

		if (download()) {
			cmd = L"RETR ";
		}
		else if (resume_) {
			if (CServerCapabilities::GetCapability(currentServer_, rest_stream) == yes) {
				cmd = L"STOR "; // In this case REST gets sent since resume offset was set earlier
			}
			else {
				assert(resumeOffset == 0);
				cmd = L"APPE ";
			}
		}
		else {
			cmd = L"STOR ";
		}
		cmd += remotePath_.FormatFilename(remoteFile_, !tryAbsolutePath_);

		opState = filetransfer_waittransfer;
		controlSocket_.Transfer(cmd, this);
		return FZ_REPLY_CONTINUE;
	case filetransfer_mfmt:
	{
		cmd = L"MFMT ";
		fz::datetime t = fileTime_;
		t -= fz::duration::from_minutes(currentServer_.GetTimezoneOffset());
		cmd += t.format(L"%Y%m%d%H%M%S ", fz::datetime::utc);
		cmd += remotePath_.FormatFilename(remoteFile_, !tryAbsolutePath_);

		break;
	}
	default:
		log(logmsg::debug_warning, L"Unhandled opState: %d", opState);
		return FZ_REPLY_ERROR;
	}

	if (!cmd.empty()) {
		return controlSocket_.SendCommand(cmd);
	}

	return FZ_REPLY_WOULDBLOCK;
}

int CFtpFileTransferOpData::TestResumeCapability()
{
	log(logmsg::debug_verbose, L"CFtpFileTransferOpData::TestResumeCapability()");

	if (!download()) {
		return FZ_REPLY_CONTINUE;
	}

	for (int i = 0; i < 2; ++i) {
		if (localFileSize_ >= (1ll << (i ? 31 : 32))) {
			switch (CServerCapabilities::GetCapability(currentServer_, i ? resume2GBbug : resume4GBbug))
			{
			case yes:
				if (remoteFileSize_ == localFileSize_) {
					log(logmsg::debug_info, _("Server does not support resume of files > %d GB. End transfer since file sizes match."), i ? 2 : 4);
					return FZ_REPLY_OK;
				}
				log(logmsg::error, _("Server does not support resume of files > %d GB."), i ? 2 : 4);
				return FZ_REPLY_CRITICALERROR;
			case unknown:
				if (remoteFileSize_ < localFileSize_) {
					// Don't perform test
					break;
				}
				if (remoteFileSize_ == localFileSize_) {
					log(logmsg::debug_info, _("Server may not support resume of files > %d GB. End transfer since file sizes match."), i ? 2 : 4);
					return FZ_REPLY_OK;
				}
				else if (remoteFileSize_ > localFileSize_) {
					log(logmsg::status, _("Testing resume capabilities of server"));

					opState = filetransfer_waitresumetest;
					resumeOffset = remoteFileSize_ - 1;

					controlSocket_.m_pTransferSocket = std::make_unique<CTransferSocket>(engine_, controlSocket_, TransferMode::resumetest);

					controlSocket_.Transfer(L"RETR " + remotePath_.FormatFilename(remoteFile_, !tryAbsolutePath_), this);
					return FZ_REPLY_CONTINUE;
				}
				break;
			case no:
				break;
			}
		}
	}

	return FZ_REPLY_CONTINUE;
}

int CFtpFileTransferOpData::ParseResponse()
{
	int code = controlSocket_.GetReplyCode();
	auto const& response = controlSocket_.m_Response;

	switch (opState)
	{
	case filetransfer_size:
		if (code != 2 && code != 3) {
			if (CServerCapabilities::GetCapability(currentServer_, size_command) == yes ||
				fz::str_tolower_ascii(response.substr(4)) == L"file not found" ||
				(fz::str_tolower_ascii(remotePath_.FormatFilename(remoteFile_)).find(L"file not found") == std::wstring::npos &&
					fz::str_tolower_ascii(response).find(L"file not found") != std::wstring::npos))
			{
				// Server supports SIZE command but command failed. Most likely MDTM will fail as well, so
				// skip it.
				opState = filetransfer_resumetest;

				int res = controlSocket_.CheckOverwriteFile();
				if (res != FZ_REPLY_OK) {
					return res;
				}
			}
			else {
				opState = filetransfer_mdtm;
			}
		}
		else {
			opState = filetransfer_mdtm;
			if (response.substr(0, 4) == L"213 " && response.size() > 4) {
				if (CServerCapabilities::GetCapability(currentServer_, size_command) == unknown) {
					CServerCapabilities::SetCapability(currentServer_, size_command, yes);
				}
				std::wstring str = response.substr(4);
				int64_t size = 0;
				for (auto c : str) {
					if (c < '0' || c > '9') {
						break;
					}

					size *= 10;
					size += c - '0';
				}
				remoteFileSize_ = size;
			}
			else {
				log(logmsg::debug_info, L"Invalid SIZE reply");
			}
		}
		break;
	case filetransfer_mdtm:
		opState = filetransfer_resumetest;
		if (response.substr(0, 4) == L"213 " && response.size() > 16) {
			fileTime_ = fz::datetime(response.substr(4), fz::datetime::utc);
			if (!fileTime_.empty()) {
				fileTime_ += fz::duration::from_minutes(currentServer_.GetTimezoneOffset());
			}
		}

		{
			int res = controlSocket_.CheckOverwriteFile();
			if (res != FZ_REPLY_OK) {
				return res;
			}
		}

		break;
	case filetransfer_mfmt:
		return FZ_REPLY_OK;
	default:
		log(logmsg::debug_warning, L"Unknown op state");
		return FZ_REPLY_INTERNALERROR;
	}

	return FZ_REPLY_CONTINUE;
}

int CFtpFileTransferOpData::SubcommandResult(int prevResult, COpData const&)
{
	if (opState == filetransfer_waitcwd) {
		if (prevResult == FZ_REPLY_OK) {
			CDirentry entry;
			bool dirDidExist;
			bool matchedCase;
			bool found = engine_.GetDirectoryCache().LookupFile(entry, currentServer_, tryAbsolutePath_ ? remotePath_ : currentPath_, remoteFile_, dirDidExist, matchedCase);
			if (!found) {
				if (!dirDidExist) {
					opState = filetransfer_waitlist;
				}
				else if (download() && engine_.GetOptions().get_int(OPTION_PRESERVE_TIMESTAMPS) && CServerCapabilities::GetCapability(currentServer_, mdtm_command) == yes) {
					opState = filetransfer_mdtm;
				}
				else {
					opState = filetransfer_resumetest;
				}
			}
			else {
				if (entry.is_unsure()) {
					opState = filetransfer_waitlist;
				}
				else {
					if (matchedCase) {
						remoteFileSize_ = entry.size;
						if (entry.has_date()) {
							fileTime_ = entry.time;
						}

						if (download() &&
							!entry.has_time() &&
							engine_.GetOptions().get_int(OPTION_PRESERVE_TIMESTAMPS) &&
							CServerCapabilities::GetCapability(currentServer_, mdtm_command) == yes)
						{
							opState = filetransfer_mdtm;
						}
						else {
							opState = filetransfer_resumetest;
						}
					}
					else {
						opState = filetransfer_size;
					}
				}
			}
			if (opState == filetransfer_waitlist) {
				controlSocket_.List(CServerPath(), L"", LIST_FLAG_REFRESH);
				return FZ_REPLY_CONTINUE;
			}
			else if (opState == filetransfer_resumetest) {
				int res = controlSocket_.CheckOverwriteFile();
				if (res != FZ_REPLY_OK) {
					return res;
				}
			}
		}
		else {
			tryAbsolutePath_ = true;
			opState = filetransfer_size;
		}
	}
	else if (opState == filetransfer_waitlist) {
		if (prevResult == FZ_REPLY_OK) {
			CDirentry entry;
			bool dirDidExist;
			bool matchedCase;
			bool found = engine_.GetDirectoryCache().LookupFile(entry, currentServer_, tryAbsolutePath_ ? remotePath_ : currentPath_, remoteFile_, dirDidExist, matchedCase);
			if (!found) {
				if (!dirDidExist) {
					opState = filetransfer_size;
				}
				else if (download() &&
					engine_.GetOptions().get_int(OPTION_PRESERVE_TIMESTAMPS) &&
					CServerCapabilities::GetCapability(currentServer_, mdtm_command) == yes)
				{
					opState = filetransfer_mdtm;
				}
				else {
					opState = filetransfer_resumetest;
				}
			}
			else {
				if (matchedCase && !entry.is_unsure()) {
					remoteFileSize_ = entry.size;
					if (entry.has_date()) {
						fileTime_ = entry.time;
					}

					if (download() &&
						!entry.has_time() &&
						engine_.GetOptions().get_int(OPTION_PRESERVE_TIMESTAMPS) &&
						CServerCapabilities::GetCapability(currentServer_, mdtm_command) == yes)
					{
						opState = filetransfer_mdtm;
					}
					else {
						opState = filetransfer_resumetest;
					}
				}
				else {
					opState = filetransfer_size;
				}
			}

			if (opState == filetransfer_resumetest) {
				int res = controlSocket_.CheckOverwriteFile();
				if (res != FZ_REPLY_OK) {
					return res;
				}
			}
		}
		else {
			opState = filetransfer_size;
		}
	}
	else if (opState == filetransfer_waittransfer) {
		if (prevResult == FZ_REPLY_OK && engine_.GetOptions().get_int(OPTION_PRESERVE_TIMESTAMPS)) {
			if (!download() &&
				CServerCapabilities::GetCapability(currentServer_, mfmt_command) == yes)
			{
				fz::datetime mtime = fz::local_filesys::get_modification_time(fz::to_native(localFile_));
				if (!mtime.empty()) {
					fileTime_ = mtime;
					opState = filetransfer_mfmt;
					return FZ_REPLY_CONTINUE;
				}
			}
			else if (download() && !fileTime_.empty()) {
				// TODO: reset socket if exists
				if (!fz::local_filesys::set_modification_time(fz::to_native(localFile_), fileTime_)) {
					log(logmsg::debug_warning, L"Could not set modification time");
				}
			}
		}
		return prevResult;
	}
	else if (opState == filetransfer_waitresumetest) {
		if (prevResult != FZ_REPLY_OK) {
			if (transferEndReason == TransferEndReason::failed_resumetest) {
				if (localFileSize_ > (1ll << 32)) {
					CServerCapabilities::SetCapability(currentServer_, resume4GBbug, yes);
					log(logmsg::error, _("Server does not support resume of files > 4GB."));
				}
				else {
					CServerCapabilities::SetCapability(currentServer_, resume2GBbug, yes);
					log(logmsg::error, _("Server does not support resume of files > 2GB."));
				}

				prevResult |= FZ_REPLY_CRITICALERROR;
			}
			return prevResult;
		}
		if (localFileSize_ > (1ll << 32)) {
			CServerCapabilities::SetCapability(currentServer_, resume4GBbug, no);
		}
		else {
			CServerCapabilities::SetCapability(currentServer_, resume2GBbug, no);
		}

		opState = filetransfer_transfer;
	}

	return FZ_REPLY_CONTINUE;
}
