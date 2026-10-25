// duckdb_compat.hpp -- one source tree for DuckDB 1.5 and 2.0.
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
#include "duckdb/parser/constraints/unique_constraint.hpp"
#include "duckdb/parser/qualified_name.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/catalog/entry_lookup_info.hpp"
#include "duckdb/storage/storage_extension.hpp"
#include "duckdb/main/config.hpp"
#include "duckdb/planner/expression/bound_conjunction_expression.hpp"
#include "duckdb/planner/expression/bound_constant_expression.hpp"
#include "duckdb/planner/expression/bound_reference_expression.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb_compat/dispatch.hpp"

#include <optional>
#include <map>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace duckdb_compat {

namespace detail {
template <class TYPE>
auto TypeInfoIdentity(const TYPE &type, int) -> decltype(static_cast<const void *>(&type.GetTypeInfo())) {
	return &type.GetTypeInfo();
}
template <class TYPE>
auto TypeInfoIdentity(const TYPE &type, long) -> decltype(static_cast<const void *>(type.AuxInfo().get())) {
	return type.AuxInfo().get();
}
} // namespace detail
inline const void *TypeInfoIdentity(const duckdb::LogicalType &type) {
	return detail::TypeInfoIdentity(type, 0);
}

namespace detail {
template <class EXTENSION = duckdb::StorageExtension, class CONFIG>
auto RegisterStorageExtension(CONFIG &config, const std::string &name,
                              duckdb::shared_ptr<duckdb::StorageExtension> extension, int)
    -> decltype(EXTENSION::Register(config, name, std::move(extension)), void()) {
	EXTENSION::Register(config, name, std::move(extension));
}
template <class CONFIG>
auto RegisterStorageExtension(CONFIG &config, const std::string &name,
                              duckdb::shared_ptr<duckdb::StorageExtension> extension, long)
    -> decltype(config.storage_extensions[name] = duckdb::unique_ptr<duckdb::StorageExtension>(), void()) {
	auto owned = duckdb::make_uniq<duckdb::StorageExtension>();
	owned->attach = extension->attach;
	owned->create_transaction_manager = extension->create_transaction_manager;
	owned->storage_info = extension->storage_info;
	config.storage_extensions[name] = std::move(owned);
}
} // namespace detail
inline void RegisterStorageExtension(duckdb::DBConfig &config, const std::string &name,
                                     duckdb::shared_ptr<duckdb::StorageExtension> extension) {
	detail::RegisterStorageExtension(config, name, std::move(extension), 0);
}

// Function::name is string on 1.x and Identifier on 2.0. Reading its type
// avoids finding an unrelated system DuckDB header on older source trees.
using IdentifierType = std::decay_t<decltype(std::declval<duckdb::Function &>().name)>;

//! Return type of Catalog::GetDefaultSchema (string on 1.x, optional<Identifier> on 2.0).
using DefaultSchemaName = decltype(std::declval<const duckdb::Catalog &>().GetDefaultSchema());

namespace detail {
template <class FLAT = duckdb::FlatVector>
auto MutableValidity(duckdb::Vector &vector, int) -> decltype(FLAT::ValidityMutable(vector)) {
	return FLAT::ValidityMutable(vector);
}
template <class FLAT = duckdb::FlatVector>
auto MutableValidity(duckdb::Vector &vector, long) -> decltype(FLAT::Validity(vector)) {
	return FLAT::Validity(vector);
}
} // namespace detail

inline duckdb::ValidityMask &MutableValidity(duckdb::Vector &vector) {
	return detail::MutableValidity(vector, 0);
}

namespace detail {
template <class TYPE>
auto WithAlias(TYPE type, const std::string &alias, int) -> decltype(type.WithAlias(alias)) {
	return type.WithAlias(alias);
}
template <class TYPE>
TYPE WithAlias(TYPE type, const std::string &alias, long) {
	type.SetAlias(alias);
	return type;
}
} // namespace detail

inline duckdb::LogicalType WithAlias(duckdb::LogicalType type, const std::string &alias) {
	return detail::WithAlias(std::move(type), alias, 0);
}

namespace detail {
template <class EXPR = duckdb::ConstantExpression>
auto ParsedConstant(const duckdb::Value &value, int) -> decltype(EXPR::FromValue(value)) {
	return EXPR::FromValue(value);
}
template <class EXPR = duckdb::ConstantExpression>
duckdb::unique_ptr<duckdb::ParsedExpression> ParsedConstant(const duckdb::Value &value, long) {
	return duckdb::make_uniq<EXPR>(value);
}
} // namespace detail

inline duckdb::unique_ptr<duckdb::ParsedExpression> ParsedConstant(const duckdb::Value &value) {
	return detail::ParsedConstant(value, 0);
}

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
using BindNames =
    typename std::remove_reference<typename detail::BindFnTraits<duckdb::table_function_bind_t>::names_ref>::type;
using BindNamedParameters =
    typename std::remove_reference<decltype(std::declval<duckdb::TableFunctionBindInput &>().named_parameters)>::type;

