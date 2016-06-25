#pragma once

#include <map>
#include <silicium/to_unique.hpp>
#include <silicium/variant.hpp>
#include <vector>

namespace warpcoil
{
    typedef std::string identifier;

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
        struct vector;

        typedef Si::variant<integer, std::unique_ptr<variant>, std::unique_ptr<tuple>, std::unique_ptr<vector>, utf8>
            type;

        BOOST_STATIC_ASSERT((std::is_nothrow_move_constructible<type>::value));
        BOOST_STATIC_ASSERT((std::is_nothrow_move_assignable<type>::value));

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
            type type_;
        };

        inline type clone(type const &original)
        {
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
                                   [](std::unique_ptr<vector> const &value)
                                   {
                                       return Si::to_unique(value->clone());
                                   },
                                   [](utf8 const &value)
                                   {
                                       return value;
                                   });
        }

        inline type get_parameter_type(std::vector<parameter> const &parameters)
        {
            tuple result;
            result.elements.reserve(parameters.size());
            for (parameter const &parameter_ : parameters)
            {
                result.elements.emplace_back(clone(parameter_.type_));
            }
            return type{Si::to_unique(std::move(result))};
        }

        struct method
        {
            type result;
            std::vector<parameter> parameters;
        };

        struct interface_definition
        {
            std::map<identifier, method> methods;

            auto add_method(identifier name, type result)
            {
                method &target =
                    methods.insert(std::make_pair(std::move(name), method{std::move(result), {}})).first->second;
                struct parameter_maker
                {
                    method &target;

                    parameter_maker operator()(identifier name, type type_) const
                    {
                        target.parameters.emplace_back(parameter{std::move(name), std::move(type_)});
                        return *this;
                    }
                };
                return parameter_maker{target};
            }
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
