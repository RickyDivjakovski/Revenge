execute_process(COMMAND "${REVENGE}" target-plan "${FIXTURE}" RESULT_VARIABLE code OUTPUT_VARIABLE text ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT text MATCHES "Delay [Uu][Ss][Ee][Rr]32[.][Dd][Ll][Ll]!GetSystemMetrics")
 message(FATAL_ERROR "Delay import inspection failed: ${text} ${err}")
endif()
