#include "duckdb_compat/filter.hpp"
#include "duckdb/planner/expression/bound_reference_expression.hpp"
#include "duckdb/planner/expression/bound_constant_expression.hpp"
#include "duckdb/planner/expression/bound_operator_expression.hpp"

static_assert(static_cast<int>(duckdb_compat::FilterKind::CONSTANT_COMPARISON) == 0, "Unexpected table filter kind");
static_assert(sizeof(duckdb_compat::ConstantFilter) > 0);
static_assert(sizeof(duckdb_compat::InFilter) > 0);
static_assert(sizeof(duckdb_compat::ConjunctionAndFilter) > 0);
static_assert(sizeof(duckdb_compat::ConjunctionOrFilter) > 0);
static_assert(sizeof(duckdb_compat::OptionalFilter) > 0);
static_assert(sizeof(duckdb_compat::DynamicFilter) > 0);
static_assert(sizeof(duckdb_compat::IsNullFilter) > 0);
static_assert(sizeof(duckdb_compat::IsNotNullFilter) > 0);
static_assert(sizeof(duckdb_compat::StructFilter) > 0);

void ProbeFilters(const duckdb::TableFilterSet &set) {
	(void)duckdb_compat::FilterCount(set);
	duckdb_compat::ForEachFilter(set, [](duckdb::idx_t, const duckdb::TableFilter &filter) {
		(void)duckdb_compat::FilterToString(filter, "probe");
		(void)duckdb_compat::IsOptionalExpressionFilter(filter);
		(void)duckdb_compat::SimpleExpressionTree(filter);
	});
}

void ProbeFilterConstruction(duckdb::TableFilterSet &set) {
	auto boolean_type = duckdb::LogicalType::BOOLEAN;
	duckdb::BoundOperatorExpression op(duckdb::ExpressionType::OPERATOR_NOT, boolean_type);
	duckdb_compat::MutableExpressionChildren(op).push_back(
	    duckdb::make_uniq<duckdb::BoundConstantExpression>(duckdb::Value(true)));
	auto varchar_type = duckdb::LogicalType::VARCHAR;
	auto boolean_return = duckdb::LogicalType::BOOLEAN;
	duckdb::ScalarFunction named_fun(duckdb_compat::Ident("probe_named_function"), {varchar_type},
	                                 boolean_return, nullptr);
	duckdb::vector<duckdb::unique_ptr<duckdb::Expression>> arguments;
	arguments.push_back(duckdb::make_uniq<duckdb::BoundReferenceExpression>(varchar_type, 0));
	(void)duckdb_compat::MakeBoundScalarExpression(boolean_return, std::move(named_fun), std::move(arguments));
	(void)duckdb_compat::MakeComparisonExpression(
	    duckdb::ExpressionType::COMPARE_EQUAL,
	    duckdb::make_uniq<duckdb::BoundReferenceExpression>(duckdb::LogicalType::VARCHAR, 0),
	    duckdb::make_uniq<duckdb::BoundConstantExpression>(duckdb::Value("x")));
	duckdb_compat::SetFilter(set, 0,
	                         duckdb::make_uniq<duckdb_compat::ConstantFilter>(duckdb::ExpressionType::COMPARE_EQUAL,
	                                                                          duckdb::Value::INTEGER(1)));
	(void)duckdb_compat::MakeDynamicFilterData(duckdb::ExpressionType::COMPARE_EQUAL, duckdb::Value::INTEGER(1), true);
}
