cmake_minimum_required(VERSION 2.8.8)
project(llvm-gen)
set(LLVM_DIR "../llvm-debug/cmake/modules/CMakeFiles")
add_definitions(-g --std=c++11 -Wall -Werror)

#setup llvm
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_DIR}")
add_definitions(${LLVM_DEFINITIONS})
include_directories("../llvm-src/include")

file(GLOB SRC "src/*.cpp" "src/*.h")

add_executable(${PROJECT_NAME} ${SRC})

llvm_map_components_to_libnames(LLVM_LIBS support core mcjit native irreader linker ipo executionengine runtimedyld)

target_link_libraries(${PROJECT_NAME} ${LLVM_LIBS})
