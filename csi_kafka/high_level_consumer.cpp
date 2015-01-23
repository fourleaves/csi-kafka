#include <algorithm>
#include <iostream>
#include "high_level_consumer.h"

namespace csi
{
    namespace kafka
    {
        highlevel_consumer::highlevel_consumer(boost::asio::io_service& io_service, const std::string& topic, int32_t rx_timeout, int32_t max_packet_size) :
            _ios(io_service),
            _timer(io_service),
            _timeout(boost::posix_time::milliseconds(5000)),
            _meta_client(io_service),
            _topic(topic), 
            _rx_timeout(rx_timeout),
            _max_packet_size(max_packet_size)
        {
        }

       
        highlevel_consumer::~highlevel_consumer()
        {
            _timer.cancel();
        }
        
        void highlevel_consumer::handle_timer(const boost::system::error_code& ec)
        {
            if (!ec)
                _try_connect_brokers();
        }

        void highlevel_consumer::close()
        {
            _timer.cancel();
            _meta_client.close();
            for (std::map<int, lowlevel_consumer2*>::iterator i = _partition2consumers.begin(); i != _partition2consumers.end(); ++i)
            {
                i->second->close();
            }
        }

        void highlevel_consumer::connect_async(const std::vector<broker_address>& brokers)
        {
            _meta_client.connect_async(brokers, [this](const boost::system::error_code& ec)
            {
                _ios.post([this]{ _try_connect_brokers(); });
            });
        }

        void highlevel_consumer::set_offset(int64_t start_time)
        {
            // return value??? TBD what to do if this fails and if # partitions changes???
            for (std::map<int, lowlevel_consumer2*>::iterator i = _partition2consumers.begin(); i != _partition2consumers.end(); ++i)
            {
                i->second->set_offset(start_time);
            }
        }

        /*
        void highlevel_consumer::refresh_metadata_async()
        {
            _meta_client.get_metadata_async({ _topic }, 0, [this](rpc_result<metadata_response> result)
            {
                _metadata = result;
            });
        }
        */


        void highlevel_consumer::_try_connect_brokers()
        {
            // the number of partitions is constand but the serving hosts might differ
            _meta_client.get_metadata_async({ _topic }, 0, [this](rpc_result<metadata_response> result)
            {
                if (!result)
                {
                    {
                        csi::kafka::spinlock::scoped_lock xxx(_spinlock);

                        if (_partition2consumers.size() == 0)
                        {
                            for (std::vector<csi::kafka::metadata_response::topic_data>::const_iterator i = result->topics.begin(); i != result->topics.end(); ++i)
                            {
                                assert(i->topic_name == _topic);
                                if (i->error_code)
                                {
                                    std::cerr << "metatdata for topic " << _topic << " failed: " << to_string((error_codes)i->error_code) << std::endl;
                                }
                                for (std::vector<csi::kafka::metadata_response::topic_data::partition_data>::const_iterator j = i->partitions.begin(); j != i->partitions.end(); ++j)
                                    _partition2consumers.insert(std::make_pair(j->partition_id, new lowlevel_consumer2(_ios, _topic, j->partition_id, _rx_timeout)));
                            };
                        }

                        for (std::vector<csi::kafka::broker_data>::const_iterator i = result->brokers.begin(); i != result->brokers.end(); ++i)
                        {
                            _broker2brokers[i->node_id] = *i;
                        };


                        for (std::vector<csi::kafka::metadata_response::topic_data>::const_iterator i = result->topics.begin(); i != result->topics.end(); ++i)
                        {
                            assert(i->topic_name == _topic);
                            for (std::vector<csi::kafka::metadata_response::topic_data::partition_data>::const_iterator j = i->partitions.begin(); j != i->partitions.end(); ++j)
                            {
                                _partition2partitions[j->partition_id] = *j;
                            };
                        };
                    }

                    for (std::map<int, lowlevel_consumer2*>::iterator i = _partition2consumers.begin(); i != _partition2consumers.end(); ++i)
                    {
                        if (!i->second->is_connected() && !i->second->is_connection_in_progress())
                        {
                            int partition = i->first;
                            int leader = _partition2partitions[partition].leader;
                            auto bd = _broker2brokers[leader];
                            broker_address broker_addr(bd.host_name, bd.port);
                            //boost::asio::ip::tcp::resolver::query query(bd.host_name, std::to_string(bd.port));
                            //std::string broker_uri = bd.host_name + ":" + std::to_string(bd.port);
                            std::cerr << "connecting to broker node_id:" << leader << " (" << to_string(broker_addr) << ") partition:" << partition << std::endl;
                            i->second->connect_async(broker_addr, 1000, [leader, partition, broker_addr](const boost::system::error_code& ec1)
                            {
                                if (ec1)
                                {
                                    std::cerr << "can't connect to broker #" << leader << " (" << to_string(broker_addr) << ") partition " << partition << " ec:" << ec1 << std::endl;
                                }
                                else
                                {
                                    std::cerr << "connected to broker #" << leader << " (" << to_string(broker_addr) << ") partition " << partition << std::endl;
                                }
                            });
                        }
                    }
                }
                _timer.expires_from_now(_timeout);
                _timer.async_wait(boost::bind(&highlevel_consumer::handle_timer, this, boost::asio::placeholders::error));
            });
        }

        void  highlevel_consumer::stream_async(datastream_callback cb)
        {
            size_t partitions = _partition2consumers.size();
            for (int i = 0; i != partitions; ++i)
            {
                _partition2consumers[i]->stream_async(cb);
            }
        }

        std::vector<highlevel_consumer::metrics>  highlevel_consumer::get_metrics() const
        {
            std::vector<metrics> metrics;
            for (std::map<int, lowlevel_consumer2*>::const_iterator i = _partition2consumers.begin(); i != _partition2consumers.end(); ++i)
            {
                highlevel_consumer::metrics item;
                item.partition = (*i).second->partition();
                //item.bytes_in_queue = (*i).second->bytes_in_queue();
                //item.msg_in_queue = (*i).second->items_in_queue();
                item.rx_kb_sec = (*i).second->metrics_kb_sec();
                item.rx_msg_sec = (*i).second->metrics_msg_sec();
                item.rx_roundtrip = (*i).second->metrics_rx_roundtrip();
                metrics.push_back(item);
            }
            return metrics;
        }
    };
};
