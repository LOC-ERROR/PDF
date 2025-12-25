cmake_minimum_required(VERSION 3.21)

project(ImageFolderToPdf LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui)

qt_standard_project_setup()

qt_add_executable(ImageFolderToPdf
    src/main.cpp
)

target_link_libraries(ImageFolderToPdf PRIVATE Qt6::Widgets Qt6::Gui)
