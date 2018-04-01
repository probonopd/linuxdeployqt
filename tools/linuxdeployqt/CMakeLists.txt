set(CMAKE_AUTOMOC ON)

# expose version data as compiler definition
add_definitions("-DLINUXDEPLOYQT_VERSION=\"${GIT_TAG_NAME}\"")
add_definitions("-DLINUXDEPLOYQT_GIT_COMMIT=\"${GIT_COMMIT}\"")
add_definitions("-DBUILD_DATE=\"${DATE}\"")
add_definitions("-DBUILD_NUMBER=\"${BUILD_NUMBER}\"")

find_package(Qt5 REQUIRED COMPONENTS Core)

# update excludelist
execute_process(
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/../excludelist.sh
    OUTPUT_VARIABLE EXCLUDELIST
    TIMEOUT 10
)

add_executable(linuxdeployqt main.cpp shared.cpp)
target_include_directories(linuxdeployqt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(linuxdeployqt Qt5::Core)
target_compile_definitions(linuxdeployqt PRIVATE -DEXCLUDELIST="${EXCLUDELIST}")
