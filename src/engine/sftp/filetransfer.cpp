#include "../filezilla.h"

#include "../directorycache.h"
#include "filetransfer.h"

#include "../../include/engine_options.h"

#include <libfilezilla/local_filesys.hpp>
#include <libfilezilla/process.hpp>

#include <assert.h>

enum filetransferStates
{
	filetransfer_init = 0,
	filetransfer_waitcwd,
	filetransfer_waitlist,
	filetransfer_mtime,
	filetransfer_transfer,
	filetransfer_chmtime
};

CSftpFileTransferOpData::~CSftpFileTransferOpData()
{
	remove_handler();
	reader_.reset();
}

int CSftpFileTransferOpData::Send()
{
	if (opState == filetransfer_init) {

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
	}
	else if (opState == filetransfer_transfer) {
		// Bit convoluted, but we need to guarantee that local filenames are passed as UTF-8 to fzsftp,
		// whereas we need to use server encoding for remote filenames.
		std::string cmd;
		std::wstring logstr;
		if (resume_) {
			cmd = "re";
			logstr = L"re";
		}
		if (download()) {
			if (!resume_) {
				controlSocket_.CreateLocalDir(localFile_);
			}

			engine_.transfer_status_.Init(remoteFileSize_, resume_ ? localFileSize_ : 0, false);
			cmd += "get ";
			logstr += L"get ";
			
			std::string remoteFile = controlSocket_.ConvToServer(controlSocket_.QuoteFilename(remotePath_.FormatFilename(remoteFile_, !tryAbsolutePath_)));
			if (remoteFile.empty()) {
				log(logmsg::error, _("Could not convert command to server encoding"));
				return FZ_REPLY_ERROR;
			}
			cmd += remoteFile + " ";
			logstr += controlSocket_.QuoteFilename(remotePath_.FormatFilename(remoteFile_, !tryAbsolutePath_)) + L" "; 
			
			std::wstring localFile = controlSocket_.QuoteFilename(localFile_);
			cmd += fz::to_utf8(localFile);
			logstr += localFile;
		}
		else {
			engine_.transfer_status_.Init(localFileSize_, resume_ ? remoteFileSize_ : 0, false);
			cmd += "put ";
			logstr += L"put ";

			std::wstring localFile = controlSocket_.QuoteFilename(localFile_);
			cmd += fz::to_utf8(localFile) + " ";
			logstr += localFile + L" ";

			std::string remoteFile = controlSocket_.ConvToServer(controlSocket_.QuoteFilename(remotePath_.FormatFilename(remoteFile_, !tryAbsolutePath_)));
			if (remoteFile.empty()) {
				log(logmsg::error, _("Could not convert command to server encoding"));
				return FZ_REPLY_ERROR;
			}
			cmd += remoteFile;
			logstr += controlSocket_.QuoteFilename(remotePath_.FormatFilename(remoteFile_, !tryAbsolutePath_));
		}
		engine_.transfer_status_.SetStartTime();
		transferInitiated_ = true;
		controlSocket_.SetWait(true);

		controlSocket_.log_raw(logmsg::command, logstr);
		return controlSocket_.AddToStream(cmd + "\r\n");
	}
	else if (opState == filetransfer_mtime) {
		std::wstring quotedFilename = controlSocket_.QuoteFilename(remotePath_.FormatFilename(remoteFile_, !tryAbsolutePath_));
		return controlSocket_.SendCommand(L"mtime " + quotedFilename);
	}
	else if (opState == filetransfer_chmtime) {
		assert(!fileTime_.empty());
		if (download()) {
			log(logmsg::debug_info, L"  filetransfer_chmtime during download");
			return FZ_REPLY_INTERNALERROR;
		}

		std::wstring quotedFilename = controlSocket_.QuoteFilename(remotePath_.FormatFilename(remoteFile_, !tryAbsolutePath_));

		fz::datetime t = fileTime_;
		t -= fz::duration::from_minutes(currentServer_.GetTimezoneOffset());

		// Y2K38
		time_t ticks = t.get_time_t();
		std::wstring seconds = fz::sprintf(L"%d", ticks);
		return controlSocket_.SendCommand(L"chmtime " + seconds + L" " + quotedFilename);
	}

	return FZ_REPLY_INTERNALERROR;
}

