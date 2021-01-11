#ifndef PMPOOL_REPLICAEVENT_H_
#define PMPOOL_REPLICAEVENT_H_

#include <HPNL/ChunkMgr.h>
#include <HPNL/Connection.h>

#include <stdint.h>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <vector>
#include <sstream>

using std::vector;

// class ProxyClientRecvCallback;
// class ProxyRequestHandler;
class ProxyServer;
// class DataServiceRequestHandler;
class Protocol;
class ReplicaService;
class DataServiceRequestHandler;

enum ReplicaOpType : uint32_t { REGISTER = 1, REPLICATE, REPLICA_REPLY };

struct ReplicaRequestMsg {
  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar& type;
    ar& rid;
    ar& key;
    ar& node;
    ar& src_address;
  }
  uint32_t type;
  uint64_t rid;
  uint64_t key;
  std::string node;
  uint64_t src_address;
};

struct ReplicaRequestReplyMsg {
  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar& type;
    ar& success;
    ar& rid;
    ar& key;
    ar& node;
    ar& src_address;
  }
  uint32_t type;
  uint32_t success;
  uint64_t rid;
  uint64_t key;
  std::string node;
  uint64_t src_address;
};

struct ReplicaRequestReplyContext {
  ReplicaOpType type;
  uint32_t success;
  uint64_t rid;
  uint64_t key;
  std::string node;
  uint64_t src_address;
  Connection* con;
};

class ReplicaRequestReply {
 public:
  ReplicaRequestReply() = delete;
  explicit ReplicaRequestReply(ReplicaRequestReplyContext& requestReplyContext)
      : data_(nullptr), size_(0), requestReplyContext_(requestReplyContext){};
  ReplicaRequestReply(char* data, uint64_t size, Connection* con)
      : size_(size) {
    data_ = static_cast<char*>(std::malloc(size_));
    memcpy(data_, data, size_);
    requestReplyContext_ = ReplicaRequestReplyContext();
    requestReplyContext_.con = con;
  };
  ~ReplicaRequestReply() {
    const std::lock_guard<std::mutex> lock(data_lock_);
    if (data_ != nullptr) {
      std::free(data_);
      data_ = nullptr;
    }
  };
  ReplicaRequestReplyContext& get_rrc() { return requestReplyContext_; };
  void set_rrc(ReplicaRequestReplyContext& rrc) {
    memcpy(&requestReplyContext_, &rrc, sizeof(ReplicaRequestReplyContext));
  };
  void encode() {
    const std::lock_guard<std::mutex> lock(data_lock_);
    ReplicaRequestReplyMsg requestReplyMsg;
    requestReplyMsg.type = (ReplicaOpType)requestReplyContext_.type;
    requestReplyMsg.success = requestReplyContext_.success;
    requestReplyMsg.rid = requestReplyContext_.rid;
    requestReplyMsg.key = requestReplyContext_.key;
    requestReplyMsg.node = requestReplyContext_.node;
    requestReplyMsg.src_address = requestReplyContext_.src_address;
    std::ostringstream os;
    boost::archive::text_oarchive ao(os);
    ao << requestReplyMsg;
    size_ = os.str().length() + 1;
    data_ = static_cast<char*>(std::malloc(size_));
    memcpy(data_, os.str().c_str(), size_);
  };
  void decode() {
    const std::lock_guard<std::mutex> lock(data_lock_);
    if (data_ == nullptr) {
      std::string err_msg = "Decode with null data";
      std::cerr << err_msg << std::endl;
      throw;
    }
    ReplicaRequestReplyMsg requestReplyMsg;
    std::string str(data_);
    std::istringstream is(str);
    boost::archive::text_iarchive ia(is);
    ia >> requestReplyMsg;
    requestReplyContext_.type = (ReplicaOpType)requestReplyMsg.type;
    requestReplyContext_.success = requestReplyMsg.success;
    requestReplyContext_.rid = requestReplyMsg.rid;
    requestReplyContext_.key = requestReplyMsg.key;
    requestReplyContext_.node = requestReplyMsg.node;
    requestReplyContext_.src_address = requestReplyMsg.src_address;
  };

 private:
  std::mutex data_lock_;
    friend ProxyServer;
    friend Protocol;
  char* data_ = nullptr;
  uint64_t size_ = 0;
  ReplicaRequestReplyContext requestReplyContext_;
};

struct ReplicaRequestContext {
  ReplicaOpType type;
  uint64_t rid;
  uint64_t key;
  std::string node;
  uint64_t src_address;
  Connection* con;
};

class ReplicaRequest {
 public:
  ReplicaRequest() = delete;
  explicit ReplicaRequest(ReplicaRequestContext requestContext)
      : data_(nullptr), size_(0), requestContext_(requestContext){};
  ReplicaRequest(char* data, uint64_t size, Connection* con) : size_(size) {
    data_ = static_cast<char*>(std::malloc(size));
    memcpy(data_, data, size_);
    requestContext_.con = con;
  };
  ~ReplicaRequest() {
    const std::lock_guard<std::mutex> lock(data_lock_);
    if (data_ != nullptr) {
      std::free(data_);
      data_ = nullptr;
    }
  };
  ReplicaRequestContext& get_rc() { return requestContext_; };
  void encode() {
    const std::lock_guard<std::mutex> lock(data_lock_);
    ReplicaRequestMsg requestMsg;
    requestMsg.type = requestContext_.type;
    requestMsg.rid = requestContext_.rid;
    requestMsg.key = requestContext_.key;
    requestMsg.node = requestContext_.node;
    requestMsg.src_address = requestContext_.src_address;
    std::ostringstream os;
    boost::archive::text_oarchive ao(os);
    ao << requestMsg;
    size_ = os.str().length() + 1;
    data_ = static_cast<char*>(std::malloc(size_));
    memcpy(data_, os.str().c_str(), size_);
  };
  void decode() {
    const std::lock_guard<std::mutex> lock(data_lock_);
    if (data_ == nullptr) {
      std::string err_msg = "Decode with null data";
      std::cerr << err_msg << std::endl;
      throw;
    }
    ReplicaRequestMsg requestMsg;
    std::string str(data_);
    std::istringstream is(str);
    boost::archive::text_iarchive ia(is);
    ia >> requestMsg;
    requestContext_.type = (ReplicaOpType)requestMsg.type;
    requestContext_.rid = requestMsg.rid;
    requestContext_.key = requestMsg.key;
    requestContext_.node = requestMsg.node;
    requestContext_.src_address = requestMsg.src_address;
  };
  char* getData() { return data_; }
  uint64_t getSize() { return size_; }

 private:
  std::mutex data_lock_;
  //   friend ProxyClientRecvCallback;
  //   friend ProxyRequestHandler;
  //   friend DataServiceRequestHandler;
    friend Protocol;
  friend ReplicaService;
  friend DataServiceRequestHandler;
  char* data_;
  uint64_t size_;
  ReplicaRequestContext requestContext_;
};

#endif  // PMPOOL_REPLICAEVENT_H_
