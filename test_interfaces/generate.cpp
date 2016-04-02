#include <warpcoil/cpp.hpp>
#include <warpcoil/update_generated_file.hpp>
#include <silicium/sink/iterator_sink.hpp>

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
	                        "#include <warpcoil/cpp_helpers.hpp>\n"
	                        "#include <silicium/source/source.hpp>\n"
	                        "#include <silicium/sink/append.hpp>\n"
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
		    "evaluate",
		    types::interface_definition::method{
		        types::integer(), Si::to_unique(std::move(parameters))}));
		cpp::generate_serializable_interface(
		    code_writer, top_level,
		    Si::make_c_str_range("binary_integer_function"), definition);
	}
	{
		types::interface_definition definition;
		definition.methods.insert(std::make_pair(
		    "no_result_no_parameter",
		    types::interface_definition::method{
		        Si::to_unique(types::tuple()), Si::to_unique(types::tuple())}));
		{
			types::tuple results;
			results.elements.emplace_back(types::integer());
			results.elements.emplace_back(types::integer());
			definition.methods.insert(std::make_pair(
			    "two_results",
			    types::interface_definition::method{
			        Si::to_unique(std::move(results)), types::integer()}));
		}
		{
			types::tuple parameters;
			parameters.elements.emplace_back(types::integer());
			parameters.elements.emplace_back(types::integer());
			definition.methods.insert(std::make_pair(
			    "two_parameters",
			    types::interface_definition::method{
			        types::integer(), Si::to_unique(std::move(parameters))}));
		}
		cpp::generate_serializable_interface(
		    code_writer, top_level, Si::make_c_str_range("test_interface"),
		    definition);
	}
	return warpcoil::update_generated_file(
	           argv[1], Si::make_contiguous_range(code), std::cerr)
	           ? 0
	           : 1;
}
