#include <warpcoil/javascript.hpp>
#include "generated.hpp"
#include <boost/test/unit_test.hpp>
#include "duktape.h"

namespace
{
    struct duktape_deleter
    {
        void operator()(duk_context *context) const
        {
            duk_destroy_heap(context);
        }
    };

    void evaluate(duk_context &interpreter, Si::memory_range code)
    {
        if (duk_peval_lstring(&interpreter, code.begin(), code.size()) != 0)
        {
            BOOST_FAIL(duk_safe_to_string(&interpreter, -1));
        }
    }
}

BOOST_AUTO_TEST_CASE(javascript)
{
    std::unique_ptr<duk_context, duktape_deleter> interpreter(duk_create_heap_default());
    BOOST_REQUIRE(interpreter);
    std::string code;
    auto code_writer = Si::make_container_sink(code);
    Si::append(code_writer, "\"use strict\";\n");

    Si::append(code_writer, "var library = ");
    warpcoil::javascript::generate_common_library(code_writer, warpcoil::indentation_level());
    Si::append(code_writer, ";\n");

    warpcoil::types::interface_definition client_definition;
    client_definition.add_method("on_event", Si::to_unique(warpcoil::types::tuple{{}}))(
        "content", warpcoil::types::utf8{warpcoil::types::integer{0, 2000}});

    warpcoil::types::interface_definition server_definition;
    server_definition.add_method("send", Si::to_unique(warpcoil::types::tuple{{}}))(
        "content", warpcoil::types::utf8{warpcoil::types::integer{0, 2000}});
    server_definition.add_method("disconnect", Si::to_unique(warpcoil::types::tuple{{}}));

    Si::append(code_writer, "var make_receiver = ");
    warpcoil::javascript::generate_input_receiver(code_writer, warpcoil::indentation_level(),
                                                  Si::make_c_str_range("library"));
    Si::append(code_writer, ";\n");

    Si::append(code_writer, "var make_client = ");
    warpcoil::javascript::generate_client(code_writer, warpcoil::indentation_level(), server_definition);
    Si::append(code_writer, ";\n");

    Si::append(code_writer, "var pending_requests = {};\n");
    Si::append(code_writer, "var server = { on_event: function (content) {} };\n");
    Si::append(code_writer, "var send_bytes = function (bytes, callback) {};\n");
    Si::append(code_writer, "var receiver = make_receiver(pending_requests, server, send_bytes);\n");
    Si::append(code_writer, "var client = make_client(pending_requests, send_bytes);\n");
    Si::append(code_writer, "client.send(\"abc\", function (error) {});\n");

    evaluate(*interpreter, Si::make_memory_range(code));
}
