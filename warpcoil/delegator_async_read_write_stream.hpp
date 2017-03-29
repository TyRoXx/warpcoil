DELEGATOR_NAME(async_read_write_stream)
DELEGATOR_BASIC_METHOD(async_read_some, BOOST_PP_EMPTY(), void, boost::asio::mutable_buffer,
                       Si::function<void(boost::system::error_code, std::size_t)>)
DELEGATOR_BASIC_METHOD(async_write_some, BOOST_PP_EMPTY(), void,
                       std::vector<boost::asio::const_buffer>,
                       Si::function<void(boost::system::error_code, std::size_t)>)
