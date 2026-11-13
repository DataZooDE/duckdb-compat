// Compile probe for duckdb_compat/create_trigger.hpp. Compile it with
// -DDUCKDB_COMPAT_CREATE_TRIGGER exactly when the DuckDB tree has
// src/include/duckdb/execution/operator/schema/physical_create_trigger.hpp
// (2.0), as a consumer's CMake does. On 2.0 it must FAIL without the
// definition (the static_assert that keeps the detection from being lost).
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include "duckdb.hpp"
#include "duckdb_compat.hpp"
#include "duckdb_compat/create_trigger.hpp"

using namespace duckdb;

std::string ProbeCreateTriggerTarget(const PhysicalOperator &op) {
	auto target = duckdb_compat::CreateTriggerTarget(op);
	if (!target) {
		return std::string();
	}
	return (*target)[0] + "." + (*target)[1] + "." + (*target)[2];
}
