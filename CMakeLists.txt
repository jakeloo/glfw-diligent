cmake_minimum_required(VERSION 3.6)

project(main)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(glfw)
add_subdirectory(DiligentCore)

include_directories("glfw/include")
include_directories(DiligentCore)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL  "GLFW lib only")
set(GLFW_BUILD_TESTS OFF CACHE BOOL  "GLFW lib only")
set(GLFW_BUILD_DOCS OFF CACHE BOOL  "GLFW lib only")
set(GLFW_BUILD_INSTALL OFF CACHE BOOL  "GLFW lib only")

set(SRC main.cpp)

add_executable(main ${SRC})
target_link_libraries(main
  glfw
  Diligent-GraphicsEngine
)

if (D3D11_SUPPORTED)
  target_link_libraries(main Diligent-GraphicsEngineD3D11-static)
endif()

if (D3D12_SUPPORTED)
  target_link_libraries(main Diligent-GraphicsEngineD3D12-static)
endif()

if (GL_SUPPORTED)
  target_link_libraries(main Diligent-GraphicsEngineOpenGL-static)
endif()

if (VULKAN_SUPPORTED)
  target_link_libraries(main Diligent-GraphicsEngineVk-static)
endif()

if (METAL_SUPPORTED)
  target_link_libraries(main Diligent-GraphicsEngineMetal-static)
endif()


