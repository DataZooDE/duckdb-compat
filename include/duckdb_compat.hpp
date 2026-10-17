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
#include "duckdb/function/copy_function.hpp"
#include "duckdb/main/query_result.hpp"
#include "duckdb/catalog/default/default_functions.hpp"
#include "duckdb/parser/parser.hpp"
#include "duckdb/parser/parsed_data/create_macro_info.hpp"
#include "duckdb/parser/statement/create_statement.hpp"
#include "duckdb_compat/dispatch.hpp"

#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

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

// --- COPY TO bind: the column-name list's element type ---------------------
// const vector<string>& before 2.0, const vector<Identifier>& in 2.0. Read off
// the copy_to_bind_t typedef, like BindNames.
namespace detail {
template <class>
struct CopyBindFnTraits;
template <class R, class... A>
struct CopyBindFnTraits<R (*)(A...)> {
	using names_ref = typename std::tuple_element<2, std::tuple<A...>>::type;
};
} // namespace detail

//! The element container of a COPY TO bind callback's `names`; declare the
//! parameter as `const duckdb_compat::CopyBindNames &names`.
using CopyBindNames = typename std::remove_cv<typename std::remove_reference<
    typename detail::CopyBindFnTraits<duckdb::copy_to_bind_t>::names_ref>::type>::type;

// --- Function names: std::string before 2.0, Identifier in 2.0 ---------------
// Identifier's construction from std::string is explicit, so a runtime string
// no longer converts where TableFunction / TableFunctionSet take a name. The
// type is read off Function::name. (String literals convert on every line.)
using FunctionNameType = typename std::decay<decltype(std::declval<duckdb::Function &>().name)>::type;

//! A function name of whatever type this DuckDB's function constructors take.
inline FunctionNameType FunctionName(const std::string &name) {
	return FunctionNameType(name);
}

// --- Named parameters of a table function ------------------------------------
// Before 2.0: fn.named_parameters[name] = type. 2.0: named parameters are part
// of the signature, fn.GetSignature().AddKeywordOnly(name, type).
namespace detail {
template <class FN>
auto AddNamedParameter(FN &fn, const std::string &name, const duckdb::LogicalType &type, int)
    -> decltype(fn.GetSignature().AddKeywordOnly(FunctionNameType(name), type), void()) {
	fn.GetSignature().AddKeywordOnly(FunctionNameType(name), type);
}
template <class FN>
void AddNamedParameter(FN &fn, const std::string &name, const duckdb::LogicalType &type, long) {
	fn.named_parameters[name] = type;
}
} // namespace detail

//! Declare a named (keyword-only) parameter on a table function.
template <class FN>
void AddNamedParameter(FN &fn, const std::string &name, const duckdb::LogicalType &type) {
	detail::AddNamedParameter(fn, name, type, 0);
}

//! The value passed for named parameter `name` at bind time, or nullptr when
//! the call did not pass it. The map's key is a string before 2.0 and an
//! Identifier in 2.0; both are constructed explicitly from the name, so the
//! map's own (case-insensitive) lookup applies on every line.
template <class MAP>
const duckdb::Value *FindNamedParameter(const MAP &map, const std::string &name) {
	auto entry = map.find(typename MAP::key_type(name));
	return entry == map.end() ? nullptr : &entry->second;
}

// --- Query results --------------------------------------------------------------
// Before 2.0: Connection::Query returns a MaterializedQueryResult with public
// `names` and GetValue(col, row). 2.0 removed MaterializedQueryResult; names
// are private (GetNames(), Identifier). Fetch() exists on every line.
namespace detail {
template <class R>
auto ResultNames(const R &result, int) -> decltype(result.GetNames(), std::vector<std::string>()) {
	std::vector<std::string> out;
	for (auto &n : result.GetNames()) {
		out.emplace_back(::duckdb_compat::Str(n));
	}
	return out;
}
template <class R>
std::vector<std::string> ResultNames(const R &result, long) {
	return std::vector<std::string>(result.names.begin(), result.names.end());
}
} // namespace detail

//! The result's column names as strings.
inline std::vector<std::string> ResultNames(const duckdb::QueryResult &result) {
	return detail::ResultNames(result, 0);
}

