set(generatedHeader ${CMAKE_BINARY_DIR}/generated.hpp)
if(WARPCOIL_USE_CLANG_FORMAT AND WARPCOIL_CLANG_FORMAT)
    add_custom_target(test_app_header
        COMMAND $<TARGET_FILE:test_app_generate> ${generatedHeader} ${WARPCOIL_CLANG_FORMAT} ${CMAKE_SOURCE_DIR}
        DEPENDS test_app_generate clang-format)
else()
    add_custom_target(test_app_header
        COMMAND $<TARGET_FILE:test_app_generate> ${generatedHeader}
        DEPENDS test_app_generate)
endif()
