cmake_minimum_required(VERSION 3.24)
set(CMAKE_RC_COMPILER "C:/Qt/Tools/mingw1310_64/bin/windres.exe" CACHE FILEPATH "Resource compiler" FORCE)
project(WholesalePOS VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(POS_BUILD_TESTS "Build business-logic tests" ON)
option(POS_ENABLE_FBR "Include the isolated FBR queue module" OFF)
option(POS_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Gui PrintSupport Svg)

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
    src/core/excel_export_service.cpp
    src/core/security_service.cpp
    src/core/suspended_sale_service.cpp
    src/core/notification_service.cpp
    src/core/return_service.cpp
    src/core/database.cpp
    src/core/migrations.cpp
    src/core/inventory_service.cpp
    src/core/barcode_service.cpp
    src/core/pos_service.cpp
    src/core/backup_service.cpp
    src/core/thermal_print_service.cpp
    src/core/seed_service.cpp
    src/core/data_change_bus.cpp
    src/core/commission_service.cpp
    src/core/bundle_service.cpp
)
target_include_directories(pos_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(pos_core PUBLIC pos_sqlite Qt6::Core Qt6::Gui)
target_compile_definitions(pos_core PUBLIC $<$<BOOL:${POS_ENABLE_FBR}>:POS_ENABLE_FBR=1>)
if(POS_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(pos_core PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(pos_core PUBLIC -fsanitize=address,undefined)
endif()

set(UI_PAGE_SOURCES
    src/ui/pages/dashboard_page.cpp
    src/ui/pages/dashboard_page.h
    src/ui/pages/inventory_page.cpp
    src/ui/pages/inventory_page.h
    src/ui/pages/sales_pos_page.cpp
    src/ui/pages/sales_pos_page.h
    src/ui/pages/purchases_page.cpp
    src/ui/pages/purchases_page.h
    src/ui/pages/customers_page.cpp
    src/ui/pages/customers_page.h
    src/ui/pages/suppliers_page.cpp
    src/ui/pages/suppliers_page.h
    src/ui/pages/cash_management_page.cpp
    src/ui/pages/cash_management_page.h
    src/ui/pages/cheques_page.cpp
    src/ui/pages/cheques_page.h
    src/ui/pages/reports_page.cpp
    src/ui/pages/reports_page.h
    src/ui/pages/courses_page.cpp
    src/ui/pages/courses_page.h
    src/ui/pages/audit_log_page.cpp
    src/ui/pages/audit_log_page.h
    src/ui/pages/settings_page.cpp
    src/ui/pages/settings_page.h
    src/ui/pages/backup_restore_page.cpp
    src/ui/pages/backup_restore_page.h
    src/ui/pages/sales_trend_graph.cpp
    src/ui/pages/sales_trend_graph.h
    src/ui/pages/page_helper.h
)

add_executable(invento WIN32
    src/main.cpp
    src/ui/main_window.cpp
    src/ui/main_window.h
    src/ui/resources.qrc
    ${UI_PAGE_SOURCES}
)
target_link_libraries(invento PRIVATE pos_core Qt6::Widgets Qt6::Sql Qt6::PrintSupport Qt6::Svg)

if(WIN32)
  target_sources(invento PRIVATE src/ui/app_icon.rc)
endif()

if(MSVC)
  target_compile_options(pos_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(pos_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(POS_BUILD_TESTS)
  include(CTest)
  if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(pos_core_tests tests/pos_service_test.cpp)
    target_link_libraries(pos_core_tests PRIVATE pos_core Qt6::Test)
    if(POS_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options(pos_core_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(pos_core_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME pos_core_tests COMMAND pos_core_tests)
    add_executable(pos_ui_smoke_tests tests/ui_smoke_test.cpp src/ui/main_window.cpp src/ui/main_window.h src/ui/resources.qrc ${UI_PAGE_SOURCES})
    target_link_libraries(pos_ui_smoke_tests PRIVATE pos_core Qt6::Widgets Qt6::Sql Qt6::PrintSupport Qt6::Svg Qt6::Test)
    add_test(NAME pos_ui_smoke_tests COMMAND pos_ui_smoke_tests)
    set_tests_properties(pos_ui_smoke_tests PROPERTIES
      ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Widgets>;$ENV{PATH}")
  endif()
endif()
