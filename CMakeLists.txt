cmake_minimum_required(VERSION 3.10)

project(
	winMessenger
	DESCRIPTION "GUI based messaging app for Windows!"
	LANGUAGES CXX
)

find_package(OpenSSL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

add_executable(winMessenger 
    "src/main.cpp"
    "src/scenes/menu_screen.cpp"
    "src/scenes/scene_manager.cpp" 
    "src/net/server.cpp" 
    "src/net/client.cpp"
    "src/net/connection.cpp" 
    "src/net/iocp.cpp"
    "src/server/server.cpp"
)
target_compile_features(winMessenger PRIVATE cxx_std_20)

target_include_directories(winMessenger PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(imgui STATIC 
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/misc/cpp/imgui_stdlib.cpp
    imgui/backends/imgui_impl_opengl3.cpp
    imgui/backends/imgui_impl_glfw.cpp
)
target_include_directories(imgui PUBLIC imgui imgui/backends)
target_link_libraries(imgui PRIVATE glfw OpenGL::GL)

target_link_libraries(winMessenger PRIVATE 
    imgui
    OpenSSL::Crypto
    glfw
    OpenGL::GL
)

if(WIN32)
	 target_link_libraries(winMessenger PRIVATE ws2_32)
     target_link_libraries(winMessenger PRIVATE gdi32)
endif(WIN32)

