// Compile probe for duckdb_compat/httplib.hpp against DuckDB's bundled httplib
// (third_party/httplib) of each DuckDB line, built with OpenSSL as extensions do.
#ifdef _WIN32
#define NOMINMAX // as duckdb/common/windows.hpp does; DuckDB headers need it
#include <windows.h>
#endif
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.hpp"
#include "duckdb_compat/httplib.hpp"

bool ProbeLoadSystemCerts(SSL_CTX *ctx) {
	return duckdb_compat::LoadSystemCerts(ctx);
}

