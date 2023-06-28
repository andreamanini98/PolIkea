cmake_minimum_required(VERSION 3.13)
get_filename_component(ProjectId ${CMAKE_CURRENT_SOURCE_DIR} NAME)
string(REPLACE " " "_" ProjectId ${ProjectId})
project(${ProjectId})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_ARCHITECTURES arm64)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(${PROJECT_NAME} "${ProjectId}.cpp")

target_include_directories(${PROJECT_NAME} PUBLIC /opt/homebrew/include)
target_include_directories(${PROJECT_NAME} PUBLIC /Applications/Development/VulkanSDK/macOS/include)
target_include_directories(${PROJECT_NAME} PUBLIC /opt/homebrew/lib)
target_include_directories(${PROJECT_NAME} PUBLIC /Applications/Development/VulkanSDK/macOS/lib)

target_include_directories(${PROJECT_NAME} PUBLIC headers)

target_link_libraries(${PROJECT_NAME} /opt/homebrew/lib/libglfw.3.3.dylib)
target_link_libraries(${PROJECT_NAME} /Applications/Development/VulkanSDK/macOS/lib/libvulkan.1.dylib)
target_link_libraries(${PROJECT_NAME} /Applications/Development/VulkanSDK/macOS/lib/libvulkan.1.3.211.dylib)