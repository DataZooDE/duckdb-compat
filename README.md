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
| `dc::SetFallible(fn)` | `fn.errors = CAN_THROW_RUNTIME_ERROR` (1.4), `fn.SetFallible()` (1.5) | `fn.SetFallible()`. **Required** for any scalar function that can throw: 2.0 turns its error into an INTERNAL error |
| `dc::IsInterrupted(ctx)` | `ctx.interrupted` | `ctx.IsInterrupted()` |
| `dc::TryGetSetting(opener, key, v)` | `FileOpener::TryGetCurrentSetting(opener, key, v)` | same, with `Identifier(key)` |
| `dc::BindNames` | `vector<string>` | `vector<Identifier>`; fill it with `emplace_back` |
| `dc::ExecuteUnaryNullable<INPUT_TYPE, RESULT_TYPE>(in, out, n, f)` | `UnaryExecutor::ExecuteWithNulls` | `UnaryExecutor::Execute` returning `optional<RESULT_TYPE>` |
| `dc::CopyBindNames` | `vector<string>` | `vector<Identifier>`; declare COPY TO bind `names` as `const dc::CopyBindNames &` |
| `dc::FunctionName(s)` | `s` | `Identifier(s)`: function constructors take `Identifier`, explicit from `std::string` (literals still convert) |
| `dc::AddNamedParameter(fn, name, type)` | `fn.named_parameters[name] = type` | typed `**options` kwargs (`WithTypedKwargs`/`ExtendTypedKwargs`), as `read_csv` does. Not `AddKeywordOnly`: without a default that makes the parameter **required** |
| `dc::FindNamedParameter(input.named_parameters, name)` | `case_insensitive_map_t` lookup | `InsertionOrderPreservingMap` keyed by `Identifier`; returns `const Value*`, nullptr if not passed |
| `dc::NameParameters(fn, {"path", ...})` | no-op: `FunctionDescription::parameter_names` names them | renames the signature's positional parameters, which `duckdb_functions()` prefers (else `col0`, `col1`, ...) |
| `dc::ForEachFunction(set, [](T &fun) {...})` | loop over `set.functions` (`vector<T>`) | `set.ApplyToFunctions(...)`: `functions` holds immutable `shared_ptr<const T>` |
| `dc::ResultNames(result)` | `result.names` | `result.GetNames()` (Identifiers), as strings |
| `dc::FetchRows(result)` | `Fetch()` loop | same; 2.0 has no `MaterializedQueryResult` and no `GetValue(col, row)`. Hold results as `unique_ptr<QueryResult>` |
| `dc::TryCastValue(v, type, out, &err, strict)` | `DefaultTryCastAs(type, out, err, strict)` | `DefaultTryCastAs(type, err, strict)` returning `optional<Value>` |
| `dc::ScalarMacroInfo(schema, name, "(a, b := 1) AS a + b")` | parses that `CREATE MACRO` | `DefaultMacro {schema, name, definition}` + `CreateInternalMacroInfo`. Table macros (`DefaultTableMacro`) are unchanged |
| `dc::SetChildCardinality(chunk, n)` | `chunk.SetCardinality(n)` (1.5.6: `SetChildCardinality` is a backport alias) | `chunk.SetChildCardinality(n)`: vectors carry their own size; the deprecated `SetCardinality` leaves them at 0, and `IS NULL` over a scan's output then returns garbage |
| `dc::ToUnifiedFormat(vector, n, format)` | `vector.ToUnifiedFormat(n, format)` | `vector.ToUnifiedFormat(format)`; 2.0 reads the vector's own size and deprecates the count-taking overload |
| `dc::SetPathEncode(client, on)` | httplib `set_url_encode` | `set_path_encode` |
| `dc::TlsContext(client)` | httplib `ssl_context()` | `tls_context()`; cast the `void*` to `SSL_CTX*` |
| `dc::LoadSystemCerts(ssl_ctx)` (`duckdb_compat/httplib.hpp`) | `detail::load_system_certs_on_windows`, Windows only | `tls::load_system_certs`, every platform |
| `dc::Ident(s)`, `dc::Str(x)`, `dc::Strs(names)`, `dc::Idents(names)` | `std::string` (1.4); 1.5.6 has an `Identifier` that converts implicitly | catalog names are `Identifier`, explicit from and to `std::string`. `Ident` into catalog APIs (`GetCatalogEntry`, `GetSchema`, `ColumnList::GetColumn`, `ColumnDefinition`, ...), `Str` out of them. 2.0 formats an `Identifier` exception argument with quotes: pass `Str(...)` |
| `dc::SetQualifiedName(info, catalog, schema, name)`, `dc::SetSchema(info, s)`, `dc::SetCatalog(info, c)` | `info.catalog`/`info.schema`/`info.table` (1.4); 1.5.6 also has the 2.0 setters | one private `QualifiedName` behind `SetQualifiedName` / `SetSchema` / `SetCatalog`. Works for `CreateTableInfo`, `CreateViewInfo`, `CreateSequenceInfo`, `CreateSchemaInfo` and `DropInfo` |
| `dc::MakeUniqueConstraint({"a", "b"}, is_pk)` | `UniqueConstraint(vector<string>, bool)` | `UniqueConstraint(vector<Identifier>, bool)` |
| `dc::EntryName(name)` | the string itself: **`EntryLookupInfo` keeps a reference to it**, so never pass a temporary | `QualifiedName(Identifier(name))` by value |
| `dc::GetEntry<T>(catalog, ctx, schema, name, if_not_found)`, `dc::GetEntryInSchema(catalog, ctx, schema, lookup, if_not_found)` | `catalog.GetEntry(...)` | the same instance lookups, deprecated on 2.0 (whose replacement is static and resolves the catalog by name); the deprecation is silenced in one place |
| `dc::ParseStatements(ctx, sql)` | `Parser().ParseQuery(sql)` | `Parser(ctx)`: `ParserOptions()` is private |
| `dc::ReferenceValue(vec, value, n)`, `dc::MakeValueVector(value, n)` | `vec.Reference(value)`, `Vector(value)` | vectors carry their size: `Reference(value, count_t(n))`, `Vector(value, count_t(n))` |
| `dc::SetVectorSize(vec, n)` | no-op | `FlatVector::SetSize`: a vector filled with `SetValue` or through its data keeps size 0 otherwise. For chunks use `chunk.SetChildCardinality(n)` (1.5.6 has it too) |
| `dc::FlatData<T>(vec)` | `FlatVector::GetData<T>` | `FlatVector::GetDataMutable<T>` (`GetData` is const) |
| `dc::ReferenceIndex(ref)`, `dc::Children(expr)`, `dc::ConstantValue(constant)` | public `index` / `children` / `value` | `Index()`, `GetChildren[Mutable]()`, `GetValue()` |
| `dc::MutableExpressionChildren(op)` | public `BoundOperatorExpression::children` | `BoundOperatorExpression::GetChildrenMutable()`; used to construct filter expression trees in tests |
| `dc::MakeBoundScalarExpression(return_type, scalar_function, arguments)` | `BoundFunctionExpression(return_type, ScalarFunction, arguments, bind_data)` | `BoundFunctionExpression(BoundScalarFunction, arguments, bind_data)`; used to probe optional filter trees |
| `dc::WithAlias(type, alias)`, `dc::ParsedConstant(value)` | legacy aliases and constant parser nodes | typed aliases and `ConstantExpression::FromValue` |
| `dc::SetVarArgs(fn, type)` | `varargs` member | signature method |
| `dc::GetBindData(input)`, `dc::GetBindInfoInput` | get-bind-info callback receives bind data directly | callback receives an input wrapper with `bind_data`; the fallback accepts only the callback input type |
| `dc::MakeTableBindInfo(table)`, `dc::SetTableEntryGetter(fn, callback)` | `BindInfo(table)`; `get_table_entry` is a no-op | `BindInfo(ScanType::TABLE)` drops the table; `get_table_entry` identifies it, so register both callbacks together |
| `dc::NameFunctionsFromDescriptions(info)` | names every mutable overload by position when descriptions are present | names every signature overload by position; an empty description list is allowed, but a nonempty count mismatch throws |
| `dc::ResultValue(result, col, row)` | `result.GetValue` | `result.Collection().GetValue` |
| `dc::ResultTypes(result)` | public `types` | `GetTypes()` |
| `dc::MutableValidity(vector)` | `FlatVector::Validity` | `FlatVector::ValidityMutable` |
| `dc::AssignBindNames(out, names)`, `dc::BindNamedParameters` | binds string names and named-parameter map | binds identifier names and the current named-parameter map type |
| `dc::CopyOptions` | copy bind input option type | copy bind input option type, read from the current API shape |
| `dc::HasInitGlobal(fn)` | reads a function object's `init_global` | reads the pointer's `init_global` |
| `dc::DefaultSchemaName` | string return type of `GetDefaultSchema` | optional identifier return type of `GetDefaultSchema` |
| `dc::QualifiedCatalog`, `dc::QualifiedSchema`, `dc::QualifiedLocalName`, `dc::QualifiedWithSchema` | read and rebuild the public catalog/schema/name fields | read and rebuild the private qualified-name representation through its accessors |
| `dc::RegisterStorageExtension(config, name, extension)` | assign into `config.storage_extensions` | call the storage-extension registration method |
| `dc::NamedParameterTypes(function)` | read named-parameter types from the legacy map | read typed keyword arguments from the signature |
| `dc::FunctionArgumentTypes(function)` | read the public positional argument vector | read the signature's positional argument types |
| `dc::TypeInfoIdentity(type)` | identify shared logical-type extra info through the legacy API | identify it through the current type-info handle |

