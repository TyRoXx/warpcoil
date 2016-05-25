#pragma once

#include <warpcoil/cpp/expected_response_registry.hpp>
#include <warpcoil/cpp/buffered_writer.hpp>
#include <warpcoil/cpp/integer_parser.hpp>

namespace warpcoil
{
    namespace cpp
    {
        enum class client_pipeline_request_status
        {
            ok,
            failure
        };

        template <class AsyncWriteStream, class AsyncReadStream>
        struct client_pipeline
        {
            explicit client_pipeline(AsyncWriteStream &requests, message_splitter<AsyncReadStream> &incoming)
                : writer(requests)
                , responses(incoming)
            {
            }

            std::size_t pending_requests() const
            {
                return responses.pending_requests();
            }

            template <class ResultParser, class RequestBuilder, class ResultHandler>
            void request(RequestBuilder build_request, ResultHandler &handler)
            {
                if (!writer.is_running())
                {
                    writer.async_run([this](boost::system::error_code ec)
                                     {
                                         responses.on_error(ec);
                                     });
                }
                auto sink = Si::Sink<std::uint8_t>::erase(writer.buffer_sink());
                write_integer(sink, static_cast<message_type_int>(message_type::request));
                request_id const current_id = next_request_id;
                ++next_request_id;
                write_integer(sink, current_id);
                switch (build_request(sink))
                {
                case client_pipeline_request_status::ok:
                    break;

                case client_pipeline_request_status::failure:
                    return;
                }
                writer.send_buffer();
                responses.template expect_response<ResultParser>(current_id, handler);
            }

        private:
            buffered_writer<AsyncWriteStream> writer;
            expected_response_registry<AsyncReadStream> responses;
            request_id next_request_id = 0;
        };
    }
}
