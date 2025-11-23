cmake_minimum_required(VERSION 3.28)

project(yodau VERSION 0.1.0 LANGUAGES CXX)

option(KDE "Enable KDE Frameworks and KDEGames integration" OFF)
option(BUILD_TESTS "Build unit tests (GTest + QtTest)" ON)
option(ENABLE_COVERAGE "Enable coverage instrumentation for gcc/clang" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer for libyodau_unittests" OFF)
option(BUILD_GUI "Build Qt-based GUI application and Qt-based tests" ON)

if (KDE AND NOT BUILD_GUI)
    message(FATAL_ERROR "KDE integration requires BUILD_GUI to be ON")
endif ()

if (ENABLE_COVERAGE AND NOT BUILD_TESTS)
    set(BUILD_TESTS ON CACHE BOOL "Build unit tests (forced by ENABLE_COVERAGE)" FORCE)
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(
            -Og
            -g
            -fno-omit-frame-pointer
            -Werror
            -Wall
            -Wextra
            -Wpedantic
            -Wcast-align
            -Wcast-qual
            -Wconversion
            -Wctor-dtor-privacy
            -Wenum-compare
            -Wfloat-equal
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Woverloaded-virtual
            -Wredundant-decls
            -Wsign-conversion
            -Wsign-promo
    )

    if (ENABLE_COVERAGE)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            add_compile_options(--coverage)
            add_link_options(--coverage)
        elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
            add_link_options(-fprofile-instr-generate -fcoverage-mapping)
        endif ()
    endif ()
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " -O3 -march=native -mtune=native -DNDEBUG")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " -s")
endif ()

if (NOT ANDROID AND BUILD_TESTS)
    enable_testing()
endif ()

if (KDE)
    set(kf6_min_version "6.0.0")
    find_package(ECM 1.0.0 REQUIRED NO_MODULE)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
    include(KDEInstallDirs)
    include(KDECMakeSettings)
    include(KDECompilerSettings NO_POLICY_SCOPE)
else ()
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
    include(GNUInstallDirs)
endif ()

include(FeatureSummary)

if (BUILD_GUI)
    set(QT_MAJOR_VERSION 6)
    set(QT_DEFAULT_MAJOR_VERSION 6)
    set(qt_min_version "6.0.0")

    find_package(Qt6 ${qt_min_version} CONFIG REQUIRED
            COMPONENTS
            Core
            Widgets
            Test
            Multimedia
            MultimediaWidgets
    )

    if (KDE)
        find_package(KF6 ${kf6_min_version} REQUIRED COMPONENTS
                CoreAddons
                I18n
                XmlGui
                ConfigWidgets
                WidgetsAddons
                KIO
        )
        find_package(KDEGames6 REQUIRED)
    endif ()

    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
endif ()

set(libyodau_headers
        backend/include/stream_manager.hpp
        backend/include/stream.hpp
        backend/include/geometry.hpp
        backend/include/frame.hpp
        backend/include/event.hpp

        backend/include/opencv_client.hpp
)

set(libyodau_sources
        backend/src/stream_manager.cpp
        backend/src/stream.cpp
        backend/src/geometry.cpp

        backend/src/opencv_client.cpp
)

find_package(OpenCV QUIET)

add_library(libyodau STATIC
        ${libyodau_sources}
        ${libyodau_headers}
)

