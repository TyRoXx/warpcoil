#include <silicium/sink/iterator_sink.hpp>
#include <silicium/source/memory_source.hpp>
#include <ventura/run_process.hpp>
#include <warpcoil/cpp/generate/asynchronous.hpp>
#include <warpcoil/update_generated_file.hpp>
#include <warpcoil/make_vector.hpp>

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
        types::tuple parameters;
        parameters.elements.emplace_back(types::integer());
        parameters.elements.emplace_back(types::integer());
        definition.add_method("evaluate", types::integer())("argument", Si::to_unique(std::move(parameters)));
        indentation_level const top_level;
        cpp::generate_serializable_interface(interfaces_writer, shared, top_level,
                                             Si::make_c_str_range("binary_integer_function"), definition);
    }
    {
        types::interface_definition definition;
        definition.add_method("no_result_no_parameter", Si::to_unique(types::tuple()))("argument",
                                                                                       Si::to_unique(types::tuple()));
        {
            types::tuple results;
            results.elements.emplace_back(types::integer());
            results.elements.emplace_back(types::integer());
            definition.add_method("two_results", Si::to_unique(std::move(results)))("argument", types::integer());
        }

        {
            types::tuple parameters;
            parameters.elements.emplace_back(types::integer());
            parameters.elements.emplace_back(types::integer());
            definition.add_method("two_parameters", types::integer())("argument", Si::to_unique(std::move(parameters)));
        }

        definition.add_method("vectors", Si::to_unique(types::vector{types::integer(), types::integer()}))(
            "argument", Si::to_unique(types::vector{types::integer(), types::integer()}));

        definition.add_method("vectors_256", Si::to_unique(types::vector{types::integer{0, 255}, types::integer()}))(
            "argument", Si::to_unique(types::vector{types::integer{0, 255}, types::integer()}));

        {
            types::tuple parameters;
            parameters.elements.emplace_back(types::integer{0, 0xff});
            parameters.elements.emplace_back(types::integer{0, 0xffff});
            parameters.elements.emplace_back(types::integer{0, 0xffffffff});
            parameters.elements.emplace_back(types::integer{0, 0xffffffffffffffff});
            definition.add_method("integer_sizes",
                                  Si::to_unique(types::vector{types::integer{0, 0xff}, types::integer{0, 0xffff}}))(
                "argument", Si::to_unique(std::move(parameters)));
        }

        definition.add_method("utf8", types::utf8{types::integer{0, 255}})("argument",
                                                                           types::utf8{types::integer{0, 255}});
        definition.add_method("atypical_int", types::integer{1, 1000})("argument", types::integer{1, 1000});

        definition.add_method("real_multi_parameters", types::integer{0, 255})(
            "first", types::utf8{types::integer{0, 255}})("second", types::integer{0, 0xffff});

        definition.add_method("variant", Si::to_unique(types::variant{warpcoil::make_vector<types::type>(
                                             types::integer{0, 0xffff}, types::utf8{types::integer{0, 255}})}))(
            "argument", Si::to_unique(types::variant{warpcoil::make_vector<types::type>(
                            types::integer{1, 0xffffffff}, types::utf8{types::integer{0, 255}})}));

        definition.add_method("structure", Si::to_unique(types::structure{make_vector<types::structure::element>(
                                               types::structure::element{types::integer{0, 3}, "member"})}))(
            "argument", Si::to_unique(types::structure{make_vector<types::structure::element>()}));

        indentation_level const top_level;
        cpp::generate_serializable_interface(interfaces_writer, shared, top_level,
                                             Si::make_c_str_range("test_interface"), definition);
    }
    {
        types::interface_definition definition;
        types::tuple parameters;
        parameters.elements.emplace_back(types::integer());
        parameters.elements.emplace_back(types::integer());
        definition.add_method("make_directory", Si::to_unique(types::tuple()))("path",
                                                                               types::utf8{types::integer{0, 0xffff}});
        definition.add_method("remove", Si::to_unique(types::tuple()))("path", types::utf8{types::integer{0, 0xffff}});
        indentation_level const top_level;
        cpp::generate_serializable_interface(interfaces_writer, shared, top_level, Si::make_c_str_range("directory"),
                                             definition);
    }
    file.insert(file.end(), interfaces.begin(), interfaces.end());
    return run_code_generator_command_line_tool(Si::make_iterator_range(argv, argv + argc), std::cerr,
                                                Si::make_contiguous_range(file));
}
