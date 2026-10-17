// httplib helpers for extensions that use DuckDB's bundled httplib with
// OpenSSL. Include after "httplib.hpp" with CPPHTTPLIB_OPENSSL_SUPPORT defined.
//
// These dispatch on CPPHTTPLIB_SSL_ENABLED, httplib's own feature macro for
// its TLS backend layer (namespace tls), which DuckDB 2.0's httplib introduced.
// The functions involved are namespace members, which SFINAE cannot test.
#pragma once

#include <openssl/ssl.h>

namespace duckdb_compat {

//! Load the operating system's trust store into `ctx`, as httplib's own
//! SSLClient does. Returns false when nothing was loaded; the caller then
//! falls back to SSL_CTX_set_default_verify_paths.
//! Before 2.0: detail::load_system_certs_on_windows, on Windows only.
//! 2.0: tls::load_system_certs, on every platform. On macOS it reads the
//! keychain, so the extension must link -framework CoreFoundation and
//! -framework Security (2.0's httplib needs those anyway).
inline bool LoadSystemCerts(SSL_CTX *ctx) {
#if defined(CPPHTTPLIB_SSL_ENABLED)
	return duckdb_httplib_openssl::tls::load_system_certs(ctx);
#elif defined(_WIN32)
	return duckdb_httplib_openssl::detail::load_system_certs_on_windows(SSL_CTX_get_cert_store(ctx));
#else
	(void)ctx;
	return false;
#endif
}

} // namespace duckdb_compat
