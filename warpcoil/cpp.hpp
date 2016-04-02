#pragma once

#include <warpcoil/warpcoil.hpp>
#include <silicium/sink/append.hpp>
#include <silicium/sink/ptr_sink.hpp>
#include <boost/lexical_cast.hpp>

namespace warpcoil
{
	namespace cpp
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

		struct indentation_level
		{
			explicit indentation_level(std::size_t amount = 0)
			    : m_amount(amount)
			{
			}

			indentation_level deeper() const
			{
				return indentation_level(m_amount + 1);
			}

			template <class CharSink>
			void render(CharSink &&sink) const
			{
				for (std::size_t i = 0; i < m_amount; ++i)
				{
					Si::append(sink, "    ");
				}
			}

		private:
			std::size_t m_amount;
		};

		enum class type_emptiness
		{
			empty,
			non_empty
		};

		template <class CharSink>
		type_emptiness generate_type(CharSink &&code, types::type const &root)
		{
			return Si::visit<type_emptiness>(
			    root,
			    [&code](types::integer)
			    {
				    Si::append(code, "::std::uint64_t");
				    return type_emptiness::non_empty;
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
				    return root->elements.empty() ? type_emptiness::empty
				                                  : type_emptiness::non_empty;
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
				    return root->elements.empty() ? type_emptiness::empty
				                                  : type_emptiness::non_empty;
				},
			    [&code](std::unique_ptr<types::subset> const &root)
			    {
				    Si::append(code, "/*subset of*/");
				    return generate_type(code, root->superset);
				});
		}

