#ifndef FILEZILLA_ENGINE_STORJ_FILETRANSFER_HEADER
#define FILEZILLA_ENGINE_STORJ_FILETRANSFER_HEADER

#include "storjcontrolsocket.h"

class CStorjFileTransferOpData final : public CFileTransferOpData, public CStorjOpData
{
public:
	CStorjFileTransferOpData(CStorjControlSocket & controlSocket, std::wstring const& local_file, std::wstring const& remote_file, CServerPath const& remote_path, transfer_flags const& flags)
		: CFileTransferOpData(L"CStorjFileTransferOpData", local_file, remote_file, remote_path, flags)
		, CStorjOpData(controlSocket)
	{
	}

	virtual int Send() override;
	virtual int ParseResponse() override;
	virtual int SubcommandResult(int prevResult, COpData const& previousOperation) override;
};

#endif
