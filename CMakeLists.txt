cmake_minimum_required(VERSION 3.10)
project(my_project)

set(CMAKE_C_STANDARD 11)

# Добавляем исполняемые файлы для каждой программы
add_executable(5_1 signal.c)
add_executable(5_2 nonamefifo.c)

# Если ochered и signals - это директории с дополнительными файлами
# можно добавить их как поддиректории
if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ochered)
    add_subdirectory(ochered)
endif()

if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/signals)
    add_subdirectory(signals)
endif()
