// Compile probe: every DuckDB-facing helper, instantiated against the REAL
// DuckDB headers. CI compiles this (-fsyntax-only) against each supported
// DuckDB line; a helper that breaks on one line fails that line's job.
//
// <windows.h> comes first, as it does in any extension that talks to the OS:
// it defines IN, OUT and OPTIONAL as empty macros (minwindef.h), which once
// broke every Windows build of a template parameter called IN. The Linux job
// simulates the same macros with -DIN= -DOUT= -DOPTIONAL=.
#ifdef _WIN32
#define NOMINMAX // as duckdb/common/windows.hpp does; DuckDB headers need it
#include <windows.h>
#endif
#include "duckdb.hpp"
#include "duckdb/common/file_opener.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/secret/secret.hpp"
#include "duckdb/main/connection.hpp"
#include "duckdb/function/copy_function.hpp"
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

static unique_ptr<FunctionData> ProbeCopyBind(ClientContext &, CopyFunctionBindInput &,
                                              const duckdb_compat::CopyBindNames &names,
                                              const vector<LogicalType> &) {
	for (auto &n : names) {
		(void)duckdb_compat::Str(n);
	}
	return nullptr;
}

static void ProbeBindInput(TableFunctionBindInput &input) {
	const Value *framing = duckdb_compat::FindNamedParameter(input.named_parameters, "framing");
	(void)framing;
}

void ProbeQuery(ClientContext &ctx) {
	Connection con(*ctx.db);
	auto res = con.Query("SELECT 1 AS a");
	std::vector<std::string> names = duckdb_compat::ResultNames(*res);
	std::vector<std::vector<Value>> rows = duckdb_compat::FetchRows(*res);
	(void)names;
	(void)rows;
}

void ProbeFunctions() {
	const std::string name = "probe_runtime_name";
	TableFunctionSet set(duckdb_compat::FunctionName(name));
	TableFunction tf(duckdb_compat::FunctionName(name), {LogicalType::VARCHAR}, nullptr, ProbeBind);
	duckdb_compat::AddNamedParameter(tf, "framing", LogicalType::VARCHAR);
	duckdb_compat::NameParameters(tf, {"path"});
	set.AddFunction(tf);
	duckdb_compat::ForEachFunction(set, [](TableFunction &fun) { duckdb_compat::NameParameters(fun, {"path"}); });
	CopyFunction cf("probe");
	cf.copy_to_bind = ProbeCopyBind;
	(void)ProbeBindInput;

	Value out;
	std::string error;
	bool ok = duckdb_compat::TryCastValue(Value("42"), LogicalType::BIGINT, out, &error, true);
	(void)ok;

	unique_ptr<CreateMacroInfo> macro =
	    duckdb_compat::ScalarMacroInfo(DEFAULT_SCHEMA, "probe_macro", "(a, b := '4') AS a || b");
	(void)macro;
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
