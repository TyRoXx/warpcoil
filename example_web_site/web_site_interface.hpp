#include <warpcoil/warpcoil.hpp>

namespace warpcoil
{
	inline types::interface_definition create_web_site_interface()
	{
		types::interface_definition definition;
		definition.add_method("hello", types::utf8{ types::integer{ 0, 255 } })("name",
			types::utf8{ types::integer{ 0, 255 } });
		return definition;
	}
}
