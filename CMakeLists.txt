cmake_minimum_required(VERSION 3.15)

project(tomosrv C ASM)

find_path(CSV_INCLUDE_DIRS csv.h REQUIRED)
find_library(CSV_LIBRARIES csv REQUIRED)

set(CMAKE_C_STANDARD 17)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /O2 /W3 /D_CRT_SECURE_NO_DEPRECATE /std:c17")

add_executable(${PROJECT_NAME}
               ${CMAKE_SOURCE_DIR}/src/main.c
               ${CMAKE_SOURCE_DIR}/src/server.c
               ${CMAKE_SOURCE_DIR}/src/endpoint.c
               ${CMAKE_SOURCE_DIR}/src/multiplex.c
               ${CMAKE_SOURCE_DIR}/src/structures/table.c
               ${CMAKE_SOURCE_DIR}/src/csv.c
               ${CMAKE_SOURCE_DIR}/src/error.c
               ${CMAKE_SOURCE_DIR}/src/log.c)

target_link_libraries(${PROJECT_NAME}
               PUBLIC ws2_32
                      ${CSV_LIBRARIES})

target_include_directories(${PROJECT_NAME}
                    PUBLIC ${CSV_INCLUDE_DIRS})


add_executable(test
               ${CMAKE_SOURCE_DIR}/test.c
               ${CMAKE_SOURCE_DIR}/src/endpoint.c
               ${CMAKE_SOURCE_DIR}/src/error.c
               ${CMAKE_SOURCE_DIR}/src/log.c)

target_link_libraries(test
               PUBLIC ws2_32
                      ${CSV_LIBRARIES})

target_include_directories(test
                    PUBLIC ${CSV_INCLUDE_DIRS})
