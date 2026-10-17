// duckdb_compat.hpp -- one source tree for DuckDB 1.4 LTS, 1.5 and 2.0.
//
// Include AFTER the DuckDB headers you use. Every helper detects the API by
// shape (see duckdb_compat/dispatch.hpp), never by version number. The rule
// for consumers: no `#if` on DuckDB versions at call sites -- add a helper
// here instead, with a case in test/probe.cpp.
#pragma once

#include "duckdb.hpp"
#include "duckdb/common/file_opener.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb_compat/dispatch.hpp"

#include <optional>
#include <string>
#include <tuple>
#include <type_traits>

#if __has_include("duckdb/common/identifier.hpp")
#include "duckdb/common/identifier.hpp"
#define DUCKDB_COMPAT_HAS_IDENTIFIER 1
#endif

namespace duckdb_compat {

// --- Table-function bind: the column-name list's element type --------------
// vector<string> before 2.0, vector<Identifier> in 2.0. Read off the
// table_function_bind_t typedef itself, so it is right by construction.
namespace detail {
template <class>
struct BindFnTraits;
template <class R, class... A>
struct BindFnTraits<R (*)(A...)> {
	using names_ref = typename std::tuple_element<3, std::tuple<A...>>::type;
};
} // namespace detail

//! The `names` parameter type of a table-function bind callback. Fill it with
//! names.emplace_back(...): Identifier's construction from std::string is
//! explicit, which emplace_back honours and push_back does not.
using BindNames = typename std::remove_reference<
    typename detail::BindFnTraits<duckdb::table_function_bind_t>::names_ref>::type;

// --- FileOpener::TryGetCurrentSetting: key is a string before 2.0, an
// Identifier in 2.0 (explicit from string). ----------------------------------
namespace detail {
template <class O>
auto TryGetSetting(O opener, const std::string &key, duckdb::Value &value, int)
    -> decltype(duckdb::FileOpener::TryGetCurrentSetting(opener, key, value)) {
	return duckdb::FileOpener::TryGetCurrentSetting(opener, key, value);
}
#ifdef DUCKDB_COMPAT_HAS_IDENTIFIER
// Guarded: 1.4 has no Identifier type, and a non-dependent name inside a
// template is looked up at definition, which SFINAE cannot rescue.
template <class O>
auto TryGetSetting(O opener, const std::string &key, duckdb::Value &value, long)
    -> decltype(duckdb::FileOpener::TryGetCurrentSetting(opener, duckdb::Identifier(key), value)) {
	return duckdb::FileOpener::TryGetCurrentSetting(opener, duckdb::Identifier(key), value);
}
#endif
} // namespace detail

//! FileOpener::TryGetCurrentSetting(opener, key, value) on every line.
inline duckdb::SettingLookupResult TryGetSetting(duckdb::optional_ptr<duckdb::FileOpener> opener,
                                                 const std::string &key, duckdb::Value &value) {
	return detail::TryGetSetting(opener, key, value, 0);
}

// --- Unary scalar that may return NULL --------------------------------------
// Before 2.0: UnaryExecutor::ExecuteWithNulls with a (input, mask, idx) lambda.
// 2.0: UnaryExecutor::Execute with a lambda returning optional<RESULT>.
namespace detail {
template <class INPUT_TYPE, class RESULT_TYPE, class F>
struct WithNullsAdapter {
	F &fun;
	RESULT_TYPE operator()(INPUT_TYPE input, duckdb::ValidityMask &mask, duckdb::idx_t idx) const {
		bool is_null = false;
		RESULT_TYPE out = fun(input, is_null);
		if (is_null) {
			mask.SetInvalid(idx);
			return RESULT_TYPE();
		}
		return out;
	}
};
template <class INPUT_TYPE, class RESULT_TYPE, class F>
struct OptionalAdapter {
	F &fun;
	std::optional<RESULT_TYPE> operator()(INPUT_TYPE input) const {
		bool is_null = false;
		RESULT_TYPE out = fun(input, is_null);
		if (is_null) {
			return std::nullopt;
		}
		return out;
	}
};

// EXEC is a template parameter only to make the member lookup DEPENDENT: a
// name looked up in a concrete class (duckdb::UnaryExecutor::ExecuteWithNulls)
// is resolved when the template is defined, so its absence on 2.0 would be a
// hard error rather than a discarded overload.
template <class INPUT_TYPE, class RESULT_TYPE, class F, class EXEC = duckdb::UnaryExecutor>
auto ExecuteUnaryNullable(duckdb::Vector &input, duckdb::Vector &result, duckdb::idx_t count, F &fun, int)
    -> decltype(EXEC::template ExecuteWithNulls<INPUT_TYPE, RESULT_TYPE>(input, result, count,
                                                         std::declval<WithNullsAdapter<INPUT_TYPE, RESULT_TYPE, F>>()),
                void()) {
	EXEC::template ExecuteWithNulls<INPUT_TYPE, RESULT_TYPE>(input, result, count, WithNullsAdapter<INPUT_TYPE, RESULT_TYPE, F> {fun});
}
template <class INPUT_TYPE, class RESULT_TYPE, class F, class EXEC = duckdb::UnaryExecutor>
void ExecuteUnaryNullable(duckdb::Vector &input, duckdb::Vector &result, duckdb::idx_t count, F &fun, long) {
	EXEC::template Execute<INPUT_TYPE, RESULT_TYPE>(input, result, count, OptionalAdapter<INPUT_TYPE, RESULT_TYPE, F> {fun});
}
} // namespace detail

//! Run `fun(INPUT_TYPE input, bool &is_null) -> RESULT_TYPE` over a vector; setting is_null
//! makes that row NULL. Replaces UnaryExecutor::ExecuteWithNulls.
template <class INPUT_TYPE, class RESULT_TYPE, class F>
void ExecuteUnaryNullable(duckdb::Vector &input, duckdb::Vector &result, duckdb::idx_t count, F fun) {
	detail::ExecuteUnaryNullable<INPUT_TYPE, RESULT_TYPE>(input, result, count, fun, 0);
}

} // namespace duckdb_compat
