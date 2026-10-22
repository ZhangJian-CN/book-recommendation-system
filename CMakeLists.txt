cmake_minimum_required(VERSION 3.19)
project(BookRecommendation LANGUAGES CXX)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)

qt_standard_project_setup()

# 集成讲解：业务与算法集中编译进 bookcore，主程序及测试使用同一份实现。
add_library(bookcore STATIC
    bookrepository.cpp bookrepository.h
    bookindex.cpp bookindex.h
    booksearch.cpp booksearch.h
    csvutil.cpp csvutil.h
    librarystore.cpp librarystore.h
    recommendation.cpp recommendation.h
)
target_link_libraries(bookcore PUBLIC Qt::Core)
target_compile_features(bookcore PUBLIC cxx_std_17)

# 软件演示选择这个主程序目标；下面的 *_smoke 各自有 main，是独立验证入口。
qt_add_executable(BookRecommendation
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow_ui.cpp
    mainwindow.h
    mainwindow.ui
    seed.qrc
    ui.qrc
)

qt_add_translations(
    TARGETS BookRecommendation
    TS_FILES BookRecommendation_zh_CN.ts
)

target_link_libraries(BookRecommendation
    PRIVATE
        Qt::Core
        Qt::Widgets
        bookcore
)

add_executable(bookrepository_smoke bookrepository_smoke.cpp)
target_link_libraries(bookrepository_smoke PRIVATE bookcore)
enable_testing()
add_test(NAME bookrepository_smoke COMMAND bookrepository_smoke)

add_executable(mainwindow_smoke mainwindow_smoke.cpp mainwindow.cpp mainwindow_ui.cpp
    mainwindow.h mainwindow.ui ui.qrc)
target_link_libraries(mainwindow_smoke PRIVATE Qt::Widgets bookcore)
target_compile_definitions(mainwindow_smoke PRIVATE
    SAMPLE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    SAMPLE_GOODBOOKS_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/demo_data/goodbooks")
add_test(NAME mainwindow_smoke COMMAND mainwindow_smoke)
set_tests_properties(mainwindow_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(ui_polish_smoke ui_polish_smoke.cpp mainwindow.cpp mainwindow_ui.cpp
    mainwindow.h mainwindow.ui ui.qrc)
target_link_libraries(ui_polish_smoke PRIVATE Qt::Widgets bookcore)
target_compile_definitions(ui_polish_smoke PRIVATE
    SAMPLE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    SAMPLE_COLD_START_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/demo_data/cold_start"
    SAMPLE_GOODBOOKS_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/demo_data/goodbooks")
add_test(NAME ui_polish_smoke COMMAND ui_polish_smoke)
configure_file(ui_screen_1280x720.json ui_screen_1280x720.json COPYONLY)
set_tests_properties(ui_polish_smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen:configfile=ui_screen_1280x720.json;UI_SCREENSHOT_DIR=;SPIN_SCREENSHOT_DIR=")

add_executable(selection_toggle_smoke selection_toggle_smoke.cpp mainwindow.cpp mainwindow_ui.cpp
    mainwindow.h mainwindow.ui ui.qrc)
target_link_libraries(selection_toggle_smoke PRIVATE Qt::Widgets bookcore)
target_compile_definitions(selection_toggle_smoke PRIVATE
    SAMPLE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    SAMPLE_GOODBOOKS_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/demo_data/goodbooks")
add_test(NAME selection_toggle_smoke COMMAND selection_toggle_smoke)
set_tests_properties(selection_toggle_smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen:configfile=ui_screen_1280x720.json;TOGGLE_SCREENSHOT_DIR=")

add_executable(hash_smoke hash_smoke.cpp)
target_link_libraries(hash_smoke PRIVATE bookcore)
target_compile_definitions(hash_smoke PRIVATE SAMPLE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_test(NAME hash_smoke COMMAND hash_smoke)

add_executable(core_smoke core_smoke.cpp)
target_link_libraries(core_smoke PRIVATE bookcore)
target_compile_definitions(core_smoke PRIVATE SAMPLE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_test(NAME core_smoke COMMAND core_smoke)

add_executable(cold_start_smoke cold_start_smoke.cpp)
target_link_libraries(cold_start_smoke PRIVATE bookcore)
target_compile_definitions(cold_start_smoke PRIVATE
    SAMPLE_COLD_START_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/demo_data/cold_start")
add_test(NAME cold_start_smoke COMMAND cold_start_smoke)

add_executable(store_lock_smoke store_lock_smoke.cpp seed.qrc)
target_link_libraries(store_lock_smoke PRIVATE bookcore)
target_compile_definitions(store_lock_smoke PRIVATE SAMPLE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_dependencies(store_lock_smoke BookRecommendation)
add_test(NAME store_lock_smoke COMMAND store_lock_smoke)
set_tests_properties(store_lock_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(borrow_smoke borrow_smoke.cpp)
target_link_libraries(borrow_smoke PRIVATE bookcore)
target_compile_definitions(borrow_smoke PRIVATE SAMPLE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_test(NAME borrow_smoke COMMAND borrow_smoke)

add_executable(large_demo_smoke large_demo_smoke.cpp)
target_link_libraries(large_demo_smoke PRIVATE bookcore)
target_compile_definitions(large_demo_smoke PRIVATE
    SAMPLE_LARGE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/demo_data/large"
    SAMPLE_GOODBOOKS_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/demo_data/goodbooks")
add_test(NAME large_demo_smoke COMMAND large_demo_smoke)

# 性能对照单独运行，不加入 CTest 的功能正确性测试计数。
add_executable(search_benchmark search_benchmark.cpp)
target_link_libraries(search_benchmark PRIVATE bookcore)

# 效果/分段耗时测量独立运行，不计入功能 CTest；生产推荐实现保持单一来源。
add_executable(recommendation_evaluation recommendation_evaluation.cpp mainwindow.cpp mainwindow_ui.cpp
    mainwindow.h mainwindow.ui ui.qrc)
target_link_libraries(recommendation_evaluation PRIVATE Qt::Widgets bookcore)
target_compile_definitions(recommendation_evaluation PRIVATE EVALUATION_BUILD_TYPE="${CMAKE_BUILD_TYPE}")

install(TARGETS BookRecommendation
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET BookRecommendation
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
