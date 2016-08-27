#pragma once

#include <warpcoil/identifier.hpp>
#include <map>
#include <vector>
#include <silicium/to_unique.hpp>
#include <silicium/variant.hpp>

namespace warpcoil
{
    enum class type_emptiness
    {
        empty,
        non_empty
    };

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
        struct structure;

        typedef Si::variant<integer, std::unique_ptr<variant>, std::unique_ptr<tuple>, std::unique_ptr<vector>, utf8,
                            std::unique_ptr<structure>> type;

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

        bool less(type const &left, type const &right);

        inline bool less(variant const &left, variant const &right)
        {
            return std::lexicographical_compare(left.elements.begin(), left.elements.end(), right.elements.begin(),
                                                right.elements.end(), [](type const &left, type const &right)
                                                {
                                                    return less(left, right);
                                                });
        }

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

        inline bool less(tuple const &left, tuple const &right)
        {
            return std::lexicographical_compare(left.elements.begin(), left.elements.end(), right.elements.begin(),
                                                right.elements.end(), [](type const &left, type const &right)
                                                {
                                                    return less(left, right);
                                                });
        }

        struct vector
        {
            integer length;
            type element;

            vector clone() const
            {
                return {length, types::clone(element)};
            }
        };

        bool less(integer const &left, integer const &right);

        inline bool less(vector const &left, vector const &right)
        {
            if (less(left.length, right.length))
            {
                return true;
            }
            if (less(right.length, left.length))
            {
                return false;
            }
            return less(left.element, right.element);
        }

        struct structure
        {
            struct element
            {
                type what;
                identifier name;
            };

            std::vector<element> elements;

            structure clone() const
            {
                structure result;
                for (element const &e : elements)
                {
                    result.elements.emplace_back(element{types::clone(e.what), e.name});
                }
                return result;
            }
        };

        inline bool less(integer const &left, integer const &right)
        {
            return std::tie(left.minimum, left.maximum) < std::tie(right.minimum, right.maximum);
        }

        bool less(structure const &left, structure const &right);

        inline bool less(type const &left, type const &right)
        {
            if (left.index() < right.index())
            {
                return true;
            }
            if (left.index() > right.index())
            {
                return false;
            }
            return Si::visit<bool>(
                left,
                [&right](integer const &left_value)
                {
                    return less(left_value, *Si::try_get_ptr<std::decay<decltype(left_value)>::type>(right));
                },
                [&right](std::unique_ptr<variant> const &left_value)
                {
                    return less(*left_value, **Si::try_get_ptr<std::decay<decltype(left_value)>::type>(right));
                },
                [&right](std::unique_ptr<tuple> const &left_value)
                {
                    return less(*left_value, **Si::try_get_ptr<std::decay<decltype(left_value)>::type>(right));
                },
                [&right](std::unique_ptr<vector> const &left_value)
                {
                    return less(*left_value, **Si::try_get_ptr<std::decay<decltype(left_value)>::type>(right));
                },
                [&right](utf8 const &left_value)
                {
                    return less(left_value.code_units,
                                Si::try_get_ptr<std::decay<decltype(left_value)>::type>(right)->code_units);
                },
                [&right](std::unique_ptr<structure> const &left_value)
                {
                    return less(*left_value, **Si::try_get_ptr<std::decay<decltype(left_value)>::type>(right));
                });
        }

        inline bool less(structure const &left, structure const &right)
        {
            return std::lexicographical_compare(left.elements.begin(), left.elements.end(), right.elements.begin(),
                                                right.elements.end(),
                                                [](structure::element const &left, structure::element const &right)
                                                {
                                                    if (less(left.what, right.what))
                                                    {
                                                        return true;
                                                    }
                                                    if (less(right.what, left.what))
                                                    {
                                                        return false;
                                                    }
                                                    return (left.name < right.name);
                                                });
        }

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
                                   },
                                   [](std::unique_ptr<structure> const &value)
                                   {
                                       return Si::to_unique(value->clone());
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
}
