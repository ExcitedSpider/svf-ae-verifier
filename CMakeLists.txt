cmake_minimum_required(VERSION 3.20)
project(svf-ae-verifier VERSION 0.1.0 LANGUAGES C CXX)

set(SVF_COMMIT a8a0f5181748059c2df2281e5e4ada63ccda3832)
find_package(SVF 3.4 CONFIG REQUIRED)
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE verifier_commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(NOT verifier_commit)
    set(verifier_commit unknown)
endif()

add_executable(svf-verifier src/main.cpp)
target_compile_features(svf-verifier PRIVATE cxx_std_17)
target_link_libraries(svf-verifier PRIVATE SVF::SvfLLVM)
target_compile_definitions(svf-verifier PRIVATE
    VERIFIER_VERSION="${PROJECT_VERSION}" VERIFIER_COMMIT="${verifier_commit}"
    VERIFIER_SVF_VERSION="${SVF_VERSION}" VERIFIER_SVF_COMMIT="${SVF_COMMIT}"
    VERIFIER_EXTAPI_PATH="${SVF_INSTALL_EXTAPI_BC}")

include(CTest)
if(BUILD_TESTING)
    find_program(CLANG_EXECUTABLE clang HINTS "${LLVM_TOOLS_BINARY_DIR}" REQUIRED)
    find_program(OPT_EXECUTABLE opt HINTS "${LLVM_TOOLS_BINARY_DIR}" REQUIRED)
    find_program(BASH_EXECUTABLE bash REQUIRED)
    find_program(TIMEOUT_EXECUTABLE timeout REQUIRED)
    foreach(property IN ITEMS reach overflow)
        file(GLOB cases CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/${property}/*.c")
        foreach(case IN LISTS cases)
            get_filename_component(name "${case}" NAME_WE)
            foreach(mode IN ITEMS dense semi-sparse sparse)
                set(test_name "${property}.${mode}.${name}")
                add_test(NAME "${test_name}" COMMAND "${CMAKE_COMMAND}"
                    "-Dverifier=$<TARGET_FILE:svf-verifier>" "-Dcase=${case}"
                    "-Dproperty=${property}" "-Dmode=${mode}"
                    "-Dwork=${CMAKE_CURRENT_BINARY_DIR}/tests/${test_name}"
                    "-Dinc=${CMAKE_CURRENT_SOURCE_DIR}/include"
                    "-Dclang=${CLANG_EXECUTABLE}" "-Dopt=${OPT_EXECUTABLE}"
                    "-Dbash=${BASH_EXECUTABLE}" "-Dtimeout=${TIMEOUT_EXECUTABLE}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-report.cmake")
                set_tests_properties("${test_name}" PROPERTIES LABELS "${property};${mode}" TIMEOUT 130)
            endforeach()
        endforeach()
    endforeach()
    add_test(NAME cli.exit-status COMMAND "${CMAKE_COMMAND}"
        "-Dverifier=$<TARGET_FILE:svf-verifier>"
        "-Dwork=${CMAKE_CURRENT_BINARY_DIR}/tests/cli.exit-status"
        "-Dcase=${CMAKE_CURRENT_SOURCE_DIR}/tests/reach/conditions.c"
        "-Dinc=${CMAKE_CURRENT_SOURCE_DIR}/include"
        "-Dclang=${CLANG_EXECUTABLE}" "-Dopt=${OPT_EXECUTABLE}"
        "-Dbash=${BASH_EXECUTABLE}" "-Dtimeout=${TIMEOUT_EXECUTABLE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-cli.cmake")
    set_tests_properties(cli.exit-status PROPERTIES TIMEOUT 130)
endif()
