#pragma once

#include "duckdb_compat.hpp"
#include "duckdb/parser/statement/select_statement.hpp"
// list.hpp exists on every line; on 2.0 it also brings in UpdateQueryNode (UPDATE
// became a query node there), so no consumer-side feature macro is needed.
#include "duckdb/parser/query_node/list.hpp"
#include <array>
#include <utility>

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
template <class T> auto DmlNode(const T &stmt, int) -> decltype(((*stmt.node))) { return *stmt.node; }
template <class T> const T &DmlNode(const T &stmt, long) { return stmt; }
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
template <class T, class F> auto ForEachSetChild(const T &node, F &&fn, int) -> decltype(node.children, void()) {
	for (const auto &child : node.children) if (child) fn(*child);
}
template <class T, class F> auto ForEachSetChild(const T &node, F &&fn, long) -> decltype(node.left, node.right, void()) {
	if (node.left) fn(*node.left);
	if (node.right) fn(*node.right);
}
} // namespace detail

inline auto ParseBuiltin(const std::string &sql) { return detail::ParseBuiltin(sql, 0); }
template <class T> auto AstTarget(const T &obj) { return detail::AstTarget(obj, 0); }
template <class T> decltype(auto) DmlNode(const T &stmt) { return detail::DmlNode(stmt, 0); }
template <class T> auto CteQuery(const T &info) { return detail::CteQuery(info, 0); }
template <class T> decltype(auto) Subquery(const T &expr) { return detail::Subquery(expr, 0); }
template <class T> decltype(auto) ColumnNames(const T &expr) { return detail::ColumnNames(expr, 0); }
template <class T> std::string FunctionName(const T &expr) { return detail::FunctionName(expr, 0); }
template <class T, class F> void ForEachSetChild(const T &node, F &&fn) { detail::ForEachSetChild(node, std::forward<F>(fn), 0); }

} // namespace duckdb_compat
