cmake_minimum_required(VERSION 3.14)
project(AiAgentExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ищем библиотеки
find_package(nlohmann_json 3.11.3 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann/json not found locally, downloading...")
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Добавляем CPR
include(FetchContent)
FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.10.5
)
FetchContent_MakeAvailable(cpr)

find_package(OpenSSL REQUIRED)

# Простой способ найти SQLite3
find_library(SQLITE3_LIBRARY sqlite3)
find_path(SQLITE3_INCLUDE_DIR sqlite3.h)

if(NOT SQLITE3_LIBRARY)
    message(FATAL_ERROR "SQLite3 library not found")
endif()

add_executable(ai_agent
    src/AiAgent.cpp
    src/main.cpp
)

target_include_directories(ai_agent 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SQLITE3_INCLUDE_DIR}
)

target_link_libraries(ai_agent
    PRIVATE
        nlohmann_json::nlohmann_json
        cpr::cpr
        OpenSSL::SSL
        OpenSSL::Crypto
        ${SQLITE3_LIBRARY}
)