inline void AssignBindNames(BindNames &out, const std::vector<std::string> &names) {
	out.clear();
	out.reserve(names.size());
	for (const auto &name : names) {
		out.emplace_back(name);
	}
}

// --- FileOpener::TryGetCurrentSetting: key is a string before 2.0, an
// Identifier in 2.0 (explicit from string). ----------------------------------
namespace detail {
template <class O>
auto TryGetSetting(O opener, const std::string &key, duckdb::Value &value, int)
    -> decltype(duckdb::FileOpener::TryGetCurrentSetting(opener, key, value)) {
	return duckdb::FileOpener::TryGetCurrentSetting(opener, key, value);
}
template <class O>
auto TryGetSetting(O opener, const std::string &key, duckdb::Value &value, long)
    -> decltype(duckdb::FileOpener::TryGetCurrentSetting(opener, IdentifierType(key), value)) {
	return duckdb::FileOpener::TryGetCurrentSetting(opener, IdentifierType(key), value);
}
} // namespace detail

//! FileOpener::TryGetCurrentSetting(opener, key, value) on every line.
inline duckdb::SettingLookupResult TryGetSetting(duckdb::optional_ptr<duckdb::FileOpener> opener,
                                                 const std::string &key, duckdb::Value &value) {
	return detail::TryGetSetting(opener, key, value, 0);
}

//! A memory size ('4GB', '80%', ...) in bytes, with the maximum idx_t for
//! "unlimited" -- DBConfig::ParseMemoryLimit's contract before 2.0.
inline duckdb::idx_t MemoryLimitBytes(const std::string &arg) {
	return detail::MemoryLimitBytes<duckdb::DBConfig>(arg, 0);
}

