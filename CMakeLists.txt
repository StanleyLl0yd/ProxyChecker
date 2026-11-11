cmake_minimum_required(VERSION 3.25)
project(ProxyChecker VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "ProxyChecker requires MSVC on Windows")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(ProxyChecker WIN32
    src/main.cpp
    src/settings.cpp
    src/network.cpp
    src/engine.cpp
)
target_compile_definitions(ProxyChecker PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(ProxyChecker PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(ProxyChecker PRIVATE
    user32 gdi32 comctl32 shell32 winhttp crypt32 advapi32 ole32)

include(CTest)
if(BUILD_TESTING)
    add_executable(proxychecker-state-tests tests/state_tests.cpp)
    target_compile_options(proxychecker-state-tests PRIVATE /W4 /WX /permissive-)
    add_test(NAME state-machine COMMAND proxychecker-state-tests)
endif()
