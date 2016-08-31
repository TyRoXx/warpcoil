#pragma once

#include "generated.hpp"

namespace warpcoil
{
    struct failing_test_interface : async_test_interface
    {
        void integer_sizes(std::tuple<std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t> argument,
                           std::function<void(Si::error_or<std::vector<std::uint16_t>>)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void no_result_no_parameter(std::tuple<> argument,
                                    std::function<void(Si::error_or<std::tuple<>>)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void real_multi_parameters(std::string first, std::uint16_t second,
                                   std::function<void(Si::error_or<std::uint8_t>)> on_result) override
        {
            Si::ignore_unused_variable_warning(first);
            Si::ignore_unused_variable_warning(second);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void two_parameters(std::tuple<std::uint64_t, std::uint64_t> argument,
                            std::function<void(Si::error_or<std::uint64_t>)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void two_results(std::uint64_t argument,
                         std::function<void(Si::error_or<std::tuple<std::uint64_t, std::uint64_t>>)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void utf8(std::string argument, std::function<void(Si::error_or<std::string>)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void vectors(std::vector<std::uint64_t> argument,
                     std::function<void(Si::error_or<std::vector<std::uint64_t>>)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void vectors_256(std::vector<std::uint64_t> argument,
                         std::function<void(Si::error_or<std::vector<std::uint64_t>>)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void atypical_int(std::uint16_t argument, std::function<void(Si::error_or<std::uint16_t>)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void variant(Si::variant<std::uint32_t, std::string> argument,
                     std::function<void(Si::error_or<Si::variant<std::uint16_t, std::string>>)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void structure(structure_to_do argument,
                       std::function<void(Si::error_or<structure_to_do_member>)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }
    };
}
