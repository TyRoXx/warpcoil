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

    #SILICIUM_TRAIT triggers warnings about missing arguments for the macro BOOST_PP_EXPAND_I (maybe a VC++ 2013 bug?)
    add_definitions("/wd4003")

    #Boost future has unreachable code for no reason
    add_definitions("/wd4702")
    
    #Boost in_place triggers /W4 warning about an assignment operator that could not be generated
    add_definitions("/wd4512")

    #"decorated name length exceeded, name was truncated"
    add_definitions("/wd4503")
    
    #"needs to have dll-interface to be used by clients of class"
    add_definitions("/wd4251")

    #stupid warning about std::copy
    add_definitions("/wd4996")

    #workaround for Boost 1.55 Context error LNK2026
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")

    if(MSVC14)
        #Boost 1.58 Asio has a macro redefinition on Visual Studio 2015
        add_definitions("/wd4005")

        #unrestricted union bug
        add_definitions("/wd4624")

        #Boost 1.58 Coroutine warning about throw in a noexcept function
        add_definitions("/wd4297")
    endif()

    add_definitions("-DBOOST_ASIO_NO_DEPRECATED")

    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    
    if(MSVC14 AND (CMAKE_SIZEOF_VOID_P EQUAL 8))
        set(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /MDd /Zi /Ob0 /Od")
    endif()

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
find_package(Boost REQUIRED COMPONENTS unit_test_framework test_exec_monitor filesystem coroutine context thread chrono program_options system)

if(WIN32)
    #Boost.Asio wants this for no reason
    add_definitions("-D_WIN32_WINDOWS")

    add_definitions("-DBOOST_ASIO_HAS_IOCP")
endif()

find_package(Threads)

if(UNIX AND NOT APPLE)
    set(WARPCOIL_LIBRT rt)
endif()

add_definitions("-DSILICIUM_NO_DEPRECATED")

include_directories(".")

file(GLOB generateSources "test_interfaces/generate.cpp")
set(formatted ${formatted} ${generateSources})
add_executable(test_app_generate ${generateSources})
target_link_libraries(test_app_generate ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${WARPCOIL_LIBRT})
set(generatedHeader ${CMAKE_BINARY_DIR}/generated.hpp)

option(WARPCOIL_USE_CLANG_FORMAT "look for and use clang-format if available" ON)
if(WARPCOIL_USE_CLANG_FORMAT)
    find_program(WARPCOIL_CLANG_FORMAT NAMES clang-format clang-format-3.7 PATHS "C:/Program Files/LLVM/bin")
endif()

if(WARPCOIL_USE_CLANG_FORMAT AND WARPCOIL_CLANG_FORMAT)
    add_custom_target(generatedHeaderTarget
        COMMAND $<TARGET_FILE:test_app_generate> ${generatedHeader} ${WARPCOIL_CLANG_FORMAT} ${CMAKE_SOURCE_DIR}
        DEPENDS test_app_generate)
else()
    add_custom_target(generatedHeaderTarget
        COMMAND $<TARGET_FILE:test_app_generate> ${generatedHeader}
        DEPENDS test_app_generate)
endif()

add_subdirectory("test")
add_subdirectory("example")

add_custom_target(clang-format COMMAND ${WARPCOIL_CLANG_FORMAT} -i ${formatted} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
