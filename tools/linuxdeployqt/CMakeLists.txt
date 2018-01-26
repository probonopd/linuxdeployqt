set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core)

add_executable(linuxdeployqt main.cpp shared.cpp)
target_include_directories(linuxdeployqt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(linuxdeployqt Qt5::Core)
