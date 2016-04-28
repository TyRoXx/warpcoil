#pragma once

#include <map>
#include <silicium/to_unique.hpp>
#include <silicium/variant.hpp>
#include <vector>

namespace warpcoil
{
    namespace expressions
    {
        typedef std::string identifier;

        struct call;
        struct tuple;
        struct closure;
        struct literal;
        struct tuple_element;

        typedef Si::variant<std::unique_ptr<call>, std::unique_ptr<tuple>, std::unique_ptr<closure>,
                            std::unique_ptr<literal>, identifier, std::unique_ptr<tuple_element>> expression;

        struct call
        {
            expression callee;
            expression argument;
        };

        struct tuple
        {
            std::vector<expression> elements;
        };

        struct closure
        {
            expression result;
        };

        struct tuple_element
        {
            expression tuple;
            std::size_t element;
        };
    }

    namespace values
    {
        typedef std::uint64_t integer;

        struct closure;
        struct tuple;

        typedef Si::variant<integer, std::unique_ptr<closure>, std::unique_ptr<tuple>> value;

        struct closure
        {
            std::map<expressions::identifier, value> bound;
            expressions::expression result;
        };

        struct tuple
        {
            std::vector<value> elements;
        };
    }

    namespace expressions
    {
        struct literal
        {
            values::value value;
        };
    }

    namespace types
    {
        struct integer
        {
            std::uint64_t minimum;
            std::uint64_t maximum;

            integer()
                : minimum(0)
                , maximum((std::numeric_limits<std::uint64_t>::max)())
            {
            }

            integer(std::uint64_t minimum, std::uint64_t maximum)
                : minimum(minimum)
                , maximum(maximum)
            {
            }
        };

        struct utf8
        {
            integer code_units;
        };

        struct variant;
        struct tuple;
        struct subset;
        struct vector;

        typedef Si::variant<integer, std::unique_ptr<variant>, std::unique_ptr<tuple>, std::unique_ptr<subset>,
                            std::unique_ptr<vector>, utf8> type;

        type clone(type const &original);

        struct variant
        {
            std::vector<type> elements;

            variant clone() const
            {
                variant result;
                for (type const &element : elements)
                {
                    result.elements.emplace_back(types::clone(element));
                }
                return result;
            }
        };

        struct tuple
        {
            std::vector<type> elements;

            tuple clone() const
            {
                tuple result;
                for (type const &element : elements)
                {
                    result.elements.emplace_back(types::clone(element));
                }
                return result;
            }
        };

        struct subset
        {
            type superset;
            values::closure is_inside;

            subset clone() const
            {
                throw std::logic_error("not implemented");
            }
        };

        struct vector
        {
            integer length;
            type element;

            vector clone() const
            {
                return {length, types::clone(element)};
            }
        };

        struct parameter
        {
            std::string name;
            type type;
        };

        inline type clone(type const &original)
        {
            // integer, std::unique_ptr<variant>, std::unique_ptr<tuple>, std::unique_ptr<subset>,
            // std::unique_ptr<vector>, utf8
            return Si::visit<type>(original,
                                   [](integer value)
                                   {
                                       return value;
                                   },
                                   [](std::unique_ptr<variant> const &value)
                                   {
                                       return Si::to_unique(value->clone());
                                   },
                                   [](std::unique_ptr<tuple> const &value)
                                   {
                                       return Si::to_unique(value->clone());
                                   },
                                   [](std::unique_ptr<subset> const &value)
                                   {
                                       return Si::to_unique(value->clone());
                                   },
                                   [](std::unique_ptr<vector> const &value)
                                   {
                                       return Si::to_unique(value->clone());
                                   },
                                   [](utf8 const &value)
                                   {
                                       return value;
                                   });
        }

        inline tuple get_parameter_type(std::vector<parameter> const &parameters)
        {
            tuple result;
            result.elements.reserve(parameters.size());
            for (parameter const &parameter : parameters)
            {
                result.elements.emplace_back(clone(parameter.type));
            }
            return result;
        }

        struct method
        {
            type result;
            std::vector<parameter> parameters;
        };

        struct interface_definition
        {
            std::map<expressions::identifier, method> methods;
        };
    }

    template <class T, class... Elements>
    std::vector<T> make_vector(Elements &&... elements)
    {
        std::vector<T> result;
        result.reserve(sizeof...(Elements));
        (void)std::initializer_list<int>{(result.emplace_back(std::forward<Elements>(elements)), 0)...};
        return result;
    }
}
