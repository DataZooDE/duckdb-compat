// Pure dispatch tests: fake "old" and "new" DuckDB/httplib shapes, no DuckDB
// needed. Each helper must pick the new API when it exists and fall back to
// the old one otherwise -- this is the whole contract of the library.
#include "duckdb_compat/dispatch.hpp"

#include <atomic>
#include <cassert>
#include <cstdio>
#include <string>
#include <utility>

namespace dc = duckdb_compat;

enum class Stab { A, B };

struct OldFn {
	Stab stability = Stab::A;
};
struct NewFn {
	Stab s = Stab::A;
	bool used_setter = false;
	void SetStability(Stab v) {
		s = v;
		used_setter = true;
	}
};
// 1.5.6 has both: the setter must win.
struct BothFn {
	Stab stability = Stab::A;
	bool used_setter = false;
	void SetStability(Stab v) {
		stability = v;
		used_setter = true;
	}
};

enum class Errs { CANNOT_ERROR, CAN_THROW_RUNTIME_ERROR };
// 1.4: a public `errors` field; 1.5 and 2.0: SetFallible().
struct OldErrFn {
	Errs errors = Errs::CANNOT_ERROR;
};
struct NewErrFn {
	bool fallible = false;
	void SetFallible() {
		fallible = true;
	}
};

enum class Nulls { DEFAULT_NULL_HANDLING, SPECIAL_HANDLING };
// 1.4: a public `null_handling` field; 1.5 and 2.0: SetNullHandling().
struct OldNullFn {
	Nulls null_handling = Nulls::DEFAULT_NULL_HANDLING;
};
struct NewNullFn {
	Nulls n = Nulls::DEFAULT_NULL_HANDLING;
	void SetNullHandling(Nulls v) {
		n = v;
	}
};

struct OldCtx {
	std::atomic<bool> interrupted {true};
};
struct NewCtx {
	bool IsInterrupted() const {
		return true;
	}
};

struct Ident {
	std::string v;
	const std::string &GetIdentifierName() const {
		return v;
	}
};

struct OldClient {
	bool url = false;
	int ctx = 1;
	void set_url_encode(bool on) {
		url = on;
	}
	int *ssl_context() const {
		return const_cast<int *>(&ctx);
	}
};
struct NewClient {
	bool path = false;
	int ctx = 2;
	void set_path_encode(bool on) {
		path = on;
	}
	void *tls_context() const {
		return const_cast<int *>(&ctx);
	}
};

// 1.4: only SetCardinality. 1.5.6: both (SetChildCardinality is a backport
// alias). 2.0: both, but SetCardinality leaves the vectors' own size alone --
// the helper must prefer SetChildCardinality whenever it exists.
struct OldChunk {
	unsigned long count = 0;
	void SetCardinality(unsigned long n) { count = n; }
};
struct NewChunk {
	unsigned long count = 0, child = 0;
	void SetCardinality(unsigned long n) { count = n; }
	void SetChildCardinality(unsigned long n) { count = n; child = n; }
};

struct OldUnifiedVector {
	void ToUnifiedFormat(unsigned long count, int &format) { format = static_cast<int>(count); }
};
struct NewUnifiedVector {
	void ToUnifiedFormat(int &format) { format = 20; }
};
struct BothUnifiedVector {
	void ToUnifiedFormat(unsigned long, int &format) { format = 10; }
	void ToUnifiedFormat(int &format) { format = 20; }
};

struct OldBind {
	int value = 3;
};
struct NewBind {
	OldBind *bind_data = nullptr;
};
struct UnrelatedBind {};
struct UnrelatedBindWithMember { int bind_data = 0; };
template <class Expected, class T, class = void>
struct CanGetBindData : std::false_type {};
template <class Expected, class T>
struct CanGetBindData<Expected, T, std::void_t<decltype(dc::detail::GetBindData<Expected>(std::declval<T &>(), 0))>>
    : std::true_type {};
static_assert(CanGetBindData<OldBind, OldBind>::value);
static_assert(CanGetBindData<NewBind, NewBind>::value);
static_assert(!CanGetBindData<OldBind, NewBind>::value);
static_assert(!CanGetBindData<NewBind, UnrelatedBind>::value);
static_assert(!CanGetBindData<NewBind, UnrelatedBindWithMember>::value);

