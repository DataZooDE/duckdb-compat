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
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include "duckdb/parser/parsed_data/create_view_info.hpp"
#include "duckdb/parser/parsed_data/drop_info.hpp"
#include "duckdb/parser/parsed_data/alter_info.hpp"
#include "duckdb/storage/storage_extension.hpp"
#include "duckdb/main/config.hpp"
#include "duckdb_compat.hpp"
#include "duckdb_compat/filter.hpp"
#include "duckdb_compat/sql_ast.hpp"
#include "duckdb_compat/aggregate.hpp"
#include "duckdb/parser/tableref/basetableref.hpp"
#include "duckdb/parser/tableref/showref.hpp"
#include "duckdb/parser/statement/insert_statement.hpp"
#include "duckdb/parser/statement/update_statement.hpp"
#include "duckdb/parser/statement/delete_statement.hpp"
#include "duckdb/parser/statement/merge_into_statement.hpp"
#include "duckdb/parser/parsed_data/copy_info.hpp"
#include "duckdb/parser/expression/columnref_expression.hpp"
#include "duckdb/parser/expression/subquery_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/query_node/set_operation_node.hpp"
#include "duckdb/planner/expression/bound_operator_expression.hpp"
#include "duckdb/planner/expression/bound_reference_expression.hpp"
#include <type_traits>

static_assert(!duckdb_compat::FilterKind::BLOOM_FILTER ||
                  static_cast<int>(*duckdb_compat::FilterKind::BLOOM_FILTER) == 10,
              "Bloom filter must retain its type value");
static_assert(static_cast<int>(duckdb_compat::FilterKind::STRUCT_EXTRACT) == 5,
              "Struct filter kind must retain its type value");
