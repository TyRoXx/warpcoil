file(GLOB sources "*.hpp" "*.cpp")
set(formatted ${formatted} ${sources} PARENT_SCOPE)

add_executable(hello_interfaces "hello_interface.cpp")
target_link_libraries(hello_interfaces ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CONAN_LIBS})
set(generatedHeader ${CMAKE_BINARY_DIR}/hello_interfaces.hpp)

if(WARPCOIL_CLANG_FORMAT)
	add_custom_target(hello_interfaces_header
		COMMAND $<TARGET_FILE:hello_interfaces> ${generatedHeader} ${WARPCOIL_CLANG_FORMAT} ${CMAKE_SOURCE_DIR}
		DEPENDS hello_interfaces)
else()
	add_custom_target(hello_interfaces_header
		COMMAND $<TARGET_FILE:hello_interfaces> ${generatedHeader}
		DEPENDS hello_interfaces)
endif()

set(demoSources "hello_tcp_callback.cpp")
if(EXISTS ${generatedHeader})
	set(demoSources ${demoSources} ${generatedHeader})
endif()
add_executable(hello_tcp_callback ${demoSources})
target_link_libraries(hello_tcp_callback ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CONAN_LIBS})
add_dependencies(hello_tcp_callback hello_interfaces_header)
target_include_directories(hello_tcp_callback PRIVATE ${CMAKE_BINARY_DIR})

if(NOT WIN32)
	#boost::asio::spawn can make the creation of a socket crash with an Access Violation.
	#WSASocketW indirectly calls LoadLibraryExW which is not fully supported by Boost Coroutines.
	#http://boost.2283326.n4.nabble.com/boost-context-cause-strange-crash-on-certan-WIN32-API-td4669587.html
	set(demoSources "hello_tcp_coroutine.cpp")
	if(EXISTS ${generatedHeader})
		set(demoSources ${demoSources} ${generatedHeader})
	endif()
	add_executable(hello_tcp_coroutine ${demoSources})
	target_link_libraries(hello_tcp_coroutine ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CONAN_LIBS})
	add_dependencies(hello_tcp_coroutine hello_interfaces_header)
	target_include_directories(hello_tcp_coroutine PRIVATE ${CMAKE_BINARY_DIR})
endif()

set(demoSources "hello_tcp_coroutine_hack.cpp")
if(EXISTS ${generatedHeader})
    set(demoSources ${demoSources} ${generatedHeader})
endif()
add_executable(hello_tcp_coroutine_hack ${demoSources})
target_link_libraries(hello_tcp_coroutine_hack ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CONAN_LIBS})
add_dependencies(hello_tcp_coroutine_hack hello_interfaces_header)
target_include_directories(hello_tcp_coroutine_hack PRIVATE ${CMAKE_BINARY_DIR})
    
set(demoSources "hello_websocket.cpp")
if(EXISTS ${generatedHeader})
	set(demoSources ${demoSources} ${generatedHeader})
endif()
add_executable(hello_websocket ${demoSources})
target_link_libraries(hello_websocket ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CONAN_LIBS})
add_dependencies(hello_websocket hello_interfaces_header)
target_include_directories(hello_websocket PRIVATE ${CMAKE_BINARY_DIR})
