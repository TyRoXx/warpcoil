file(GLOB sources "*.hpp" "*.cpp")
set(formatted ${formatted} ${sources} PARENT_SCOPE)

add_executable(hello_interfaces "hello_interface.cpp")
target_link_libraries(hello_interfaces ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
set(generatedHeader ${CMAKE_BINARY_DIR}/hello_interfaces.hpp)

if(WARPCOIL_USE_CLANG_FORMAT AND WARPCOIL_CLANG_FORMAT)
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
target_link_libraries(hello_tcp_callback ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
add_dependencies(hello_tcp_callback hello_interfaces_header)
target_include_directories(hello_tcp_callback PRIVATE ${CMAKE_BINARY_DIR})

set(demoSources "hello_tcp_coroutine.cpp")
if(EXISTS ${generatedHeader})
    set(demoSources ${demoSources} ${generatedHeader})
endif()
add_executable(hello_tcp_coroutine ${demoSources})
target_link_libraries(hello_tcp_coroutine ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
add_dependencies(hello_tcp_coroutine hello_interfaces_header)
target_include_directories(hello_tcp_coroutine PRIVATE ${CMAKE_BINARY_DIR})

set(demoSources "hello_tcp_coroutine_hack.cpp")
if(EXISTS ${generatedHeader})
    set(demoSources ${demoSources} ${generatedHeader})
endif()
add_executable(hello_tcp_coroutine_hack ${demoSources})
target_link_libraries(hello_tcp_coroutine_hack ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
add_dependencies(hello_tcp_coroutine_hack hello_interfaces_header)
target_include_directories(hello_tcp_coroutine_hack PRIVATE ${CMAKE_BINARY_DIR})
    
set(demoSources "hello_websocket.cpp")
if(EXISTS ${generatedHeader})
    set(demoSources ${demoSources} ${generatedHeader})
endif()
add_executable(hello_websocket ${demoSources})
target_link_libraries(hello_websocket ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
add_dependencies(hello_websocket hello_interfaces_header)
target_include_directories(hello_websocket PRIVATE ${CMAKE_BINARY_DIR})
