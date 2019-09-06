#ifndef FILEZILLA_ENGINE_RESOLVE_HEADER
#define FILEZILLA_ENGINE_RESOLVE_HEADER

#include "controlsocket.h"

class LookupOpData final : public COpData, public CProtocolOpData<CControlSocket>
{
public:
	LookupOpData(CControlSocket &controlSocket, CServerPath const &path, std::wstring const &file, CDirentry * entry);

	virtual int Send() override;
	virtual int ParseResponse() override { return FZ_REPLY_INTERNALERROR; }
	virtual int SubcommandResult(int prevResult, COpData const& previousOperation) override;

	CDirentry const& entry() const { return *entry_; }

private:
	CServerPath const path_;
	std::wstring const file_;

	CDirentry * entry_;
	std::unique_ptr<CDirentry> internal_entry_;
};
/*
class LookupManyOpData final : public COpData, public CProtocolOpData<CControlSocket>
{
public:
	LookupManyOpData(CControlSocket &controlSocket, CServerPath const &path, std::deque<std::wstring> const &files, std::deque<std::wstring> &ids)
		: COpData(Command::lookup, L"LookupManyOpData")
		, CProtocolOpData(controlSocket)
		, path_(path)
		, files_(files)
		, ids_(ids)
	{
	}

	virtual int Send() override;
	virtual int ParseResponse() override { return FZ_REPLY_INTERNALERROR; }
	virtual int SubcommandResult(int prevResult, COpData const& previousOperation) override;

private:
	CServerPath const path_;
	std::deque<std::wstring> const files_;
	std::deque<std::wstring> &ids_;
};
*/
#endif
