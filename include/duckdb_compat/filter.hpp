#pragma once

#include "duckdb/planner/filter/conjunction_filter.hpp"
#include "duckdb/planner/filter/constant_filter.hpp"
#include "duckdb/planner/filter/dynamic_filter.hpp"
#include "duckdb/planner/filter/in_filter.hpp"
#include "duckdb/planner/filter/null_filter.hpp"
#include "duckdb/planner/filter/optional_filter.hpp"
#include "duckdb/planner/filter/struct_filter.hpp"
#include "duckdb/planner/table_filter.hpp"
#include "duckdb_compat.hpp"
// LogicalGet brings in TableFilterSet's iterator entry on 2.0.
#include "duckdb/planner/expression/bound_comparison_expression.hpp"
#include "duckdb/planner/expression/bound_operator_expression.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/planner/filter/expression_filter.hpp"
#include "duckdb/planner/operator/logical_get.hpp"
// join_hashtable brings in legacy bloom filter declarations on 1.x.
#include "duckdb/execution/join_hashtable.hpp"
#include "duckdb/common/value_operations/value_operations.hpp"

// The legacy classes keep the old filter semantics in DuckDB 2.0. Forward
// declarations make both names legal; only the selected, complete type is used.
namespace duckdb {
class ConstantFilter;
class LegacyConstantFilter;
class InFilter;
class LegacyInFilter;
class ConjunctionAndFilter;
class LegacyConjunctionAndFilter;
class ConjunctionOrFilter;
class LegacyConjunctionOrFilter;
class OptionalFilter;
class LegacyOptionalFilter;
class DynamicFilter;
class LegacyDynamicFilter;
class IsNullFilter;
class LegacyIsNullFilter;
class IsNotNullFilter;
class LegacyIsNotNullFilter;
class StructFilter;
class LegacyStructFilter;
} // namespace duckdb