struct OldResult {
	int GetValue(int column, int row) {
		return column + row;
	}
};
struct NewCollection {
	int GetValue(int column, int row) {
		return column * 10 + row;
	}
};
struct NewResult {
	NewCollection collection;
	NewCollection &Collection() {
		return collection;
	}
};
struct BothResult : OldResult {
	NewCollection collection;
	NewCollection &Collection() {
		return collection;
	}
};

enum class OldKinds {
	CONSTANT_COMPARISON,
	IS_NULL,
	IS_NOT_NULL,
	CONJUNCTION_AND,
	CONJUNCTION_OR,
	OPTIONAL_FILTER,
	DYNAMIC_FILTER,
	IN_FILTER,
	BLOOM_FILTER
};
enum class NewKinds {
	LEGACY_CONSTANT_COMPARISON,
	LEGACY_IS_NULL,
	LEGACY_IS_NOT_NULL,
	LEGACY_CONJUNCTION_AND,
	LEGACY_CONJUNCTION_OR,
	LEGACY_OPTIONAL_FILTER,
	LEGACY_DYNAMIC_FILTER,
	LEGACY_IN_FILTER,
	LEGACY_BLOOM_FILTER
};
enum class BothKinds {
	CONSTANT_COMPARISON,
	IS_NULL,
	IS_NOT_NULL,
	CONJUNCTION_AND,
	CONJUNCTION_OR,
	OPTIONAL_FILTER,
	DYNAMIC_FILTER,
	IN_FILTER,
	BLOOM_FILTER,
	LEGACY_CONSTANT_COMPARISON,
	LEGACY_IS_NULL,
	LEGACY_IS_NOT_NULL,
	LEGACY_CONJUNCTION_AND,
	LEGACY_CONJUNCTION_OR,
	LEGACY_OPTIONAL_FILTER,
	LEGACY_DYNAMIC_FILTER,
	LEGACY_IN_FILTER,
	LEGACY_BLOOM_FILTER
};
enum class NoBloom { CONSTANT_COMPARISON };

static_assert(dc::detail::ConstantKind<OldKinds>(0) == OldKinds::CONSTANT_COMPARISON);
static_assert(dc::detail::ConstantKind<NewKinds>(0) == NewKinds::LEGACY_CONSTANT_COMPARISON);
static_assert(dc::detail::ConstantKind<BothKinds>(0) == BothKinds::LEGACY_CONSTANT_COMPARISON);
static_assert(dc::detail::NullKind<OldKinds>(0) == OldKinds::IS_NULL);
static_assert(dc::detail::NullKind<NewKinds>(0) == NewKinds::LEGACY_IS_NULL);
static_assert(dc::detail::NullKind<BothKinds>(0) == BothKinds::LEGACY_IS_NULL);
static_assert(dc::detail::NotNullKind<OldKinds>(0) == OldKinds::IS_NOT_NULL);
static_assert(dc::detail::NotNullKind<NewKinds>(0) == NewKinds::LEGACY_IS_NOT_NULL);
static_assert(dc::detail::NotNullKind<BothKinds>(0) == BothKinds::LEGACY_IS_NOT_NULL);
static_assert(dc::detail::AndKind<OldKinds>(0) == OldKinds::CONJUNCTION_AND);
static_assert(dc::detail::AndKind<NewKinds>(0) == NewKinds::LEGACY_CONJUNCTION_AND);
static_assert(dc::detail::AndKind<BothKinds>(0) == BothKinds::LEGACY_CONJUNCTION_AND);
static_assert(dc::detail::OrKind<OldKinds>(0) == OldKinds::CONJUNCTION_OR);
static_assert(dc::detail::OrKind<NewKinds>(0) == NewKinds::LEGACY_CONJUNCTION_OR);
static_assert(dc::detail::OrKind<BothKinds>(0) == BothKinds::LEGACY_CONJUNCTION_OR);
static_assert(dc::detail::OptionalKind<OldKinds>(0) == OldKinds::OPTIONAL_FILTER);
static_assert(dc::detail::OptionalKind<NewKinds>(0) == NewKinds::LEGACY_OPTIONAL_FILTER);
static_assert(dc::detail::OptionalKind<BothKinds>(0) == BothKinds::LEGACY_OPTIONAL_FILTER);
static_assert(dc::detail::DynamicKind<OldKinds>(0) == OldKinds::DYNAMIC_FILTER);
static_assert(dc::detail::DynamicKind<NewKinds>(0) == NewKinds::LEGACY_DYNAMIC_FILTER);
static_assert(dc::detail::DynamicKind<BothKinds>(0) == BothKinds::LEGACY_DYNAMIC_FILTER);
static_assert(dc::detail::InKind<OldKinds>(0) == OldKinds::IN_FILTER);
static_assert(dc::detail::InKind<NewKinds>(0) == NewKinds::LEGACY_IN_FILTER);
static_assert(dc::detail::InKind<BothKinds>(0) == BothKinds::LEGACY_IN_FILTER);
static_assert(dc::detail::BloomKind<OldKinds>(0) == OldKinds::BLOOM_FILTER);
static_assert(dc::detail::BloomKind<NewKinds>(0) == NewKinds::LEGACY_BLOOM_FILTER);
static_assert(dc::detail::BloomKind<BothKinds>(0) == BothKinds::LEGACY_BLOOM_FILTER);
static_assert(!dc::detail::BloomKind<NoBloom>(0).has_value());

