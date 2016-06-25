cmake_minimum_required(VERSION 2.8)
project(warpcoil)

option(WARPCOIL_WARNING_IS_ERROR "turn all warnings into errors (-Werror)" OFF)

if(UNIX)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
    else()
        execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
        if(GCC_VERSION VERSION_GREATER 4.7)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
        else()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
        endif()
    endif()

    add_definitions("-Wall -Wextra -Wconversion -Wvla")

    if(WARPCOIL_WARNING_IS_ERROR)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    endif()

    option(WARPCOIL_PEDANTIC "pass -pedantic to the compiler (may generate useless warnings)" OFF)
    if(WARPCOIL_PEDANTIC)
        add_definitions("-pedantic")
        add_definitions("-DSILICIUM_PEDANTIC")

        #warn when using undefined macros
        add_definitions("-Wundef")
    endif()

    #reduce size of binaries in the order of ~10%
    add_definitions("-fvisibility=hidden -fvisibility-inlines-hidden")

    option(WARPCOIL_COVERAGE "passes -coverage to the compiler" OFF)
    if(WARPCOIL_COVERAGE)
        add_definitions("-coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -coverage")
    endif()
endif()

if(MSVC)
    #make Visual C++ 2013 build in parallel
    add_definitions("/MP")

    #we want more warnings
    add_definitions("/W4")

    add_definitions("-DBOOST_ALL_NO_LIB")
    
    #Boost Asio uses deprecated Winsock APIs, but we are not interested in warnings about that
    add_definitions("-D_WINSOCK_DEPRECATED_NO_WARNINGS")

    #Boost in_place triggers /W4 warning about an assignment operator that could not be generated
    add_definitions("/wd4512")

    #"decorated name length exceeded, name was truncated"
    add_definitions("/wd4503")
    
    #"needs to have dll-interface to be used by clients of class"
    add_definitions("/wd4251")

    #stupid warning about std::copy
    add_definitions("/wd4996")

    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    
    add_definitions("/bigobj")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
elseif(EXISTS ${CMAKE_BINARY_DIR}/../conanbuildinfo.cmake)
    #for CLion
    include(${CMAKE_BINARY_DIR}/../conanbuildinfo.cmake)
else()
    message(FATAL_ERROR "currently you have to install the dependencies with conan")
endif()

conan_basic_setup()

if(WIN32)
    #Boost.Asio wants this for no reason
    add_definitions("-D_WIN32_WINDOWS")

    add_definitions("-DBOOST_ASIO_HAS_IOCP")
endif()

add_definitions("-DBOOST_ASIO_NO_DEPRECATED")

find_package(Threads)

if(UNIX AND NOT APPLE)
    set(WARPCOIL_LIBRT rt)
endif()

add_definitions("-DSILICIUM_NO_DEPRECATED")

include_directories(".")

file(GLOB generateSources "test_interfaces/generate.cpp")
set(formatted ${formatted} ${generateSources})
add_executable(test_app_generate ${generateSources})
target_link_libraries(test_app_generate ${CONAN_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${WARPCOIL_LIBRT})

option(WARPCOIL_USE_CLANG_FORMAT "look for and use clang-format if available" ON)
if(WARPCOIL_USE_CLANG_FORMAT)
    find_program(WARPCOIL_CLANG_FORMAT NAMES clang-format clang-format-3.7 PATHS "C:/Program Files/LLVM/bin")
endif()

add_subdirectory("test_interfaces")
add_subdirectory("test")

option(WARPCOIL_BUILD_EXAMPLES "" ON)
if(WARPCOIL_BUILD_EXAMPLES)
	add_subdirectory("example")
	add_subdirectory("example_web_site")
endif()

if(WARPCOIL_USE_CLANG_FORMAT)
	add_custom_target(clang-format COMMAND ${WARPCOIL_CLANG_FORMAT} -i ${formatted} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
