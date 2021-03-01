#ifndef FILEZILLA_COMMONUI_CERT_STORE_HEADER
#define FILEZILLA_COMMONUI_CERT_STORE_HEADER

#include "visibility.h"

#include <libfilezilla/tls_info.hpp>

#include <list>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

class FZCUI_PUBLIC_SYMBOL cert_store
{
public:
	bool HasCertificate(std::string const& host, unsigned int port);
	bool IsTrusted(fz::tls_session_info const& info);
	bool IsInsecure(std::string const& host, unsigned int port, bool permanentOnly = false);

	void SetTrusted(fz::tls_session_info const& info, bool permanent, bool trustAllHostnames);
	void SetInsecure(std::string const& host, unsigned int port, bool permanent);

	std::optional<bool> GetSessionResumptionSupport(std::wstring const& host, unsigned short port);
	void SetSessionResumptionSupport(std::wstring const& host, unsigned short port, bool secure);

protected:
	virtual ~cert_store() = default;

	struct t_certData {
		std::string host;
		bool trustSans{};
		unsigned int port{};
		std::vector<uint8_t> data;
	};

	virtual bool DoSetTrusted(t_certData const& cert, fz::x509_certificate const&);
	virtual bool DoSetInsecure(std::string const& host, unsigned int port);
	virtual void LoadTrustedCerts() {}

protected:
	bool IsTrusted(std::string const& host, unsigned int port, std::vector<uint8_t> const& data, bool permanentOnly, bool allowSans);
	bool DoIsTrusted(std::string const& host, unsigned int port, std::vector<uint8_t> const& data, std::list<t_certData> const& trustedCerts, bool allowSans);

	std::list<t_certData> trustedCerts_;
	std::list<t_certData> sessionTrustedCerts_;
	std::set<std::tuple<std::string, unsigned int>> insecureHosts_;
	std::set<std::tuple<std::string, unsigned int>> sessionInsecureHosts_;
};

#endif
