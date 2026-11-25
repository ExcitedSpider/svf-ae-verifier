cmake_minimum_required(VERSION 3.20)
project(svf-ae-verifier VERSION 0.1.0 LANGUAGES C CXX)

find_package(SVF CONFIG REQUIRED)
set(SVF_COMMIT "" CACHE STRING "Source commit of the installed SVF")
if(EXISTS "${SVF_INSTALL_PREFIX}/SVF_COMMIT")
    file(STRINGS "${SVF_INSTALL_PREFIX}/SVF_COMMIT" svf_commit LIMIT_COUNT 1)
else()
    set(svf_commit "${SVF_COMMIT}")
endif()
if(NOT svf_commit)
    set(svf_commit unknown)
endif()
set(VERIFIER_GIT_COMMIT "" CACHE STRING "Source commit for a build without Git metadata")
set(verifier_commit "${VERIFIER_GIT_COMMIT}")
if(NOT verifier_commit)
    find_package(Git QUIET)
    if(Git_FOUND)
        execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE verifier_commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    endif()
endif()
if(NOT verifier_commit)
    set(verifier_commit unknown)
endif()
# Releases are versioned by Git tags; the release workflow passes the version in.
set(VERIFIER_RELEASE_VERSION "" CACHE STRING "Release version reported as tool_version (default: project version)")
set(verifier_version "${PROJECT_VERSION}")
if(VERIFIER_RELEASE_VERSION)
    if(NOT VERIFIER_RELEASE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
        message(FATAL_ERROR "VERIFIER_RELEASE_VERSION must be MAJOR.MINOR.PATCH, got ${VERIFIER_RELEASE_VERSION}")
    endif()
    set(verifier_version "${VERIFIER_RELEASE_VERSION}")
endif()

add_executable(svf-verifier src/main.cpp)
target_compile_features(svf-verifier PRIVATE cxx_std_17)
target_link_libraries(svf-verifier PRIVATE SVF::SvfLLVM)
target_compile_definitions(svf-verifier PRIVATE
    VERIFIER_VERSION="${verifier_version}" VERIFIER_COMMIT="${verifier_commit}"
    VERIFIER_SVF_VERSION="${SVF_VERSION}" VERIFIER_SVF_COMMIT="${svf_commit}"
    VERIFIER_LLVM_VERSION="${LLVM_PACKAGE_VERSION}"
    VERIFIER_EXTAPI_PATH="${SVF_INSTALL_EXTAPI_BC}")

include(CTest)
if(BUILD_TESTING)
    find_program(CLANG_EXECUTABLE clang HINTS "${LLVM_TOOLS_BINARY_DIR}" REQUIRED)
    find_program(OPT_EXECUTABLE opt HINTS "${LLVM_TOOLS_BINARY_DIR}" REQUIRED)
    find_program(BASH_EXECUTABLE bash REQUIRED)
    find_program(TIMEOUT_EXECUTABLE timeout REQUIRED)

    # Validate every test report against schema/report.schema.json when Python's
    # jsonschema is available. The container build requires it.
    option(VERIFIER_REQUIRE_SCHEMA_CHECK "Fail configuration if reports cannot be schema-checked" OFF)
    set(schema_python "")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import jsonschema"
            RESULT_VARIABLE jsonschema_missing OUTPUT_QUIET ERROR_QUIET)
        if(NOT jsonschema_missing)
            set(schema_python "${Python3_EXECUTABLE}")
        endif()
    endif()
    if(NOT schema_python)
        if(VERIFIER_REQUIRE_SCHEMA_CHECK)
            message(FATAL_ERROR "Schema checks need python3 with the jsonschema module")
        endif()
        message(WARNING "python3 with jsonschema not found: reports are not schema-checked")
    endif()
    set(schema_args "-Dpython=${schema_python}"
        "-Dvalidator=${CMAKE_CURRENT_SOURCE_DIR}/tests/validate-report.py"
        "-Dschema=${CMAKE_CURRENT_SOURCE_DIR}/schema/report.schema.json")
    if(schema_python)
        add_test(NAME schema.check COMMAND "${CMAKE_COMMAND}" ${schema_args}
            "-Ddocs=${CMAKE_CURRENT_SOURCE_DIR}/docs/report.md"
            "-Dfixtures=${CMAKE_CURRENT_SOURCE_DIR}/tests/schema"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-schema.cmake")
    endif()

    foreach(property IN ITEMS reach overflow)
        file(GLOB cases CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/${property}/*.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/${property}/*.ll")
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
                    ${schema_args}
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
