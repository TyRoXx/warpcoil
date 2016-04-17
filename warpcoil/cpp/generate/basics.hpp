#pragma once

#include <boost/lexical_cast.hpp>
#include <silicium/sink/append.hpp>
#include <silicium/sink/ptr_sink.hpp>
#include <warpcoil/warpcoil.hpp>

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

		template <class CharSink, class... Content>
		void start_line(CharSink &&code, indentation_level indentation, Content const &... content)
		{
			indentation.render(code);
			Si::unit dummy[] = {(Si::append(code, content), Si::unit())...};
			Si::ignore_unused_variable_warning(dummy);
		}

		template <class CharSink, class ContentGenerator>
		void block(CharSink &&code, indentation_level indentation, ContentGenerator &&content, char const *end)
		{
			indentation.render(code);
			Si::append(code, "{\n");
			{
				indentation_level const in_block = indentation.deeper();
				std::forward<ContentGenerator>(content)(in_block);
			}
			indentation.render(code);
			Si::append(code, "}");
			Si::append(code, end);
		}

		inline Si::memory_range find_suitable_uint_cpp_type(types::integer range)
		{
			if (range.maximum <= 0xffu)
			{
				return Si::make_c_str_range("std::uint8_t");
			}
			else if (range.maximum <= 0xffffu)
			{
				return Si::make_c_str_range("std::uint16_t");
			}
			else if (range.maximum <= 0xffffffffu)
			{
				return Si::make_c_str_range("std::uint32_t");
			}
			else
			{
				return Si::make_c_str_range("std::uint64_t");
			}
		}

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
			    [&code](types::integer range)
			    {
				    Si::append(code, find_suitable_uint_cpp_type(range));
				    return type_emptiness::non_empty;
				},
			    [&code](std::unique_ptr<types::variant> const &root)
			    {
				    Si::append(code, "Si::variant<");
				    auto comma = make_comma_separator(Si::ref_sink(code));
				    for (types::type const &element : root->elements)
				    {
					    comma.add_element();
					    generate_type(code, element);
				    }
				    Si::append(code, ">");
				    return root->elements.empty() ? type_emptiness::empty : type_emptiness::non_empty;
				},
			    [&code](std::unique_ptr<types::tuple> const &root)
			    {
				    Si::append(code, "std::tuple<");
				    auto comma = make_comma_separator(Si::ref_sink(code));
				    for (types::type const &element : root->elements)
				    {
					    comma.add_element();
					    generate_type(code, element);
				    }
				    Si::append(code, ">");
				    return root->elements.empty() ? type_emptiness::empty : type_emptiness::non_empty;
				},
			    [&code](std::unique_ptr<types::subset> const &root)
			    {
				    Si::append(code, "/*subset of*/");
				    return generate_type(code, root->superset);
				},
			    [&code](std::unique_ptr<types::vector> const &root)
			    {
				    Si::append(code, "std::vector<");
				    generate_type(code, root->element);
				    Si::append(code, ">");
				    return type_emptiness::non_empty;
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
				                       Si::append(code, "std::make_tuple(");
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
				                       Si::append(code, "std::make_tuple(");
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
				                       Si::append(code, "std::get<");
				                       Si::append(code, boost::lexical_cast<std::string>(root->element));
				                       Si::append(code, ">(");
				                       generate_expression(code, root->tuple);
				                       Si::append(code, ")");
				                   });
		}

		template <class CharSink>
		void generate_value_serialization(CharSink &&code, indentation_level indentation, Si::memory_range sink,
		                                  Si::memory_range value, types::type const &type)
		{
			return Si::visit<void>(
			    type,
			    [&code, sink, value, indentation](types::integer range)
			    {
				    indentation.render(code);
				    Si::append(code, "warpcoil::cpp::write_integer<");
				    Si::append(code, find_suitable_uint_cpp_type(range));
				    Si::append(code, ">(");
				    Si::append(code, sink);
				    Si::append(code, ", ");
				    Si::append(code, value);
				    Si::append(code, ");\n");
				},
			    [&code](std::unique_ptr<types::variant> const &)
			    {
				    throw std::logic_error("to do");
				},
			    [&code, sink, value, indentation](std::unique_ptr<types::tuple> const &root)
			    {
				    std::size_t element_index = 0;
				    for (types::type const &element : root->elements)
				    {
					    std::string element_name = "std::get<" + boost::lexical_cast<std::string>(element_index) + ">(";
					    element_name.insert(element_name.end(), value.begin(), value.end());
					    element_name += ")";
					    generate_value_serialization(code, indentation, sink, Si::make_contiguous_range(element_name),
					                                 element);
					    ++element_index;
				    }
				},
			    [&code](std::unique_ptr<types::subset> const &)
			    {
				    throw std::logic_error("to do");
				},
			    [&code, sink, value, indentation](std::unique_ptr<types::vector> const &root)
			    {
				    {
					    std::string size = "static_cast<";
					    Si::memory_range const size_type = find_suitable_uint_cpp_type(root->length);
					    size.append(size_type.begin(), size_type.end());
					    size += ">(";
					    size.append(value.begin(), value.end());
					    size += ".size())";
					    generate_value_serialization(code, indentation, sink, Si::make_contiguous_range(size),
					                                 root->length);
				    }
				    indentation.render(code);
				    Si::append(code, "for (auto const &element : ");
				    Si::append(code, value);
				    Si::append(code, ")\n");
				    indentation.render(code);
				    Si::append(code, "{\n");
				    {
					    indentation_level const in_for = indentation.deeper();
					    generate_value_serialization(code, in_for, sink, Si::make_c_str_range("element"),
					                                 root->element);
				    }
				    indentation.render(code);
				    Si::append(code, "}\n");
				});
		}

		template <class CharSink>
		void generate_value_deserialization(CharSink &&code, indentation_level indentation, Si::memory_range source,
		                                    types::type const &type)
		{
			return Si::visit<void>(type,
			                       [&code, source](types::integer range)
			                       {
				                       Si::append(code, "warpcoil::cpp::read_integer<");
				                       Si::append(code, find_suitable_uint_cpp_type(range));
				                       Si::append(code, ">(");
				                       Si::append(code, source);
				                       Si::append(code, ")");
				                   },
			                       [&code](std::unique_ptr<types::variant> const &)
			                       {
				                       throw std::logic_error("to do");
				                   },
			                       [&code, source, &type, indentation](std::unique_ptr<types::tuple> const &root)
			                       {
				                       Si::append(code, "[&] {\n");
				                       {
					                       indentation_level const in_lambda = indentation.deeper();
					                       in_lambda.render(code);
					                       generate_type(code, type);
					                       Si::append(code, " result;\n");
					                       for (std::size_t i = 0; i < root->elements.size(); ++i)
					                       {
						                       in_lambda.render(code);
						                       Si::append(code, "std::get<");
						                       Si::append(code, boost::lexical_cast<std::string>(i));
						                       Si::append(code, ">(result) = ");
						                       generate_value_deserialization(code, in_lambda, source,
						                                                      root->elements[i]);
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
				                   },
			                       [&code, source, indentation, &type](std::unique_ptr<types::vector> const &root)
			                       {
				                       Si::append(code, "[&] {\n");
				                       {
					                       indentation_level const in_lambda = indentation.deeper();
					                       in_lambda.render(code);
					                       generate_type(code, type);
					                       Si::append(code, " result(");
					                       generate_value_deserialization(code, in_lambda, source, root->length);
					                       Si::append(code, ");\n");
					                       in_lambda.render(code);
					                       Si::append(code, "for (auto &element : result) { element = ");
					                       generate_value_deserialization(code, in_lambda, source, root->element);
					                       Si::append(code, "; }\n");
					                       in_lambda.render(code);
					                       Si::append(code, "return result;\n");
				                       }
				                       indentation.render(code);
				                       Si::append(code, "}()");
				                   });
		}

		template <class CharSink>
		void generate_parameters(CharSink &&code, types::type const &parameter)
		{
			switch (generate_type(code, parameter))
			{
			case type_emptiness::empty:
				break;

			case type_emptiness::non_empty:
				Si::append(code, " argument");
				break;
			}
		}
	}
}