		template <class CharSink>
		void generate_value(CharSink &&code, values::value const &root)
		{
			return Si::visit<void>(
			    root,
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
		void generate_expression(CharSink &&code,
		                         expressions::expression const &root)
		{
			return Si::visit<void>(
			    root,
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
				    for (expressions::expression const &element :
				         root->elements)
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
				    Si::append(code,
				               boost::lexical_cast<std::string>(root->element));
				    Si::append(code, ">(");
				    generate_expression(code, root->tuple);
				    Si::append(code, ")");
				});
		}

		template <class CharSink>
		void generate_interface(CharSink &&code, indentation_level indentation,
		                        Si::memory_range name,
		                        types::interface_definition const &definition)
		{
			Si::append(code, "struct ");
			Si::append(code, name);
			Si::append(code, "\n");
			indentation.render(code);
			Si::append(code, "{\n");
			{
				indentation_level const in_class = indentation.deeper();
				in_class.render(code);
				Si::append(code, "virtual ~");
				Si::append(code, name);
				Si::append(code, "() {}\n");
				for (auto const &entry : definition.methods)
				{
					in_class.render(code);
					Si::append(code, "virtual ");
					generate_type(code, entry.second.result);
					Si::append(code, " ");
					Si::append(code, entry.first);
					Si::append(code, "(");
					switch (generate_type(code, entry.second.parameter))
					{
					case type_emptiness::empty:
						break;

					case type_emptiness::non_empty:
						Si::append(code, " argument");
						break;
					}
					Si::append(code, ") = 0;\n");
				}
			}
			indentation.render(code);
			Si::append(code, "};\n\n");
		}

		template <class CharSink>
		void generate_value_serialization(CharSink &&code,
		                                  indentation_level indentation,
		                                  Si::memory_range sink,
		                                  Si::memory_range value,
		                                  types::type const &type)
		{
			return Si::visit<void>(
			    type,
			    [&code, sink, value, indentation](types::integer)
			    {
				    indentation.render(code);
				    Si::append(code, "warpcoil::cpp::write_integer(");
				    Si::append(code, sink);
				    Si::append(code, ", ");
				    Si::append(code, value);
				    Si::append(code, ");\n");
				},
			    [&code](std::unique_ptr<types::variant> const &)
			    {
				    throw std::logic_error("to do");
				},
			    [&code, sink, value,
			     indentation](std::unique_ptr<types::tuple> const &root)
			    {
				    std::size_t element_index = 0;
				    for (types::type const &element : root->elements)
				    {
					    std::string element_name =
					        "std::get<" +
					        boost::lexical_cast<std::string>(element_index) +
					        ">(";
					    element_name.insert(element_name.end(), value.begin(),
					                        value.end());
					    element_name += ")";
					    generate_value_serialization(
					        code, indentation, sink,
					        Si::make_contiguous_range(element_name), element);
					    ++element_index;
				    }
				},
			    [&code](std::unique_ptr<types::subset> const &)
			    {
				    throw std::logic_error("to do");
				});
		}

		template <class CharSink>
		void generate_value_deserialization(CharSink &&code,
		                                    indentation_level indentation,
		                                    Si::memory_range source,
		                                    types::type const &type)
		{
			return Si::visit<void>(
			    type,
			    [&code, source](types::integer)
			    {
				    Si::append(code, "warpcoil::cpp::read_integer(");
				    Si::append(code, source);
				    Si::append(code, ")");
				},
			    [&code](std::unique_ptr<types::variant> const &)
			    {
				    throw std::logic_error("to do");
				},
			    [&code, source, &type,
			     indentation](std::unique_ptr<types::tuple> const &root)
			    {
				    Si::append(code, "[&] {\n");
				    {
					    indentation_level const in_lambda =
					        indentation.deeper();
					    in_lambda.render(code);
					    generate_type(code, type);
					    Si::append(code, " result;\n");
					    for (std::size_t i = 0; i < root->elements.size(); ++i)
					    {
						    in_lambda.render(code);
						    Si::append(code, "std::get<");
						    Si::append(code,
						               boost::lexical_cast<std::string>(i));
						    Si::append(code, ">(result) = ");
						    generate_value_deserialization(
						        code, in_lambda, source, root->elements[i]);
						    Si::append(code, ";\n");
					    }
					    in_lambda.render(code);
					    Si::append(code, "return result;\n");
				    }
				    indentation.render(code);
				    Si::append(code, "}()");
				},
			    [&code](std::unique_ptr<types::subset> const &)
			    {
				    throw std::logic_error("to do");
				});
		}

		template <class CharSink>
		void generate_serialization_client(
		    CharSink &&code, indentation_level indentation,
		    Si::memory_range name,
		    types::interface_definition const &definition)
		{
			Si::append(code, "struct ");
			Si::append(code, name);
			Si::append(code, "_client : ");
			Si::append(code, name);
			Si::append(code, "\n");
			indentation.render(code);
			Si::append(code, "{\n");
			{
				indentation_level const in_class = indentation.deeper();
				in_class.render(code);
				Si::append(code, "explicit ");
				Si::append(code, name);
				Si::append(code, "_client(Si::Sink<std::uint8_t, "
				                 "Si::success>::interface &requests, "
				                 "Si::Source<std::uint8_t>::interface "
				                 "&responses)\n");
				in_class.deeper().render(code);
				Si::append(code,
				           ": requests(requests), responses(responses) {}\n");
				for (auto const &entry : definition.methods)
				{
					in_class.render(code);
					generate_type(code, entry.second.result);
					Si::append(code, " ");
					Si::append(code, entry.first);
					Si::append(code, "(");
					switch (generate_type(code, entry.second.parameter))
					{
					case type_emptiness::empty:
						break;

					case type_emptiness::non_empty:
						Si::append(code, " argument");
						break;
					}
					Si::append(code, ") override\n");
					in_class.render(code);
					Si::append(code, "{\n");
					{
						indentation_level const in_method = in_class.deeper();
						generate_value_serialization(
						    code, in_method, Si::make_c_str_range("requests"),
						    Si::make_c_str_range("argument"),
						    entry.second.parameter);
						in_method.render(code);
						Si::append(code, "return ");
						generate_value_deserialization(
						    code, in_method, Si::make_c_str_range("responses"),
						    entry.second.result);
						Si::append(code, ";\n");
					}
					in_class.render(code);
					Si::append(code, "}\n");
				}
				indentation.render(code);
				Si::append(code, "private:\n");

				in_class.render(code);
				Si::append(code, "Si::Sink<std::uint8_t, "
				                 "Si::success>::interface &requests;\n");
				in_class.render(code);
				Si::append(code,
				           "Si::Source<std::uint8_t>::interface &responses;\n");
			}
			indentation.render(code);
			Si::append(code, "};\n\n");
		}

		template <class CharSink>
		void generate_serializable_interface(
		    CharSink &&code, indentation_level indentation,
		    Si::memory_range name,
		    types::interface_definition const &definition)
		{
			generate_interface(code, indentation, name, definition);
			generate_serialization_client(code, indentation, name, definition);
		}
	}
}
