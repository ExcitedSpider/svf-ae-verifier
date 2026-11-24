function(compile_case)
    file(MAKE_DIRECTORY "${work}")
    get_filename_component(case_dir "${case}" DIRECTORY)
    get_filename_component(case_name "${case}" NAME)
    execute_process(COMMAND "${clang}" -g -S -emit-llvm -Xclang -disable-O0-optnone
        -fno-discard-value-names "-I${inc}" "${case_name}" -o "${work}/raw.ll"
        WORKING_DIRECTORY "${case_dir}" COMMAND_ERROR_IS_FATAL ANY)
    execute_process(COMMAND "${opt}" -p=mem2reg -S "${work}/raw.ll" -o "${work}/input.ll"
        COMMAND_ERROR_IS_FATAL ANY)
endfunction()

function(run_verifier expected_code expected_status)
    execute_process(COMMAND "${bash}" -c "ulimit -v 4194304 && exec \"\$@\"" _
        "${timeout}" 120 "${verifier}" ${ARGN}
        RESULT_VARIABLE code OUTPUT_VARIABLE stdout ERROR_VARIABLE diagnostics)
    if(NOT code STREQUAL "${expected_code}")
        message(FATAL_ERROR "Expected exit ${expected_code}, got ${code}\n${diagnostics}\n${stdout}")
    endif()
    if(DEFINED output_file)
        if(NOT stdout STREQUAL "")
            message(FATAL_ERROR "Unexpected stdout with -o: ${stdout}")
        endif()
        file(READ "${output_file}" stdout)
    endif()
    string(JSON status GET "${stdout}" status)
    if(NOT status STREQUAL "${expected_status}")
        message(FATAL_ERROR "Expected status ${expected_status}\n${stdout}")
    endif()
    set(report "${stdout}" PARENT_SCOPE)
endfunction()
