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
            "evaluate", types::method{types::integer(), make_vector<types::parameter>(types::parameter{
                                                            "argument", Si::to_unique(std::move(parameters))})}));
        cpp::async::generate_serializable_interface(code_writer, top_level,
                                                    Si::make_c_str_range("binary_integer_function"), definition);
    }
    {
        types::interface_definition definition;
        definition.methods.insert(std::make_pair(
            "no_result_no_parameter",
            types::method{Si::to_unique(types::tuple()),
                          make_vector<types::parameter>(types::parameter{"argument", Si::to_unique(types::tuple())})}));
        {
            types::tuple results;
            results.elements.emplace_back(types::integer());
            results.elements.emplace_back(types::integer());
            definition.methods.insert(std::make_pair(
                "two_results",
                types::method{Si::to_unique(std::move(results)),
                              make_vector<types::parameter>(types::parameter{"argument", types::integer()})}));
        }
        {
            types::tuple parameters;
            parameters.elements.emplace_back(types::integer());
            parameters.elements.emplace_back(types::integer());
            definition.methods.insert(std::make_pair(
                "two_parameters",
                types::method{types::integer(), make_vector<types::parameter>(types::parameter{
                                                    "argument", Si::to_unique(std::move(parameters))})}));
        }
        definition.methods.insert(std::make_pair(
            "vectors",
            types::method{Si::to_unique(types::vector{types::integer(), types::integer()}),
                          make_vector<types::parameter>(types::parameter{
                              "argument", Si::to_unique(types::vector{types::integer(), types::integer()})})}));
        definition.methods.insert(std::make_pair(
            "vectors_256",
            types::method{Si::to_unique(types::vector{types::integer{0, 255}, types::integer()}),
                          make_vector<types::parameter>(types::parameter{
                              "argument", Si::to_unique(types::vector{types::integer{0, 255}, types::integer()})})}));
        {
            types::tuple parameters;
            parameters.elements.emplace_back(types::integer{0, 0xff});
            parameters.elements.emplace_back(types::integer{0, 0xffff});
            parameters.elements.emplace_back(types::integer{0, 0xffffffff});
            parameters.elements.emplace_back(types::integer{0, 0xffffffffffffffff});
            definition.methods.insert(std::make_pair(
                "integer_sizes",
                types::method{Si::to_unique(types::vector{types::integer{0, 0xff}, types::integer{0, 0xffff}}),
                              make_vector<types::parameter>(
                                  types::parameter{"argument", Si::to_unique(std::move(parameters))})}));
        }
        definition.methods.insert(
            std::make_pair("utf8", types::method{types::utf8{types::integer{0, 255}},
                                                 make_vector<types::parameter>(types::parameter{
                                                     "argument", types::utf8{types::integer{0, 255}}})}));

        definition.methods.insert(std::make_pair(
            "atypical_int", types::method{types::integer{1, 1000}, make_vector<types::parameter>(types::parameter{
                                                                       "argument", types::integer{1, 1000}})}));

        definition.methods.insert(std::make_pair(
            "real_multi_parameters",
            types::method{types::integer{0, 255},
                          make_vector<types::parameter>(types::parameter{"first", types::utf8{types::integer{0, 255}}},
                                                        types::parameter{"second", types::integer{0, 0xffff}})}));

        definition.methods.insert(std::make_pair(
            "variant", types::method{Si::to_unique(types::variant{warpcoil::make_vector<types::type>(
                                         types::integer{0, 0xffff}, types::utf8{types::integer{0, 255}})}),
                                     make_vector<types::parameter>(types::parameter{
                                         "argument",
                                         Si::to_unique(types::variant{warpcoil::make_vector<types::type>(
                                             types::integer{1, 0xffffffff}, types::utf8{types::integer{0, 255}})})})}));

        cpp::async::generate_serializable_interface(code_writer, top_level, Si::make_c_str_range("test_interface"),
                                                    definition);
    }
    return run_code_generator_command_line_tool(Si::make_iterator_range(argv, argv + argc), std::cerr,
                                                Si::make_contiguous_range(code));
}