namespace duckdb_compat {
inline constexpr bool HAS_BLOOM_FILTER = detail::BloomKind<duckdb::TableFilterType>(0).has_value();
// Public shape flag used by consumers that need to name a legacy filter class.
inline constexpr bool NEW_TABLE_FILTERS = detail::HasLegacyFilterKind<duckdb::TableFilterType>::value;
using ConstantFilter = std::conditional_t<NEW_TABLE_FILTERS, duckdb::LegacyConstantFilter, duckdb::ConstantFilter>;
using InFilter = std::conditional_t<NEW_TABLE_FILTERS, duckdb::LegacyInFilter, duckdb::InFilter>;
using ConjunctionAndFilter =
	std::conditional_t<NEW_TABLE_FILTERS, duckdb::LegacyConjunctionAndFilter, duckdb::ConjunctionAndFilter>;
using ConjunctionOrFilter =
	std::conditional_t<NEW_TABLE_FILTERS, duckdb::LegacyConjunctionOrFilter, duckdb::ConjunctionOrFilter>;
using OptionalFilter = std::conditional_t<NEW_TABLE_FILTERS, duckdb::LegacyOptionalFilter, duckdb::OptionalFilter>;
using DynamicFilter = std::conditional_t<NEW_TABLE_FILTERS, duckdb::LegacyDynamicFilter, duckdb::DynamicFilter>;
using IsNullFilter = std::conditional_t<NEW_TABLE_FILTERS, duckdb::LegacyIsNullFilter, duckdb::IsNullFilter>;
using IsNotNullFilter = std::conditional_t<NEW_TABLE_FILTERS, duckdb::LegacyIsNotNullFilter, duckdb::IsNotNullFilter>;
using StructFilter = std::conditional_t<NEW_TABLE_FILTERS, duckdb::LegacyStructFilter, duckdb::StructFilter>;
namespace detail {
template <class COMPARISON>
auto MakeComparisonExpression(duckdb::ExpressionType type, duckdb::unique_ptr<duckdb::Expression> left,
                              duckdb::unique_ptr<duckdb::Expression> right, int)
    -> decltype(COMPARISON::Create(type, std::move(left), std::move(right))) {
    return COMPARISON::Create(type, std::move(left), std::move(right));
}
template <class COMPARISON>
auto MakeComparisonExpression(duckdb::ExpressionType type, duckdb::unique_ptr<duckdb::Expression> left,
                              duckdb::unique_ptr<duckdb::Expression> right, long)
    -> decltype(duckdb::make_uniq<COMPARISON>(type, std::move(left), std::move(right))) {
    return duckdb::make_uniq<COMPARISON>(type, std::move(left), std::move(right));
}
template <class FILTER> auto CopyFilter(const FILTER &filter, int) -> decltype(filter.Copy()) {
	return filter.Copy();
}
template <class FILTER, class EF = duckdb::ExpressionFilter>
auto CopyFilter(const FILTER &filter, long) -> decltype(filter.template Cast<EF>().Copy()) {
	return filter.template Cast<EF>().Copy();
}
template <class FILTER>
auto MatchesConstantFilter(const FILTER &filter, const duckdb::Value &value, int)
    -> decltype(filter.Compare(value)) {
	return filter.Compare(value);
}
template <class FILTER>
bool MatchesConstantFilter(const FILTER &filter, const duckdb::Value &value, long) {
	using duckdb::ExpressionType;
	using duckdb::ValueOperations;
	switch (filter.comparison_type) {
	case ExpressionType::COMPARE_EQUAL: return ValueOperations::Equals(value, filter.constant);
	case ExpressionType::COMPARE_NOTEQUAL: return ValueOperations::NotEquals(value, filter.constant);
	case ExpressionType::COMPARE_GREATERTHAN: return ValueOperations::GreaterThan(value, filter.constant);
	case ExpressionType::COMPARE_GREATERTHANOREQUALTO: return ValueOperations::GreaterThanEquals(value, filter.constant);
	case ExpressionType::COMPARE_LESSTHAN: return ValueOperations::LessThan(value, filter.constant);
	case ExpressionType::COMPARE_LESSTHANOREQUALTO: return ValueOperations::LessThanEquals(value, filter.constant);
	default: throw duckdb::InternalException("Unsupported constant filter comparison");
	}
}
} // namespace detail
inline duckdb::unique_ptr<duckdb::Expression>
MakeComparisonExpression(duckdb::ExpressionType type, duckdb::unique_ptr<duckdb::Expression> left,
                         duckdb::unique_ptr<duckdb::Expression> right) {
    return detail::MakeComparisonExpression<duckdb::BoundComparisonExpression>(type, std::move(left), std::move(right), 0);
}
inline duckdb::unique_ptr<duckdb::TableFilter> CopyFilter(const duckdb::TableFilter &filter) {
	return detail::CopyFilter(filter, 0);
}
template <class FILTER>
bool MatchesConstantFilter(const FILTER &filter, const duckdb::Value &value) {
	return detail::MatchesConstantFilter(filter, value, 0);
}
struct FilterKind {
	//! Optional: check has_value() before comparing a bloom filter kind.
	static constexpr auto BLOOM_FILTER = detail::BloomKind<duckdb::TableFilterType>(0);
	static constexpr auto CONSTANT_COMPARISON = detail::ConstantKind<duckdb::TableFilterType>(0);
	static constexpr auto IS_NULL = detail::NullKind<duckdb::TableFilterType>(0);
	static constexpr auto IS_NOT_NULL = detail::NotNullKind<duckdb::TableFilterType>(0);
	static constexpr auto CONJUNCTION_AND = detail::AndKind<duckdb::TableFilterType>(0);
	static constexpr auto CONJUNCTION_OR = detail::OrKind<duckdb::TableFilterType>(0);
	static constexpr auto OPTIONAL_FILTER = detail::OptionalKind<duckdb::TableFilterType>(0);
	static constexpr auto DYNAMIC_FILTER = detail::DynamicKind<duckdb::TableFilterType>(0);
	static constexpr auto IN_FILTER = detail::InKind<duckdb::TableFilterType>(0);
	static constexpr auto STRUCT_EXTRACT = detail::StructKind<duckdb::TableFilterType>(0);
};

namespace detail {
template <class SET> auto FilterCount(const SET &set, int) -> decltype(set.FilterCount()) { return set.FilterCount(); }
template <class SET> auto FilterCount(const SET &set, long) -> decltype(set.filters.size()) {
	return set.filters.size();
}

template <class SET, class F>
auto ForEachFilter(const SET &set, F &&f, int) -> decltype(set.FilterCount(), set.HasMultiColumnFilters(), set.begin(),
														   (*set.begin()).GetIndex(), (*set.begin()).Filter(), void()) {
	if (set.HasMultiColumnFilters()) {
		throw duckdb::NotImplementedException("Multi-column table filters are not supported");
	}
	for (const auto &entry : set) {
		f(static_cast<duckdb::idx_t>(entry.GetIndex()), entry.Filter());
	}
}
template <class SET, class F> void ForEachFilter(const SET &set, F &&f, long) {
	for (const auto &entry : set.filters) {
		f(entry.first, *entry.second);
	}
}

template <class FILTER>
auto FilterToString(const FILTER &filter, const std::string &column, int) -> decltype(filter.ToString(column)) {
	return filter.ToString(column);
}
template <class FILTER, class EF = duckdb::ExpressionFilter>
auto FilterToString(const FILTER &filter, const std::string &column, long)
	-> decltype(filter.template Cast<EF>().ToString(column)) {
	if (filter.filter_type == duckdb::TableFilterType::EXPRESSION_FILTER) {
		return filter.template Cast<EF>().ToString(column);
	}
	return "table filter type " + std::to_string(static_cast<int>(filter.filter_type));
}
template <class DATA>
auto DynamicSnapshot(const DATA &data, int)
	-> decltype(data.constant, std::optional<std::pair<duckdb::ExpressionType, duckdb::Value>>()) {
	if (!data.initialized) {
		return std::nullopt;
	}
	return std::make_pair(data.comparison_type, data.constant);
}
template <class DATA>
auto DynamicSnapshot(const DATA &data, long)
	-> decltype(data.filter, std::optional<std::pair<duckdb::ExpressionType, duckdb::Value>>()) {
	if (!data.initialized || !data.filter) {
		return std::nullopt;
	}
	return std::make_pair(data.filter->comparison_type, data.filter->constant);
}
} // namespace detail

inline duckdb::idx_t FilterCount(const duckdb::TableFilterSet &set) { return detail::FilterCount(set, 0); }
namespace detail {
template <class SET, class FILTER>
auto SetFilter(SET &set, duckdb::idx_t index, FILTER filter, int)
	-> decltype(set.SetFilterByColumnIndex({}, std::move(filter)), void()) {
	using Index = decltype((*set.begin()).GetIndex());
	set.SetFilterByColumnIndex(Index(index), std::move(filter));
}
template <class SET, class FILTER>
auto SetFilter(SET &set, duckdb::idx_t index, FILTER filter, long)
	-> decltype(set.filters[index] = std::move(filter), void()) {
	set.filters[index] = std::move(filter);
}
template <class DATA>
auto MakeDynamicFilterData(duckdb::ExpressionType comparison, duckdb::Value constant, bool initialized, int)
	-> std::enable_if_t<std::is_constructible_v<DATA, duckdb::ExpressionType, duckdb::Value>,
						duckdb::shared_ptr<DATA>> {
	auto data = duckdb::make_shared_ptr<DATA>(comparison, std::move(constant));
	data->initialized = initialized;
	return data;
}
template <class DATA>
auto MakeDynamicFilterData(duckdb::ExpressionType comparison, duckdb::Value constant, bool initialized, long)
	-> decltype(std::declval<DATA &>().filter, duckdb::shared_ptr<DATA>()) {
	auto data = duckdb::make_shared_ptr<DATA>();
	data->filter = duckdb::make_uniq<ConstantFilter>(comparison, std::move(constant));
	data->initialized = initialized;
	return data;
}
} // namespace detail
inline void SetFilter(duckdb::TableFilterSet &set, duckdb::idx_t index,
					  duckdb::unique_ptr<duckdb::TableFilter> filter) {
	detail::SetFilter(set, index, std::move(filter), 0);
}
inline duckdb::shared_ptr<duckdb::DynamicFilterData> MakeDynamicFilterData(duckdb::ExpressionType comparison,
																		   duckdb::Value constant, bool initialized) {
	return detail::MakeDynamicFilterData<duckdb::DynamicFilterData>(comparison, std::move(constant), initialized, 0);
}
namespace detail {
template <class OP> auto MutableExpressionChildren(OP &op, int) -> decltype(op.GetChildrenMutable()) {
	return op.GetChildrenMutable();
}
template <class OP> auto MutableExpressionChildren(OP &op, long) -> decltype((op.children)) {
	return op.children;
}
} // namespace detail
template <class OP> auto &MutableExpressionChildren(OP &op) {
	return detail::MutableExpressionChildren(op, 0);
}
namespace detail {
template <class BFE, class FUNC>
auto MakeBoundScalarExpression(duckdb::LogicalType return_type, FUNC function,
                               duckdb::vector<duckdb::unique_ptr<duckdb::Expression>> arguments, int)
    -> decltype(duckdb::make_uniq<BFE>(
        std::decay_t<decltype(std::declval<BFE &>().Function())>(function), std::move(arguments),
        duckdb::unique_ptr<duckdb::FunctionData>())) {
    (void)return_type;
    using BoundFunction = std::decay_t<decltype(std::declval<BFE &>().Function())>;
    return duckdb::make_uniq<BFE>(BoundFunction(function), std::move(arguments),
                                  duckdb::unique_ptr<duckdb::FunctionData>());
}
template <class BFE, class FUNC>
auto MakeBoundScalarExpression(duckdb::LogicalType return_type, FUNC function,
                               duckdb::vector<duckdb::unique_ptr<duckdb::Expression>> arguments, long)
    -> decltype(duckdb::make_uniq<BFE>(return_type, function, std::move(arguments),
                                      duckdb::unique_ptr<duckdb::FunctionData>())) {
    return duckdb::make_uniq<BFE>(return_type, function, std::move(arguments),
                                  duckdb::unique_ptr<duckdb::FunctionData>());
}
} // namespace detail
inline duckdb::unique_ptr<duckdb::BoundFunctionExpression>
MakeBoundScalarExpression(duckdb::LogicalType return_type, duckdb::ScalarFunction function,
                          duckdb::vector<duckdb::unique_ptr<duckdb::Expression>> arguments) {
    return detail::MakeBoundScalarExpression<duckdb::BoundFunctionExpression>(
        return_type, std::move(function), std::move(arguments), 0);
}
template <class F> void ForEachFilter(const duckdb::TableFilterSet &set, F &&f) {
	detail::ForEachFilter(set, std::forward<F>(f), 0);
}
inline std::string FilterToString(const duckdb::TableFilter &filter, const std::string &column) {
	return detail::FilterToString(filter, column, 0);
}
namespace detail {
template <class EF, class FILTER>
auto IsOptionalExpressionFilter(const FILTER &filter, int) -> decltype(EF::IsOptionalFilter(filter), bool()) {
	return filter.filter_type == duckdb::TableFilterType::EXPRESSION_FILTER && EF::IsOptionalFilter(filter);
}
template <class EF, class FILTER> bool IsOptionalExpressionFilter(const FILTER &, long) { return false; }

} // namespace detail
inline bool IsOptionalExpressionFilter(const duckdb::TableFilter &filter) {
	return detail::IsOptionalExpressionFilter<duckdb::ExpressionFilter>(filter, 0);
}
struct FilterExpressionNode {
	enum class Kind { COMPARISON, IS_NULL, IS_NOT_NULL, CONJUNCTION_AND, CONJUNCTION_OR };
	// comparison_type and constant are used only for COMPARISON. AND/OR use
	// children, and null checks need only kind and the scan's column name.
	Kind kind;
	duckdb::ExpressionType comparison_type = duckdb::ExpressionType::COMPARE_EQUAL;
	duckdb::Value constant;
	std::vector<FilterExpressionNode> children;
};

namespace detail {
template <class CHILDREN, class IS_OPTIONAL, class CONVERT>
std::optional<FilterExpressionNode> FoldConjunction(FilterExpressionNode::Kind kind, const CHILDREN &children,
													IS_OPTIONAL optional, CONVERT convert) {
	FilterExpressionNode node{kind, duckdb::ExpressionType::COMPARE_EQUAL, duckdb::Value(), {}};
	for (const auto &child : children) {
		if (optional(child)) {
			// Optional expressions are scan hints, not redundant SQL conjuncts.
			// Dropping one would make the generated OPTIONS differ from the tree
			// handed to the scan. Keep the entire filter for local evaluation.
			return std::nullopt;
		}
		auto converted = convert(child);
		if (!converted) {
			return std::nullopt;
		}
		node.children.push_back(std::move(*converted));
	}
	return node.children.empty() ? std::nullopt : std::optional<FilterExpressionNode>(std::move(node));
}

template <class EF, class = void> struct HasColumnConstantComparison : std::false_type {};
template <class EF>
struct HasColumnConstantComparison<
	EF, std::void_t<decltype(EF::TryGetColumnConstantComparison(std::declval<const duckdb::BoundFunctionExpression &>(),
																std::declval<duckdb::ExpressionType &>()))>>
	: std::true_type {};

template <class EF, class EXPR>
auto IsOptionalExpression(const EXPR &expression, int) -> decltype(EF::IsOptionalExpression(expression), bool()) {
	return EF::IsOptionalExpression(expression);
}
template <class EF, class EXPR> bool IsOptionalExpression(const EXPR &, long) { return false; }

template <class EF, class EXPR, class CMP = duckdb::BoundComparisonExpression>
auto SimpleExpressionTree(const EXPR &expression, int)
	-> std::enable_if_t<HasColumnConstantComparison<EF>::value, std::optional<FilterExpressionNode>> {
	if (expression.GetExpressionClass() == duckdb::ExpressionClass::BOUND_FUNCTION &&
		CMP::IsComparison(expression.GetExpressionType())) {
		auto comparison_type = expression.GetExpressionType();
		auto constant = EF::TryGetColumnConstantComparison(expression.template Cast<duckdb::BoundFunctionExpression>(),
														   comparison_type);
		if (!constant) {
			return std::nullopt;
		}
		return FilterExpressionNode{FilterExpressionNode::Kind::COMPARISON, comparison_type, constant->GetValue(), {}};
	}
	if (expression.GetExpressionClass() == duckdb::ExpressionClass::BOUND_OPERATOR) {
		const auto &op = expression.template Cast<duckdb::BoundOperatorExpression>();
		const auto &children = op.GetChildren();
		const auto type = expression.GetExpressionType();
		if ((type == duckdb::ExpressionType::OPERATOR_IS_NULL ||
			 type == duckdb::ExpressionType::OPERATOR_IS_NOT_NULL) &&
			children.size() == 1 && EF::IsSimpleFilterColumnRef(*children[0])) {
			return FilterExpressionNode{type == duckdb::ExpressionType::OPERATOR_IS_NULL
											? FilterExpressionNode::Kind::IS_NULL
											: FilterExpressionNode::Kind::IS_NOT_NULL,
										duckdb::ExpressionType::COMPARE_EQUAL,
										duckdb::Value(),
										{}};
		}
		if (type == duckdb::ExpressionType::COMPARE_IN && children.size() > 1 &&
			EF::IsSimpleFilterColumnRef(*children[0])) {
			FilterExpressionNode node{
				FilterExpressionNode::Kind::CONJUNCTION_OR, duckdb::ExpressionType::COMPARE_EQUAL, duckdb::Value(), {}};
			for (size_t i = 1; i < children.size(); i++) {
				if (children[i]->GetExpressionClass() != duckdb::ExpressionClass::BOUND_CONSTANT) {
					return std::nullopt;
				}
				node.children.push_back(
					{FilterExpressionNode::Kind::COMPARISON,
					 duckdb::ExpressionType::COMPARE_EQUAL,
					 duckdb_compat::ConstantValue(children[i]->template Cast<duckdb::BoundConstantExpression>()),
					 {}});
			}
			return node;
		}
		if (type == duckdb::ExpressionType::OPERATOR_NOT && children.size() == 1) {
			auto child = SimpleExpressionTree<EF>(*children[0], 0);
			if (!child || child->kind != FilterExpressionNode::Kind::COMPARISON) {
				return std::nullopt;
			}
			switch (child->comparison_type) {
			case duckdb::ExpressionType::COMPARE_EQUAL:
				child->comparison_type = duckdb::ExpressionType::COMPARE_NOTEQUAL;
				break;
			case duckdb::ExpressionType::COMPARE_NOTEQUAL:
				child->comparison_type = duckdb::ExpressionType::COMPARE_EQUAL;
				break;
			case duckdb::ExpressionType::COMPARE_GREATERTHAN:
				child->comparison_type = duckdb::ExpressionType::COMPARE_LESSTHANOREQUALTO;
				break;
			case duckdb::ExpressionType::COMPARE_GREATERTHANOREQUALTO:
				child->comparison_type = duckdb::ExpressionType::COMPARE_LESSTHAN;
				break;
			case duckdb::ExpressionType::COMPARE_LESSTHAN:
				child->comparison_type = duckdb::ExpressionType::COMPARE_GREATERTHANOREQUALTO;
				break;
			case duckdb::ExpressionType::COMPARE_LESSTHANOREQUALTO:
				child->comparison_type = duckdb::ExpressionType::COMPARE_GREATERTHAN;
				break;
			default:
				return std::nullopt;
			}
			return child;
		}
	}
	if (expression.GetExpressionClass() != duckdb::ExpressionClass::BOUND_CONJUNCTION) {
		return std::nullopt;
	}
	const auto &conjunction = expression.template Cast<duckdb::BoundConjunctionExpression>();
	auto type = conjunction.GetExpressionType();
	if (type != duckdb::ExpressionType::CONJUNCTION_AND && type != duckdb::ExpressionType::CONJUNCTION_OR) {
		return std::nullopt;
	}
	const auto kind = type == duckdb::ExpressionType::CONJUNCTION_AND ? FilterExpressionNode::Kind::CONJUNCTION_AND
																	  : FilterExpressionNode::Kind::CONJUNCTION_OR;
	return FoldConjunction(
		kind, conjunction.GetChildren(), [](const auto &child) { return IsOptionalExpression<EF>(*child, 0); },
		[](const auto &child) { return SimpleExpressionTree<EF>(*child, 0); });
}
template <class EF, class FILTER>
auto SimpleExpressionTreeFilter(const FILTER &filter, int)
	-> std::enable_if_t<HasColumnConstantComparison<EF>::value, std::optional<FilterExpressionNode>> {
	if (filter.filter_type == duckdb::TableFilterType::EXPRESSION_FILTER) {
		return SimpleExpressionTree<EF>(*filter.template Cast<EF>().expr, 0);
	}
	return std::nullopt;
}
template <class EF, class FILTER> std::optional<FilterExpressionNode> SimpleExpressionTreeFilter(const FILTER &, long) {
	return std::nullopt;
}
} // namespace detail
inline std::optional<FilterExpressionNode> SimpleExpressionTree(const duckdb::TableFilter &filter) {
	return detail::SimpleExpressionTreeFilter<duckdb::ExpressionFilter>(filter, 0);
}
inline std::optional<std::pair<duckdb::ExpressionType, duckdb::Value>>
DynamicSnapshot(const duckdb::DynamicFilterData &data) {
	return detail::DynamicSnapshot(data, 0);
}
} // namespace duckdb_compat
