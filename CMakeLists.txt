cmake_minimum_required(VERSION 3.25)
project(ProxyChecker VERSION 0.0.1 LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "ProxyChecker currently requires MSVC on Windows")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(ProxyChecker WIN32 src/main.cpp)
target_compile_definitions(ProxyChecker PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(ProxyChecker PRIVATE /W4 /WX /permissive-)
target_link_libraries(ProxyChecker PRIVATE user32 gdi32)

# Future components: WinHTTP transport, registry+DPAPI settings,
# monitor state machine, Healthchecks.io heartbeat, tray UI and unit tests.
