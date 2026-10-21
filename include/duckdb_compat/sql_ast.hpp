#pragma once

#include "duckdb_compat.hpp"
#include "duckdb/parser/statement/select_statement.hpp"
#include "duckdb/common/exception.hpp"
// list.hpp exists on every line; on 2.0 it also brings in UpdateQueryNode (UPDATE
// became a query node there), so no consumer-side feature macro is needed.
#include "duckdb/parser/query_node/list.hpp"
#include "duckdb/parser/result_modifier.hpp"
#ifdef DUCKDB_COMPAT_DML_QUERY_NODES
#include "duckdb/parser/query_node/update_query_node.hpp"
#include "duckdb/parser/query_node/delete_query_node.hpp"
#include "duckdb/parser/query_node/insert_query_node.hpp"
#include "duckdb/parser/query_node/merge_query_node.hpp"
#include "duckdb/parser/query_node/copy_query_node.hpp"
#endif
#include <array>
#include <type_traits>
#include <utility>

namespace duckdb {
// Both names, so the shape-based selection below compiles on every line; only the
// one this DuckDB defines is ever instantiated.
class LimitPercentModifier;
class LegacyLimitPercentModifier;
} // namespace duckdb

namespace duckdb_compat {

// Parse without a connection. 2.0 exposes the built-in parser factory;
// earlier releases have a default constructor.
namespace detail {
template <class P = duckdb::Parser>
auto ParseBuiltin(const std::string &sql, int) -> decltype(P::GetBuiltinParser().statements) {
	auto parser = P::GetBuiltinParser();
	parser.ParseQuery(sql);
	return std::move(parser.statements);
}
template <class P = duckdb::Parser>
auto ParseBuiltin(const std::string &sql, long) -> decltype(P().statements) {
	P parser;
	parser.ParseQuery(sql);
	return std::move(parser.statements);
}

template <class P = duckdb::Parser>
auto ParseExpressionListBuiltin(const std::string &sql, int) -> decltype(P::GetBuiltinParser().ParseExpressionList(sql)) {
	auto parser = P::GetBuiltinParser();
	return parser.ParseExpressionList(sql);
}
template <class P = duckdb::Parser>
auto ParseExpressionListBuiltin(const std::string &sql, long) -> decltype(P::ParseExpressionList(sql)) {
	return P::ParseExpressionList(sql);
}

template <class T> auto AstName(const T &obj, int) -> decltype(obj.GetQualifiedName()) {
	return obj.GetQualifiedName();
}
template <class T> auto AstName(const T &obj, long) -> decltype((obj.qualified_name)) {
	return obj.qualified_name;
}
template <class T> auto AstTarget(const T &obj, int) -> decltype(AstName(obj, 0), std::array<std::string, 3>()) {
	const auto &name = AstName(obj, 0);
	return {::duckdb_compat::QualifiedCatalog(name), ::duckdb_compat::QualifiedSchema(name), ::duckdb_compat::QualifiedLocalName(name)};
}
template <class T> auto AstTarget(const T &obj, long) -> decltype(obj.catalog, obj.schema, obj.table, std::array<std::string, 3>()) {
	return {::duckdb_compat::Str(obj.catalog), ::duckdb_compat::Str(obj.schema), ::duckdb_compat::Str(obj.table)};
}
template <class T> auto AstTarget(const T &obj, ...) -> decltype(obj.catalog_name, obj.schema_name, obj.table_name, std::array<std::string, 3>()) {
	return {::duckdb_compat::Str(obj.catalog_name), ::duckdb_compat::Str(obj.schema_name), ::duckdb_compat::Str(obj.table_name)};
}
template <class T> auto DmlNode(const T &stmt, int) -> decltype(((*stmt.node))) {
	if (!stmt.node) throw duckdb::InternalException("null DML query node");
	return *stmt.node;
}
template <class T> const T &DmlNode(const T &stmt, long) { return stmt; }
template <class T> auto MergeNode(const T &stmt, int) -> decltype(((*stmt.node))) {
	if (!stmt.node) throw duckdb::InternalException("null MERGE query node");
	return *stmt.node;
}
template <class T> const T &MergeNode(const T &stmt, long) { return stmt; }
template <class T> auto CteQuery(const T &info, int) -> decltype(info.query_node.get()) { return info.query_node.get(); }
template <class T> auto CteQuery(const T &info, long) -> decltype(info.query->node.get()) {
	return info.query ? info.query->node.get() : nullptr;
}
template <class T> auto Subquery(const T &expr, int) -> decltype((expr.Subquery())) { return expr.Subquery(); }
template <class T> auto Subquery(const T &expr, long) -> decltype((expr.subquery)) { return expr.subquery; }
template <class T> auto ColumnNames(const T &expr, int) -> decltype((expr.ColumnNames())) { return expr.ColumnNames(); }
template <class T> auto ColumnNames(const T &expr, long) -> decltype((expr.column_names)) { return expr.column_names; }
template <class T> auto FunctionName(const T &expr, int) -> decltype(::duckdb_compat::Str(expr.FunctionName())) {
	return ::duckdb_compat::Str(expr.FunctionName());
}
template <class T> auto FunctionName(const T &expr, long) -> decltype(::duckdb_compat::Str(expr.function_name)) {
	return ::duckdb_compat::Str(expr.function_name);
}
template <class T> auto CreateLocalName(const T &info, int) -> decltype(::duckdb_compat::Str(info.table)) {
	return ::duckdb_compat::Str(info.table);
}
template <class T> auto CreateLocalName(const T &info, long) -> decltype(::duckdb_compat::Str(info.view_name)) {
	return ::duckdb_compat::Str(info.view_name);
}
template <class T> auto CreateLocalName(const T &info, ...) -> decltype(::duckdb_compat::Str(info.name)) {
	return ::duckdb_compat::Str(info.name);
}
template <class T> auto CreateTarget(const T &info, int) -> decltype(AstTarget(info, 0)) {
	return AstTarget(info, 0);
}
template <class T> auto CreateTarget(const T &info, long) -> decltype(info.catalog, info.schema, std::array<std::string, 3>()) {
	return {::duckdb_compat::Str(info.catalog), ::duckdb_compat::Str(info.schema), CreateLocalName(info, 0)};
}
template <class T> auto SchemaTarget(const T &info, int) -> decltype(info.GetQualifiedName(), std::array<std::string, 2>()) {
	const auto &name = info.GetQualifiedName();
	return {::duckdb_compat::QualifiedCatalog(name), ::duckdb_compat::QualifiedSchema(name)};
}
template <class T> auto SchemaTarget(const T &info, long) -> decltype(info.catalog, info.schema, std::array<std::string, 2>()) {
	return {::duckdb_compat::Str(info.catalog), ::duckdb_compat::Str(info.schema)};
}
template <class T, class F> auto ForEachSetChild(const T &node, F &&fn, int) -> decltype(node.children, void()) {
	for (const auto &child : node.children) if (child) fn(*child);
}
template <class T, class F> auto ForEachSetChild(const T &node, F &&fn, long) -> decltype(node.left, node.right, void()) {
	if (node.left) fn(*node.left);
	if (node.right) fn(*node.right);
}
template <class T, class F> auto VisitCreateTableExtras(const T &table, F &&fn, int)
    -> decltype(table.partition_keys, table.sort_keys, table.options, void()) {
	fn(table.partition_keys, table.sort_keys, table.options);
}
template <class T, class = void> struct HasPartitionKeys : std::false_type {};
template <class T> struct HasPartitionKeys<T, std::void_t<decltype(std::declval<const T &>().partition_keys)>> : std::true_type {};
template <class T, class = void> struct HasSortKeys : std::false_type {};
template <class T> struct HasSortKeys<T, std::void_t<decltype(std::declval<const T &>().sort_keys)>> : std::true_type {};
template <class T, class = void> struct HasTableOptions : std::false_type {};
template <class T> struct HasTableOptions<T, std::void_t<decltype(std::declval<const T &>().options)>> : std::true_type {};
template <class T, class F> void VisitCreateTableExtras(const T &, F &&, long) {
	static_assert(!HasPartitionKeys<T>::value && !HasSortKeys<T>::value && !HasTableOptions<T>::value,
	              "CREATE TABLE extras must be visited together; partial AST is unsafe");
}
template <class T, class F> auto VisitCreateSchemaOptions(const T &schema, F &&fn, int)
    -> decltype(schema.options, void()) { fn(schema.options); }
template <class T, class = void> struct HasSchemaOptions : std::false_type {};
template <class T> struct HasSchemaOptions<T, std::void_t<decltype(std::declval<const T &>().options)>> : std::true_type {};
template <class T, class F> void VisitCreateSchemaOptions(const T &, F &&, long) {
	static_assert(!HasSchemaOptions<T>::value, "CREATE SCHEMA options must be visited");
}
} // namespace detail

inline auto ParseBuiltin(const std::string &sql) { return detail::ParseBuiltin(sql, 0); }
inline auto ParseExpressionListBuiltin(const std::string &sql) { return detail::ParseExpressionListBuiltin(sql, 0); }
template <class T> auto AstTarget(const T &obj) { return detail::AstTarget(obj, 0); }
template <class T> decltype(auto) DmlNode(const T &stmt) { return detail::DmlNode(stmt, 0); }
template <class T> decltype(auto) MergeNode(const T &stmt) { return detail::MergeNode(stmt, 0); }
template <class T> auto CteQuery(const T &info) { return detail::CteQuery(info, 0); }
template <class T> decltype(auto) Subquery(const T &expr) { return detail::Subquery(expr, 0); }
template <class T> decltype(auto) ColumnNames(const T &expr) { return detail::ColumnNames(expr, 0); }
template <class T> std::string FunctionName(const T &expr) { return detail::FunctionName(expr, 0); }
template <class T> auto CreateTarget(const T &info) { return detail::CreateTarget(info, 0); }
template <class T> auto SchemaTarget(const T &info) { return detail::SchemaTarget(info, 0); }
template <class T, class F> void ForEachSetChild(const T &node, F &&fn) { detail::ForEachSetChild(node, std::forward<F>(fn), 0); }
template <class T, class F> void VisitCreateTableExtras(const T &table, F &&fn) {
	detail::VisitCreateTableExtras(table, std::forward<F>(fn), 0);
}
template <class T, class F> void VisitCreateSchemaOptions(const T &schema, F &&fn) {
	detail::VisitCreateSchemaOptions(schema, std::forward<F>(fn), 0);
}

// Only 2.0 has DML QueryNodes. Keep those enum names and casts here so the
// consumer has one source tree without version conditionals.
template <class U, class D, class I, class M, class C>
bool VisitDmlQuery(const duckdb::QueryNode &node, U update, D del, I insert, M merge, C copy) {
#ifdef DUCKDB_COMPAT_DML_QUERY_NODES
	switch (node.type) {
	case duckdb::QueryNodeType::UPDATE_QUERY_NODE: update(node.Cast<duckdb::UpdateQueryNode>()); return true;
	case duckdb::QueryNodeType::DELETE_QUERY_NODE: del(node.Cast<duckdb::DeleteQueryNode>()); return true;
	case duckdb::QueryNodeType::INSERT_QUERY_NODE: insert(node.Cast<duckdb::InsertQueryNode>()); return true;
	case duckdb::QueryNodeType::MERGE_QUERY_NODE: merge(node.Cast<duckdb::MergeQueryNode>()); return true;
	case duckdb::QueryNodeType::COPY_QUERY_NODE: copy(node.Cast<duckdb::CopyQueryNode>()); return true;
	default: break;
	}
#else
	(void)node; (void)update; (void)del; (void)insert; (void)merge; (void)copy;
#endif
	return false;
}

namespace detail {
// 2.0 renamed LIMIT_PERCENT_MODIFIER / LimitPercentModifier to the LEGACY_ names.
// Detected from the enum's shape, so no consumer macro is needed.
template <class E, class = void>
struct HasLegacyLimitPercent : std::false_type {};
template <class E>
struct HasLegacyLimitPercent<E, std::void_t<decltype(E::LEGACY_LIMIT_PERCENT_MODIFIER)>> : std::true_type {};
} // namespace detail

//! Call fn with the percent-limit modifier as its concrete class on every line.
template <class F, class M = duckdb::ResultModifier>
void VisitLegacyLimitPercent(const M &modifier, F &&fn) {
	using T = typename std::conditional<detail::HasLegacyLimitPercent<duckdb::ResultModifierType>::value,
	                                    duckdb::LegacyLimitPercentModifier, duckdb::LimitPercentModifier>::type;
	fn(modifier.template Cast<T>());
}

} // namespace duckdb_compat
