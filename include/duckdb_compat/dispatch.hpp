// duckdb_compat/dispatch.hpp -- API-shape dispatch with no DuckDB dependency.
//
// Each helper offers two overloads: `(..., int)` uses the NEW API and exists
// only when that expression compiles (SFINAE on the trailing return type);
// `(..., long)` uses the OLD one. Calls pass `0`, so the new API wins whenever
// the type has it -- including on DuckDB 1.5.6, which carries both. Detection
// is by API shape, not version number: 1.5.6 already has several "2.0" APIs
// as backports, so a version check cannot express the truth. A helper whose
// new API exists on every supported line has only the `int` overload.
#pragma once

#include <optional>
#include <string>
#include <type_traits>

namespace duckdb_compat {
namespace detail {
// DBConfig::ParseMemoryLimit: idx_t with the maximum for "unlimited" before 2.0;
// 2.0 returns optional_idx (empty = unlimited) and adds ParseMemoryLimitOrMaximum,
// which keeps the old contract. Both arms return the old contract.
template <class C>
auto MemoryLimitBytes(const std::string &arg, int) -> decltype(C::ParseMemoryLimitOrMaximum(arg)) {
	return C::ParseMemoryLimitOrMaximum(arg);
}
template <class C>
auto MemoryLimitBytes(const std::string &arg, long) -> decltype(C::ParseMemoryLimit(arg)) {
	return C::ParseMemoryLimit(arg);
}


// Legacy table-filter kind names appeared in DuckDB 2.0.
template <class E, class = void>
struct HasLegacyFilterKind : std::false_type {};
template <class E>
struct HasLegacyFilterKind<E, std::void_t<decltype(E::LEGACY_CONSTANT_COMPARISON)>> : std::true_type {};
template <class E>
constexpr auto BloomKind(int) -> decltype(E::LEGACY_BLOOM_FILTER, std::optional<E>()) {
	return E::LEGACY_BLOOM_FILTER;
}
template <class E>
constexpr auto BloomKind(long) -> decltype(E::BLOOM_FILTER, std::optional<E>()) {
	return E::BLOOM_FILTER;
}

template <class E>
constexpr auto ConstantKind(int) -> decltype(E::LEGACY_CONSTANT_COMPARISON) {
	return E::LEGACY_CONSTANT_COMPARISON;
}
template <class E>
constexpr auto ConstantKind(long) -> decltype(E::CONSTANT_COMPARISON) {
	return E::CONSTANT_COMPARISON;
}
template <class E>
constexpr auto NullKind(int) -> decltype(E::LEGACY_IS_NULL) {
	return E::LEGACY_IS_NULL;
}
template <class E>
constexpr auto NullKind(long) -> decltype(E::IS_NULL) {
	return E::IS_NULL;
}
template <class E>
constexpr auto NotNullKind(int) -> decltype(E::LEGACY_IS_NOT_NULL) {
	return E::LEGACY_IS_NOT_NULL;
}
template <class E>
constexpr auto NotNullKind(long) -> decltype(E::IS_NOT_NULL) {
	return E::IS_NOT_NULL;
}
template <class E>
constexpr auto AndKind(int) -> decltype(E::LEGACY_CONJUNCTION_AND) {
	return E::LEGACY_CONJUNCTION_AND;
}
template <class E>
constexpr auto AndKind(long) -> decltype(E::CONJUNCTION_AND) {
	return E::CONJUNCTION_AND;
}
template <class E>
constexpr auto OrKind(int) -> decltype(E::LEGACY_CONJUNCTION_OR) {
	return E::LEGACY_CONJUNCTION_OR;
}
template <class E>
constexpr auto OrKind(long) -> decltype(E::CONJUNCTION_OR) {
	return E::CONJUNCTION_OR;
}
template <class E>
constexpr auto OptionalKind(int) -> decltype(E::LEGACY_OPTIONAL_FILTER) {
	return E::LEGACY_OPTIONAL_FILTER;
}
template <class E>
constexpr auto OptionalKind(long) -> decltype(E::OPTIONAL_FILTER) {
	return E::OPTIONAL_FILTER;
}
template <class E>
constexpr auto DynamicKind(int) -> decltype(E::LEGACY_DYNAMIC_FILTER) {
	return E::LEGACY_DYNAMIC_FILTER;
}
template <class E>
constexpr auto DynamicKind(long) -> decltype(E::DYNAMIC_FILTER) {
	return E::DYNAMIC_FILTER;
}
template <class E>
constexpr auto InKind(int) -> decltype(E::LEGACY_IN_FILTER) {
	return E::LEGACY_IN_FILTER;
}
template <class E>
constexpr auto InKind(long) -> decltype(E::IN_FILTER) {
	return E::IN_FILTER;
}
template <class E>
constexpr auto StructKind(int) -> decltype(E::LEGACY_STRUCT_EXTRACT) {
	return E::LEGACY_STRUCT_EXTRACT;
}
template <class E>
constexpr auto StructKind(long) -> decltype(E::STRUCT_EXTRACT) {
	return E::STRUCT_EXTRACT;
}

// Bind-info callbacks receive bind data directly on 1.x and an input wrapper
// on 2.0. Only the exact legacy callback argument may use the fallback.
template <class EXPECTED, class INPUT>
auto GetBindData(INPUT &input, int)
    -> std::enable_if_t<std::is_same_v<std::decay_t<INPUT>, std::decay_t<EXPECTED>>,
                        decltype(input.bind_data)> {
	return input.bind_data;
}
template <class EXPECTED, class INPUT>
auto GetBindData(INPUT &input, long)
    -> std::enable_if_t<std::is_same_v<std::decay_t<INPUT>, std::decay_t<EXPECTED>>, INPUT &> {
	return input;
}

template <class RESULT, class INDEX>
auto ResultValue(RESULT &result, INDEX column, INDEX row, int) -> decltype(result.Collection().GetValue(column, row)) {
	return result.Collection().GetValue(column, row);
}
template <class RESULT, class INDEX>
auto ResultValue(RESULT &result, INDEX column, INDEX row, long) -> decltype(result.GetValue(column, row)) {
	return result.GetValue(column, row);
}
// --- ScalarFunction stability: 2.0 field -> setter (1.5.6 has both) -------
template <class F, class S>
auto SetStability(F &fn, S value, int) -> decltype(fn.SetStability(value), void()) {
	fn.SetStability(value);
}

// --- Scalar function "may throw": 2.0 turns an error thrown by a function
// NOT marked fallible into an INTERNAL error. 1.5 and 2.0: SetFallible().
template <class F>
auto SetFallible(F &fn, int) -> decltype(fn.SetFallible(), void()) {
	fn.SetFallible();
}

// --- Function NULL handling: 1.5 has the `null_handling` field and
// SetNullHandling(); 2.0 keeps only the setter.
template <class F, class N>
auto SetNullHandling(F &fn, N value, int) -> decltype(fn.SetNullHandling(value), void()) {
	fn.SetNullHandling(value);
}

// --- DataChunk cardinality: on 2.0 vectors carry their own size, and
// DataChunk::SetCardinality (deprecated) sets only the chunk's count. A scan that
// fills rows with SetValue and then calls SetCardinality leaves its vectors at
// size 0, and operators that read Vector::size() -- IS NULL among them -- return
// garbage. SetChildCardinality also sizes the vectors. 1.5.6 backports it as an
// alias of SetCardinality.
template <class CHUNK, class COUNT>
auto SetChildCardinality(CHUNK &chunk, COUNT count, int) -> decltype(chunk.SetChildCardinality(count), void()) {
	chunk.SetChildCardinality(count);
}

// 2.0 sizes vectors independently, so ToUnifiedFormat reads their size rather
// than a caller-provided count. The deprecated count overload remains on 2.0.
template <class VECTOR, class FORMAT, class COUNT>
auto ToUnifiedFormat(VECTOR &vector, COUNT, FORMAT &format, int) -> decltype(vector.ToUnifiedFormat(format), void()) {
	//! 2.0 ignores the supplied count: the producer must size its vector.
	vector.ToUnifiedFormat(format);
}
template <class VECTOR, class FORMAT, class COUNT>
void ToUnifiedFormat(VECTOR &vector, COUNT count, FORMAT &format, long) {
	vector.ToUnifiedFormat(count, format);
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

//! Mark a scalar function as one that may throw at runtime. REQUIRED on 2.0
//! for any function that can raise an error (I/O, validation): otherwise the
//! error surfaces as an INTERNAL error asking the author to call SetFallible().
template <class F>
void SetFallible(F &fn) {
	detail::SetFallible(fn, 0);
}

//! fn.SetNullHandling(v). Works for scalar and aggregate functions.
template <class F, class N>
void SetNullHandling(F &fn, N value) {
	detail::SetNullHandling(fn, value, 0);
}

//! Set a DataChunk's row count AND its vectors' sizes. Use it wherever a table
//! function or COPY writer ends a chunk with SetCardinality(n): on 2.0 that
//! leaves the vectors unsized, and IS NULL over the output returns garbage.
template <class CHUNK, class COUNT>
void SetChildCardinality(CHUNK &chunk, COUNT count) {
	detail::SetChildCardinality(chunk, count, 0);
}

//! Vector::ToUnifiedFormat(format) on 2.0; count-taking form on 1.x.
template <class VECTOR, class COUNT, class FORMAT>
void ToUnifiedFormat(VECTOR &vector, COUNT count, FORMAT &format) {
	detail::ToUnifiedFormat(vector, count, format, 0);
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
