#include <silicium/sink/iterator_sink.hpp>
#include <silicium/source/memory_source.hpp>
#include <ventura/run_process.hpp>
#include <warpcoil/cpp/generate/asynchronous.hpp>
#include <warpcoil/update_generated_file.hpp>

int main(int argc, char **argv)
{
    using namespace warpcoil;
    std::vector<char> code;
    auto code_writer = Si::make_container_sink(code);
    Si::append(code_writer, "#pragma once\n"
                            "#include <warpcoil/cpp/begin_parse_value.hpp>\n"
                            "#include <warpcoil/cpp/request_operation.hpp>\n"
                            "#include <silicium/source/source.hpp>\n"
                            "#include <silicium/sink/iterator_sink.hpp>\n"
                            "#include <silicium/sink/append.hpp>\n"
                            "#include <silicium/variant.hpp>\n"
                            "#include <boost/asio/write.hpp>\n"
                            "#include <boost/range/algorithm/equal.hpp>\n"
                            "#include <cstdint>\n"
                            "#include <tuple>\n"
                            "\n");
    {
        types::interface_definition definition;
        definition.methods.insert(
            std::make_pair("hello", types::method{types::utf8{types::integer{0, 255}},
                                                  make_vector<types::parameter>(
                                                      types::parameter{"name", types::utf8{types::integer{0, 255}}})}));
        cpp::indentation_level const top_level;
        cpp::async::generate_serializable_interface(code_writer, top_level, Si::make_c_str_range("hello_as_a_service"),
                                                    definition);
    }
    return run_code_generator_command_line_tool(Si::make_iterator_range(argv, argv + argc), std::cerr,
                                                Si::make_contiguous_range(code));
}