int CSftpFileTransferOpData::ParseResponse()
{
	if (opState == filetransfer_transfer) {
		if (controlSocket_.result_ == FZ_REPLY_OK && engine_.GetOptions().get_int(OPTION_PRESERVE_TIMESTAMPS)) {
			if (download()) {
				if (!fileTime_.empty()) {
					if (!fz::local_filesys::set_modification_time(fz::to_native(localFile_), fileTime_))
						log(logmsg::debug_warning, L"Could not set modification time");
				}
			}
			else {
				fileTime_ = fz::local_filesys::get_modification_time(fz::to_native(localFile_));
				if (!fileTime_.empty()) {
					opState = filetransfer_chmtime;
					return FZ_REPLY_CONTINUE;
				}
			}
		}
		return controlSocket_.result_;
	}
	else if (opState == filetransfer_mtime) {
		if (controlSocket_.result_ == FZ_REPLY_OK && !controlSocket_.response_.empty()) {
			time_t seconds = 0;
			bool parsed = true;
			for (auto const& c : controlSocket_.response_) {
				if (c < '0' || c > '9') {
					parsed = false;
					break;
				}
				seconds *= 10;
				seconds += c - '0';
			}
			if (parsed) {
				fz::datetime fileTime = fz::datetime(seconds, fz::datetime::seconds);
				if (!fileTime.empty()) {
					fileTime_ = fileTime;
					fileTime_ += fz::duration::from_minutes(currentServer_.GetTimezoneOffset());
				}
			}
		}
		opState = filetransfer_transfer;
		int res = controlSocket_.CheckOverwriteFile();
		if (res != FZ_REPLY_OK) {
			return res;
		}

		return FZ_REPLY_CONTINUE;
	}
	else if (opState == filetransfer_chmtime) {
		if (download()) {
			log(logmsg::debug_info, L"  filetransfer_chmtime during download");
			return FZ_REPLY_INTERNALERROR;
		}
		return FZ_REPLY_OK;
	}
	else {
		log(logmsg::debug_info, L"  Called at improper time: opState == %d", opState);
	}

	return FZ_REPLY_INTERNALERROR;
}

int CSftpFileTransferOpData::SubcommandResult(int prevResult, COpData const&)
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
				else if (download() && engine_.GetOptions().get_int(OPTION_PRESERVE_TIMESTAMPS)) {
					opState = filetransfer_mtime;
				}
				else {
					opState = filetransfer_transfer;
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

						if (download() && !entry.has_time() &&
							engine_.GetOptions().get_int(OPTION_PRESERVE_TIMESTAMPS))
						{
							opState = filetransfer_mtime;
						}
						else {
							opState = filetransfer_transfer;
						}
					}
					else {
						opState = filetransfer_mtime;
					}
				}
			}
			if (opState == filetransfer_waitlist) {
				controlSocket_.List(CServerPath(), L"", LIST_FLAG_REFRESH);
				return FZ_REPLY_CONTINUE;
			}
			else if (opState == filetransfer_transfer) {
				int res = controlSocket_.CheckOverwriteFile();
				if (res != FZ_REPLY_OK) {
					return res;
				}
			}
		}
		else {
			tryAbsolutePath_ = true;
			opState = filetransfer_mtime;
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
					opState = filetransfer_mtime;
				}
				else if (download() &&
					engine_.GetOptions().get_int(OPTION_PRESERVE_TIMESTAMPS))
				{
					opState = filetransfer_mtime;
				}
				else {
					opState = filetransfer_transfer;
				}
			}
			else {
				if (matchedCase && !entry.is_unsure()) {
					remoteFileSize_ = entry.size;
					if (entry.has_date()) {
						fileTime_ = entry.time;
					}

					if (download() && !entry.has_time() &&
						engine_.GetOptions().get_int(OPTION_PRESERVE_TIMESTAMPS))
					{
						opState = filetransfer_mtime;
					}
					else {
						opState = filetransfer_transfer;
					}
				}
				else {
					opState = filetransfer_mtime;
				}
			}
			if (opState == filetransfer_transfer) {
				int res = controlSocket_.CheckOverwriteFile();
				if (res != FZ_REPLY_OK) {
					return res;
				}
			}
		}
		else {
			opState = filetransfer_mtime;
		}
	}
	else {
		log(logmsg::debug_warning, L"  Unknown opState (%d)", opState);
		return FZ_REPLY_INTERNALERROR;
	}

	return FZ_REPLY_CONTINUE;
}

