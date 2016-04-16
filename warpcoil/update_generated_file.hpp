#pragma once

#include <boost/range/algorithm/equal.hpp>
#include <ventura/open.hpp>
#include <ventura/read_file.hpp>
#include <ventura/write_file.hpp>

namespace warpcoil
{
	inline bool update_generated_file(char const *file, Si::memory_range new_content, std::ostream &log)
	{
		Si::os_string const file_name = Si::to_os_string(file);
		return Si::visit<bool>(ventura::read_file(ventura::safe_c_str(file_name)),
		                       [&](std::vector<char> old_content) -> bool
		                       {
			                       if (boost::range::equal(old_content, new_content))
			                       {
				                       log << "Generated file does not change\n";
				                       return true;
			                       }

			                       boost::system::error_code const error =
			                           ventura::write_file(ventura::safe_c_str(file_name), new_content);
			                       if (!!error)
			                       {
				                       log << "Could not open " << file << "\n" << error << '\n';
				                       return false;
			                       }
			                       return true;
			                   },
		                       [&](boost::system::error_code error)
		                       {
			                       log << "Could not read " << file << "\n" << error << '\n';
			                       error = ventura::write_file(ventura::safe_c_str(file_name), new_content);
			                       if (!!error)
			                       {
				                       log << "Could not open " << file << "\n" << error << '\n';
				                       return false;
			                       }
			                       return true;
			                   },
		                       [&](ventura::read_file_problem const problem)
		                       {
			                       switch (problem)
			                       {
			                       case ventura::read_file_problem::concurrent_write_detected:
				                       log << "Someone seems to have access " << file << " concurrently.\n";
				                       return false;

			                       case ventura::read_file_problem::file_too_large_for_memory:
				                       log << "Could not be loaded into memory: " << file << "\n";
				                       return false;
			                       }
			                       SILICIUM_UNREACHABLE();
			                   });
	}
}
