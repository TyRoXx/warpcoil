#include <silicium/sink/iterator_sink.hpp>
#include <silicium/source/memory_source.hpp>
#include <ventura/run_process.hpp>
#include <warpcoil/cpp/generate/everything.hpp>
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
		definition.methods.insert(
		    std::make_pair("hello", types::interface_definition::method{types::make_string(), types::make_string()}));
		cpp::async::generate_serializable_interface(code_writer, top_level, Si::make_c_str_range("hello_as_a_service"),
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
