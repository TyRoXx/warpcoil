#include "generated.hpp"
#include "checkpoint.hpp"
#include <boost/test/unit_test.hpp>
#include <ventura/file_operations.hpp>

namespace
{
    struct local_directory : async_directory
    {
        explicit local_directory(ventura::absolute_path root)
            : m_root(std::move(root))
        {
        }

        void make_directory(std::string path, std::function<void(Si::error_or<std::tuple<>>)> on_result) override
        {
            boost::system::error_code ec =
                ventura::create_directories(m_root / ventura::relative_path(path), Si::return_);
            if (!!ec)
            {
                on_result(ec);
            }
            else
            {
                on_result(std::tuple<>());
            }
        }

        void remove(std::string path, std::function<void(Si::error_or<std::tuple<>>)> on_result) override
        {
            Si::error_or<boost::uintmax_t> ec =
                ventura::remove_all(m_root / ventura::relative_path(path), Si::variant_);
            if (ec.is_error())
            {
                on_result(ec.error());
            }
            else
            {
                on_result(std::tuple<>());
            }
        }

    private:
        ventura::absolute_path m_root;
    };
}

BOOST_AUTO_TEST_CASE(file_api_directory)
{
    local_directory directory(ventura::get_current_working_directory(Si::throw_));
    warpcoil::checkpoint got_result;
    got_result.enable();
    directory.make_directory("make_directory_test", [&got_result](Si::error_or<std::tuple<>> result)
                             {
                                 BOOST_CHECK(!result.is_error());
                                 got_result.enter();
                             });
}
