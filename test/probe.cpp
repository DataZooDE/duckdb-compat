// Compile probe: every DuckDB-facing helper, instantiated against the REAL
// DuckDB headers. CI compiles this (-fsyntax-only) against each supported
// DuckDB line; a helper that breaks on one line fails that line's job.
//
// <windows.h> comes first, as it does in any extension that talks to the OS:
// it defines IN, OUT and OPTIONAL as empty macros (minwindef.h), which once
// broke every Windows build of a template parameter called IN. The Linux job
// simulates the same macros with -DIN= -DOUT= -DOPTIONAL=.
#ifdef _WIN32
#include <windows.h>
#endif
#include "duckdb.hpp"
#include "duckdb/common/file_opener.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/secret/secret.hpp"
#include "duckdb_compat.hpp"

namespace duckdb {

static unique_ptr<FunctionData> ProbeBind(ClientContext &, TableFunctionBindInput &, vector<LogicalType> &types,
                                          duckdb_compat::BindNames &names) {
	types.emplace_back(LogicalType::BIGINT);
	names.emplace_back("n");
	names.emplace_back(std::string("runtime_name")); // emplace_back: Identifier(string) is explicit
	return nullptr;
}

static void ProbeScalar(DataChunk &args, ExpressionState &, Vector &result) {
	duckdb_compat::ExecuteUnaryNullable<string_t, int64_t>(args.data[0], result, args.size(),
	                                                       [](string_t s, bool &is_null) -> int64_t {
		                                                       is_null = s.GetSize() == 0;
		                                                       return static_cast<int64_t>(s.GetSize());
	                                                       });
}

void Probe(optional_ptr<FileOpener> opener, ClientContext &ctx, const BaseSecret &secret) {
	Value v;
	(void)duckdb_compat::TryGetSetting(opener, std::string("threads"), v);
	(void)duckdb_compat::IsInterrupted(ctx);
	const std::string &name = duckdb_compat::Str(secret.GetName());
	(void)name;

	TableFunction tf("probe", {}, nullptr, ProbeBind);
	ScalarFunction sf("probe", {LogicalType::VARCHAR}, LogicalType::BIGINT, ProbeScalar);
	duckdb_compat::SetStability(sf, FunctionStability::VOLATILE);
	duckdb_compat::SetFallible(sf);
	(void)tf;
}

} // namespace duckdb