if (OpenCV_FOUND)
    target_link_libraries(libyodau PUBLIC ${OpenCV_LIBS})
    target_include_directories(libyodau PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_compile_definitions(libyodau PUBLIC YODAU_OPENCV)
endif ()

set_target_properties(libyodau PROPERTIES
        OUTPUT_NAME yodau
)

target_include_directories(libyodau
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/backend/include
)

target_compile_features(libyodau PUBLIC cxx_std_23)

add_executable(yodau_cli
        backend/src/main.cpp
        backend/include/cli_client.hpp
        backend/src/cli_client.cpp
)

target_include_directories(yodau_cli
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/backend/include
)

target_link_libraries(yodau_cli
        PRIVATE
        libyodau
)
if (BUILD_GUI)
    set(yodau_frontend_headers
            frontend/include/main_window.hpp

            frontend/include/helpers/str_label.hpp
            frontend/include/helpers/icon_loader.hpp
            frontend/include/helpers/controller.hpp

            frontend/include/widgets/board.hpp
            frontend/include/widgets/grid_view.hpp
            frontend/include/widgets/settings_panel.hpp
            frontend/include/widgets/stream_cell.hpp
    )

    set(yodau_frontend_sources
            frontend/src/main_window.cpp

            frontend/src/helpers/icon_loader.cpp
            frontend/src/helpers/controller.cpp

            frontend/src/widgets/board.cpp
            frontend/src/widgets/grid_view.cpp
            frontend/src/widgets/settings_panel.cpp
            frontend/src/widgets/stream_cell.cpp
    )

    set(yodau_qt_libs
            Qt6::Core
            Qt6::Widgets
            Qt6::Multimedia
            Qt6::MultimediaWidgets
    )

    if (KDE)
        set(yodau_kde_libs
                KF6::CoreAddons
                KF6::I18n
                KF6::XmlGui
                KF6::ConfigWidgets
                KF6::WidgetsAddons
                KF6::KIOCore
                KDEGames6
        )
    endif ()

    qt_add_executable(yodau
            MANUAL_FINALIZATION
            frontend/src/main.cpp
            ${yodau_frontend_sources}
            ${yodau_frontend_headers}
    )

    target_include_directories(yodau
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/backend/include
            ${CMAKE_CURRENT_SOURCE_DIR}/frontend/include
    )

    target_link_libraries(yodau
            PRIVATE
            libyodau
            ${yodau_qt_libs}
            $<$<BOOL:${KDE}>:${yodau_kde_libs}>
    )

    target_compile_definitions(yodau
            PRIVATE
            $<$<BOOL:${KDE}>:YODAU_KDE>
    )

    if (NOT ANDROID)
        target_precompile_headers(yodau PRIVATE
                <QtCore>
                <QtWidgets>
                <vector>
                <tuple>
                <memory>
                <optional>
                <string>
        )
    endif ()

    if (ANDROID)
        set_property(TARGET yodau PROPERTY QT_ANDROID_PACKAGE_NAME org.example.yodau)
    endif ()

    qt_finalize_executable(yodau)
endif ()

if (KDE)
    set(install_args ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
else ()
    set(install_args
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif ()

if (NOT ANDROID)
    set(install_targets
            yodau_cli
            libyodau
    )
    if (BUILD_GUI)
        list(APPEND install_targets yodau)
    endif ()

    install(TARGETS
            ${install_targets}
            ${install_args}
    )
endif ()

if (NOT ANDROID AND BUILD_TESTS)
    find_package(GTest QUIET)

    if (GTest_FOUND)
        set(libyodau_test_sources
                backend/tests/stream_manager_tests.cpp
        )

        add_executable(libyodau_unittests
                ${libyodau_test_sources}
        )

        target_include_directories(libyodau_unittests
                PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/backend/include
                ${CMAKE_CURRENT_SOURCE_DIR}/backend/tests/include
        )

        target_link_libraries(libyodau_unittests
                PRIVATE
                libyodau
                GTest::gtest_main
        )

        if (ENABLE_ASAN)
            target_compile_options(libyodau_unittests PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
            target_link_options(libyodau_unittests PRIVATE -fsanitize=address)
        endif ()

        add_test(
                NAME libyodau_unittests
                COMMAND libyodau_unittests
        )
    else ()
        message(WARNING "GTest not found; libyodau_unittests will not be built")
    endif ()
endif ()

if (BUILD_GUI AND NOT ANDROID AND BUILD_TESTS)
    set(yodau_qttest_headers
            frontend/tests/include/main_window_tests.hpp
    )

    set(yodau_qttest_sources
            frontend/tests/main.cpp
            frontend/tests/main_window_tests.cpp
    )

    qt_add_executable(yodau_unittests
            ${yodau_qttest_sources}
            ${yodau_qttest_headers}
            ${yodau_frontend_sources}
            ${yodau_frontend_headers}
    )

    target_include_directories(yodau_unittests
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/backend/include
            ${CMAKE_CURRENT_SOURCE_DIR}/frontend/include
            ${CMAKE_CURRENT_SOURCE_DIR}/frontend/tests/include
    )

    target_link_libraries(yodau_unittests
            PRIVATE
            libyodau
            ${yodau_qt_libs}
            Qt6::Test
            $<$<BOOL:${KDE}>:${yodau_kde_libs}>
    )

    add_test(
            NAME yodau_unittests
            COMMAND yodau_unittests
    )
endif ()

if (ENABLE_COVERAGE AND NOT ANDROID)
    set(COVERAGE_DIR "${CMAKE_BINARY_DIR}/../doc/cov")
    set(COVERAGE_IGNORE_REGEX ".*/tests/.*|.*/include/.*|.*/usr/lib/.*|.*/[^/]*_autogen/.*|.*\\.moc|.*/moc_.*")

    if (TARGET libyodau_unittests)
        add_custom_command(
                TARGET libyodau_unittests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}
        )

        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            find_program(LLVM_PROFDATA llvm-profdata)
            find_program(LLVM_COV llvm-cov)

            if (LLVM_PROFDATA AND LLVM_COV)
                set(PROFRAW_FILE "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.profraw")
                set(PROFDATA_FILE "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.profdata")

                add_custom_target(coverage
                        COMMENT "Generating coverage with llvm-cov"
                        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                        COMMAND ${CMAKE_COMMAND} --build . --target libyodau_unittests
                        COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=${PROFRAW_FILE}
                        $<TARGET_FILE:libyodau_unittests>
                        COMMAND ${LLVM_PROFDATA} merge -sparse
                        -o ${PROFDATA_FILE} ${PROFRAW_FILE}
                        COMMAND ${LLVM_COV} show $<TARGET_FILE:libyodau_unittests>
                        -instr-profile=${PROFDATA_FILE}
                        -format=html
                        -output-dir=${COVERAGE_DIR}
                        -ignore-filename-regex=${COVERAGE_IGNORE_REGEX}
                        -show-branches=count
                        -Xdemangler=c++filt
                        -show-line-counts
                        -show-regions
                        -show-instantiations
                        -show-expansions
                        -use-color
                        -coverage-watermark=90,60
                        COMMAND ${LLVM_COV} export $<TARGET_FILE:libyodau_unittests>
                        -instr-profile=${PROFDATA_FILE}
                        -format=lcov
                        -ignore-filename-regex=${COVERAGE_IGNORE_REGEX}
                        > ${CMAKE_BINARY_DIR}/coverage.info
                        DEPENDS libyodau_unittests
                        VERBATIM
                )
            else ()
                message(WARNING
                        "ENABLE_COVERAGE=ON but llvm-profdata/llvm-cov not found; "
                        "'coverage' target will be unavailable."
                )
            endif ()
        elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            find_program(GCOVR gcovr)
            find_program(GCOV_EXECUTABLE gcov-14)

            if (GCOVR)
                set(coverage_deps libyodau_unittests)
                set(coverage_runs)

                if (TARGET libyodau_unittests)
                    list(APPEND coverage_runs COMMAND $<TARGET_FILE:libyodau_unittests>)
                endif ()

                if (TARGET yodau_unittests)
                    list(APPEND coverage_deps yodau_unittests)
                    list(APPEND coverage_runs COMMAND $<TARGET_FILE:yodau_unittests>)
                endif ()

                add_custom_target(coverage
                        COMMENT "Generating coverage with gcovr"
                        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                        COMMAND ${CMAKE_COMMAND} --build . --target ${coverage_deps}
                        ${coverage_runs}
                        COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}
                        COMMAND ${GCOVR}
                        --gcov-executable ${GCOV_EXECUTABLE}
                        --root ${CMAKE_SOURCE_DIR}
                        --exclude "${COVERAGE_IGNORE_REGEX}"
                        --html --html-details
                        -o ${COVERAGE_DIR}/index.html
                        DEPENDS ${coverage_deps}
                        VERBATIM
                )
            else ()
                message(WARNING
                        "ENABLE_COVERAGE=ON but gcovr not found; "
                        "'coverage' target will be unavailable."
                )
            endif ()
        endif ()
    else ()
        message(STATUS
                "ENABLE_COVERAGE=ON but tests are not built; 'coverage' target will be a no-op."
        )
    endif ()
endif ()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