int main() {
	OldFn o;
	dc::SetStability(o, Stab::B);
	assert(o.stability == Stab::B);
	NewFn n;
	dc::SetStability(n, Stab::B);
	assert(n.s == Stab::B && n.used_setter);
	BothFn b;
	dc::SetStability(b, Stab::B);
	assert(b.used_setter);

	OldErrFn oe;
	dc::SetFallible(oe);
	assert(oe.errors == Errs::CAN_THROW_RUNTIME_ERROR);
	NewErrFn ne;
	dc::SetFallible(ne);
	assert(ne.fallible);

	OldNullFn on;
	dc::SetNullHandling(on, Nulls::SPECIAL_HANDLING);
	assert(on.null_handling == Nulls::SPECIAL_HANDLING);
	NewNullFn nn;
	dc::SetNullHandling(nn, Nulls::SPECIAL_HANDLING);
	assert(nn.n == Nulls::SPECIAL_HANDLING);

	OldChunk och;
	dc::SetChildCardinality(och, 7);
	assert(och.count == 7);
	NewChunk nch;
	dc::SetChildCardinality(nch, 7);
	assert(nch.count == 7 && nch.child == 7);
	int format = 0;
	OldUnifiedVector old_vector;
	dc::ToUnifiedFormat(old_vector, 7UL, format);
	assert(format == 7);
	NewUnifiedVector new_vector;
	dc::ToUnifiedFormat(new_vector, 7UL, format);
	assert(format == 20);
	BothUnifiedVector both_vector;
	dc::ToUnifiedFormat(both_vector, 7UL, format);
	assert(format == 20);

	OldCtx oc;
	assert(dc::IsInterrupted(oc));
	NewCtx nc;
	assert(dc::IsInterrupted(nc));

	std::string s = "plain";
	assert(&dc::Str(s) == &s);
	Ident id {"Named"};
	assert(dc::Str(id) == "Named");

	OldClient ocl;
	dc::SetPathEncode(ocl, true);
	assert(ocl.url);
	NewClient ncl;
	dc::SetPathEncode(ncl, true);
	assert(ncl.path);
	assert(*static_cast<int *>(dc::TlsContext(ocl)) == 1);
	assert(*static_cast<int *>(dc::TlsContext(ncl)) == 2);

	OldBind old_bind;
	NewBind new_bind {&old_bind};
	assert(&dc::detail::GetBindData<OldBind>(old_bind, 0) == &old_bind);
	assert(dc::detail::GetBindData<NewBind>(new_bind, 0) == &old_bind);
	OldResult old_result;
	NewResult new_result;
	BothResult both_result;
	assert(dc::detail::ResultValue(old_result, 2, 3, 0) == 5);
	assert(dc::detail::ResultValue(new_result, 2, 3, 0) == 23);
	assert(dc::detail::ResultValue(both_result, 2, 3, 0) == 23);

	std::puts("dispatch: all passed");
	return 0;
}
