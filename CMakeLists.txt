cmake_minimum_required(VERSION 3.24)
project(WholesalePOS VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(POS_BUILD_TESTS "Build business-logic tests" ON)
option(POS_ENABLE_FBR "Include the isolated FBR queue module" OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Sql Gui PrintSupport)

# Vendored official SQLite amalgamation. The business layer never relies on Qt's
# SQLite plugin, keeping builds fully offline after the Qt toolchain is installed.
add_library(pos_sqlite STATIC third_party/sqlite/sqlite3.c)
target_include_directories(pos_sqlite PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite)
target_compile_definitions(pos_sqlite PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_JSON1
)

add_library(pos_core
    src/core/logger.cpp
    src/core/supplier_service.cpp
    src/core/purchase_service.cpp
    src/core/payment_service.cpp
    src/core/customer_service.cpp
    src/core/shift_service.cpp
    src/core/cheque_service.cpp
    src/core/report_service.cpp
    src/core/audit_service.cpp
    src/core/settings_service.cpp
    src/core/return_service.cpp
    src/core/database.cpp
    src/core/migrations.cpp
    src/core/inventory_service.cpp
    src/core/barcode_service.cpp
    src/core/pos_service.cpp
    src/core/backup_service.cpp
)
target_include_directories(pos_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(pos_core PUBLIC pos_sqlite Qt6::Core Qt6::Gui)
target_compile_definitions(pos_core PUBLIC $<$<BOOL:${POS_ENABLE_FBR}>:POS_ENABLE_FBR=1>)

add_executable(wholesale_pos
    src/main.cpp
    src/ui/main_window.cpp
    src/ui/main_window.h
)
target_link_libraries(wholesale_pos PRIVATE pos_core Qt6::Widgets Qt6::Sql Qt6::PrintSupport)

if(MSVC)
  target_compile_options(pos_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(pos_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(POS_BUILD_TESTS)
  include(CTest)
  if(BUILD_TESTING)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    add_executable(pos_core_tests tests/pos_service_test.cpp)
    target_link_libraries(pos_core_tests PRIVATE pos_core Qt6::Test)
    add_test(NAME pos_core_tests COMMAND pos_core_tests)
  endif()
endif()
