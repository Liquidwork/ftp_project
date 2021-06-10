cmake_minimum_required(VERSION 2.8)
project(ftp_project C)

set(CMAKE_C_STANDARD 11)

add_executable(ftp_project
        ftp_server.c)
