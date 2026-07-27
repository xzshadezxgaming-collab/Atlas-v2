cmake_minimum_required(VERSION 3.20)

project(Atlas
    VERSION 0.0.1
    DESCRIPTION "Atlas Engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(Engine)
add_subdirectory(Game)