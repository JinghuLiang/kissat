cmake_minimum_required(VERSION 3.20)
project(kissat C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckLibraryExists)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(KISSAT_BUILD_TISSAT "Build tissat test runner" OFF)
option(KISSAT_BUILD_KITTEN "Build standalone kitten binary" OFF)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" KISSAT_VERSION)
string(STRIP "${KISSAT_VERSION}" KISSAT_VERSION)

find_package(Git QUIET)
set(KISSAT_GIT_ID "unknown")
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE KISSAT_GIT_ID
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
endif()

string(TIMESTAMP KISSAT_BUILD_TIME "%a %b %d %H:%M:%S %Z %Y")
set(KISSAT_COMPILER_DESC "${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
set(KISSAT_BUILD_DESC "${KISSAT_BUILD_TIME} ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/build.h"
"#define VERSION \"${KISSAT_VERSION}\"\n"
"#define COMPILER \"${KISSAT_COMPILER_DESC}\"\n"
"#define ID \"${KISSAT_GIT_ID}\"\n"
"#define BUILD \"${KISSAT_BUILD_DESC}\"\n"
"#define DIR \"${CMAKE_CURRENT_BINARY_DIR}\"\n"
)

file(GLOB_RECURSE KISSAT_ALL_SRC CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

set(KISSAT_APP_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/util/build/application.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/util/misc/handle.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/util/io/parse.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/util/data-structures/witness.c"
)

set(KISSAT_MAIN_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/core/main/main.c")

set(KISSAT_LIB_SRC ${KISSAT_ALL_SRC})
list(REMOVE_ITEM KISSAT_LIB_SRC "${KISSAT_MAIN_SRC}" ${KISSAT_APP_SRC})

add_library(kissat-lib STATIC ${KISSAT_LIB_SRC})
target_include_directories(kissat-lib
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}"
)

check_library_exists(m sqrt "" KISSAT_HAVE_LIBM)
if(KISSAT_HAVE_LIBM)
  target_link_libraries(kissat-lib PUBLIC m)
endif()

add_executable(kissat "${KISSAT_MAIN_SRC}" ${KISSAT_APP_SRC})
target_link_libraries(kissat PRIVATE kissat-lib)

if(KISSAT_BUILD_TISSAT)
  file(GLOB KISSAT_TEST_SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.c"
  )
  list(REMOVE_ITEM KISSAT_TEST_SRC "${CMAKE_CURRENT_SOURCE_DIR}/test/test.c")

  add_executable(tissat
    "${CMAKE_CURRENT_SOURCE_DIR}/test/test.c"
    ${KISSAT_TEST_SRC}
    ${KISSAT_APP_SRC}
  )
  target_include_directories(tissat PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
  target_link_libraries(tissat PRIVATE kissat-lib)
endif()

if(KISSAT_BUILD_KITTEN)
  add_executable(kitten "${CMAKE_CURRENT_SOURCE_DIR}/src/kitten/kitten.c")
  target_include_directories(kitten PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}"
  )
  target_compile_definitions(kitten PRIVATE STAND_ALONE_KITTEN)
  if(KISSAT_HAVE_LIBM)
    target_link_libraries(kitten PRIVATE m)
  endif()
endif()
