# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

string(TIMESTAMP BUILD_TIMESTAMP "%d/%m/%Y - %H:%M")
set(PROJECT_VER "0.2.2")

set(JSON_CONTENT [==[
{
    "project_version": "@PROJECT_VER@",
    "build_timestamp": "@BUILD_TIMESTAMP@"
}
]==])

# 3. Replace CMake variables inside the string
string(CONFIGURE "${JSON_CONTENT}" FORMATTED_JSON @ONLY)

# 4. Write directly to the build directory
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/version.json" "${FORMATTED_JSON}")

project(digital_clock)
