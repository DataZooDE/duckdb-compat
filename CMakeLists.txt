cmake_minimum_required(VERSION 3.12)
project(duckdb_compat LANGUAGES CXX)

# Header-only. Consumers: add_subdirectory(duckdb-compat) and link
# duckdb_compat, or just add include/ to the include path.
add_library(duckdb_compat INTERFACE)
target_include_directories(duckdb_compat INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(duckdb_compat INTERFACE cxx_std_17)

# The pure dispatch tests need no DuckDB. Only built standalone.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(test_dispatch test/test_dispatch.cpp)
  target_link_libraries(test_dispatch PRIVATE duckdb_compat)
  add_test(NAME dispatch COMMAND test_dispatch)
endif()
