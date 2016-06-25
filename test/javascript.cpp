#include <warpcoil/javascript.hpp>
#include "generated.hpp"
#include <boost/test/unit_test.hpp>
#include "duktape.h"
#include <fstream>

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
        if (duk_peval_lstring(&interpreter, code.begin(), code.size()) == 0)
        {
            return;
        }

        duk_get_prop_string(&interpreter, -1, "stack");
        std::string const stack = duk_safe_to_string(&interpreter, -1);
        duk_pop(&interpreter);

        duk_pop(&interpreter);
        BOOST_FAIL(stack);
    }

    void javascript_test(std::string name, std::function<void(Si::Sink<char, Si::success>::interface &)> test_code)
    {
        std::unique_ptr<duk_context, duktape_deleter> interpreter(duk_create_heap_default());
        BOOST_REQUIRE(interpreter);
        std::string code;
        auto code_writer = Si::Sink<char, Si::success>::erase(Si::make_container_sink(code));
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
        server_definition.add_method("hello", warpcoil::types::utf8{warpcoil::types::integer{0, 2000}})(
            "name", warpcoil::types::utf8{warpcoil::types::integer{0, 1992}});

        Si::memory_range const library = Si::make_c_str_range("library");
        Si::append(code_writer, "var make_receiver = ");
        warpcoil::javascript::generate_input_receiver(code_writer, warpcoil::indentation_level(), client_definition,
                                                      library);
        Si::append(code_writer, ";\n");

        Si::append(code_writer, "var make_client = ");
        warpcoil::javascript::generate_client(code_writer, warpcoil::indentation_level(), server_definition, library);
        Si::append(code_writer, ";\n");

        test_code(code_writer);

        {
            // for reading the code while debugging
            std::ofstream code_file("javascript_" + name + ".js");
            code_file << code;
        }

        evaluate(*interpreter, Si::make_memory_range(code));
    }
}

BOOST_AUTO_TEST_CASE(javascript_parse_int_1)
{
    javascript_test("parse_int_1", [](Si::Sink<char, Si::success>::interface &code_writer)
                    {
                        Si::append(code_writer, "var parser = library.parse_int_1(0, 0xff);\n");
                        Si::append(code_writer, "var result = parser(0x44);\n");
                        Si::append(code_writer, "library.assert(0x44 === result);\n");
                    });
}

BOOST_AUTO_TEST_CASE(javascript_parse_int_2)
{
    javascript_test("parse_int_2", [](Si::Sink<char, Si::success>::interface &code_writer)
                    {
                        Si::append(code_writer, "var parser = library.parse_int_2(0, 0xffff);\n");
                        Si::append(code_writer, "library.assert(undefined === parser(0x11));\n");
                        Si::append(code_writer, "var result = parser(0x44);\n");
                        Si::append(code_writer, "library.assert(0x1144 === result);\n");
                    });
}

BOOST_AUTO_TEST_CASE(javascript_parse_int_4)
{
    javascript_test("parse_int_4", [](Si::Sink<char, Si::success>::interface &code_writer)
                    {
                        Si::append(code_writer, "var parser = library.parse_int_4(0, 0xffffffff);\n");
                        Si::append(code_writer, "library.assert(undefined === parser(0x11));\n");
                        Si::append(code_writer, "library.assert(undefined === parser(0x22));\n");
                        Si::append(code_writer, "library.assert(undefined === parser(0x33));\n");
                        Si::append(code_writer, "var result = parser(0x44);\n");
                        Si::append(code_writer, "library.assert(0x11223344 === result);\n");
                    });
}

