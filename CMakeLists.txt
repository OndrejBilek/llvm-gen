cmake_minimum_required(VERSION 2.8.8)

project(mila+)

# Include LLVM 6.0 source
include_directories("../llvm-src-600/include")
include_directories("${CMAKE_SOURCE_DIR}/src")

# Make Debug
set(LLVM_DIR "../llvm-debug-600/cmake/modules/CMakeFiles")

# Make Release
# set(LLVM_DIR "../llvm-release-600/cmake/modules/CMakeFiles")

# set C++11 definitions, error on everything, enable debugging
add_definitions(--std=c++11 -Wall -Werror -g)

# find llvm package and set it up
find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# add sources
file(GLOB_RECURSE SRC "src/*.cpp" "src/*.h" "tests/*.mila")
add_executable(${PROJECT_NAME} ${SRC})

# Find the libraries that correspond to the LLVM components that we wish to use
llvm_map_components_to_libnames(LLVM_LIBS support core mcjit native irreader linker ipo bitwriter)
target_link_libraries(${PROJECT_NAME} ${LLVM_LIBS})