//! Every row of a (successful) result, as Values, row-major. Check HasError()
//! first. Consumes the result: it fetches until exhausted.
inline std::vector<std::vector<duckdb::Value>> FetchRows(duckdb::QueryResult &result) {
	std::vector<std::vector<duckdb::Value>> rows;
	while (auto chunk = result.Fetch()) {
		if (chunk->size() == 0) {
			break;
		}
		for (duckdb::idx_t r = 0; r < chunk->size(); r++) {
			std::vector<duckdb::Value> row;
			row.reserve(chunk->ColumnCount());
			for (duckdb::idx_t c = 0; c < chunk->ColumnCount(); c++) {
				row.emplace_back(chunk->GetValue(c, r));
			}
			rows.emplace_back(std::move(row));
		}
	}
	return rows;
}

// --- Value::DefaultTryCastAs ----------------------------------------------------
// Before 2.0: bool DefaultTryCastAs(type, Value &out, string *error, bool strict).
// 2.0: optional<Value> DefaultTryCastAs(type, string *error, bool strict).
namespace detail {
template <class V>
auto TryCastValue(const V &value, const duckdb::LogicalType &type, V &out, std::string *error, bool strict, int)
    -> decltype(*value.DefaultTryCastAs(type, error, strict), bool()) {
	auto cast = value.DefaultTryCastAs(type, error, strict);
	if (!cast) {
		return false;
	}
	out = std::move(*cast);
	return true;
}
template <class V>
bool TryCastValue(const V &value, const duckdb::LogicalType &type, V &out, std::string *error, bool strict, long) {
	return value.DefaultTryCastAs(type, out, error, strict);
}
} // namespace detail

//! Cast `value` to `type`; on success store it in `out` and return true.
inline bool TryCastValue(const duckdb::Value &value, const duckdb::LogicalType &type, duckdb::Value &out,
                         std::string *error = nullptr, bool strict = false) {
	return detail::TryCastValue(value, type, out, error, strict, 0);
}

// --- Built-in scalar macros -------------------------------------------------------
// Before 2.0: DefaultMacro {schema, name, parameters[], named_parameters[], body}.
// 2.0: DefaultMacro {schema, name, "(params) AS body"}, parsed as CREATE MACRO.
// One definition string serves every line: 2.0 hands it to
// CreateInternalMacroInfo; older lines parse the same CREATE MACRO statement and
// set the fields CreateInternalMacroInfo sets there (temporary, internal).
namespace detail {
template <class DM = duckdb::DefaultMacro>
auto ScalarMacroInfo(const char *schema, const char *name, const char *definition, int)
    -> decltype(DM {schema, name, definition}.macro_definition, duckdb::unique_ptr<duckdb::CreateMacroInfo>()) {
	DM macro {schema, name, definition};
	return duckdb::DefaultFunctionGenerator::CreateInternalMacroInfo(macro);
}
template <class INFO = duckdb::CreateMacroInfo>
duckdb::unique_ptr<INFO> ScalarMacroInfo(const char *schema, const char *name, const char *definition, long) {
	duckdb::Parser parser;
	parser.ParseQuery(std::string("CREATE MACRO __duckdb_compat_macro__") + definition);
	if (parser.statements.size() != 1 || parser.statements[0]->type != duckdb::StatementType::CREATE_STATEMENT) {
		throw duckdb::InternalException("ScalarMacroInfo: '%s' is not a macro definition", definition);
	}
	auto &create = parser.statements[0]->template Cast<duckdb::CreateStatement>();
	auto info = duckdb::unique_ptr_cast<duckdb::CreateInfo, INFO>(std::move(create.info));
	info->schema = schema;
	info->name = name;
	info->temporary = true;
	info->internal = true;
	return info;
}
} // namespace detail

//! A built-in scalar macro from a 2.0-style definition, e.g.
//! ScalarMacroInfo(DEFAULT_SCHEMA, "f", "(a, b := '4') AS a || b").
inline duckdb::unique_ptr<duckdb::CreateMacroInfo> ScalarMacroInfo(const char *schema, const char *name,
                                                                   const char *definition) {
	return detail::ScalarMacroInfo(schema, name, definition, 0);
}

} // namespace duckdb_compat