static void ProbeMutableExpressionChildren(duckdb::BoundOperatorExpression &op) {
	(void)duckdb_compat::MutableExpressionChildren(op);
}
static void ProbeBoundScalarExpression() {
	auto varchar_type = duckdb::LogicalType::VARCHAR;
	auto boolean_type = duckdb::LogicalType::BOOLEAN;
	duckdb::ScalarFunction function(duckdb_compat::Ident("probe_function"), {varchar_type}, boolean_type, nullptr);
	duckdb::vector<duckdb::unique_ptr<duckdb::Expression>> arguments;
	arguments.push_back(duckdb::make_uniq<duckdb::BoundReferenceExpression>(varchar_type, 0));
	(void)duckdb_compat::MakeBoundScalarExpression(boolean_type, std::move(function), std::move(arguments));
}
static void ProbeConstantFilter(const duckdb_compat::ConstantFilter &filter, const duckdb::Value &value) {
	(void)duckdb_compat::MatchesConstantFilter(filter, value);
}
static void ProbeCopyFilter(const duckdb::TableFilter &filter) {
	(void)duckdb_compat::CopyFilter(filter);
}
static void ProbeComparisonExpression() {
	(void)duckdb_compat::MakeComparisonExpression(
	    duckdb::ExpressionType::COMPARE_EQUAL,
	    duckdb::make_uniq<duckdb::BoundReferenceExpression>(duckdb::LogicalType::VARCHAR, 0),
	    duckdb::make_uniq<duckdb::BoundConstantExpression>(duckdb::Value("x")));
}
static void ProbeStorageRegistration(duckdb::DBConfig &config, duckdb::shared_ptr<duckdb::StorageExtension> extension) {
	duckdb_compat::RegisterStorageExtension(config, "probe", std::move(extension));
}
static void ProbeNamedParameterTypes(const duckdb::TableFunction &function) {
	(void)duckdb_compat::NamedParameterTypes(function);
}
static void ProbeFunctionArgumentTypes(const duckdb::TableFunction &function) {
	(void)duckdb_compat::FunctionArgumentTypes(function);
}
static void ProbeTypeInfoIdentity(const duckdb::LogicalType &type) {
	(void)duckdb_compat::TypeInfoIdentity(type);
}
using ProbeDefaultSchemaName = duckdb_compat::DefaultSchemaName;
static void ProbeQualifiedName(const duckdb::QualifiedName &name) {
	(void)duckdb_compat::QualifiedCatalog(name);
	(void)duckdb_compat::QualifiedSchema(name);
	(void)duckdb_compat::QualifiedLocalName(name);
	(void)duckdb_compat::QualifiedWithSchema(name, "main");
}
static_assert(!std::is_convertible_v<duckdb_compat::detail::RowCount, std::string>);
static_assert(!std::is_convertible_v<duckdb_compat::detail::RowCount, void *>);
static void ProbeMutableValidity(duckdb::Vector &v) {
	duckdb_compat::MutableValidity(v).SetInvalid(0);
}
static void ProbeAlias() {
	auto type = duckdb_compat::WithAlias(duckdb::LogicalType::VARCHAR, "probe");
	(void)type;
}
static void ProbeAssignBindNames(duckdb_compat::BindNames &names) {
	duckdb_compat::AssignBindNames(names, std::vector<std::string> {"a", "b"});
}
using ProbeBindNamedParameters = duckdb_compat::BindNamedParameters;
using ProbeCopyOptions = duckdb_compat::CopyOptions;
using ProbeGetBindInfoInput = duckdb_compat::GetBindInfoInput;
static duckdb::BindInfo ProbeBindInfo(ProbeGetBindInfoInput input, duckdb::TableCatalogEntry &table) {
	auto data = duckdb_compat::GetBindData(input);
	(void)data;
	return duckdb_compat::MakeTableBindInfo(table);
}
static void ProbeParsedConstant(const duckdb::Value &value) {
	auto expression = duckdb_compat::ParsedConstant(value);
	(void)expression;
}
static void ProbeSchemaOptions(const duckdb::CreateSchemaInfo &schema) {
	(void)duckdb_compat::SchemaTarget(schema);
	duckdb_compat::VisitCreateSchemaOptions(schema, [](const auto &options) { (void)options; });
}

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
                                              const duckdb_compat::CopyBindNames &names, const vector<LogicalType> &) {
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

void ProbeUnifiedFormat(Vector &vector, UnifiedVectorFormat &format) {
	duckdb_compat::ToUnifiedFormat(vector, 1, format);
}

void ProbeQuery(ClientContext &ctx) {
	Connection con(*ctx.db);
	auto res = con.Query("SELECT 1 AS a");
	std::vector<std::string> names = duckdb_compat::ResultNames(*res);
	const auto &types = duckdb_compat::ResultTypes(*res);
	Value first = duckdb_compat::ResultValue(*res, 0, 0);
	std::vector<std::vector<Value>> rows = duckdb_compat::FetchRows(*res);
	(void)names;
	(void)types;
	(void)first;
	(void)rows;
}

void ProbeFunctions() {
	const std::string name = "probe_runtime_name";
	TableFunctionSet set(duckdb_compat::FunctionName(name));
	TableFunction tf(duckdb_compat::FunctionName(name), {LogicalType::VARCHAR}, nullptr, ProbeBind);
	duckdb_compat::SetTableEntryGetter(
	    tf, +[](duckdb::optional_ptr<const duckdb::FunctionData>) -> duckdb::optional_ptr<duckdb::TableCatalogEntry> {
		    return nullptr;
	    });
	duckdb_compat::AddNamedParameter(tf, "framing", LogicalType::VARCHAR);
	duckdb_compat::SetVarArgs(tf, LogicalType::VARCHAR);
	duckdb_compat::NameParameters(tf, {"path"});
	set.AddFunction(tf);
	duckdb_compat::ForEachFunction(set, [](TableFunction &fun) { duckdb_compat::NameParameters(fun, {"path"}); });
	CreateTableFunctionInfo described(tf);
	FunctionDescription description;
	description.parameter_names = {"path"};
	described.descriptions.push_back(std::move(description));
	duckdb_compat::NameFunctionsFromDescriptions(described);
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
	duckdb_compat::SetNullHandling(sf, FunctionNullHandling::SPECIAL_HANDLING);
	duckdb_compat::NameArguments(sf, {"text"});
	ScalarFunctionSet sset("probe");
	sset.AddFunction(sf);
	CreateScalarFunctionInfo sinfo(sset);
	duckdb_compat::NameArgumentsFromDescriptions(sinfo);
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
	Value setting;
	(void)duckdb_compat::TryGetContextSetting(ctx, schema, setting);
}

void ProbeSqlAst(const BaseTableRef &base, const ShowRef &show, const DropInfo &drop, const AlterInfo &alter,
                 const InsertStatement &insert, const UpdateStatement &update,
                 const DeleteStatement &deletion, const MergeIntoStatement &merge, const CopyInfo &copy, const CommonTableExpressionInfo &cte,
                 const SubqueryExpression &subquery, const ColumnRefExpression &column,
                 const FunctionExpression &function, const SetOperationNode &set,
                 const CreateTableInfo &table_info, const CreateViewInfo &view_info,
                 const ResultModifier &modifier) {
	(void)duckdb_compat::ParseBuiltin("SELECT 1");
	(void)duckdb_compat::AstTarget(base);
	(void)duckdb_compat::AstTarget(show);
	(void)duckdb_compat::CreateTarget(drop);
	(void)duckdb_compat::CreateTarget(alter);
	(void)duckdb_compat::AstTarget(duckdb_compat::DmlNode(insert));
	(void)duckdb_compat::DmlNode(update).table;
	(void)duckdb_compat::DmlNode(deletion).table;
	(void)duckdb_compat::MergeNode(merge).target;
	(void)duckdb_compat::AstTarget(copy);
	(void)duckdb_compat::CteQuery(cte);
	(void)duckdb_compat::Subquery(subquery);
	(void)duckdb_compat::ColumnNames(column);
	(void)duckdb_compat::FunctionName(function);
	duckdb_compat::ForEachSetChild(set, [](const QueryNode &) {});
	(void)duckdb_compat::CreateTarget(table_info);
	duckdb_compat::VisitCreateTableExtras(table_info, [](const auto &partitions, const auto &sorts, const auto &options) {
		(void)partitions; (void)sorts; (void)options;
	});
	(void)duckdb_compat::CreateTarget(view_info);
	duckdb_compat::VisitLegacyLimitPercent(modifier, [](const auto &limit) { (void)limit.offset; });
	duckdb_compat::VisitDmlQuery(set, [](const auto &) {}, [](const auto &) {}, [](const auto &) {},
	                             [](const auto &) {}, [](const auto &) {});
}

// Fake shapes exercise both dispatch arms even when compiling against just one
// real DuckDB line. These are syntax probes, so the functions need no runtime.
namespace sql_ast_fake {
struct OldTarget { string catalog, schema, table; };
struct OldViewTarget { string catalog, schema, view_name; };
struct OldDml { int table; };
struct NewDml { unique_ptr<OldDml> node; };
struct FakeSelect { unique_ptr<int> node; };
struct OldCte { unique_ptr<FakeSelect> query; };
struct NewCte { unique_ptr<int> query_node; };
struct OldSubquery { unique_ptr<int> subquery; };
struct NewSubquery {
	unique_ptr<int> subquery;
	const unique_ptr<int> &Subquery() const { return subquery; }
};
struct OldColumn { vector<string> column_names; };
struct NewColumn {
	vector<string> names;
	const vector<string> &ColumnNames() const { return names; }
};
struct OldFunction { string function_name; };
struct NewFunction {
	string name;
	const string &FunctionName() const { return name; }
};
struct OldSet { unique_ptr<int> left, right; };
struct NewSet { vector<unique_ptr<int>> children; };

void Probe(const OldTarget &target, const OldDml &old_dml, const NewDml &new_dml,
           const OldCte &old_cte, const NewCte &new_cte, const OldSubquery &old_subquery,
           const NewSubquery &new_subquery, const OldColumn &old_column, const NewColumn &new_column,
           const OldFunction &old_function, const NewFunction &new_function,
           const OldSet &old_set, const NewSet &new_set) {
	(void)duckdb_compat::AstTarget(target);
	(void)duckdb_compat::CreateTarget(target);
	(void)duckdb_compat::CreateTarget(OldViewTarget {});
	(void)duckdb_compat::DmlNode(old_dml).table;
	(void)duckdb_compat::DmlNode(new_dml).table;
	(void)duckdb_compat::CteQuery(old_cte);
	(void)duckdb_compat::CteQuery(new_cte);
	(void)duckdb_compat::Subquery(old_subquery);
	(void)duckdb_compat::Subquery(new_subquery);
	(void)duckdb_compat::ColumnNames(old_column);
	(void)duckdb_compat::ColumnNames(new_column);
	(void)duckdb_compat::FunctionName(old_function);
	(void)duckdb_compat::FunctionName(new_function);
	duckdb_compat::ForEachSetChild(old_set, [](const int &) {});
	duckdb_compat::ForEachSetChild(new_set, [](const int &) {});
}
} // namespace sql_ast_fake

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

namespace aggregate_probe {
struct ProbeState {
	double sum;
};
void ProbeInit(data_ptr_t state) {
	new (state) ProbeState {0.0};
}
void ProbeUpdate(Vector[], AggregateInputData &, idx_t, Vector &, idx_t) {
}
void ProbeCombine(Vector &, Vector &, AggregateInputData &, idx_t) {
}
void ProbeFinalize(Vector &, AggregateInputData &, Vector &, idx_t, idx_t) {
}
unique_ptr<FunctionData> ProbeBind(ClientContext &, duckdb_compat::AggregateBindFunction &function,
                                   vector<unique_ptr<Expression>> &arguments) {
	duckdb_compat::SetReturnType(function, duckdb_compat::ReturnType(*arguments[0]));
	return nullptr;
}
} // namespace aggregate_probe

void ProbeAggregateCallbacks() {
	using namespace aggregate_probe;
	AggregateFunction fn(duckdb_compat::FunctionName(std::string("probe")), {LogicalType::DOUBLE}, LogicalType::DOUBLE,
	                     duckdb_compat::AggregateStateSize<sizeof(ProbeState)>(),
	                     duckdb_compat::AggregateInit<ProbeInit>(), ProbeUpdate, ProbeCombine,
	                     duckdb_compat::AggregateFinalize<ProbeFinalize>(), nullptr,
	                     duckdb_compat::AggregateBind<ProbeBind>(), nullptr);
	(void)fn;
	auto expressions = duckdb_compat::ParseExpressionListBuiltin("1 + 1");
	(void)expressions;
}

} // namespace duckdb

// DBConfig::ParseMemoryLimit returns idx_t (max = unlimited) before 2.0 and
// optional_idx (empty = unlimited) in 2.0.
void ProbeMemoryLimit() {
	idx_t bytes = duckdb_compat::MemoryLimitBytes("4GB");
	(void)bytes;
}
