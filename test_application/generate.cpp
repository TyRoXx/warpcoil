#include <warpcoil/cpp.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <ventura/write_file.hpp>
#include <ventura/read_file.hpp>
#include <ventura/open.hpp>

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		std::cerr << "requires output file name as the command line argument\n";
		return 1;
	}
	using namespace warpcoil;
	types::interface_definition definition;
	types::tuple parameters;
	parameters.elements.emplace_back(types::integer());
	parameters.elements.emplace_back(types::integer());
	definition.methods.insert(std::make_pair(
	    "evaluate", types::interface_definition::method{types::integer(), Si::to_unique(std::move(parameters))}));
	std::vector<char> code;
	auto code_writer = Si::make_container_sink(code);
	Si::append(code_writer, "#pragma once\n"
	                        "#include <cstdint>\n"
	                        "#include <tuple>\n"
	                        "\n");
	generate_interface(code_writer, Si::make_c_str_range("binary_integer_function"), definition);

	Si::os_string const file_name = Si::to_os_string(argv[1]);
	return Si::visit<int>(ventura::read_file(ventura::safe_c_str(file_name)),
	                      [&](std::vector<char> content) -> int
	                      {
		                      if (code == content)
		                      {
								  std::cout << "Generated file does not change\n";
			                      return 0;
		                      }

		                      boost::system::error_code const error =
		                          ventura::write_file(ventura::safe_c_str(file_name), Si::make_contiguous_range(code));
		                      if (!!error)
		                      {
			                      std::cerr << "Could not open " << argv[1] << "\n" << error << '\n';
			                      return 1;
		                      }

		                      return 0;
		                  },
	                      [&](boost::system::error_code const error)
	                      {
		                      std::cerr << "Could not read " << argv[1] << "\n" << error << '\n';
		                      return 1;
		                  },
	                      [&](ventura::read_file_problem const problem)
	                      {
		                      switch (problem)
		                      {
		                      case ventura::read_file_problem::concurrent_write_detected:
			                      std::cerr << "Someone seems to have access " << file_name << " concurrently.\n";
			                      return 1;

		                      case ventura::read_file_problem::file_too_large_for_memory:
			                      std::cerr << "Could not be loaded into memory: " << file_name << "\n";
			                      return 1;
		                      }
		                      SILICIUM_UNREACHABLE();
		                  });
}
