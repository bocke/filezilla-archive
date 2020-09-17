#ifndef FILEZILLA_UPDATER_CERT_HEADER
#define FILEZILLA_UPDATER_CERT_HEADER

#include <string_view>

// BASE-64 encoded DER without the BEGIN/END CERTIFICATE
extern std::string_view const updater_cert;

#endif
