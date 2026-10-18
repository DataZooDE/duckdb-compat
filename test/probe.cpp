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
#include "duckdb/catalog/catalog_entry/table_catalog_entry.hpp"
#include "duckdb/parser/parsed_data/create_schema_info.hpp"
#include "duckdb/parser/parsed_data/create_sequence_info.hpp"
#include "duckdb/parser/parsed_data/create_table_info.hpp"
#include "duckdb/parser/parsed_data/create_view_info.hpp"
#include "duckdb/parser/parsed_data/drop_info.hpp"
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

void ProbeChunk(DataChunk &chunk) {
	duckdb_compat::SetChildCardinality(chunk, 0);
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

void ProbeCatalogNames(ClientContext &ctx, Catalog &catalog, const ColumnList &columns) {
	const std::string schema = "main", table = "runtime_table";
	// runtime strings into catalog APIs, Identifiers back into strings
	optional_ptr<Catalog> attached = Catalog::GetCatalogEntry(ctx, duckdb_compat::Ident("memory"));
	(void)attached;
	optional_ptr<SchemaCatalogEntry> schema_entry =
	    catalog.GetSchema(ctx, duckdb_compat::Ident(schema), OnEntryNotFound::RETURN_NULL);
	(void)schema_entry;
	std::string catalog_name = duckdb_compat::Str(catalog.GetName());
	(void)catalog_name;
	bool exists = columns.ColumnExists(duckdb_compat::Ident("a"));
	(void)exists;
	ColumnDefinition column(duckdb_compat::Ident(table), LogicalType::BIGINT);
	std::vector<std::string> names = duckdb_compat::Strs(columns.GetColumnNames());
	(void)names;

	// CreateInfo / DropInfo qualification
	CreateTableInfo table_info;
	duckdb_compat::SetQualifiedName(table_info, "memory", schema, table);
	table_info.constraints.push_back(duckdb_compat::MakeUniqueConstraint({"a", "b"}, true));
	CreateSequenceInfo sequence_info;
	duckdb_compat::SetQualifiedName(sequence_info, "memory", schema, "seq");
	CreateViewInfo view_info;
	duckdb_compat::SetCatalog(view_info, "memory");
	duckdb_compat::SetSchema(view_info, schema);
	CreateSchemaInfo schema_info;
	duckdb_compat::SetCatalog(schema_info, "memory");
	duckdb_compat::SetSchema(schema_info, schema);
	DropInfo drop;
	duckdb_compat::SetQualifiedName(drop, "memory", schema, table);
	duckdb_compat::SetSchema(drop, schema);
	duckdb_compat::SetCatalog(drop, "memory");

	// lookups
	optional_ptr<TableCatalogEntry> entry = duckdb_compat::GetEntry<TableCatalogEntry>(catalog, ctx, schema, table);
	(void)entry;
	EntryLookupInfo lookup(CatalogType::TABLE_ENTRY, duckdb_compat::EntryName(table));
	optional_ptr<CatalogEntry> found =
	    duckdb_compat::GetEntryInSchema(catalog, ctx, schema, lookup, OnEntryNotFound::RETURN_NULL);
	(void)found;

	auto statements = duckdb_compat::ParseStatements(ctx, "SELECT 42");
	(void)statements;
}

void ProbeVectorsAndExpressions(BoundReferenceExpression &ref, BoundConjunctionExpression &conjunction,
                                const BoundConjunctionExpression &const_conjunction,
                                const BoundConstantExpression &constant) {
	Vector vec(LogicalType::BIGINT);
	duckdb_compat::ReferenceValue(vec, Value::BIGINT(1), 10);
	Vector constant_vector = duckdb_compat::MakeValueVector(Value::BIGINT(1), 1);
	(void)constant_vector;
	Vector flat(LogicalType::BIGINT);
	int64_t *data = duckdb_compat::FlatData<int64_t>(flat);
	data[0] = 1;
	duckdb_compat::SetVectorSize(flat, 1);

	idx_t column = duckdb_compat::ReferenceIndex(ref);
	(void)column;
	duckdb_compat::Children(conjunction).push_back(nullptr);
	idx_t child_count = duckdb_compat::Children(const_conjunction).size();
	(void)child_count;
	const Value &value = duckdb_compat::ConstantValue(constant);
	(void)value;
}

} // namespace duckdb
