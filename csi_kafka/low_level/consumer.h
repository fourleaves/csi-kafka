#include "client.h"

#pragma once
namespace csi
{
    namespace kafka
    {
        class lowlevel_consumer
        {
        public:
            typedef boost::function <void(const boost::system::error_code&)> connect_callback;
            typedef boost::function <void(const boost::system::error_code& ec1, csi::kafka::error_codes ec2)> set_offset_callback;
            typedef boost::function <void(const boost::system::error_code& ec1, csi::kafka::error_codes ec2, const csi::kafka::fetch_response::topic_data::partition_data&)> datastream_callback;
            typedef boost::function <void(const boost::system::error_code& ec1, csi::kafka::error_codes, std::shared_ptr<metadata_response>)> get_metadata_callback;

            lowlevel_consumer(boost::asio::io_service& io_service, const boost::asio::ip::tcp::resolver::query& query, const std::string& topic);

            void                                                           connect_async(connect_callback cb);
            boost::system::error_code                                      connect();
            void                                                           set_offset_async(int32_t partition, int64_t start_time, set_offset_callback cb);
            std::pair<boost::system::error_code, csi::kafka::error_codes>  set_offset(int32_t partition, int64_t start_time);
            void                                                           stream_async(datastream_callback cb);
            void                                                           get_metadata_async(get_metadata_callback cb);
            inline bool                                                    is_connected() const { return _client.is_connected(); }
        protected:
            boost::asio::io_service&        _ios;
            csi::kafka::low_level::client   _client;
            std::string                     _topic_name;
            std::vector<partition_cursor>   _cursors;
        };
    }
};