// Aggregate function callbacks across DuckDB 1.4 / 1.5 / 2.0.
//
// 2.0 changed the shape of the hand-written aggregate callbacks:
//   size        (const AggregateFunction &)           -> (AggregateStateInput &)
//   initialize  (const AggregateFunction &, state)    -> (AggregateStateInput &, states[], count)  (batched)
//   finalize    (..., AggregateInputData &, ...)      -> (..., AggregateFinalizeInputData &, ...)
//   bind        (ClientContext &, AggregateFunction &, arguments) -> (BindAggregateFunctionInput &)
// update, combine and destroy are unchanged. Write each callback once, in the
// shape below, and pass it through these adapters; each one deduces DuckDB's
// own callback typedef, so the result is a plain function pointer on every line.
//
//   AggregateFunction fn("f", {DOUBLE}, DOUBLE,
//       dc::AggregateStateSize<sizeof(MyState)>(), dc::AggregateInit<MyInit>(),
//       MyUpdate, MyCombine, dc::AggregateFinalize<MyFinalize>(),
//       nullptr, dc::AggregateBind<MyBind>(), nullptr);
//
//   void MyInit(data_ptr_t state);
//   void MyFinalize(Vector &states, AggregateInputData &, Vector &result, idx_t count, idx_t offset);
//   unique_ptr<FunctionData> MyBind(ClientContext &, dc::AggregateBindFunction &, vector<unique_ptr<Expression>> &);
//
// In a bind, set the result type with dc::SetReturnType(function, type):
// AggregateBindFunction is AggregateFunction before 2.0 and the
// BoundAggregateFunction being bound on 2.0.
#pragma once

#include "duckdb/function/aggregate_function.hpp"
#include "duckdb/planner/expression.hpp"
#include "duckdb_compat.hpp"

#include <type_traits>
#include <utility>

namespace duckdb_compat {

namespace detail {

template <class SHAPE>
struct AggregateSizeShape;
template <class ARG>
struct AggregateSizeShape<duckdb::idx_t (*)(ARG)> {
	template <duckdb::idx_t SIZE>
	static duckdb::idx_t Call(ARG) {
		return SIZE;
	}
};

template <class SHAPE>
struct AggregateInitShape;
// Before 2.0: one state per call.
template <class ARG>
struct AggregateInitShape<void (*)(ARG, duckdb::data_ptr_t)> {
	template <void (*INIT)(duckdb::data_ptr_t)>
	static void Call(ARG, duckdb::data_ptr_t state) {
		INIT(state);
	}
};
// 2.0: batched.
template <class ARG>
struct AggregateInitShape<void (*)(ARG, duckdb::data_ptr_t *, duckdb::idx_t)> {
	template <void (*INIT)(duckdb::data_ptr_t)>
	static void Call(ARG, duckdb::data_ptr_t *states, duckdb::idx_t count) {
		for (duckdb::idx_t i = 0; i < count; i++) {
			INIT(states[i]);
		}
	}
};

using LegacyAggregateFinalize = void (*)(duckdb::Vector &, duckdb::AggregateInputData &, duckdb::Vector &,
                                         duckdb::idx_t, duckdb::idx_t);
template <class SHAPE>
struct AggregateFinalizeShape;
template <class FINALIZE_DATA>
struct AggregateFinalizeShape<void (*)(duckdb::Vector &, FINALIZE_DATA &, duckdb::Vector &, duckdb::idx_t,
                                       duckdb::idx_t)> {
	template <LegacyAggregateFinalize FINALIZE>
	static void Call(duckdb::Vector &states, FINALIZE_DATA &data, duckdb::Vector &result, duckdb::idx_t count,
	                 duckdb::idx_t offset) {
		FINALIZE(states, data, result, count, offset);
	}
};

using AggregateArguments = duckdb::vector<duckdb::unique_ptr<duckdb::Expression>>;
template <class SHAPE>
struct AggregateBindShape;
// Before 2.0: (ClientContext &, AggregateFunction &, arguments).
template <class FUNCTION>
struct AggregateBindShape<duckdb::unique_ptr<duckdb::FunctionData> (*)(duckdb::ClientContext &, FUNCTION &,
                                                                      AggregateArguments &)> {
	using Function = FUNCTION;
	template <duckdb::unique_ptr<duckdb::FunctionData> (*BIND)(duckdb::ClientContext &, Function &,
	                                                           AggregateArguments &)>
	static duckdb::unique_ptr<duckdb::FunctionData> Call(duckdb::ClientContext &context, FUNCTION &function,
	                                                     AggregateArguments &arguments) {
		return BIND(context, function, arguments);
	}
};
// 2.0: one BindAggregateFunctionInput.
template <class BIND_INPUT>
struct AggregateBindShape<duckdb::unique_ptr<duckdb::FunctionData> (*)(BIND_INPUT &)> {
	using Function = typename std::remove_reference<decltype(std::declval<BIND_INPUT &>().GetBoundFunction())>::type;
	template <duckdb::unique_ptr<duckdb::FunctionData> (*BIND)(duckdb::ClientContext &, Function &,
	                                                           AggregateArguments &)>
	static duckdb::unique_ptr<duckdb::FunctionData> Call(BIND_INPUT &input) {
		return BIND(input.GetClientContext(), input.GetBoundFunction(), input.GetArguments());
	}
};

} // namespace detail

//! The function an aggregate bind receives: AggregateFunction before 2.0, BoundAggregateFunction on 2.0.
using AggregateBindFunction = detail::AggregateBindShape<duckdb::bind_aggregate_function_t>::Function;
using AggregateBindCallback = duckdb::unique_ptr<duckdb::FunctionData> (*)(duckdb::ClientContext &,
                                                                           AggregateBindFunction &,
                                                                           detail::AggregateArguments &);

//! A state-size callback returning SIZE.
template <duckdb::idx_t SIZE>
constexpr duckdb::aggregate_size_t AggregateStateSize() {
	return &detail::AggregateSizeShape<duckdb::aggregate_size_t>::template Call<SIZE>;
}

//! An initialize callback that runs INIT on each new state.
template <void (*INIT)(duckdb::data_ptr_t)>
constexpr duckdb::aggregate_initialize_t AggregateInit() {
	return &detail::AggregateInitShape<duckdb::aggregate_initialize_t>::template Call<INIT>;
}

//! A finalize callback from one written against AggregateInputData.
template <detail::LegacyAggregateFinalize FINALIZE>
constexpr duckdb::aggregate_finalize_t AggregateFinalize() {
	return &detail::AggregateFinalizeShape<duckdb::aggregate_finalize_t>::template Call<FINALIZE>;
}

//! A bind callback from one written as (ClientContext &, AggregateBindFunction &, arguments).
template <AggregateBindCallback BIND>
constexpr duckdb::bind_aggregate_function_t AggregateBind() {
	return &detail::AggregateBindShape<duckdb::bind_aggregate_function_t>::template Call<BIND>;
}

namespace detail {
template <class F>
auto SetReturnType(F &function, duckdb::LogicalType type, int) -> decltype(function.SetReturnType(type), void()) {
	function.SetReturnType(std::move(type));
}
template <class F>
auto SetReturnType(F &function, duckdb::LogicalType type, long) -> decltype(function.return_type = type, void()) {
	function.return_type = std::move(type);
}
} // namespace detail

//! Set a function's result type from its bind (`return_type` member before 1.5, setter after).
template <class F>
void SetReturnType(F &function, duckdb::LogicalType type) {
	detail::SetReturnType(function, std::move(type), 0);
}

} // namespace duckdb_compat
