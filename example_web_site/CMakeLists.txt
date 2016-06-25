file(GLOB sources "*.hpp" "*.cpp")
set(formatted ${formatted} ${sources} PARENT_SCOPE)

add_executable(web_site_interfaces_generate "web_site_interface.cpp")
target_link_libraries(web_site_interfaces_generate ${CONAN_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${WARPCOIL_LIBRT})

set(generatedHeader ${CMAKE_BINARY_DIR}/web_site_interfaces.hpp)
if(WARPCOIL_USE_CLANG_FORMAT AND WARPCOIL_CLANG_FORMAT)
    add_custom_target(web_site_interfaces_header
        COMMAND $<TARGET_FILE:web_site_interfaces_generate> ${generatedHeader} ${WARPCOIL_CLANG_FORMAT} ${CMAKE_SOURCE_DIR}
        DEPENDS web_site_interfaces_generate clang-format)
else()
    add_custom_target(web_site_interfaces_header
        COMMAND $<TARGET_FILE:web_site_interfaces_generate> ${generatedHeader}
        DEPENDS web_site_interfaces_generate)
endif()

set(demoSources "web_site_server.cpp")
if(EXISTS ${generatedHeader})
    set(demoSources ${demoSources} ${generatedHeader})
endif()
add_executable(web_site_server ${demoSources})
target_link_libraries(web_site_server ${CONAN_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_dependencies(web_site_server web_site_interfaces_header)
target_include_directories(web_site_server PRIVATE ${CMAKE_BINARY_DIR})
