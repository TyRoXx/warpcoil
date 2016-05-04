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
                                   log << "Wrote file " << file << " (" << new_content.size() << " bytes)\n";
                                   return true;
                               },
                               [&](boost::system::error_code error)
                               {
                                   log << "Could not read " << file << "\nError code: " << error << '\n';
                                   error = ventura::write_file(ventura::safe_c_str(file_name), new_content);
                                   if (!!error)
                                   {
                                       log << "Could not open " << file << "\n" << error << '\n';
                                       return false;
                                   }
                                   log << "Wrote file " << file << " (" << new_content.size() << " bytes)\n";
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

    inline int run_code_generator_command_line_tool(Si::iterator_range<char **> command_line_arguments,
                                                    std::ostream &log, Si::memory_range code)
    {
        if (command_line_arguments.size() < 2)
        {
            log << "requires output file name as the command line argument\n";
            return 1;
        }
        Si::optional<ventura::absolute_path> const clang_format =
            (command_line_arguments.size() >= 3) ? ventura::absolute_path::create(command_line_arguments[2]) : Si::none;
        Si::optional<ventura::absolute_path> const clang_format_config_dir =
            (command_line_arguments.size() >= 4) ? ventura::absolute_path::create(command_line_arguments[3]) : Si::none;
        std::vector<char> formatted_code;
        if (clang_format)
        {
            if (!clang_format_config_dir)
            {
                log << "command line argument for the clang-format config directory missing\n";
                return 1;
            }
            log << "formatting the code (originally " << code.size() << " bytes)\n";
            auto formatted_code_writer = Si::Sink<char, Si::success>::erase(Si::make_container_sink(formatted_code));
            ventura::process_parameters parameters;
            parameters.executable = *clang_format;
            parameters.current_path = *clang_format_config_dir;
            parameters.out = &formatted_code_writer;
            auto original_code_reader = Si::Source<char>::erase(Si::memory_source<char>(code));
            parameters.in = &original_code_reader;
            parameters.inheritance = ventura::environment_inheritance::inherit;
            Si::error_or<int> result = ventura::run_process(parameters);
            if (result.is_error())
            {
                log << "clang-format failed: " << result.error() << '\n';
                return 1;
            }
            if (result.get() != 0)
            {
                log << "clang-format failed with return code " << result.get() << '\n';
                return 1;
            }
            code = Si::make_contiguous_range(formatted_code);
        }
        return warpcoil::update_generated_file(command_line_arguments[1], code, std::cerr) ? 0 : 1;
    }
}
