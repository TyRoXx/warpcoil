#pragma once

namespace warpcoil
{
    struct impl_test_interface : async_test_interface
    {
        void
        integer_sizes(std::tuple<std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t> argument,
                      std::function<void(boost::system::error_code, std::vector<std::uint16_t>)> on_result) override
        {
            on_result({}, std::vector<std::uint16_t>{std::get<1>(argument)});
        }

        void no_result_no_parameter(std::tuple<> argument,
                                    std::function<void(boost::system::error_code, std::tuple<>)> on_result) override
        {
            on_result({}, argument);
        }

        void real_multi_parameters(std::string first, std::uint16_t second,
                                   std::function<void(boost::system::error_code, std::uint8_t)> on_result) override
        {
            on_result({}, static_cast<std::uint8_t>(first.size() + second));
        }

        void two_parameters(std::tuple<std::uint64_t, std::uint64_t> argument,
                            std::function<void(boost::system::error_code, std::uint64_t)> on_result) override
        {
            on_result({}, std::get<0>(argument) * std::get<1>(argument));
        }

        void two_results(
            std::uint64_t argument,
            std::function<void(boost::system::error_code, std::tuple<std::uint64_t, std::uint64_t>)> on_result) override
        {
            on_result({}, std::make_tuple(argument, argument));
        }

        void utf8(std::string argument, std::function<void(boost::system::error_code, std::string)> on_result) override
        {
            on_result({}, argument + "123");
        }

        void vectors(std::vector<std::uint64_t> argument,
                     std::function<void(boost::system::error_code, std::vector<std::uint64_t>)> on_result) override
        {
            std::reverse(argument.begin(), argument.end());
            on_result({}, std::move(argument));
        }

        void vectors_256(std::vector<std::uint64_t> argument,
                         std::function<void(boost::system::error_code, std::vector<std::uint64_t>)> on_result) override
        {
            std::sort(argument.begin(), argument.end(), std::greater<std::uint64_t>());
            on_result({}, std::move(argument));
        }

        void atypical_int(std::uint16_t argument,
                          std::function<void(boost::system::error_code, std::uint16_t)> on_result) override
        {
            on_result({}, argument);
        }

        virtual void variant(
            Si::variant<std::uint32_t, std::string> argument,
            std::function<void(boost::system::error_code, Si::variant<std::uint16_t, std::string>)> on_result) override
        {
            on_result({}, Si::visit<Si::variant<std::uint16_t, std::string>>(argument,
                                                                             [](std::uint32_t value)
                                                                             {
                                                                                 return static_cast<std::uint16_t>(
                                                                                     value + 1);
                                                                             },
                                                                             [](std::string value)
                                                                             {
                                                                                 return value + 'd';
                                                                             }));
        }
    };
}
