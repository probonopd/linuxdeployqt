# CMake configuration for linuxdeployqt
# Not meant to replace the qmake build system, but for use with CMake based IDEs.

cmake_minimum_required(VERSION 3.5)

project(linuxdeployqt)

find_program(GIT git)

if("${GIT}" STREQUAL "GIT-NOTFOUND")
    message(WARNING "Could not find git, commit and tag info cannot be updated")

    if(NOT GIT_COMMIT)
        message(FATAL_ERROR "Commit ID not set, please call with -DGIT_COMMIT=...")
    endif()

    if(NOT GIT_TAG_NAME)
        message(FATAL_ERROR "Tag name not set, please call with -DGIT_TAG_NAME=...")
    endif()
else()
    # make sure Git revision ID and latest tag is not stored in the CMake cache
    # otherwise, one would have to reset the CMake cache on every new commit to make sure the Git commit ID is up to date
    unset(GIT_COMMIT CACHE)
    unset(GIT_LATEST_TAG CACHE)

    # read Git revision ID and latest tag number
    execute_process(
        COMMAND "${GIT}" rev-parse --short HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_COMMIT_RESULT
    )
    if(NOT GIT_COMMIT_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to determine git commit ID")
    endif()
    mark_as_advanced(GIT_COMMIT GIT_COMMIT_RESULT)

    execute_process(
        COMMAND "${GIT}" rev-list --tags --skip=1 --max-count=1
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG_ID
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_TAG_ID_RESULT
    )
    if(NOT GIT_TAG_ID_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to determine git tag ID")
    endif()
    mark_as_advanced(GIT_TAG_ID GIT_TAG_ID_RESULT)

    execute_process(
        COMMAND "${GIT}" describe --tags ${GIT_TAG_ID} --abbrev=0
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG_NAME
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_TAG_NAME_RESULT
    )
    if(NOT GIT_TAG_NAME_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to determine git tag name")
    endif()
    mark_as_advanced(GIT_TAG_NAME GIT_TAG_NAME_RESULT)
endif()

# set version and build number
set(VERSION 1-alpha)
if("$ENV{TRAVIS_BUILD_NUMBER}" STREQUAL "")
    set(BUILD_NUMBER "<local dev build>")
else()
    set(BUILD_NUMBER "$ENV{TRAVIS_BUILD_NUMBER}")
endif()

# get current date
execute_process(
    COMMAND env LC_ALL=C date -u "+%Y-%m-%d %H:%M:%S %Z"
    OUTPUT_VARIABLE DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE DATE_RESULT
)
if(NOT DATE_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to determine date string")
endif()
mark_as_advanced(DATE DATE_RESULT)

add_subdirectory(tools/linuxdeployqt)
