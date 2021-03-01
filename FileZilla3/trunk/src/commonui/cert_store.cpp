#include "cert_store.h"

#include <libfilezilla/iputils.hpp>

bool cert_store::IsTrusted(fz::tls_session_info const& info)
{
	if (info.get_algorithm_warnings() != 0) {
		// These certs are never trusted.
		return false;
	}

	LoadTrustedCerts();

	fz::x509_certificate cert = info.get_certificates()[0];

	return IsTrusted(info.get_host(), info.get_port(), cert.get_raw_data(), false, !info.mismatched_hostname());
}

bool cert_store::IsInsecure(std::string const& host, unsigned int port, bool permanentOnly)
{
	auto const t = std::make_tuple(host, port);
	if (!permanentOnly && sessionInsecureHosts_.find(t) != sessionInsecureHosts_.cend()) {
		return true;
	}

	LoadTrustedCerts();

	if (insecureHosts_.find(t) != insecureHosts_.cend()) {
		return true;
	}

	return false;
}

bool cert_store::HasCertificate(std::string const& host, unsigned int port)
{
	for (auto const& cert : sessionTrustedCerts_) {
		if (cert.host == host && cert.port == port) {
			return true;
		}
	}

	LoadTrustedCerts();

	for (auto const& cert : trustedCerts_) {
		if (cert.host == host && cert.port == port) {
			return true;
		}
	}

	return false;
}

bool cert_store::DoIsTrusted(std::string const& host, unsigned int port, std::vector<uint8_t> const& data, std::list<cert_store::t_certData> const& trustedCerts, bool allowSans)
{
	if (!data.size()) {
		return false;
	}

	bool const dnsname = fz::get_address_type(host) == fz::address_type::unknown;

	for (auto const& cert : trustedCerts) {
		if (port != cert.port) {
			continue;
		}

		if (cert.data != data) {
			continue;
		}

		if (host != cert.host) {
			if (!dnsname || !allowSans || !cert.trustSans) {
				continue;
			}
		}

		return true;
	}

	return false;
}

bool cert_store::IsTrusted(std::string const& host, unsigned int port, std::vector<uint8_t> const& data, bool permanentOnly, bool allowSans)
{
	bool trusted = DoIsTrusted(host, port, data, trustedCerts_, allowSans);
	if (!trusted && !permanentOnly) {
		trusted = DoIsTrusted(host, port, data, sessionTrustedCerts_, allowSans);
	}

	return trusted;
}

void cert_store::SetInsecure(std::string const& host, unsigned int port, bool permanent)
{
	// A host can't be both trusted and insecure
	sessionTrustedCerts_.erase(
		std::remove_if(sessionTrustedCerts_.begin(), sessionTrustedCerts_.end(), [&host, &port](t_certData const& cert) { return cert.host == host && cert.port == port; }),
		sessionTrustedCerts_.end()
	);

	if (!permanent) {
		sessionInsecureHosts_.emplace(std::make_tuple(host, port));
		return;
	}

	if (!DoSetInsecure(host, port)) {
		return;
	}

	// A host can't be both trusted and insecure
	trustedCerts_.erase(
		std::remove_if(trustedCerts_.begin(), trustedCerts_.end(), [&host, &port](t_certData const& cert) { return cert.host == host && cert.port == port; }),
		trustedCerts_.end()
	);

	insecureHosts_.emplace(std::make_tuple(host, port));
}

bool cert_store::DoSetInsecure(std::string const& host, unsigned int port)
{
	LoadTrustedCerts();

	if (IsInsecure(host, port, true)) {
		return false;
	}

	return true;
}

void cert_store::SetTrusted(fz::tls_session_info const& info, bool permanent, bool trustAllHostnames)
{
	fz::x509_certificate const& certificate = info.get_certificates()[0];

	t_certData cert;
	cert.host = info.get_host();
	cert.port = info.get_port();
	cert.data = certificate.get_raw_data();

	if (trustAllHostnames) {
		cert.trustSans = true;
	}

	// A host can't be both trusted and insecure
	sessionInsecureHosts_.erase(std::make_tuple(cert.host, cert.port));

	if (!permanent) {
		sessionTrustedCerts_.emplace_back(std::move(cert));
		return;
	}

	if (!DoSetTrusted(cert, certificate)) {
		return;
	}

	// A host can't be both trusted and insecure
	insecureHosts_.erase(std::make_tuple(cert.host, cert.port));

	trustedCerts_.emplace_back(std::move(cert));
}

bool cert_store::DoSetTrusted(t_certData const& cert, fz::x509_certificate const&)
{
	LoadTrustedCerts();

	if (IsTrusted(cert.host, cert.port, cert.data, true, false)) {
		return false;
	}

	return true;
}

std::optional<bool> cert_store::GetSessionResumptionSupport(std::wstring const& host, unsigned short port)
{
	return {};
}

void cert_store::SetSessionResumptionSupport(std::wstring const& host, unsigned short port, bool secure)
{
}