BOOST_AUTO_TEST_CASE(javascript_parse_int_8)
{
    javascript_test("parse_int_8", [](Si::Sink<char, Si::success>::interface &code_writer)
                    {
                        Si::append(code_writer, "var parser = library.parse_int_8(0, 0, 0xffffffff, 0xffffffff);\n");
                        Si::append(code_writer, "library.assert(undefined === parser(0x11));\n");
                        Si::append(code_writer, "library.assert(undefined === parser(0x22));\n");
                        Si::append(code_writer, "library.assert(undefined === parser(0x33));\n");
                        Si::append(code_writer, "library.assert(undefined === parser(0x44));\n");
                        Si::append(code_writer, "library.assert(undefined === parser(0x55));\n");
                        Si::append(code_writer, "library.assert(undefined === parser(0x66));\n");
                        Si::append(code_writer, "library.assert(undefined === parser(0x77));\n");
                        Si::append(code_writer, "var result = parser(0x88);\n");
                        Si::append(code_writer, "library.assert(0x11223344 === result.high);\n");
                        Si::append(code_writer, "library.assert(0x55667788 === result.low);\n");
                    });
}

BOOST_AUTO_TEST_CASE(javascript_receiver)
{
    javascript_test(
        "receiver", [](Si::Sink<char, Si::success>::interface &code_writer)
        {
            Si::append(code_writer, "var pending_requests = {};\n");
            Si::append(code_writer, "var should_get_request = false;\n");
            Si::append(code_writer, "var got_request = false;\n");
            Si::append(code_writer, "var should_send = false;\n");
            Si::append(code_writer, "var server = { on_event: function (content, on_result)\n");
            warpcoil::block(code_writer, warpcoil::indentation_level(),
                            [&](warpcoil::indentation_level const in_function)
                            {
                                start_line(code_writer, in_function, "library.assert(should_get_request);\n");
                                start_line(code_writer, in_function, "library.assert(!got_request);\n");
                                start_line(code_writer, in_function, "got_request = true;\n");
                                start_line(code_writer, in_function, "should_get_request = false;\n");
                                start_line(code_writer, in_function, "library.assert(content == \"XY\");\n");
                                start_line(code_writer, in_function, "should_send = true;\n");
                                start_line(code_writer, in_function, "on_result([]);\n");
                            },
                            "\n};\n");
            Si::append(code_writer, "var got_send = false;\n");
            Si::append(code_writer, "var send_bytes = function (bytes)\n");
            warpcoil::block(code_writer, warpcoil::indentation_level(),
                            [&](warpcoil::indentation_level const in_function)
                            {
                                start_line(code_writer, in_function, "library.assert(should_send);\n");
                                start_line(code_writer, in_function, "library.assert(!got_send);\n");
                                start_line(code_writer, in_function, "got_send = true;\n");
                                start_line(code_writer, in_function, "should_send = false;\n");
                                start_line(code_writer, in_function, "library.assert(bytes.length === 9);\n");
                                start_line(code_writer, in_function, "var view = new Uint8Array(bytes);\n");
                                start_line(code_writer, in_function, "library.assert_eq(1, view[0]);\n");
                                start_line(code_writer, in_function, "library.assert_eq(1, view[1]);\n");
                                start_line(code_writer, in_function, "library.assert_eq(2, view[2]);\n");
                                start_line(code_writer, in_function, "library.assert_eq(3, view[3]);\n");
                                start_line(code_writer, in_function, "library.assert_eq(4, view[4]);\n");
                                start_line(code_writer, in_function, "library.assert_eq(5, view[5]);\n");
                                start_line(code_writer, in_function, "library.assert_eq(6, view[6]);\n");
                                start_line(code_writer, in_function, "library.assert_eq(7, view[7]);\n");
                                start_line(code_writer, in_function, "library.assert_eq(8, view[8]);\n");
                            },
                            ";\n");
            Si::append(code_writer, "var receiver = make_receiver(pending_requests, server, send_bytes);\n");
            Si::append(code_writer,
                       "[0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 'o'.charCodeAt(), 'n'.charCodeAt(), '_'.charCodeAt(), "
                       "'e'.charCodeAt(), 'v'.charCodeAt(), 'e'.charCodeAt(), 'n'.charCodeAt(), 't'.charCodeAt(), 0, "
                       "2, 'X'.charCodeAt()].forEach(function (byte)\n");
            warpcoil::block(code_writer, warpcoil::indentation_level(),
                            [&](warpcoil::indentation_level const in_function)
                            {
                                start_line(code_writer, in_function, "receiver(byte);\n");
                            },
                            ");\n");
            Si::append(code_writer, "should_get_request = true;\n");
            Si::append(code_writer, "receiver('Y'.charCodeAt());\n");
            Si::append(code_writer, "library.assert(got_request);\n");
            Si::append(code_writer, "library.assert(got_send);\n");
        });
}