### Table filters (`duckdb_compat/filter.hpp`)

| Helper | 1.4 / 1.5 | 2.0 |
|---|---|---|
| `dc::FilterKind`, `dc::HAS_BLOOM_FILTER`, `dc::NEW_TABLE_FILTERS`, filter class aliases | current legacy enum and classes; Bloom is absent on 1.4 | `LEGACY_*` enum and classes; Bloom is `LEGACY_BLOOM_FILTER`; the shape flag selects the complete alias |
| `dc::FilterKind::STRUCT_EXTRACT` | legacy struct-extract filter kind | renamed legacy struct-extract filter kind |
| `dc::MatchesConstantFilter(filter, value)` | calls the filter's comparison method | compares values using the legacy filter's comparison type |
| `dc::CopyFilter(filter)` | calls the virtual copy method | copies the active expression filter |
| `dc::MakeComparisonExpression(type, left, right)` | constructs a bound comparison expression | uses the comparison function factory; useful for version-neutral filter tests |
| `dc::FilterCount`, `dc::ForEachFilter`, `dc::SetFilter` | public filter map | `TableFilterSet` iterators and setters; `ForEachFilter` rejects unsupported multi-column filters |
| `dc::FilterToString` | `TableFilter::ToString` | `ExpressionFilter::ToString` for expression filters; deserialization-only legacy filters have a type label because those classes have no `ToString` |
| `dc::MakeDynamicFilterData`, `dc::DynamicSnapshot` | nested legacy constant filter | direct comparison and constant state |
| `dc::IsOptionalExpressionFilter`, `dc::SimpleExpressionTree` | no expression filter; returns false or empty | recognizes optional wrappers and simple comparisons/conjunctions; drops optional AND children but never OR children |

