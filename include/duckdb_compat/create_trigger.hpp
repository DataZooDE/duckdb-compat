// duckdb_compat/create_trigger.hpp -- CREATE TRIGGER in a physical plan.
//
// DuckDB 2.0 plans CREATE TRIGGER as PhysicalCreateTrigger
// (PhysicalOperatorType::CREATE_TRIGGER); 1.5 has no triggers. The operator's
// header exists only on 2.0, so it is included only when the consumer defines
// DUCKDB_COMPAT_CREATE_TRIGGER, which it sets when the DuckDB source tree it
// builds against has
// src/include/duckdb/execution/operator/schema/physical_create_trigger.hpp
// (not a __has_include probe: a system DuckDB 2.0 would make that true on a
// 1.5 build). Compiling against a DuckDB that has the operator type without the
// definition fails with a static_assert, so the detection cannot be lost
// silently.
#pragma once

#include "duckdb_compat.hpp"
#include "duckdb/execution/physical_operator.hpp"
#ifdef DUCKDB_COMPAT_CREATE_TRIGGER
#include "duckdb/execution/operator/schema/physical_create_trigger.hpp"
#endif
#include <array>
#include <optional>
#include <string>
#include <type_traits>

namespace duckdb_compat {
namespace detail {
// PhysicalOperatorType::CREATE_TRIGGER exists (2.0)
template <class E, class = void>
struct HasCreateTriggerOperator : std::false_type {};
template <class E>
struct HasCreateTriggerOperator<E, std::void_t<decltype(E::CREATE_TRIGGER)>> : std::true_type {};
} // namespace detail

#ifndef DUCKDB_COMPAT_CREATE_TRIGGER
static_assert(!detail::HasCreateTriggerOperator<duckdb::PhysicalOperatorType>::value,
              "this DuckDB plans CREATE TRIGGER (PhysicalOperatorType::CREATE_TRIGGER): define "
              "DUCKDB_COMPAT_CREATE_TRIGGER (see duckdb_compat/create_trigger.hpp)");
#endif

//! For a CREATE TRIGGER operator: {catalog, schema, table} of the table the
//! trigger is on, as bound (the binder creates the trigger in that table's
//! catalog and schema). std::nullopt for every other operator, and always on
//! DuckDB lines without triggers.
inline std::optional<std::array<std::string, 3>> CreateTriggerTarget(const duckdb::PhysicalOperator &op) {
#ifdef DUCKDB_COMPAT_CREATE_TRIGGER
	if (op.type == duckdb::PhysicalOperatorType::CREATE_TRIGGER) {
		auto &info = *op.Cast<duckdb::PhysicalCreateTrigger>().info;
		const auto &name = info.GetQualifiedName();
		std::string table = info.base_table ? Str(info.base_table->Table()) : std::string();
		return std::array<std::string, 3> {QualifiedCatalog(name), QualifiedSchema(name), std::move(table)};
	}
#else
	(void)op;
#endif
	return std::nullopt;
}

} // namespace duckdb_compat
