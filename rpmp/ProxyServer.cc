#include "HPNL/Callback.h"
#include "HPNL/Connection.h"
#include "HPNL/ChunkMgr.h"
#include "HPNL/Server.h"

#include <iostream>
#include <string>
// #include "pmpool/proxy/ConsistentHash.h"
#include "ProxyServer.h"
// #include "pmpool/Event.h"

using namespace std;

ProxyRecvCallback::ProxyRecvCallback(std::shared_ptr<ProxyServer> proxyServer,
                                     std::shared_ptr<ChunkMgr> chunkMgr)
    : proxyServer_(proxyServer), chunkMgr_(chunkMgr) {}

void ProxyRecvCallback::operator()(void* param_1, void* param_2) {
  cout << "ProxyRecvCallback::operator()" <<endl;
  int mid = *static_cast<int*>(param_1);
  auto chunk = chunkMgr_->get(mid);
  auto request = std::make_shared<ProxyRequest>(
      reinterpret_cast<char*>(chunk->buffer), chunk->size,
      reinterpret_cast<Connection*>(chunk->con));
  request->decode();
  ProxyRequestMsg* requestMsg = (ProxyRequestMsg*)(request->getData());
  if (requestMsg->type != 0) {
    proxyServer_->enqueue_recv_msg(request);
  } else {
    std::cout << "[RecvCallback::RecvCallback][" << requestMsg->type
              << "] size is " << chunk->size << std::endl;
    for (int i = 0; i < chunk->size; i++) {
      printf("%X ", *(request->getData() + i));
    }
    printf("\n");
  }
  chunkMgr_->reclaim(chunk, static_cast<Connection*>(chunk->con));
}

/**
 * The SendCallback is mainly for the reclaim of chunk
 */
ProxySendCallback::ProxySendCallback(std::shared_ptr<ChunkMgr> chunkMgr)
    : chunkMgr_(chunkMgr) {}

void ProxySendCallback::operator()(void* param_1, void* param_2) {
  cout << "ProxySendCallback::operator()" << endl;
  int mid = *static_cast<int*>(param_1);
  auto chunk = chunkMgr_->get(mid);
  auto connection = static_cast<Connection*>(chunk->con);
  chunkMgr_->reclaim(chunk, connection);
}

Worker::Worker(std::shared_ptr<ProxyServer> proxyServer) : proxyServer_(proxyServer) {}

void Worker::addTask(std::shared_ptr<ProxyRequest> request) {
  pendingRecvRequestQueue_.enqueue(request);
}

void Worker::abort() {}

int Worker::entry() {
  std::shared_ptr<ProxyRequest> request;
  bool res = pendingRecvRequestQueue_.wait_dequeue_timed(
      request, std::chrono::milliseconds(1000));
  if (res) {
    proxyServer_->handle_recv_msg(request);
  }
  return 0;
}

ProxyServer::ProxyServer(std::shared_ptr<Config> config, std::shared_ptr<Log> log) :
 config_(config), log_(log) {}

ProxyServer::~ProxyServer() {
    worker_->stop();
    worker_->join();
}

void ProxyServer::enqueue_recv_msg(std::shared_ptr<ProxyRequest> request) {
  worker_->addTask(request);
}

void ProxyServer::handle_recv_msg(std::shared_ptr<ProxyRequest> request) {
  ProxyRequestContext rc = request->get_rc();
  string node = consistentHash_->getNode(rc.key).getKey();
//   cout << "get node from consistent hash: " << node << endl;
  auto rrc = ProxyRequestReplyContext();
  rrc.type = rc.type;
  rrc.success = 0;
  rrc.rid = rc.rid;
  rrc.host = node;
//   rrc.host = const_cast<char*> (node.c_str());
  // memcpy(rrc.host, node.c_str(), node.length());
  rrc.con = rc.con;
  std::shared_ptr<ProxyRequestReply> requestReply = std::make_shared<ProxyRequestReply>(rrc);
  // rrc.ck->ptr = requestReply.get();

  requestReply->encode();
  auto ck = chunkMgr_->get(rrc.con);
  memcpy(reinterpret_cast<char *>(ck->buffer), requestReply->data_, requestReply->size_);
  ck->size = requestReply->size_;
  rrc.con->send(ck);
}

bool ProxyServer::launchServer() {
  /**
   * Set the number of virtual nodes for load balance
   */
  int loadBalanceFactor = 5;
  /**
   * The nodes should be come from config, hard code for temp use
   */
  consistentHash_ = std::make_shared<ConsistentHash<PhysicalNode>>();
  vector<string> nodes = config_->get_nodes();
  for (string node : nodes) {
    PhysicalNode* physicalNode = new PhysicalNode(node);
    consistentHash_->addNode(*physicalNode, loadBalanceFactor);
  }

  int worker_number = config_->get_network_worker_num();
  int buffer_number = config_->get_network_buffer_num();
  int buffer_size = config_->get_network_buffer_size();
  server_ = std::make_shared<Server>(worker_number, buffer_number);
  if(server_->init() != 0){
    cout<<"HPNL server init failed"<<endl;
    return false;
  }
  chunkMgr_ = std::make_shared<ChunkPool>(server_.get(), buffer_size, buffer_number);
  server_->set_chunk_mgr(chunkMgr_.get());

  recvCallback_ = std::make_shared<ProxyRecvCallback>(shared_from_this(), chunkMgr_);
  sendCallback_ = std::make_shared<ProxySendCallback>(chunkMgr_);
  shutdownCallback_ = std::make_shared<ProxyShutdownCallback>();
  connectCallback_ = std::make_shared<ProxyConnectCallback>();

  worker_ = std::make_shared<Worker>(shared_from_this());
  worker_->start();

  server_->set_recv_callback(recvCallback_.get());
  server_->set_send_callback(sendCallback_.get());
  server_->set_connected_callback(connectCallback_.get());
  server_->set_shutdown_callback(shutdownCallback_.get());

  server_->start();
  server_->listen(config_->get_proxy_ip().c_str(), config_->get_proxy_port().c_str());
  log_->get_file_log()->info("Proxy server started at ", config_->get_proxy_ip(), ":", config_->get_proxy_port());
  server_->wait();
  return true;
}

int main(int argc, char* argv[]){
  std::shared_ptr<Config> config = std::make_shared<Config>();
  if (argc > 1) {
    CHK_ERR("init config", config->init(argc, argv));
  } else {
    config->readFromFile();
  }
  std::shared_ptr<Log> log = std::make_shared<Log>(config.get());
  // ProxyServer* proxyServer;
  std::shared_ptr<ProxyServer> proxyServer = std::make_shared<ProxyServer>(config, log);
  proxyServer->launchServer();
  return 0;
}
