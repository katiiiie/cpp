cmake_minimum_required(VERSION 3.14)
project(language_teacher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# nlohmann/json для чтения config.json/prompt.json и формирования тела запроса
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
find_package(nlohmann_json 3.11.3 QUIET)

if(nlohmann_json_FOUND)
    message(STATUS "Found system nlohmann_json")
endif()

# OpenSSL для TLS
find_package(OpenSSL REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

add_executable(language_teacher
    src/AiAgent.cpp
    src/LanguageTeacher.cpp
    src/main_language.cpp
)

target_include_directories(language_teacher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(language_teacher PRIVATE ${SQLITE3_INCLUDE_DIRS})

target_link_libraries(language_teacher
    PRIVATE
      nlohmann_json::nlohmann_json
      OpenSSL::SSL
      OpenSSL::Crypto
	  ${SQLITE3_LIBRARIES}
)
