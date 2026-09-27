cmake_minimum_required(VERSION 3.19)
project(BookRecommendation LANGUAGES CXX)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)

qt_standard_project_setup()

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

qt_add_executable(BookRecommendation
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    seed.qrc
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

add_executable(mainwindow_smoke mainwindow_smoke.cpp mainwindow.cpp mainwindow.h
    mainwindow.ui)
target_link_libraries(mainwindow_smoke PRIVATE Qt::Widgets bookcore)
target_compile_definitions(mainwindow_smoke PRIVATE SAMPLE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_test(NAME mainwindow_smoke COMMAND mainwindow_smoke)
set_tests_properties(mainwindow_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(hash_smoke hash_smoke.cpp)
target_link_libraries(hash_smoke PRIVATE bookcore)
target_compile_definitions(hash_smoke PRIVATE SAMPLE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_test(NAME hash_smoke COMMAND hash_smoke)

add_executable(core_smoke core_smoke.cpp)
target_link_libraries(core_smoke PRIVATE bookcore)
target_compile_definitions(core_smoke PRIVATE SAMPLE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_test(NAME core_smoke COMMAND core_smoke)

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

add_executable(search_benchmark search_benchmark.cpp)
target_link_libraries(search_benchmark PRIVATE bookcore)

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
