#include "benchmark_interface.hpp"
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
    Si::append(code_writer, cpp::headers);
    {
        indentation_level const top_level;
        cpp::generate_serializable_interface(code_writer, top_level, Si::make_c_str_range("benchmark_service"),
                                             warpcoil::create_benchmark_interface());
    }
    return run_code_generator_command_line_tool(Si::make_iterator_range(argv, argv + argc), std::cerr,
                                                Si::make_contiguous_range(code));
}