**macOS on 2.0:** the bundled httplib reads the keychain whenever it is built
with OpenSSL, so an extension that uses `duckdb_httplib_openssl` must link
`-framework CoreFoundation -framework Security`, or linking fails with
`_SecTrustSettingsCopyCertificates` undefined:

```cmake
if(APPLE)
  foreach(t ${EXTENSION_NAME} ${LOADABLE_EXTENSION_NAME})
    target_link_libraries(${t} "-framework CoreFoundation" "-framework Security")
  endforeach()
endif()
```

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
3. Never name anything `IN`, `OUT`, `OPTIONAL`, `CALLBACK`, `WINAPI`, `PASCAL`, `APIENTRY`, `FAR`, `NEAR`, `CONST`, `min`, `max`, `ERROR` or
   `DELETE`: `<windows.h>` defines them as macros. CI probes with MSVC and the
   real `<windows.h>`.
4. Make every member lookup *dependent*: reach it through a template
   parameter. A name looked up in a concrete class, or a type that only some
   versions have (`Identifier`), is resolved when the template is defined, and
   SFINAE cannot discard it. Do not use `__has_include` for DuckDB headers:
   a system installation can make it true for the wrong source tree.

## Tests

- `test/test_dispatch.cpp` checks the dispatch with fake old, new and
  both-at-once types. It needs no DuckDB:
  `cmake -S . -B build && cmake --build build && ctest --test-dir build`.
- `test/probe.cpp` instantiates every helper against the real headers of each
  DuckDB line. CI runs it for 1.4.5, 1.5.6 and the 2.0 commit that
  community-extensions builds against. Locally:
  `g++ -std=c++17 -fsyntax-only -Iinclude -I<duckdb>/src/include test/probe.cpp`.
- The table-filter probe uses the same include order:
  `g++ -std=c++17 -fsyntax-only -Iinclude -I<duckdb>/src/include test/filter_probe.cpp`.
