// duckdb_compat/list_vector.hpp -- make duckdb::ListVector available on every line.
//
// DuckDB 2.0.0 moved ListVector into its own header (duckdb/common/vector/list_vector.hpp),
// which duckdb.hpp and the vector header no longer pull in; before 2.0 it is declared in
// the vector header itself. generic_executor.hpp exists on 1.5.6 and 2.0.0 and
// includes whatever declares ListVector on that line, so including it is shape-neutral.
#pragma once

#include "duckdb.hpp"
#include "duckdb/common/vector_operations/generic_executor.hpp"
