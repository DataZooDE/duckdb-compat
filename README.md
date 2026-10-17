# duckdb-compat

This header-only library lets **one source tree of a DuckDB C++ extension**
build against DuckDB **1.4 LTS (andium), 1.5 (variegata) and 2.0**.

community-extensions builds `repo.ref` against DuckDB 2.0, `repo.variegata`
against v1.5.6 and `repo.andium` against v1.4.5. With this header all three
can point at the same commit.

## Use

```bash
git submodule add https://github.com/DataZooDE/duckdb-compat.git duckdb-compat
```

```cmake
# extension CMakeLists.txt
include_directories(duckdb-compat/include)
```

```cpp
#include "duckdb.hpp"
#include "duckdb_compat.hpp"   // after the DuckDB headers you use

namespace dc = duckdb_compat;
```

The repository must stay **public**, because community-extensions clones
submodules when it builds.

## Helpers

| Helper | 1.4 / 1.5 | 2.0 |
|---|---|---|
| `dc::Str(x)` | `x` (a `std::string`) | `x.GetIdentifierName()`: secret names and option keys are `Identifier` |
| `dc::SetStability(fn, s)` | `fn.stability = s` (1.5 also has the setter) | `fn.SetStability(s)` |
| `dc::IsInterrupted(ctx)` | `ctx.interrupted` | `ctx.IsInterrupted()` |
| `dc::TryGetSetting(opener, key, v)` | `FileOpener::TryGetCurrentSetting(opener, key, v)` | same, with `Identifier(key)` |
| `dc::BindNames` | `vector<string>` | `vector<Identifier>`; fill it with `emplace_back` |
| `dc::ExecuteUnaryNullable<IN, OUT>(in, out, n, f)` | `UnaryExecutor::ExecuteWithNulls` | `UnaryExecutor::Execute` returning `optional<OUT>` |
| `dc::SetPathEncode(client, on)` | httplib `set_url_encode` | `set_path_encode` |
| `dc::TlsContext(client)` | httplib `ssl_context()` | `tls_context()`; cast the `void*` to `SSL_CTX*` |

## How it works

Every helper detects the API **by its shape, never by version number**.
There are two overloads: one that compiles only when the new API exists
(SFINAE), and one for the old API. The new API wins whenever it is
present. A version check can't express the real situation: 1.5.6 already
ships several "2.0" APIs that 1.4.5 lacks.

Rules for adding a helper:

1. Call sites never use `#if` on DuckDB versions. Add a helper here instead.
2. Add the helper to `test/probe.cpp`, and a fake-type case to
   `test/test_dispatch.cpp` if it is generic.
3. Make every member lookup *dependent*: reach it through a template
   parameter. A name looked up in a concrete class, or a type that only some
   versions have (`Identifier`), is resolved when the template is defined, and
   SFINAE cannot discard it. Guard such types with `__has_include`.

## Tests

- `test/test_dispatch.cpp` checks the dispatch with fake old, new and
  both-at-once types. It needs no DuckDB:
  `cmake -S . -B build && cmake --build build && ctest --test-dir build`.
- `test/probe.cpp` instantiates every helper against the real headers of each
  DuckDB line. CI runs it for 1.4.5, 1.5.6 and the 2.0 commit that
  community-extensions builds against. Locally:
  `g++ -std=c++17 -fsyntax-only -Iinclude -I<duckdb>/src/include test/probe.cpp`.
