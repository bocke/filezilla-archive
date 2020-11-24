#ifndef FILEZILLA_ENGINE_STORJ_FILETRANSFER_HEADER
#define FILEZILLA_ENGINE_STORJ_FILETRANSFER_HEADER

#include "storjcontrolsocket.h"

class CStorjFileTransferOpData final : public CFileTransferOpData, public CStorjOpData
{
public:
	CStorjFileTransferOpData(CStorjControlSocket & controlSocket, CFileTransferCommand const& cmd)
		: CFileTransferOpData(L"CStorjFileTransferOpData", cmd)
		, CStorjOpData(controlSocket)
	{
	}

	virtual int Send() override;
	virtual int ParseResponse() override;
	virtual int SubcommandResult(int prevResult, COpData const& previousOperation) override;
};

#endif
