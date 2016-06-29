#include <warpcoil/warpcoil.hpp>

namespace warpcoil
{
    inline types::interface_definition create_publisher_interface()
    {
        types::interface_definition definition;
        definition.add_method("publish", Si::to_unique(types::tuple{{}}))("message",
                                                                          types::utf8{types::integer{0, 255}});
        return definition;
    }

    inline types::interface_definition create_viewer_interface()
    {
        types::interface_definition definition;
        definition.add_method("display", Si::to_unique(types::tuple{{}}))("message",
                                                                          types::utf8{types::integer{0, 255}});
        return definition;
    }
}
