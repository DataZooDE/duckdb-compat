// Pure dispatch tests: fake "old" and "new" DuckDB/httplib shapes, no DuckDB
// needed. Each helper must pick the new API when it exists and fall back to
// the old one otherwise -- this is the whole contract of the library.
#include "duckdb_compat/dispatch.hpp"

#include <atomic>
#include <cassert>
#include <cstdio>
#include <string>

namespace dc = duckdb_compat;

enum class Stab { A, B };

struct OldFn { Stab stability = Stab::A; };
struct NewFn {
	Stab s = Stab::A;
	bool used_setter = false;
	void SetStability(Stab v) { s = v; used_setter = true; }
};
// 1.5.6 has both: the setter must win.
struct BothFn {
	Stab stability = Stab::A;
	bool used_setter = false;
	void SetStability(Stab v) { stability = v; used_setter = true; }
};

enum class Errs { CANNOT_ERROR, CAN_THROW_RUNTIME_ERROR };
// 1.4: a public `errors` field; 1.5 and 2.0: SetFallible().
struct OldErrFn { Errs errors = Errs::CANNOT_ERROR; };
struct NewErrFn {
	bool fallible = false;
	void SetFallible() { fallible = true; }
};

struct OldCtx { std::atomic<bool> interrupted {true}; };
struct NewCtx { bool IsInterrupted() const { return true; } };

struct Ident {
	std::string v;
	const std::string &GetIdentifierName() const { return v; }
};

struct OldClient {
	bool url = false;
	int ctx = 1;
	void set_url_encode(bool on) { url = on; }
	int *ssl_context() const { return const_cast<int *>(&ctx); }
};
struct NewClient {
	bool path = false;
	int ctx = 2;
	void set_path_encode(bool on) { path = on; }
	void *tls_context() const { return const_cast<int *>(&ctx); }
};

int main() {
	OldFn o; dc::SetStability(o, Stab::B); assert(o.stability == Stab::B);
	NewFn n; dc::SetStability(n, Stab::B); assert(n.s == Stab::B && n.used_setter);
	BothFn b; dc::SetStability(b, Stab::B); assert(b.used_setter);

	OldErrFn oe; dc::SetFallible(oe); assert(oe.errors == Errs::CAN_THROW_RUNTIME_ERROR);
	NewErrFn ne; dc::SetFallible(ne); assert(ne.fallible);

	OldCtx oc; assert(dc::IsInterrupted(oc));
	NewCtx nc; assert(dc::IsInterrupted(nc));

	std::string s = "plain";
	assert(&dc::Str(s) == &s);
	Ident id {"Named"};
	assert(dc::Str(id) == "Named");

	OldClient ocl; dc::SetPathEncode(ocl, true); assert(ocl.url);
	NewClient ncl; dc::SetPathEncode(ncl, true); assert(ncl.path);
	assert(*static_cast<int *>(dc::TlsContext(ocl)) == 1);
	assert(*static_cast<int *>(dc::TlsContext(ncl)) == 2);

	std::puts("dispatch: all passed");
	return 0;
}
