cmake_minimum_required(VERSION 3.19)
project(BookRecommendation LANGUAGES CXX)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)

qt_standard_project_setup()

qt_add_executable(BookRecommendation
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    bookrepository.cpp
    bookrepository.h
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
)

add_executable(bookrepository_smoke bookrepository_smoke.cpp bookrepository.cpp)
target_link_libraries(bookrepository_smoke PRIVATE Qt::Core)
enable_testing()
add_test(NAME bookrepository_smoke COMMAND bookrepository_smoke)

add_executable(mainwindow_smoke mainwindow_smoke.cpp mainwindow.cpp mainwindow.h
    mainwindow.ui bookrepository.cpp bookrepository.h)
target_link_libraries(mainwindow_smoke PRIVATE Qt::Core Qt::Widgets)
add_test(NAME mainwindow_smoke COMMAND mainwindow_smoke)
set_tests_properties(mainwindow_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

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
