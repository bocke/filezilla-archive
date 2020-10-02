#ifndef FILEZILLA_ENGINE_SFTP_FILETRANSFER_HEADER
#define FILEZILLA_ENGINE_SFTP_FILETRANSFER_HEADER

#include "sftpcontrolsocket.h"

class CSftpFileTransferOpData final : public CFileTransferOpData, public CSftpOpData
{
public:
	CSftpFileTransferOpData(CSftpControlSocket & controlSocket, std::wstring const& local_file, std::wstring const& remote_file, CServerPath const& remote_path, transfer_flags const& flags)
		: CFileTransferOpData(L"CSftpFileTransferOpData", local_file, remote_file, remote_path, flags)
		, CSftpOpData(controlSocket)
	{}

	virtual int Send() override;
	virtual int ParseResponse() override;
	virtual int SubcommandResult(int, COpData const&) override;
};

#endif
