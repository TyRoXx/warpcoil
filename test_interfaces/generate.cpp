#include <silicium/sink/iterator_sink.hpp>
#include <silicium/source/memory_source.hpp>
#include <ventura/run_process.hpp>
#include <warpcoil/cpp/generate/asynchronous.hpp>
#include <warpcoil/update_generated_file.hpp>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "requires output file name as the command line argument\n";
        return 1;
    }
    using namespace warpcoil;
    std::vector<char> code;
    auto code_writer = Si::make_container_sink(code);
    Si::append(code_writer, "#pragma once\n"
                            "#include <warpcoil/cpp/helpers.hpp>\n"
                            "#include <silicium/source/source.hpp>\n"
                            "#include <silicium/sink/iterator_sink.hpp>\n"
                            "#include <silicium/sink/append.hpp>\n"
                            "#include <silicium/variant.hpp>\n"
                            "#include <boost/asio/write.hpp>\n"
                            "#include <boost/range/algorithm/equal.hpp>\n"
                            "#include <cstdint>\n"
                            "#include <tuple>\n"
                            "\n");
    cpp::indentation_level const top_level;
    {
        types::interface_definition definition;
        types::tuple parameters;
        parameters.elements.emplace_back(types::integer());
        parameters.elements.emplace_back(types::integer());
        definition.methods.insert(std::make_pair(
            "evaluate", types::interface_definition::method{types::integer(), Si::to_unique(std::move(parameters))}));
        cpp::async::generate_serializable_interface(code_writer, top_level,
                                                    Si::make_c_str_range("binary_integer_function"), definition);
    }
    {
        types::interface_definition definition;
        definition.methods.insert(std::make_pair(
            "no_result_no_parameter",
            types::interface_definition::method{Si::to_unique(types::tuple()), Si::to_unique(types::tuple())}));
        {
            types::tuple results;
            results.elements.emplace_back(types::integer());
            results.elements.emplace_back(types::integer());
            definition.methods.insert(
                std::make_pair("two_results", types::interface_definition::method{Si::to_unique(std::move(results)),
                                                                                  types::integer()}));
        }
        {
            types::tuple parameters;
            parameters.elements.emplace_back(types::integer());
            parameters.elements.emplace_back(types::integer());
            definition.methods.insert(std::make_pair(
                "two_parameters",
                types::interface_definition::method{types::integer(), Si::to_unique(std::move(parameters))}));
        }
        definition.methods.insert(std::make_pair(
            "vectors",
            types::interface_definition::method{Si::to_unique(types::vector{types::integer(), types::integer()}),
                                                Si::to_unique(types::vector{types::integer(), types::integer()})}));
        {
            types::tuple parameters;
            parameters.elements.emplace_back(types::integer{0, 0xff});
            parameters.elements.emplace_back(types::integer{0, 0xffff});
            parameters.elements.emplace_back(types::integer{0, 0xffffffff});
            parameters.elements.emplace_back(types::integer{0, 0xffffffffffffffff});
            definition.methods.insert(std::make_pair(
                "integer_sizes", types::interface_definition::method{
                                     Si::to_unique(types::vector{types::integer{0, 0xff}, types::integer{0, 0xffff}}),
                                     Si::to_unique(std::move(parameters))}));
        }
        definition.methods.insert(
            std::make_pair("utf8", types::interface_definition::method{types::utf8{types::integer{0, 255}},
                                                                       types::utf8{types::integer{0, 255}}}));
        cpp::async::generate_serializable_interface(code_writer, top_level, Si::make_c_str_range("test_interface"),
                                                    definition);
    }
    Si::optional<ventura::absolute_path> const clang_format =
        (argc >= 3) ? ventura::absolute_path::create(argv[2]) : Si::none;
    Si::optional<ventura::absolute_path> const clang_format_config_dir =
        (argc >= 4) ? ventura::absolute_path::create(argv[3]) : Si::none;
    if (clang_format)
    {
        if (!clang_format_config_dir)
        {
            std::cerr << "command line argument for the clang-format config directory missing\n";
            return 1;
        }
        std::vector<char> formatted_code;
        auto formatted_code_writer = Si::Sink<char, Si::success>::erase(Si::make_container_sink(formatted_code));
        ventura::process_parameters parameters;
        parameters.executable = *clang_format;
        parameters.current_path = *clang_format_config_dir;
        parameters.out = &formatted_code_writer;
        auto original_code_reader = Si::Source<char>::erase(Si::make_container_source(code));
        parameters.in = &original_code_reader;
        parameters.inheritance = ventura::environment_inheritance::inherit;
        Si::error_or<int> result = ventura::run_process(parameters);
        if (result.is_error())
        {
            std::cerr << "clang-format failed: " << result.error() << '\n';
            return 1;
        }
        if (result.get() != 0)
        {
            std::cerr << "clang-format failed with return code " << result.get() << '\n';
            return 1;
        }
        code = std::move(formatted_code);
    }
    return warpcoil::update_generated_file(argv[1], Si::make_contiguous_range(code), std::cerr) ? 0 : 1;
}
