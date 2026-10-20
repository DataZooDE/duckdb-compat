#include "duckdb_compat/filter.hpp"

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
	duckdb_compat::SetFilter(set, 0,
	                         duckdb::make_uniq<duckdb_compat::ConstantFilter>(duckdb::ExpressionType::COMPARE_EQUAL,
	                                                                          duckdb::Value::INTEGER(1)));
	(void)duckdb_compat::MakeDynamicFilterData(duckdb::ExpressionType::COMPARE_EQUAL, duckdb::Value::INTEGER(1), true);
}
