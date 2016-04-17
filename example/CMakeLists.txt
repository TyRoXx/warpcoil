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

set(demoSources "hello_demo.cpp")
if(EXISTS ${generatedHeader})
	set(demoSources ${demoSources} ${generatedHeader})
endif()
add_executable(hello_demo ${demoSources})
target_link_libraries(hello_demo ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CONAN_LIBS})
add_dependencies(hello_demo hello_interfaces_header)
target_include_directories(hello_demo PRIVATE ${CMAKE_BINARY_DIR})
