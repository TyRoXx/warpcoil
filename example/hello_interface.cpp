#include <silicium/sink/iterator_sink.hpp>
#include <silicium/source/memory_source.hpp>
#include <ventura/run_process.hpp>
#include <warpcoil/cpp/generate/generate_serializable_interface.hpp>
#include <warpcoil/update_generated_file.hpp>

int main(int argc, char **argv)
{
    using namespace warpcoil;
    std::vector<char> file;
    auto file_writer = Si::make_container_sink(file);
    cpp::shared_code_generator<decltype(file_writer)> shared(file_writer, indentation_level());
    Si::append(file_writer, cpp::headers);
    std::vector<char> interfaces;
    auto interfaces_writer = Si::make_container_sink(interfaces);
    {
        types::interface_definition definition;
        definition.add_method("hello", types::utf8{types::integer{0, 255}})("name",
                                                                            types::utf8{types::integer{0, 255}});
        indentation_level const top_level;
        cpp::generate_serializable_interface(interfaces_writer, shared, top_level,
                                             Si::make_c_str_range("hello_as_a_service"), definition);
    }
    file.insert(file.end(), interfaces.begin(), interfaces.end());
    return run_code_generator_command_line_tool(Si::make_iterator_range(argv, argv + argc), std::cerr,
                                                Si::make_contiguous_range(file));
}
