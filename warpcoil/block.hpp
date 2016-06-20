#pragma once

#include <silicium/sink/append.hpp>

namespace warpcoil
{
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
    void continuation_block(CharSink &&code, indentation_level indentation, ContentGenerator &&content, char const *end)
    {
        Si::append(code, "{\n");
        {
            indentation_level const in_block = indentation.deeper();
            std::forward<ContentGenerator>(content)(in_block);
        }
        indentation.render(code);
        Si::append(code, "}");
        Si::append(code, end);
    }

    template <class CharSink, class ContentGenerator>
    void block(CharSink &&code, indentation_level indentation, ContentGenerator &&content, char const *end)
    {
        indentation.render(code);
        continuation_block(code, indentation, content, end);
    }
}
