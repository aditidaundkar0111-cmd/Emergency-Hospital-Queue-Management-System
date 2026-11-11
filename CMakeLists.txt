cmake_minimum_required(VERSION 3.12)
project(EmergencyHospitalPatientQueueOptimizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB HOSPITAL_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(hospital_core ${HOSPITAL_SOURCES})
target_include_directories(hospital_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_executable(emergency_hospital main.cpp)
target_link_libraries(emergency_hospital PRIVATE hospital_core)

add_executable(emergency_hospital_web web_server_main.cpp web_server/WebServer.cpp)
target_include_directories(emergency_hospital_web PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_link_libraries(emergency_hospital_web PRIVATE hospital_core)
target_compile_definitions(emergency_hospital_web PRIVATE CPPHTTPLIB_THREAD_POOL_COUNT=-1)
if(WIN32)
    target_compile_definitions(emergency_hospital_web PRIVATE _WIN32_WINNT=0x0A00)
    target_link_libraries(emergency_hospital_web PRIVATE ws2_32)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(emergency_hospital_tests tests/test_main.cpp)
    target_link_libraries(emergency_hospital_tests PRIVATE hospital_core)
    add_test(NAME hospital_regression_tests COMMAND emergency_hospital_tests)
    set_tests_properties(hospital_regression_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles("#include <thread>\nint main() { std::thread t([]{}); t.join(); }"
        HOSPITAL_COMPILER_HAS_STD_THREAD)
    if(HOSPITAL_COMPILER_HAS_STD_THREAD)
        find_package(Threads REQUIRED)
        add_executable(emergency_hospital_web_tests tests/web_api_test.cpp web_server/WebServer.cpp)
        target_include_directories(emergency_hospital_web_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
        target_link_libraries(emergency_hospital_web_tests PRIVATE hospital_core Threads::Threads)
        target_compile_definitions(emergency_hospital_web_tests PRIVATE
            CPPHTTPLIB_THREAD_POOL_COUNT=-1)
        if(WIN32)
            target_compile_definitions(emergency_hospital_web_tests PRIVATE _WIN32_WINNT=0x0A00)
            target_link_libraries(emergency_hospital_web_tests PRIVATE ws2_32)
        endif()
        add_test(NAME hospital_web_api_tests COMMAND emergency_hospital_web_tests)
        set_tests_properties(hospital_web_api_tests PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    else()
        message(STATUS "Skipping the threaded HTTP API test harness; compiler lacks std::thread")
    endif()
endif()
