set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# expose version data as compiler definition
add_definitions("-DLINUXDEPLOYQT_VERSION=\"${GIT_TAG_NAME}\"")
add_definitions("-DLINUXDEPLOYQT_GIT_COMMIT=\"${GIT_COMMIT}\"")
add_definitions("-DBUILD_DATE=\"${DATE}\"")
add_definitions("-DBUILD_NUMBER=\"${BUILD_NUMBER}\"")

find_package(Qt5 REQUIRED COMPONENTS Core)

# update excludelist
message(STATUS "Updating excludelist...")
execute_process(
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/../generate-excludelist.sh
    OUTPUT_VARIABLE EXCLUDELIST
    TIMEOUT 10
    RESULT_VARIABLE EXCLUDELIST_RESULT
)
if(NOT EXCLUDELIST_RESULT EQUAL 0)
    message(WARNING "Updating excludelist failed, using outdated copy")
endif()
mark_as_advanced(EXCLUDELIST EXCLUDELIST_RESULT)

add_executable(linuxdeployqt main.cpp shared.cpp)
target_include_directories(linuxdeployqt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(linuxdeployqt Qt5::Core)
target_compile_definitions(linuxdeployqt PRIVATE -DEXCLUDELIST="${EXCLUDELIST}")
