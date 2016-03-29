#include <warpcoil/warpcoil.hpp>
#include <boost/test/unit_test.hpp>
#include <silicium/sink/ptr_sink.hpp>
#include <silicium/sink/ostream_sink.hpp>
#include <silicium/sink/append.hpp>
#include <silicium/make_unique.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>

namespace warpcoil
{
	template <class CharSink>
	struct comma_separator
	{
		explicit comma_separator(CharSink out)
		    : m_out(out)
		    , m_first(true)
		{
		}

		void add_element()
		{
			if (m_first)
			{
				m_first = false;
				return;
			}
			Si::append(m_out, ", ");
		}

	private:
		CharSink m_out;
		bool m_first;
	};

	template <class CharSink>
	auto make_comma_separator(CharSink out)
	{
		return comma_separator<CharSink>(out);
	}

	template <class CharSink>
	void generate_type(CharSink &&code, types::type const &root)
	{
		return Si::visit<void>(root,
		                       [&code](types::integer)
		                       {
			                       Si::append(code, "::std::uint64_t");
			                   },
		                       [&code](std::unique_ptr<types::variant> const &root)
		                       {
			                       Si::append(code, "::Si::variant<");
			                       auto comma = make_comma_separator(Si::ref_sink(code));
			                       for (types::type const &element : root->elements)
			                       {
				                       comma.add_element();
				                       generate_type(code, element);
			                       }
			                       Si::append(code, ">");
			                   },
		                       [&code](std::unique_ptr<types::tuple> const &root)
		                       {
			                       Si::append(code, "::std::tuple<");
			                       auto comma = make_comma_separator(Si::ref_sink(code));
			                       for (types::type const &element : root->elements)
			                       {
				                       comma.add_element();
				                       generate_type(code, element);
			                       }
			                       Si::append(code, ">");
			                   },
		                       [&code](std::unique_ptr<types::subset> const &root)
		                       {
			                       Si::append(code, "/*subset of*/");
			                       generate_type(code, root->superset);
			                   });
	}

	template <class CharSink>
	void generate_value(CharSink &&code, values::value const &root)
	{
		return Si::visit<void>(root,
		                       [&code](values::integer const root)
		                       {
			                       Si::append(code, boost::lexical_cast<std::string>(root));
			                   },
		                       [](std::unique_ptr<values::closure> const &)
		                       {
			                       throw std::logic_error("???");
			                   },
		                       [&code](std::unique_ptr<values::tuple> const &root)
		                       {
			                       Si::append(code, "::std::make_tuple(");
			                       auto comma = make_comma_separator(Si::ref_sink(code));
			                       for (values::value const &element : root->elements)
			                       {
				                       comma.add_element();
				                       generate_value(code, element);
			                       }
			                       Si::append(code, ")");
			                   });
	}

	template <class CharSink>
	void generate_expression(CharSink &&code, expressions::expression const &root)
	{
		return Si::visit<void>(root,
		                       [&code](std::unique_ptr<expressions::call> const &root)
		                       {
			                       generate_expression(code, root->callee);
			                       Si::append(code, "(");
			                       generate_expression(code, root->argument);
			                       Si::append(code, ")");
			                   },
		                       [&code](std::unique_ptr<expressions::tuple> const &root)
		                       {
			                       Si::append(code, "::std::make_tuple(");
			                       auto comma = make_comma_separator(Si::ref_sink(code));
			                       for (expressions::expression const &element : root->elements)
			                       {
				                       comma.add_element();
				                       generate_expression(code, element);
			                       }
			                       Si::append(code, ")");
			                   },
		                       [&code](std::unique_ptr<expressions::closure> const &root)
		                       {
			                       Si::append(code, "[=]() { return ");
			                       generate_expression(code, root->result);
			                       Si::append(code, "; }");
			                   },
		                       [&code](std::unique_ptr<expressions::literal> const &root)
		                       {
			                       generate_value(code, root->value);
			                   },
		                       [&code](expressions::identifier const &root)
		                       {
			                       Si::append(code, root);
			                   },
		                       [&code](std::unique_ptr<expressions::tuple_element> const &root)
		                       {
			                       Si::append(code, "::std::get<");
			                       Si::append(code, boost::lexical_cast<std::string>(root->element));
			                       Si::append(code, ">(");
			                       generate_expression(code, root->tuple);
			                       Si::append(code, ")");
			                   });
	}

	template <class CharSink>
	void generate_function_definition(CharSink &&code, types::type const &result, Si::memory_range name,
	                                  types::type const &parameter, expressions::closure const &function)
	{
		generate_type(code, result);
		Si::append(code, " ");
		Si::append(code, name);
		Si::append(code, "(");
		generate_type(code, parameter);
		Si::append(code, " argument");
		Si::append(code, ")\n");
		Si::append(code, "{\n");
		Si::append(code, "    return ");
		generate_expression(code, function.result);
		Si::append(code, ";\n");
		Si::append(code, "}\n");
	}
}

BOOST_AUTO_TEST_CASE(cpp)
{
	using namespace warpcoil;
	types::tuple parameters;
	parameters.elements.emplace_back(types::integer());
	parameters.elements.emplace_back(Si::make_unique<types::tuple>());
	generate_function_definition(Si::ostream_ref_sink(std::cout), types::integer(), Si::make_c_str_range("func"),
	                             Si::to_unique(std::move(parameters)),
	                             expressions::closure{expressions::expression{Si::make_unique<expressions::tuple>()}});
}
