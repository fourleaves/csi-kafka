#include <deque>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <csi_kafka/lowlevel_client.h>

#pragma once

namespace csi {
namespace kafka {
class lowlevel_producer
{
  public:
  typedef std::function <void(const boost::system::error_code&)> connect_callback;
  typedef std::function <void(int32_t ec)>                       tx_ack_callback;

  lowlevel_producer(boost::asio::io_service& io_service, const std::string& topic, int32_t partition, int32_t required_acks, int32_t timeout, int32_t max_packet_size = -1);
  ~lowlevel_producer();
  boost::asio::io_service&          io_service() { return _ios; }
  void                              connect_async(const broker_address& address, int32_t timeout, connect_callback);
  boost::system::error_code         connect(const broker_address& address, int32_t timeout);

  void                              connect_async(const boost::asio::ip::tcp::resolver::query& query, int32_t timeout, connect_callback cb);
  boost::system::error_code         connect(const boost::asio::ip::tcp::resolver::query& query, int32_t timeout);
  void                              close();
  void                              send_async(std::shared_ptr<basic_message> message, tx_ack_callback = NULL);
  int32_t                           send_sync(std::shared_ptr<basic_message> message);

  inline bool                       is_connected() const { return _client.is_connected(); }
  inline bool                       is_connection_in_progress() const { return _client.is_connection_in_progress(); }
  inline int32_t                    partition() const { return _partition_id; }
  inline const std::string&         topic() const { return _topic; }

  size_t                            items_in_queue() const { return _tx_queue.size(); } // no lock but should not matter
  size_t                            bytes_in_queue() const { return _tx_queue_byte_size; } // no lock but should not matter
  size_t                            total_tx_bytes() const { return __metrics_total_tx_bytes; }
  size_t                            total_tx_msg() const { return __metrics_total_tx_msg; }

  protected:
  struct tx_item
  {
    tx_item(std::shared_ptr<basic_message> message) : msg(message) {}
    tx_item(std::shared_ptr<basic_message> message, tx_ack_callback callback) : msg(message), cb(callback) {}
    std::shared_ptr<basic_message> msg;
    tx_ack_callback                cb;
  };

  typedef boost::accumulators::accumulator_set<double, boost::accumulators::stats<boost::accumulators::tag::rolling_mean> >   metrics_accumulator_t;

  void handle_metrics_timer(const boost::system::error_code& ec);
  void _try_send(); // gets posted from enqueue so actual call comes from correct thread

  boost::asio::io_service&                   _ios;
  csi::kafka::lowlevel_client                _client;
  const std::string                          _topic;
  const int32_t                              _partition_id;
  //TX queue
  mutable csi::spinlock                      _spinlock;
  std::deque<tx_item>                        _tx_queue;
  size_t                                     _tx_queue_byte_size;
  bool                                       _tx_in_progress;
  bool                                       _try_send_posted;
  int32_t                                    _required_acks;
  int32_t                                    _tx_timeout;
  int32_t                                    _max_packet_size;

  //METRICS
  boost::asio::deadline_timer	               _metrics_timer;
  boost::posix_time::time_duration           _metrics_timeout;
  uint64_t                                   __metrics_total_tx_bytes;
  uint64_t                                   __metrics_total_tx_msg;
};
}
};