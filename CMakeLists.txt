cmake_minimum_required(VERSION 3.15)
project(multithreading C)

set(CMAKE_C_STANDARD 99)

find_package(Threads REQUIRED)

add_executable(multithreading
        threads/example.c
               threaded_echo_server/threaded_echo.c
               echo_server/echo.c
               select/select_chat.c select/colors.h select/client.c
               poll/poll_chat.c poll/colors.h poll/client.c
               broadcast/server.c broadcast/client.c broadcast/colors.h broadcast/common.h
        )

target_link_libraries(${PROJECT_NAME} Threads::Threads)