void CSftpFileTransferOpData::OnOpenRequested(uint64_t offset)
{
	if (reader_ || writer_) {
		controlSocket_.AddToStream("-0\n");
		return;
	}

	if (download()) {
		if (resume_) {
			offset = writer_factory_.size();
			if (offset == writer_base::npos) {
				controlSocket_.AddToStream("-1\n");
				return;
			}
		}
		else {
			offset = 0;
		}
		writer_ = writer_factory_.open(offset, *this, true);
		if (!writer_) {
			controlSocket_.AddToStream("--\n");
		}
		else {
			auto const info = writer_->shared_memory_info();

			HANDLE target;
			if (!DuplicateHandle(GetCurrentProcess(), std::get<0>(info), controlSocket_.process_->handle(), &target, 0, false, DUPLICATE_SAME_ACCESS)) {
				log(logmsg::debug_warning, L"DuplicateHandle failed");
				controlSocket_.ResetOperation(FZ_REPLY_ERROR);
				return;
			}

			controlSocket_.AddToStream(fz::sprintf("-%u %u %u", reinterpret_cast<uintptr_t>(target), std::get<2>(info), offset));
			base_address_ = std::get<1>(info);
		}
	}
	else {
		reader_ = reader_factory_.open(offset, *this, true);
		if (!reader_) {
			controlSocket_.AddToStream("--\n");
		}
		else {
			auto const info = reader_->shared_memory_info();

			HANDLE target;
			if (!DuplicateHandle(GetCurrentProcess(), std::get<0>(info), controlSocket_.process_->handle(), &target, 0, false, DUPLICATE_SAME_ACCESS)) {
				log(logmsg::debug_warning, L"DuplicateHandle failed");
				controlSocket_.ResetOperation(FZ_REPLY_ERROR);
				return;
			}

			controlSocket_.AddToStream(fz::sprintf("-%u %u\n", reinterpret_cast<uintptr_t>(target), std::get<2>(info)));
			base_address_ = std::get<1>(info);
		}
	}
}

void CSftpFileTransferOpData::OnNextBufferRequested(uint64_t processed)
{
	if (reader_) {
		auto r = reader_->read();
		if (r == aio_result::wait) {
			return;
		}
		if (r.type_ == aio_result::error) {
			controlSocket_.AddToStream("--1\n");
			return;
		}
		controlSocket_.AddToStream(fz::sprintf("-%d %d\n", r.buffer_.get() - base_address_, r.buffer_.size()));
	}
	else if (writer_) {
		buffer_.resize(processed);
		auto r = writer_->get_write_buffer(buffer_);
		if (r == aio_result::wait) {
			return;
		}
		if (r.type_ == aio_result::error) {
			controlSocket_.AddToStream("--1\n");
			return;
		}
		buffer_ = r.buffer_;
		controlSocket_.AddToStream(fz::sprintf("-%d %d\n", buffer_.get() - base_address_, buffer_.capacity()));
	}
	else {
		controlSocket_.AddToStream("--1\n");
		return;
	}
}

void CSftpFileTransferOpData::OnFinalizeRequested(uint64_t lastWrite)
{
	finalizing_ = true;
	buffer_.resize(lastWrite);
	auto res = writer_->finalize(buffer_);
	if (res == aio_result::wait) {
		return;
	}
	else if (res == aio_result::ok) {
		controlSocket_.AddToStream(fz::sprintf("-1\n"));
	}
	else {
		controlSocket_.AddToStream(fz::sprintf("-0\n"));
	}
}

void CSftpFileTransferOpData::OnSizeRequested()
{
	uint64_t size = writer_base::npos;
	if (reader_) {
		size = reader_->size();
	}
	else if (writer_) {
		size = writer_->size();
	}
	if (size == writer_base::npos) {
		controlSocket_.AddToStream("--1\n");
	}
	else {
		controlSocket_.AddToStream(fz::sprintf("-%d\n", size));
	}
}

void CSftpFileTransferOpData::operator()(fz::event_base const& ev)
{
	fz::dispatch<read_ready_event, write_ready_event>(ev, this,
		&CSftpFileTransferOpData::OnReaderEvent,
		&CSftpFileTransferOpData::OnWriterEvent
	);
}

void CSftpFileTransferOpData::OnReaderEvent(reader_base*)
{
	OnNextBufferRequested(0);
}

void CSftpFileTransferOpData::OnWriterEvent(writer_base*)
{
	if (finalizing_) {
		OnFinalizeRequested(0);
	}
	else {
		OnNextBufferRequested(0);
	}
}
