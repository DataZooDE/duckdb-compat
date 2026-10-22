// duckdb_compat/struct_vector.hpp -- StructVector and its children on every line.
//
// DuckDB 2.0.0 moved StructVector into its own header (duckdb/common/vector/struct_vector.hpp)
// and changed StructVector::GetEntries to hold the children by value (vector<Vector>);
// before 2.0 it is declared in the vector header and holds them as vector<unique_ptr<Vector>>.
// generic_executor.hpp includes whatever declares StructVector on 1.5.6 and 2.0.0.
// StructChild(entry) yields the child Vector from one GetEntries element on every line:
//   auto &child = duckdb_compat::StructChild(StructVector::GetEntries(result)[i]);
#pragma once

#include "duckdb.hpp"
#include "duckdb/common/vector_operations/generic_executor.hpp"

namespace duckdb_compat {

// 2.0.0: the entry is the child Vector itself.
inline duckdb::Vector &StructChild(duckdb::Vector &entry) {
	return entry;
}
inline const duckdb::Vector &StructChild(const duckdb::Vector &entry) {
	return entry;
}

// Before 2.0: the entry is a (possibly const) unique_ptr<Vector>.
template <class POINTER>
auto StructChild(POINTER &entry) -> decltype(*entry) {
	return *entry;
}

} // namespace duckdb_compat
