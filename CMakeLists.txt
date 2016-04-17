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
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
	include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
	conan_basic_setup()
else()
	if(MSVC)
		set(Boost_USE_STATIC_LIBS ON)
	endif()
	if(BOOST_ROOT)
		set(Boost_NO_SYSTEM_PATHS ON)
	endif()
	find_package(Boost REQUIRED COMPONENTS unit_test_framework thread filesystem chrono program_options system)
	if(NOT (Boost_VERSION LESS 105300))
		set(required_boost_libraries ${Boost_LIBRARIES})
		if(MSVC AND (Boost_VERSION EQUAL 105600))
			message(STATUS "Boost.Coroutine 1.56 seems to be broken on MSVC, so we will not use it.")
			set(SILICIUM_AVOID_BOOST_COROUTINE ON)
			add_definitions("-DSILICIUM_AVOID_BOOST_COROUTINE=1")
		else()
			message(STATUS "trying to find Boost.Coroutine and Boost.Context..")
			if(NOT (Boost_VERSION LESS 105400))
				set(SILICIUM_FIND_COROUTINE coroutine)
			endif()
			find_package(Boost COMPONENTS ${SILICIUM_FIND_COROUTINE} context)
			set(Boost_LIBRARIES ${required_boost_libraries} ${Boost_LIBRARIES})
		endif()
	endif()
	if(UNIX)
		add_definitions("-DBOOST_USE_VALGRIND")
		if(NOT (Boost_VERSION LESS 105500))
			add_definitions("-DBOOST_COROUTINES_UNIDIRECT")
			#define the misspelled macro for Boost.Asio:
			add_definitions("-DBOOST_COROUTINES_UNIDRECT")
		endif()
	endif()
	include_directories(SYSTEM ${Boost_INCLUDE_DIR})
	link_directories(${Boost_LIBRARY_DIR})
endif()

if(UNIX)
	add_definitions("-DBOOST_TEST_DYN_LINK")
endif()

if(WIN32)
	#Boost.Asio wants this for no reason
	add_definitions("-D_WIN32_WINDOWS")

	add_definitions("-DBOOST_ASIO_HAS_IOCP")
endif()

find_package(Threads)

add_definitions("-DSILICIUM_NO_DEPRECATED")

include_directories(".")

file(GLOB generateSources "test_interfaces/generate.cpp")
set(formatted ${formatted} ${generateSources})
add_executable(test_app_generate ${generateSources})
target_link_libraries(test_app_generate ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CONAN_LIBS})
set(generatedHeader ${CMAKE_BINARY_DIR}/generated.hpp)

find_program(WARPCOIL_CLANG_FORMAT NAMES clang-format clang-format-3.7 clang-format-3.8 PATHS "C:/Program Files/LLVM/bin")
if(WARPCOIL_CLANG_FORMAT)
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
