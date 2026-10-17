// duckdb_compat/dispatch.hpp -- API-shape dispatch with no DuckDB dependency.
//
// Each helper offers two overloads: `(..., int)` uses the NEW API and exists
// only when that expression compiles (SFINAE on the trailing return type);
// `(..., long)` uses the OLD one. Calls pass `0`, so the new API wins whenever
// the type has it -- including on DuckDB 1.5.6, which carries both. Detection
// is by API shape, not version number: 1.5.6 already has several "2.0" APIs
// that 1.4.5 LTS lacks, so a version check cannot express the truth.
#pragma once

#include <string>

namespace duckdb_compat {
namespace detail {

// --- ScalarFunction stability: 2.0 field -> setter (1.5.6 has both) -------
template <class F, class S>
auto SetStability(F &fn, S value, int) -> decltype(fn.SetStability(value), void()) {
	fn.SetStability(value);
}
template <class F, class S>
void SetStability(F &fn, S value, long) {
	fn.stability = value;
}

// --- ClientContext interrupt: 2.0 removed the public `interrupted` atomic --
template <class C>
auto IsInterrupted(const C &ctx, int) -> decltype(static_cast<bool>(ctx.IsInterrupted())) {
	return ctx.IsInterrupted();
}
template <class C>
bool IsInterrupted(const C &ctx, long) {
	return ctx.interrupted;
}

// --- Identifier -> std::string: 2.0 secret names / option keys are an
// Identifier whose string conversion is explicit. A std::string passes through.
template <class T>
auto Str(const T &value, int) -> decltype(value.GetIdentifierName()) {
	return value.GetIdentifierName();
}
inline const std::string &Str(const std::string &value, long) {
	return value;
}

// --- httplib (DuckDB's vendored copy): set_url_encode -> set_path_encode ---
template <class C>
auto SetPathEncode(C &client, bool on, int) -> decltype(client.set_path_encode(on), void()) {
	client.set_path_encode(on);
}
template <class C>
void SetPathEncode(C &client, bool on, long) {
	client.set_url_encode(on);
}

// --- httplib TLS context: ssl_context() (SSL_CTX*) -> tls_context() (void*) -
template <class C>
auto TlsContext(C &client, int) -> decltype(static_cast<void *>(client.ssl_context())) {
	return static_cast<void *>(client.ssl_context());
}
template <class C>
void *TlsContext(C &client, long) {
	return static_cast<void *>(client.tls_context());
}

} // namespace detail

//! fn.SetStability(v) where available, else fn.stability = v.
template <class F, class S>
void SetStability(F &fn, S value) {
	detail::SetStability(fn, value, 0);
}

//! ctx.IsInterrupted() where available, else ctx.interrupted.
template <class C>
bool IsInterrupted(const C &ctx) {
	return detail::IsInterrupted(ctx, 0);
}

//! The raw string of an Identifier (2.0), or the string itself (1.x).
template <class T>
auto Str(const T &value) -> decltype(detail::Str(value, 0)) {
	return detail::Str(value, 0);
}

//! httplib: set_path_encode(on) where available, else set_url_encode(on).
template <class C>
void SetPathEncode(C &client, bool on) {
	detail::SetPathEncode(client, on, 0);
}

//! httplib: the TLS context as void* (cast to SSL_CTX* under OpenSSL);
//! ssl_context() on DuckDB 1.x's httplib, tls_context() on 2.0's.
template <class C>
void *TlsContext(C &client) {
	return detail::TlsContext(client, 0);
}

} // namespace duckdb_compat
