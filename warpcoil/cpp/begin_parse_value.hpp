#pragma once

#include <warpcoil/cpp/helpers.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class AsyncReadStream, class Buffer, class Parser, class ResultHandler>
        void begin_parse_value(AsyncReadStream &input, Buffer receive_buffer, std::size_t &receive_buffer_used,
                               Parser parser, ResultHandler &&on_result)
        {
            for (std::size_t i = 0; i < receive_buffer_used; ++i)
            {
                std::uint8_t *const data = boost::asio::buffer_cast<std::uint8_t *>(receive_buffer);
                parse_result<typename Parser::result_type> response = parser.parse_byte(data[i]);
                if (Si::visit<bool>(response,
                                    [&](typename Parser::result_type &message)
                                    {
                                        std::copy(data + i + 1, data + receive_buffer_used, data);
                                        receive_buffer_used -= 1 + i;
                                        std::forward<ResultHandler>(on_result)(boost::system::error_code(),
                                                                               std::move(message));
                                        return true;
                                    },
                                    [](need_more_input)
                                    {
                                        return false;
                                    },
                                    [&](invalid_input)
                                    {
                                        std::forward<ResultHandler>(on_result)(make_invalid_input_error(), {});
                                        return true;
                                    }))
                {
                    return;
                }
            }
            input.async_read_some(receive_buffer,
                                  [
                                    &input,
                                    receive_buffer,
                                    &receive_buffer_used,
                                    parser = std::move(parser),
                                    on_result = std::forward<ResultHandler>(on_result)
                                  ](boost::system::error_code ec, std::size_t read) mutable
                                  {
                                      if (!!ec)
                                      {
                                          std::forward<ResultHandler>(on_result)(ec, {});
                                          return;
                                      }
                                      receive_buffer_used = read;
                                      begin_parse_value(input, receive_buffer, receive_buffer_used, std::move(parser),
                                                        std::forward<ResultHandler>(on_result));
                                  });
        }
    }
}
