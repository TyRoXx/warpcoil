add_executable(hello "hello.cpp")
target_link_libraries(hello ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CONAN_LIBS})
