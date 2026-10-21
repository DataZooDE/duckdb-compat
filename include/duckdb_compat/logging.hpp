#pragma once

#include "duckdb/logging/logger.hpp"

// DuckDB renamed this logging macro. Keep the alias in the compatibility
// layer so callers use one spelling on all supported lines.
#if !defined(DUCKDB_LOG_WARNING) && defined(DUCKDB_LOG_WARN)
#define DUCKDB_LOG_WARNING DUCKDB_LOG_WARN
#endif
