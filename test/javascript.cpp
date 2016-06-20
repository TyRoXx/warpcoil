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

    Si::append(code_writer, "var parse_int_1 = ");
    warpcoil::javascript::generate_integer_parser(code_writer, warpcoil::indentation_level(), 1);
    Si::append(code_writer, ";\n");

    Si::append(code_writer, "var parse_int_2 = ");
    warpcoil::javascript::generate_integer_parser(code_writer, warpcoil::indentation_level(), 2);
    Si::append(code_writer, ";\n");

    Si::append(code_writer, "var parse_int_4 = ");
    warpcoil::javascript::generate_integer_parser(code_writer, warpcoil::indentation_level(), 4);
    Si::append(code_writer, ";\n");

    Si::append(code_writer, "var parse_int_8 = ");
    warpcoil::javascript::generate_integer_parser(code_writer, warpcoil::indentation_level(), 8);
    Si::append(code_writer, ";\n");

    warpcoil::types::interface_definition client_definition;
    client_definition.add_method("on_event", Si::to_unique(warpcoil::types::tuple{{}}))(
        "content", warpcoil::types::utf8{warpcoil::types::integer{0, 2000}});

    warpcoil::types::interface_definition server_definition;
    server_definition.add_method("send", Si::to_unique(warpcoil::types::tuple{{}}))(
        "content", warpcoil::types::utf8{warpcoil::types::integer{0, 2000}});
    server_definition.add_method("disconnect", Si::to_unique(warpcoil::types::tuple{{}}));

    Si::append(code_writer, "var make_parser = ");
    warpcoil::javascript::generate_input_parser(code_writer, warpcoil::indentation_level());
    Si::append(code_writer, ";\n");

    Si::append(code_writer, "var make_client = ");
    warpcoil::javascript::generate_client(code_writer, warpcoil::indentation_level(), server_definition);
    Si::append(code_writer, ";\n");

    Si::append(code_writer, "var pending_requests = {};\n");
    Si::append(code_writer, "var server = { on_event: function (content) {} };\n");
    Si::append(code_writer, "var parser = make_parser(pending_requests, server);\n");
    Si::append(code_writer, "var client = make_client(pending_requests);\n");
    Si::append(code_writer, "client.send(\"abc\", function (error) {});\n");

    evaluate(*interpreter, Si::make_memory_range(code));
}