BOOST_AUTO_TEST_CASE(javascript_client_empty_response)
{
    javascript_test(
        "client_empty_response", [](Si::Sink<char, Si::success>::interface &code_writer)
        {
            Si::append(code_writer, "var pending_requests = {};\n");
            Si::append(code_writer, "var should_send = false;\n");
            Si::append(code_writer, "var got_send = false;\n");
            Si::append(code_writer, "var send_bytes = function (bytes)\n");
            warpcoil::block(
                code_writer, warpcoil::indentation_level(),
                [&](warpcoil::indentation_level const in_function)
                {
                    start_line(code_writer, in_function, "library.assert(should_send);\n");
                    start_line(code_writer, in_function, "library.assert(!got_send);\n");
                    start_line(code_writer, in_function, "got_send = true;\n");
                    start_line(code_writer, in_function, "should_send = false;\n");
                    start_line(code_writer, in_function, "library.assert(bytes.length === (1 + 8 + 5 + 5));\n");
                    start_line(code_writer, in_function, "var view = new Uint8Array(bytes);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[0]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[1]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[2]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[3]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[4]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[5]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[6]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[7]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[8]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(4, view[9]);\n");
                    start_line(code_writer, in_function, "library.assert_eq('s'.charCodeAt(), view[10]);\n");
                    start_line(code_writer, in_function, "library.assert_eq('e'.charCodeAt(), view[11]);\n");
                    start_line(code_writer, in_function, "library.assert_eq('n'.charCodeAt(), view[12]);\n");
                    start_line(code_writer, in_function, "library.assert_eq('d'.charCodeAt(), view[13]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[14]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(3, view[15]);\n");
                    start_line(code_writer, in_function, "library.assert_eq('a'.charCodeAt(), view[16]);\n");
                    start_line(code_writer, in_function, "library.assert_eq('b'.charCodeAt(), view[17]);\n");
                    start_line(code_writer, in_function, "library.assert_eq('c'.charCodeAt(), view[18]);\n");
                },
                ";\n");
            Si::append(code_writer, "var client = make_client(pending_requests, send_bytes);\n");
            Si::append(code_writer, "should_send = true;\n");
            Si::append(code_writer, "var should_get_response = false;\n");
            Si::append(code_writer, "var got_response = false;\n");
            Si::append(code_writer, "client.send(\"abc\", function (error, response)\n");
            warpcoil::block(code_writer, warpcoil::indentation_level(),
                            [&](warpcoil::indentation_level const in_function)
                            {
                                start_line(code_writer, in_function, "library.assert(should_get_response);\n");
                                start_line(code_writer, in_function, "should_get_response = false;\n");
                                start_line(code_writer, in_function, "got_response = true;\n");
                                start_line(code_writer, in_function, "library.assert(error === undefined);\n");
                                start_line(code_writer, in_function, "library.assert(response.length === 0);\n");
                            },
                            ");\n");
            Si::append(code_writer, "library.assert(got_send);\n");
            Si::append(code_writer, "var no_server = {};\n");
            Si::append(code_writer, "var receiver = make_receiver(pending_requests, no_server, send_bytes);\n");
            Si::append(code_writer, "[1, 0, 0, 0, 0, 0, 0, 0].forEach(function (byte)\n");
            warpcoil::block(code_writer, warpcoil::indentation_level(),
                            [&](warpcoil::indentation_level const in_function)
                            {
                                start_line(code_writer, in_function, "receiver(byte);\n");
                            },
                            ");\n");
            Si::append(code_writer, "should_get_response = true;\n");
            Si::append(code_writer, "receiver(0);\n");
            Si::append(code_writer, "library.assert(got_response);\n");
        });
}

