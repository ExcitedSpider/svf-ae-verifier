cmake_minimum_required(VERSION 3.20)
# The documented example must be valid, and every fixture in schema/invalid
# must be rejected on its own, so a validator that accepts everything fails.
execute_process(COMMAND "${python}" "${validator}" "${schema}" --markdown "${docs}"
    RESULT_VARIABLE code ERROR_VARIABLE errors)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "docs/report.md example does not match the schema\n${errors}")
endif()

file(GLOB invalid "${fixtures}/invalid/*.json")
list(LENGTH invalid count)
if(count EQUAL 0)
    message(FATAL_ERROR "No invalid fixtures under ${fixtures}/invalid")
endif()
foreach(report IN LISTS invalid)
    execute_process(COMMAND "${python}" "${validator}" "${schema}" "${report}"
        RESULT_VARIABLE code OUTPUT_QUIET ERROR_QUIET)
    if(NOT code EQUAL 1)
        message(FATAL_ERROR "Expected the schema to reject ${report} (exit 1), got ${code}")
    endif()
endforeach()
