#include <warpcoil/warpcoil.hpp>

namespace warpcoil
{
    inline types::interface_definition create_benchmark_interface_a()
    {
        types::interface_definition definition;
        types::tuple parameters;
        parameters.elements.emplace_back(types::integer());
        parameters.elements.emplace_back(types::integer());
        definition.add_method("evaluate", types::integer())("argument", Si::to_unique(std::move(parameters)));
        return definition;
    }

	inline types::interface_definition create_benchmark_interface_b()
	{
		types::interface_definition definition;
		definition.add_method("evaluate", types::integer())("argument", types::utf8{ types::integer{ 0, 0xffffffff } });
		return definition;
	}
}