BOOST_AUTO_TEST_CASE(javascript_client_non_empty_response)
{
    javascript_test(
        "client_non_empty_response", [](Si::Sink<char, Si::success>::interface &code_writer)
        {
            Si::append(code_writer, "var pending_requests = {};\n");
            Si::append(code_writer, "var should_send = false;\n");
            Si::append(code_writer, "var got_send = false;\n");
            Si::append(code_writer, "var send_bytes = function (bytes)\n");
            warpcoil::block(
                code_writer, warpcoil::indentation_level(),
                [&](warpcoil::indentation_level const in_function)
                {
                    start_line(code_writer, in_function, "library.assert(should_send);\n");
                    start_line(code_writer, in_function, "library.assert(!got_send);\n");
                    start_line(code_writer, in_function, "got_send = true;\n");
                    start_line(code_writer, in_function, "should_send = false;\n");
                    start_line(code_writer, in_function, "library.assert(bytes.length === (1 + 8 + 6 + 5));\n");
                    start_line(code_writer, in_function, "var view = new Uint8Array(bytes);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[0]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[1]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[2]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[3]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[4]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[5]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[6]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[7]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[8]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(5, view[9]);\n");
                    start_line(code_writer, in_function, "library.assert_eq('h'.charCodeAt(), view[10]);\n");
                    start_line(code_writer, in_function, "library.assert_eq('e'.charCodeAt(), view[11]);\n");
                    start_line(code_writer, in_function, "library.assert_eq('l'.charCodeAt(), view[12]);\n");
                    start_line(code_writer, in_function, "library.assert_eq('l'.charCodeAt(), view[13]);\n");
                    start_line(code_writer, in_function, "library.assert_eq('o'.charCodeAt(), view[14]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(0, view[15]);\n");
                    start_line(code_writer, in_function, "library.assert_eq(3, view[16]);\n");
                    start_line(code_writer, in_function, "library.assert_eq('a'.charCodeAt(), view[17]);\n");
                    start_line(code_writer, in_function, "library.assert_eq('b'.charCodeAt(), view[18]);\n");
                    start_line(code_writer, in_function, "library.assert_eq('c'.charCodeAt(), view[19]);\n");
                },
                ";\n");
            Si::append(code_writer, "var client = make_client(pending_requests, send_bytes);\n");
            Si::append(code_writer, "should_send = true;\n");
            Si::append(code_writer, "var should_get_response = false;\n");
            Si::append(code_writer, "var got_response = false;\n");
            Si::append(code_writer, "client.hello(\"abc\", function (error, response)\n");
            warpcoil::block(code_writer, warpcoil::indentation_level(),
                            [&](warpcoil::indentation_level const in_function)
                            {
                                start_line(code_writer, in_function, "library.assert(should_get_response);\n");
                                start_line(code_writer, in_function, "should_get_response = false;\n");
                                start_line(code_writer, in_function, "got_response = true;\n");
                                start_line(code_writer, in_function, "library.assert(error === undefined);\n");
                                start_line(code_writer, in_function, "library.assert(response === \"Hello, abc!\");\n");
                            },
                            ");\n");
            Si::append(code_writer, "library.assert(got_send);\n");
            Si::append(code_writer, "var no_server = {};\n");
            Si::append(code_writer, "var receiver = make_receiver(pending_requests, no_server, send_bytes);\n");
            Si::append(code_writer, "[1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11, 'H'.charCodeAt(), 'e'.charCodeAt(), "
                                    "'l'.charCodeAt(), 'l'.charCodeAt(), 'o'.charCodeAt(), ','.charCodeAt(), ' "
                                    "'.charCodeAt(), 'a'.charCodeAt(), 'b'.charCodeAt() , "
                                    "'c'.charCodeAt()].forEach(function (byte)\n");
            warpcoil::block(code_writer, warpcoil::indentation_level(),
                            [&](warpcoil::indentation_level const in_function)
                            {
                                start_line(code_writer, in_function, "receiver(byte);\n");
                            },
                            ");\n");
            Si::append(code_writer, "should_get_response = true;\n");
            Si::append(code_writer, "receiver('!'.charCodeAt());\n");
            Si::append(code_writer, "library.assert(got_response);\n");
        });
}