namespace detail {
template <class C> auto TryGetContextSetting(C &context, const std::string &key, duckdb::Value &value, int)
    -> decltype(context.TryGetCurrentSetting(IdentifierType(key), value)) {
	return context.TryGetCurrentSetting(IdentifierType(key), value);
}
template <class C> auto TryGetContextSetting(C &context, const std::string &key, duckdb::Value &value, long)
    -> decltype(context.TryGetCurrentSetting(key, value)) {
	return context.TryGetCurrentSetting(key, value);
}
} // namespace detail
template <class C> auto TryGetContextSetting(C &context, const std::string &key, duckdb::Value &value) {
	return detail::TryGetContextSetting(context, key, value, 0);
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
    -> decltype(EXEC::template ExecuteWithNulls<INPUT_TYPE, RESULT_TYPE>(
                    input, result, count, std::declval<WithNullsAdapter<INPUT_TYPE, RESULT_TYPE, F>>()),
	            void()) {
	EXEC::template ExecuteWithNulls<INPUT_TYPE, RESULT_TYPE>(input, result, count,
	                                                         WithNullsAdapter<INPUT_TYPE, RESULT_TYPE, F> {fun});
}
template <class INPUT_TYPE, class RESULT_TYPE, class F, class EXEC = duckdb::UnaryExecutor>
void ExecuteUnaryNullable(duckdb::Vector &input, duckdb::Vector &result, duckdb::idx_t count, F &fun, long) {
	EXEC::template Execute<INPUT_TYPE, RESULT_TYPE>(input, result, count,
	                                                OptionalAdapter<INPUT_TYPE, RESULT_TYPE, F> {fun});
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
using CopyBindNames = typename std::remove_cv<
    typename std::remove_reference<typename detail::CopyBindFnTraits<duckdb::copy_to_bind_t>::names_ref>::type>::type;
using CopyOptions =
    typename std::remove_reference<decltype(std::declval<duckdb::CopyFunctionBindInput &>().info.options)>::type;

namespace detail {
template <class>
struct OneArgFn;
template <class R, class A>
struct OneArgFn<R (*)(A)> {
	using argument = A;
};
template <class BIND_INFO = duckdb::BindInfo>
auto MakeTableBindInfo(duckdb::TableCatalogEntry &table, int) -> decltype(BIND_INFO(table)) {
	return BIND_INFO(table);
}
template <class BIND_INFO = duckdb::BindInfo>
BIND_INFO MakeTableBindInfo(duckdb::TableCatalogEntry &, long) {
	// Only MakeTableBindInfo calls this fallback with a real table entry.
	return BIND_INFO(duckdb::ScanType::TABLE);
}
} // namespace detail

using GetBindInfoInput = typename detail::OneArgFn<duckdb::table_function_get_bind_info_t>::argument;
//! Extract bind data from a get_bind_info callback. On 2.0 the input wraps
//! bind_data; on older lines it is the bind-data pointer itself.
inline auto GetBindData(GetBindInfoInput &input) -> decltype(detail::GetBindData<GetBindInfoInput>(input, 0)) {
	return detail::GetBindData<GetBindInfoInput>(input, 0);
}
//! On 2.0 BindInfo(table) is gone: the table is identified by get_table_entry,
//! which must be registered alongside get_bind_info.
inline duckdb::BindInfo MakeTableBindInfo(duckdb::TableCatalogEntry &table) {
	return detail::MakeTableBindInfo(table, 0);
}

namespace detail {
template <class FUNCTION, class GETTER_FN>
auto SetTableEntryGetter(FUNCTION &function, GETTER_FN callback, int)
    -> decltype(function.get_table_entry = callback, void()) {
	function.get_table_entry = callback;
}
template <class FUNCTION, class GETTER_FN>
void SetTableEntryGetter(FUNCTION &, GETTER_FN, long) {
	// Older DuckDB releases use get_bind_info to identify the table entry.
}
} // namespace detail

//! Register this together with get_bind_info for catalog scans. DuckDB 2.0
//! asks get_table_entry when binding DELETE/UPDATE; older lines use bind info.
template <class FUNCTION, class GETTER_FN>
void SetTableEntryGetter(FUNCTION &function, GETTER_FN callback) {
	static_assert(std::is_same_v<std::decay_t<FUNCTION>, duckdb::TableFunction>,
	              "Table entry getter requires a TableFunction");
	static_assert(std::is_invocable_r_v<duckdb::optional_ptr<duckdb::TableCatalogEntry>, GETTER_FN,
	                                    duckdb::optional_ptr<const duckdb::FunctionData>>,
	              "Table entry getter must accept bind data and return a table entry");
	detail::SetTableEntryGetter(function, callback, 0);
}

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
// Before 2.0: fn.named_parameters[name] = type -- optional; a call that leaves
// it out does not pass it. 2.0: named parameters are part of the signature.
// AddKeywordOnly(name, type) would make the parameter REQUIRED (a keyword-only
// parameter without a default, as in Python), so every call omitting it fails
// to bind. The 1.x semantics are 2.0's typed "**kwargs" -- what read_csv uses:
// one "**options" schema per function, extended per parameter.
namespace detail {
template <class FN>
auto AddNamedParameter(FN &fn, const std::string &name, const duckdb::LogicalType &type, int)
    -> decltype(fn.GetSignature().GetTypedKwargs(), void()) {
	auto &signature = fn.GetSignature();
	auto add = [&](auto &kwargs) {
		kwargs.Add(FunctionNameType(name), type);
	};
	if (signature.GetTypedKwargs()) {
		signature.ExtendTypedKwargs(add);
	} else {
		signature.WithTypedKwargs(FunctionNameType("options"), add);
	}
}
template <class FN>
void AddNamedParameter(FN &fn, const std::string &name, const duckdb::LogicalType &type, long) {
	fn.named_parameters[typename decltype(fn.named_parameters)::key_type(name)] = type;
}
} // namespace detail

//! Declare an optional named parameter on a table function: a call may leave
//! it out, and then the bind input does not carry it (FindNamedParameter
//! returns nullptr).
template <class FN>
void AddNamedParameter(FN &fn, const std::string &name, const duckdb::LogicalType &type) {
	detail::AddNamedParameter(fn, name, type, 0);
}

namespace detail {
template <class FN>
auto NamedParameterTypes(const FN &fn, int) -> decltype(fn.GetSignature().GetTypedKwargs(),
                                                        std::map<std::string, duckdb::LogicalType>()) {
	std::map<std::string, duckdb::LogicalType> result;
	auto options = fn.GetSignature().GetTypedKwargs();
	if (options) {
		for (const auto &option : options->GetOptions()) {
			result.emplace(::duckdb_compat::Str(option.name), option.type);
		}
	}
	return result;
}
template <class FN>
auto NamedParameterTypes(const FN &fn, long) -> decltype(fn.named_parameters,
                                                          std::map<std::string, duckdb::LogicalType>()) {
	std::map<std::string, duckdb::LogicalType> result;
	for (const auto &option : fn.named_parameters) {
		result.emplace(::duckdb_compat::Str(option.first), option.second);
	}
	return result;
}
} // namespace detail
template <class FN>
std::map<std::string, duckdb::LogicalType> NamedParameterTypes(const FN &fn) {
	return detail::NamedParameterTypes(fn, 0);
}

namespace detail {
template <class FN>
auto SetVarArgs(FN &fn, const duckdb::LogicalType &type, int)
    -> decltype(fn.GetSignature().AddArgs(FunctionNameType("args"), type), void()) {
	fn.GetSignature().AddArgs(FunctionNameType("args"), type);
}
template <class FN>
void SetVarArgs(FN &fn, const duckdb::LogicalType &type, long) {
	fn.varargs = type;
}
} // namespace detail

template <class FN>
void SetVarArgs(FN &fn, const duckdb::LogicalType &type) {
	detail::SetVarArgs(fn, type, 0);
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

// --- Parameter names shown by duckdb_functions() -------------------------------
// Before 2.0 they come from FunctionDescription::parameter_names. 2.0 prefers
// the signature's own names, and positional parameters made by the
// TableFunction/ScalarFunction constructors are named col0, col1, ... -- so the
// documented names never show. Name the positional parameters in the signature.
namespace detail {
template <class FN>
auto NameParameters(FN &fn, const std::vector<std::string> &names, int)
    -> decltype(fn.GetSignature().GetParameter(0).SetName(FunctionNameType(std::string())), void()) {
	auto &signature = fn.GetSignature();
	for (duckdb::idx_t i = 0; i < signature.GetParameterCount() && i < names.size(); i++) {
		auto &param = signature.GetParameter(i);
		if (!param.AcceptsPosition()) {
			break;
		}
		param.SetName(FunctionNameType(names[i]));
	}
}
// Old lines: a no-op -- but only for an actual function (it has a `name`), so
// passing something else (e.g. a 2.0 set's shared_ptr overload) fails to
// compile instead of silently doing nothing.
template <class FN>
auto NameParameters(FN &fn, const std::vector<std::string> &, long) -> decltype((void)fn.name, void()) {
}
} // namespace detail

//! Name a function's positional parameters (as FunctionDescription::
//! parameter_names does before 2.0, where this is a no-op).
template <class FN>
void NameParameters(FN &fn, const std::vector<std::string> &names) {
	detail::NameParameters(fn, names, 0);
}

namespace detail {
// 2.0: replace each positional parameter by a STANDARD one (positional or by name). The
// parameter type is taken from the signature, so nothing names duckdb::FunctionParameter
// directly (it does not exist before 2.0).
template <class FN>
auto NameArguments(FN &fn, const std::vector<std::string> &names, int)
    -> decltype(fn.GetSignature().GetParameter(0) =
                    std::decay_t<decltype(fn.GetSignature().GetParameter(0))>(
                        FunctionNameType(std::string()), fn.GetSignature().GetParameter(0).GetType()),
                void()) {
	using PARAM = std::decay_t<decltype(fn.GetSignature().GetParameter(0))>;
	auto &signature = fn.GetSignature();
	for (duckdb::idx_t i = 0; i < signature.GetParameterCount() && i < names.size(); i++) {
		auto &param = signature.GetParameter(i);
		if (!param.AcceptsPosition()) {
			break;
		}
		param = PARAM(FunctionNameType(names[i]), param.GetType());
	}
}
template <class FN>
auto NameArguments(FN &fn, const std::vector<std::string> &, long) -> decltype((void)fn.name, void()) {
}
} // namespace detail

//! Name a scalar/aggregate function's positional parameters AND let callers pass them by
//! that name, e.g. f(a, b, model := 'x'). Before 2.0 a `name := value` argument of a scalar
//! function binds by position, so this is a no-op. On 2.0 constructor-made parameters are
//! positional-only ("col0, /") and a named argument fails to bind; this turns them into
//! standard parameters. Use NameParameters instead when only duckdb_functions() should show
//! the names.
template <class FN>
void NameArguments(FN &fn, const std::vector<std::string> &names) {
	detail::NameArguments(fn, names, 0);
}

// --- Changing every overload of a function set ---------------------------------
// Before 2.0: set.functions is vector<T>, mutable in place. 2.0: it holds
// immutable shared_ptr<const T>, changed only through ApplyToFunctions().
namespace detail {
template <class FN>
auto HasInitGlobal(const FN &fn, int) -> decltype(fn->init_global, bool()) {
	return fn->init_global != nullptr;
}
template <class FN>
auto HasInitGlobal(const FN &fn, long) -> decltype(fn.init_global, bool()) {
	return fn.init_global != nullptr;
}
template <class SET, class F>
auto ForEachFunction(SET &set, F &&f, int) -> decltype(set.ApplyToFunctions(f), void()) {
	set.ApplyToFunctions(f);
}
template <class SET, class F>
void ForEachFunction(SET &set, F &&f, long) {
	for (auto &fun : set.functions) {
		f(fun);
	}
}
} // namespace detail

template <class FN>
bool HasInitGlobal(const FN &fn) {
	return detail::HasInitGlobal(fn, 0);
}

//! Call f(T &fun) on every overload of a function set, so the change sticks.
template <class SET, class F>
void ForEachFunction(SET &set, F &&f) {
	detail::ForEachFunction(set, std::forward<F>(f), 0);
}

template <class INFO>
void NameFunctionsFromDescriptions(INFO &info) {
	if (info.descriptions.empty()) {
		return;
	}
	duckdb::idx_t count = 0;
	std::string function_name;
	ForEachFunction(info.functions, [&](auto &fn) {
		if (count == 0) {
			function_name = Str(fn.name);
		}
		count++;
	});
	if (count != info.descriptions.size()) {
		throw duckdb::InternalException("Function descriptions do not match overload count for " + function_name +
		                                " (" + std::to_string(count) + " overloads, " +
		                                std::to_string(info.descriptions.size()) + " descriptions)");
	}
	duckdb::idx_t index = 0;
	// descriptions[i] names overload i in ForEachFunction/AddFunction order.
	ForEachFunction(info.functions, [&](auto &fn) {
		NameParameters(fn, info.descriptions[index].parameter_names);
		index++;
	});
}

//! NameArguments for every overload, from the info's descriptions (descriptions[i] names
//! overload i). Same count rule as NameFunctionsFromDescriptions.
template <class INFO>
void NameArgumentsFromDescriptions(INFO &info) {
	if (info.descriptions.empty()) {
		return;
	}
	duckdb::idx_t count = 0;
	ForEachFunction(info.functions, [&](auto &) { count++; });
	if (count != info.descriptions.size()) {
		throw duckdb::InternalException("Function descriptions do not match overload count (" +
		                                std::to_string(count) + " overloads, " +
		                                std::to_string(info.descriptions.size()) + " descriptions)");
	}
	duckdb::idx_t index = 0;
	ForEachFunction(info.functions, [&](auto &fn) {
		NameArguments(fn, info.descriptions[index].parameter_names);
		index++;
	});
}

// Read positional argument types across the legacy function fields and 2.0 signatures.
namespace detail {
template <class FN>
auto FunctionArgumentTypes(const FN &fn, int)
    -> decltype(fn.GetSignature().GetPositionalParameterCount(), fn.GetSignature().GetParameter(duckdb::idx_t{}).GetType(),
                std::vector<duckdb::LogicalType>{}) {
    const auto &signature = fn.GetSignature();
    std::vector<duckdb::LogicalType> result;
    for (duckdb::idx_t i = 0; i < signature.GetPositionalParameterCount(); i++) {
        result.push_back(signature.GetParameter(i).GetType());
    }
    return result;
}
template <class FN>
auto FunctionArgumentTypes(const FN &fn, long) -> decltype(fn.arguments, std::vector<duckdb::LogicalType>{}) {
    return {fn.arguments.begin(), fn.arguments.end()};
}
} // namespace detail
template <class FN>
std::vector<duckdb::LogicalType> FunctionArgumentTypes(const FN &fn) {
    return detail::FunctionArgumentTypes(fn, 0);
}

// --- Query results --------------------------------------------------------------
// Before 2.0: Connection::Query returns a MaterializedQueryResult with public
// `names` and GetValue(col, row). 2.0 removed MaterializedQueryResult; names
// are private (GetNames(), Identifier). Fetch() exists on every line.
namespace detail {
template <class RESULT>
auto ResultTypes(const RESULT &result, int) -> decltype(result.GetTypes()) {
	return result.GetTypes();
}
template <class RESULT>
auto ResultTypes(const RESULT &result, long) -> decltype((result.types)) {
	return result.types;
}
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

template <class RESULT>
const duckdb::vector<duckdb::LogicalType> &ResultTypes(const RESULT &result) {
	return detail::ResultTypes(result, 0);
}

template <class RESULT>
duckdb::Value ResultValue(RESULT &result, duckdb::idx_t column, duckdb::idx_t row) {
	return detail::ResultValue(result, column, row, 0);
}

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
// P is a template parameter so that the default-constructed Parser (gone on
// 2.0) is only looked up when this overload is instantiated: clang rejects a
// non-dependent use at definition time.
template <class INFO = duckdb::CreateMacroInfo, class P = duckdb::Parser>
duckdb::unique_ptr<INFO> ScalarMacroInfo(const char *schema, const char *name, const char *definition, long) {
	P parser;
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

// --- Catalog identifiers ------------------------------------------------------------
// On 1.5.6 most catalog names are still std::string, and its Identifier type
// converts implicitly to and from std::string (a backport so that 2.0 spellings
// compile). On 2.0 both conversions are explicit, and catalog APIs take and
// return Identifier.

//! A runtime string as the identifier type catalog APIs take: pass the result
//! wherever 2.0 wants an Identifier (Catalog::GetCatalogEntry, GetSchema,
//! ColumnList::GetColumn, ColumnDefinition, ...). For the reverse direction use
//! Str().
inline IdentifierType Ident(const std::string &name) {
	return IdentifierType(name);
}

//! A list of names (std::string or Identifier) as std::strings.
template <class C>
std::vector<std::string> Strs(const C &names) {
	std::vector<std::string> result;
	result.reserve(names.size());
	for (auto &name : names) {
		result.emplace_back(::duckdb_compat::Str(name));
	}
	return result;
}

//! A list of strings as identifiers.
inline std::vector<IdentifierType> Idents(const std::vector<std::string> &names) {
	std::vector<IdentifierType> result;
	result.reserve(names.size());
	for (auto &name : names) {
		result.emplace_back(name);
	}
	return result;
}

namespace detail {
template <class QN> auto QualifiedCatalog(const QN &name, int) -> decltype(::duckdb_compat::Str(name.Catalog())) {
	return ::duckdb_compat::Str(name.Catalog());
}
template <class QN> auto QualifiedCatalog(const QN &name, long) -> decltype(name.catalog) {
	return name.catalog;
}
template <class QN> auto QualifiedSchema(const QN &name, int) -> decltype(::duckdb_compat::Str(name.Schema())) {
	return ::duckdb_compat::Str(name.Schema());
}
template <class QN> auto QualifiedSchema(const QN &name, long) -> decltype(name.schema) {
	return name.schema;
}
template <class QN> auto QualifiedLocalName(const QN &name, int) -> decltype(::duckdb_compat::Str(name.Name())) {
	return ::duckdb_compat::Str(name.Name());
}
template <class QN> auto QualifiedLocalName(const QN &name, long) -> decltype(name.name) {
	return name.name;
}
template <class QN>
auto QualifiedWithSchema(const QN &name, const std::string &schema, int)
    -> decltype(name.WithQualification(std::declval<duckdb::vector<IdentifierType>>())) {
	duckdb::vector<IdentifierType> path;
	auto catalog = QualifiedCatalog(name, 0);
	if (!catalog.empty()) {
		path.emplace_back(catalog);
	}
	path.emplace_back(schema);
	return name.WithQualification(std::move(path));
}
template <class QN> QN QualifiedWithSchema(QN name, const std::string &schema, long) {
	name.schema = schema;
	return name;
}
} // namespace detail
template <class QN> std::string QualifiedCatalog(const QN &name) { return detail::QualifiedCatalog(name, 0); }
template <class QN> std::string QualifiedSchema(const QN &name) { return detail::QualifiedSchema(name, 0); }
template <class QN> std::string QualifiedLocalName(const QN &name) { return detail::QualifiedLocalName(name, 0); }
template <class QN> QN QualifiedWithSchema(const QN &name, const std::string &schema) {
	return detail::QualifiedWithSchema(name, schema, 0);
}

// --- UNIQUE / PRIMARY KEY constraints over named columns --------------------------
// UniqueConstraint(vector<string>, bool) before 2.0, (vector<Identifier>, bool)
// on 2.0 -- and on 1.5.6 a vector<Identifier> does not convert to vector<string>.
namespace detail {
template <class UC>
auto MakeUniqueConstraint(std::vector<std::string> &columns, bool is_primary_key, int)
    -> decltype(UC(std::declval<std::vector<IdentifierType>>(), is_primary_key), duckdb::unique_ptr<UC>()) {
	return duckdb::make_uniq<UC>(Idents(columns), is_primary_key);
}
template <class UC>
duckdb::unique_ptr<UC> MakeUniqueConstraint(std::vector<std::string> &columns, bool is_primary_key, long) {
	return duckdb::make_uniq<UC>(std::move(columns), is_primary_key);
}
} // namespace detail

//! A UNIQUE (or PRIMARY KEY) constraint over the named columns.
inline duckdb::unique_ptr<duckdb::UniqueConstraint> MakeUniqueConstraint(std::vector<std::string> columns,
                                                                         bool is_primary_key) {
	return detail::MakeUniqueConstraint<duckdb::UniqueConstraint>(columns, is_primary_key, 0);
}

// --- Qualified names of CreateInfo / DropInfo ---------------------------------------
// 1.5.6: public `catalog`/`schema` fields plus 2.0-style setters (SetQualifiedName,
// CreateInfo::SetSchema/SetCatalog; DropInfo has only SetQualifiedName, so its
// schema and catalog still go through the fields). 2.0: one private
// QualifiedName behind those setters, taking Identifiers.
namespace detail {
template <class INFO, class QN = duckdb::QualifiedName>
auto SetQualifiedName(INFO &info, const std::string &catalog, const std::string &schema, const std::string &name, int)
    -> decltype(info.SetQualifiedName(QN(Ident(catalog), Ident(schema), Ident(name))), void()) {
	info.SetQualifiedName(QN(Ident(catalog), Ident(schema), Ident(name)));
}

template <class INFO>
auto SetSchema(INFO &info, const std::string &schema, int) -> decltype(info.SetSchema(Ident(schema)), void()) {
	info.SetSchema(Ident(schema));
}
template <class INFO>
void SetSchema(INFO &info, const std::string &schema, long) {
	info.schema = schema;
}

template <class INFO>
auto SetCatalog(INFO &info, const std::string &catalog, int) -> decltype(info.SetCatalog(Ident(catalog)), void()) {
	info.SetCatalog(Ident(catalog));
}
template <class INFO>
void SetCatalog(INFO &info, const std::string &catalog, long) {
	info.catalog = catalog;
}
} // namespace detail

//! Set catalog, schema and entry name of a CreateInfo subclass (table, view,
//! sequence, ...) or a DropInfo.
template <class INFO>
void SetQualifiedName(INFO &info, const std::string &catalog, const std::string &schema, const std::string &name) {
	detail::SetQualifiedName(info, catalog, schema, name, 0);
}

//! Set only the schema of a CreateInfo / DropInfo (for CreateSchemaInfo: the
//! schema being created).
template <class INFO>
void SetSchema(INFO &info, const std::string &schema) {
	detail::SetSchema(info, schema, 0);
}

//! Set only the catalog of a CreateInfo / DropInfo.
template <class INFO>
void SetCatalog(INFO &info, const std::string &catalog) {
	detail::SetCatalog(info, catalog, 0);
}

// --- Catalog entry lookups ----------------------------------------------------------
// 2.0 folds catalog and schema into a QualifiedName: the (schema, name)
// overloads are deprecated and EntryLookupInfo wants a QualifiedName. 1.5.6
// already has the static Catalog::GetEntry<T>(context, QualifiedName, ...).

//! True when EntryLookupInfo takes a QualifiedName (2.0)
using EntryLookupTakesQualifiedName = std::integral_constant<
    bool, std::is_constructible<duckdb::EntryLookupInfo, duckdb::CatalogType, const duckdb::QualifiedName &>::value>;

namespace detail {
template <class QN = duckdb::QualifiedName>
QN EntryName(const std::string &name, std::true_type) {
	return QN(Ident(name));
}
inline const std::string &EntryName(const std::string &name, std::false_type) {
	return name;
}
} // namespace detail

//! The name argument of EntryLookupInfo(type, EntryName(name), ...): a
//! QualifiedName by value on 2.0; before 2.0 `name` itself -- EntryLookupInfo
//! keeps a *reference* to its name there, so `name` must outlive the lookup
//! (never pass a temporary).
inline auto EntryName(const std::string &name) -> decltype(detail::EntryName(name, EntryLookupTakesQualifiedName())) {
	return detail::EntryName(name, EntryLookupTakesQualifiedName());
}

// The instance lookups within one catalog by (schema, name) or (schema,
// EntryLookupInfo) still exist on 2.0, deprecated in favour of folding the
// schema into the lookup -- whose non-deprecated form is static and resolves the
// catalog by name instead. These wrappers keep the instance semantics on every
// line with the deprecation silenced in one place.
#if defined(__GNUC__) || defined(__clang__)
#define DUCKDB_COMPAT_PUSH_NO_DEPRECATED                                                                               \
	_Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#define DUCKDB_COMPAT_POP_NO_DEPRECATED _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#define DUCKDB_COMPAT_PUSH_NO_DEPRECATED __pragma(warning(push)) __pragma(warning(disable : 4996))
#define DUCKDB_COMPAT_POP_NO_DEPRECATED  __pragma(warning(pop))
#else
#define DUCKDB_COMPAT_PUSH_NO_DEPRECATED
#define DUCKDB_COMPAT_POP_NO_DEPRECATED
#endif

//! catalog.GetEntry<T>(context, schema, name, if_not_found): the entry in this
//! catalog, or nullptr.
template <class T>
duckdb::optional_ptr<T> GetEntry(duckdb::Catalog &catalog, duckdb::ClientContext &context, const std::string &schema,
                                 const std::string &name,
                                 duckdb::OnEntryNotFound if_not_found = duckdb::OnEntryNotFound::RETURN_NULL) {
	DUCKDB_COMPAT_PUSH_NO_DEPRECATED
	return catalog.GetEntry<T>(context, Ident(schema), Ident(name), if_not_found);
	DUCKDB_COMPAT_POP_NO_DEPRECATED
}

//! catalog.GetEntry(context, schema, lookup, if_not_found): `lookup` (type,
//! name, AT clause) within one schema of this catalog.
inline duckdb::optional_ptr<duckdb::CatalogEntry>
GetEntryInSchema(duckdb::Catalog &catalog, duckdb::ClientContext &context, const std::string &schema,
                 const duckdb::EntryLookupInfo &lookup, duckdb::OnEntryNotFound if_not_found) {
	DUCKDB_COMPAT_PUSH_NO_DEPRECATED
	return catalog.GetEntry(context, Ident(schema), lookup, if_not_found);
	DUCKDB_COMPAT_POP_NO_DEPRECATED
}

// --- Constant vectors -----------------------------------------------------------------
// 2.0 vectors carry their own size: Vector(Value) and Vector::Reference(Value)
// take the row count (count_t, explicit from idx_t).
namespace detail {
struct RowCount {
	duckdb::idx_t count;
	template <class T,
	          std::enable_if_t<std::is_class_v<T> && std::is_constructible_v<T, duckdb::idx_t> &&
	                               !std::is_convertible_v<duckdb::idx_t, T>,
	                           int> = 0>
	operator T() const {
		return T(count);
	}
};
template <class V>
auto ReferenceValue(V &vector, const duckdb::Value &value, duckdb::idx_t count, int)
    -> decltype(vector.Reference(value, RowCount {count}), void()) {
	vector.Reference(value, RowCount {count});
}
template <class V>
void ReferenceValue(V &vector, const duckdb::Value &value, duckdb::idx_t, long) {
	vector.Reference(value);
}

template <class V>
auto MakeValueVector(const duckdb::Value &value, duckdb::idx_t count, int) -> decltype(V(value, RowCount {count})) {
	return V(value, RowCount {count});
}
template <class V>
V MakeValueVector(const duckdb::Value &value, duckdb::idx_t, long) {
	return V(value);
}
} // namespace detail

//! Make `vector` a constant vector holding `value` for `count` rows
//! (Vector::Reference(Value)).
inline void ReferenceValue(duckdb::Vector &vector, const duckdb::Value &value, duckdb::idx_t count) {
	detail::ReferenceValue(vector, value, count, 0);
}

//! A constant vector holding `value` for `count` rows (Vector(Value)).
inline duckdb::Vector MakeValueVector(const duckdb::Value &value, duckdb::idx_t count) {
	return detail::MakeValueVector<duckdb::Vector>(value, count, 0);
}

// --- Writing flat vector data ---------------------------------------------------------
// 2.0: FlatVector::GetData<T> returns const T*; writes go through GetDataMutable.
namespace detail {
template <class T, class FV = duckdb::FlatVector>
auto FlatData(duckdb::Vector &vector, int) -> decltype(FV::template GetDataMutable<T>(vector)) {
	return FV::template GetDataMutable<T>(vector);
}
template <class T, class FV = duckdb::FlatVector>
T *FlatData(duckdb::Vector &vector, long) {
	return FV::template GetData<T>(vector);
}
} // namespace detail

//! The writable data of a flat vector (FlatVector::GetData / GetDataMutable).
template <class T>
T *FlatData(duckdb::Vector &vector) {
	return detail::FlatData<T>(vector, 0);
}

// --- Bound expressions -------------------------------------------------------------------
// 2.0 made the fields of bound expressions private: BoundReferenceExpression::
// index -> Index() / IndexMutable(), BoundConjunctionExpression::children ->
// GetChildren() / GetChildrenMutable().
namespace detail {
template <class E>
auto ReferenceIndex(const E &expr, int) -> decltype(duckdb::idx_t(expr.Index())) {
	return expr.Index();
}
template <class E>
duckdb::idx_t ReferenceIndex(const E &expr, long) {
	return expr.index;
}

template <class E>
auto Children(E &expr, int) -> decltype(expr.GetChildrenMutable()) {
	return expr.GetChildrenMutable();
}
template <class E>
auto Children(const E &expr, int) -> decltype(expr.GetChildren()) {
	return expr.GetChildren();
}
template <class E>
auto Children(E &expr, long) -> decltype((expr.children)) {
	return expr.children;
}

template <class E>
auto ConstantValue(const E &expr, int) -> decltype(expr.GetValue()) {
	return expr.GetValue();
}
template <class E>
auto ConstantValue(const E &expr, long) -> decltype((expr.value)) {
	return expr.value;
}
} // namespace detail

//! The value of a BoundConstantExpression.
inline const duckdb::Value &ConstantValue(const duckdb::BoundConstantExpression &expr) {
	return detail::ConstantValue(expr, 0);
}

//! The chunk column a BoundReferenceExpression reads.
inline duckdb::idx_t ReferenceIndex(const duckdb::BoundReferenceExpression &expr) {
	return detail::ReferenceIndex(expr, 0);
}

//! The children of a bound expression that has a child list, e.g.
//! BoundConjunctionExpression (mutable unless the expression is const).
template <class E>
auto Children(E &expr) -> decltype(detail::Children(expr, 0)) {
	return detail::Children(expr, 0);
}

// --- Parsing SQL --------------------------------------------------------------------------
// 2.0: ParserOptions' default constructor is private; a Parser is made from a
// ClientContext (its settings) or ParserOptions::Builtin(). Before 2.0: Parser().
namespace detail {
template <class P = duckdb::Parser>
auto ParseStatements(duckdb::ClientContext &context, const std::string &sql, int)
    -> decltype(P(context), duckdb::vector<duckdb::unique_ptr<duckdb::SQLStatement>>()) {
	P parser(context);
	parser.ParseQuery(sql);
	return std::move(parser.statements);
}
template <class P = duckdb::Parser>
duckdb::vector<duckdb::unique_ptr<duckdb::SQLStatement>> ParseStatements(duckdb::ClientContext &,
                                                                         const std::string &sql, long) {
	P parser;
	parser.ParseQuery(sql);
	return std::move(parser.statements);
}
} // namespace detail

//! The statements of `sql`, parsed with the context's parser settings (2.0)
//! or the default ones (before 2.0).
inline duckdb::vector<duckdb::unique_ptr<duckdb::SQLStatement>> ParseStatements(duckdb::ClientContext &context,
                                                                                const std::string &sql) {
	return detail::ParseStatements(context, sql, 0);
}

// --- Vector sizes -------------------------------------------------------------------------
// 2.0 vectors carry their own size: a vector filled through SetValue or its
// raw data keeps its size (0 for a fresh one) until it is set, and consumers
// read that size. Before 2.0 a vector has no size of its own.
namespace detail {
template <class V, class FV = duckdb::FlatVector>
auto SetVectorSize(V &vector, duckdb::idx_t count, int) -> decltype(FV::SetSize(vector, count)) {
	FV::SetSize(vector, count);
}
template <class V>
void SetVectorSize(V &, duckdb::idx_t, long) {
}
} // namespace detail

//! Declare `count` rows of a flat vector filled in place (no-op before 2.0).
inline void SetVectorSize(duckdb::Vector &vector, duckdb::idx_t count) {
	detail::SetVectorSize(vector, count, 0);
}

// --- Expression result type ---------------------------------------------------------------
// 2.0 makes Expression::return_type protected behind GetReturnType(); before
// 2.0 it is a public member (1.5 has no getter).
namespace detail {
template <class E>
auto ExpressionType(const E &expr, int) -> decltype(expr.GetReturnType()) {
	return expr.GetReturnType();
}
template <class E>
auto ExpressionType(const E &expr, long) -> decltype((expr.return_type)) {
	return expr.return_type;
}
} // namespace detail

//! The result type of a bound expression.
template <class E>
auto ReturnType(const E &expr) -> decltype(detail::ExpressionType(expr, 0)) {
	return detail::ExpressionType(expr, 0);
}

} // namespace duckdb_compat
