/*
 * Filename: /mnt/spark-pmof/tool/rpmp/pmpool/client/PmPoolClient.cc
 * Path: /mnt/spark-pmof/tool/rpmp/pmpool/client
 * Created Date: Friday, December 13th 2019, 3:44:08 pm
 * Author: root
 *
 * Copyright (c) 2019 Intel
 */

#include "pmpool/client/PmPoolClient.h"

#include "NetworkClient.h"
#include "pmpool/Digest.h"
#include "pmpool/Event.h"
#include "pmpool/Protocol.h"
#include "pmpool/client/ProxyClient.h"


PmPoolClient::PmPoolClient(const string &proxy_address,
    const string &proxy_port) {
  std::cout<<"PmPoolClient::PmPoolclient: start"<<endl;
  tx_finished = true;
  op_finished = false;

  proxyClient_ = make_shared<ProxyClient>(proxy_address, proxy_port);
  proxyRequestHandler_ = make_shared<ProxyRequestHandler>(proxyClient_);
}

PmPoolClient::~PmPoolClient() {
  std::cout<<"PmPoolClient::Destroy PmPoolclient: start"<<endl;
  std::cout<<"PmPoolClient::Destroy Channels"<<endl;
  map<string, Channel>::iterator itr;
  for (itr = channels.begin(); itr != channels.end(); ++itr){
    itr->second.requestHandler->reset();
    itr->second.networkClient->reset();
  }
  std::cout<<"PmPoolClient::Destroy ProxyClient"<<endl;
  proxyRequestHandler_->reset();
  proxyClient_->reset();
  std::cout<<"PmPoolClient::Destroy PmPoolclient: end"<<endl;

#ifdef DEBUG
  std::cout << "PmPoolClient destructed" << std::endl;
#endif
}

Channel PmPoolClient::getChannel(string node) {
  std::lock_guard<std::mutex> lk(channel_mtx);
  if (channels.count(node)) {
    return channels.find(node)->second;
  } else {
    Channel channel;
    //TODO get port from proxy server
    channel.networkClient = std::make_shared<NetworkClient>(node, "12346");
    channel.requestHandler = std::make_shared<RequestHandler>(channel.networkClient);
    channel.networkClient->init(channel.requestHandler);
    channel.requestHandler->start();
    channels.insert(make_pair(node, channel));
    return channel;
  }
}

int PmPoolClient::init() {
  int res = proxyClient_->initProxyClient(proxyRequestHandler_);
  proxyRequestHandler_->start();
  return res;                                                        
}

void PmPoolClient::begin_tx() {
  std::unique_lock<std::mutex> lk(tx_mtx);
  while (!tx_finished) {
    tx_con.wait(lk);
  }
  tx_finished;
}

uint64_t PmPoolClient::alloc(uint64_t size) {
  /**
    RequestContext rc = {};
    rc.type = ALLOC;
    rc.rid = rid_++;
    rc.size = size;
    auto request = std::make_shared<Request>(rc);
    requestHandler_->addTask(request);
    return requestHandler_->get(request).address;
   **/
  return 0;
}

int PmPoolClient::free(uint64_t address) {
  /**
    RequestContext rc = {};
    rc.type = FREE;
    rc.rid = rid_++;
    rc.address = address;
    auto request = std::make_shared<Request>(rc);
    requestHandler_->addTask(request);
    return requestHandler_->get(request).success;
   **/
  return 0;
}

void PmPoolClient::shutdown() {
  std::cout<<"PmPoolClient::shutdown: start"<<endl;
  map<string, Channel>::iterator itr;                              
  for (itr = channels.begin(); itr != channels.end(); ++itr){      
    itr->second.networkClient->shutdown(); 
  }                                                                
  std::cout<<"PmPoolClient::shutdown: end"<<endl;
}

void PmPoolClient::wait() { 
  std::cout<<"PmPoolClient::init: start"<<endl;

  map<string, Channel>::iterator itr;
  for (itr = channels.begin(); itr != channels.end(); ++itr){
    //itr->second.networkClient->wait();
  }

  std::cout<<"PmPoolClient::init: end"<<endl;
}

int PmPoolClient::write(uint64_t address, const char *data, uint64_t size) {
  /**
    RequestContext rc = {};
    rc.type = WRITE;
    rc.rid = rid_++;
    rc.size = size;
    rc.address = address;
  // allocate memory for RMA read from client.
  rc.src_address = networkClient_->get_dram_buffer(data, rc.size);
  rc.src_rkey = networkClient_->get_rkey();
  auto request = std::make_shared<Request>(rc);
  requestHandler_->addTask(request);
  auto res = requestHandler_->get(request).success;
  networkClient_->reclaim_dram_buffer(rc.src_address, rc.size);
   **/
  return 0;
}

uint64_t PmPoolClient::write(const char *data, uint64_t size) {
  /**
    RequestContext rc = {};
    rc.type = WRITE;
    rc.rid = rid_++;
    rc.size = size;
    rc.address = 0;
  // allocate memory for RMA read from client.
  rc.src_address = networkClient_->get_dram_buffer(data, rc.size);
  rc.src_rkey = networkClient_->get_rkey();
  auto request = std::make_shared<Request>(rc);
  requestHandler_->addTask(request);
  auto res = requestHandler_->get(request).address;
  networkClient_->reclaim_dram_buffer(rc.src_address, rc.size);
   **/
  return 0;
}

int PmPoolClient::read(uint64_t address, char *data, uint64_t size) {
  /**
    RequestContext rc = {};
    rc.type = READ;
    rc.rid = rid_++;
    rc.size = size;
    rc.address = address;
  // allocate memory for RMA read from client.
  rc.src_address = networkClient_->get_dram_buffer(nullptr, rc.size);
  rc.src_rkey = networkClient_->get_rkey();
  auto request = std::make_shared<Request>(rc);
  requestHandler_->addTask(request);
  auto res = requestHandler_->get(request).success;
  if (!res) {
  memcpy(data, reinterpret_cast<char *>(rc.src_address), size);
  }
  networkClient_->reclaim_dram_buffer(rc.src_address, rc.size);
   **/
  return 0;
}

int PmPoolClient::read(uint64_t address, char *data, uint64_t size,
    std::function<void(int)> func) {
  /**
    RequestContext rc = {};
    rc.type = READ;
    rc.rid = rid_++;
    rc.size = size;
    rc.address = address;
  // allocate memory for RMA read from client.
  rc.src_address = networkClient_->get_dram_buffer(nullptr, rc.size);
  rc.src_rkey = networkClient_->get_rkey();
  auto request = std::make_shared<Request>(rc);
  requestHandler_->addTask(request, [&] {
  auto res = requestHandler_->get(request).success;
  if (res) {
  memcpy(data, reinterpret_cast<char *>(rc.src_address), size);
  }
  networkClient_->reclaim_dram_buffer(rc.src_address, rc.size);
  func(res);
  });
   **/
  return 0;
}

void PmPoolClient::end_tx() {
  std::lock_guard<std::mutex> lk(tx_mtx);
  tx_finished = true;
  tx_con.notify_one();
}

int counter = 0;

uint64_t PmPoolClient::put(const string &key, const char *value,
    uint64_t size) {
  uint64_t key_uint;
  Digest::computeKeyHash(key, &key_uint);
  RequestContext rc = {};
  rc.type = PUT;
  rc.rid = rid_++;
  rc.size = size;
  rc.key = key_uint;

  ProxyRequestContext prc = {};
  prc.type = GET_HOSTS;
  prc.rid = rc.rid;
  prc.key = key_uint;
  auto pRequest = std::make_shared<ProxyRequest>(prc);
  proxyRequestHandler_->addTask(pRequest);
  string node = proxyRequestHandler_->get(pRequest).host;

  Channel channel = getChannel(node);
  std::shared_ptr<NetworkClient> networkClient = channel.networkClient;
  std::shared_ptr<RequestHandler> requestHandler = channel.requestHandler;
  rc.address = 0;
  rc.src_address = networkClient->get_dram_buffer(value, rc.size);
  rc.src_rkey = networkClient->get_rkey();
  auto request = std::make_shared<Request>(rc);
  requestHandler->addTask(request);
  auto res = requestHandler->wait(request);
  networkClient->reclaim_dram_buffer(rc.src_address, rc.size);

#ifdef DEBUG
  std::cout << "[PmPoolClient::put start] " << key << "-" << rc.size
    << ", hashkey is " << key_uint << std::endl;
  std::cout << "[PmPoolClient::put] " << key << "-" << rc.size
    << ", hashkey is " << key_uint << std::endl;
  fprintf(stderr, "[PUT]key is %s, length is %ld, content is \n", key.c_str(),
      size);
  for (int i = 0; i < 100; i++) {
    fprintf(stderr, "%X ", *(value + i));
  }
  fprintf(stderr, " ...\n");
#endif
  return res;
}

uint64_t PmPoolClient::get(const string &key, char *value, uint64_t size) {
  uint64_t key_uint;
  Digest::computeKeyHash(key, &key_uint);


  RequestContext rc = {};
  rc.type = GET;
  rc.rid = rid_++;
  rc.size = size;
  rc.address = 0;
  ProxyRequestContext prc = {};
  prc.type = GET_HOSTS;
  prc.rid = rc.rid;
  prc.key = key_uint;
  auto pRequest = std::make_shared<ProxyRequest>(prc);
  proxyRequestHandler_->addTask(pRequest);
  string node = proxyRequestHandler_->get(pRequest).host;
  Channel channel = getChannel(node);
  std::shared_ptr<NetworkClient> networkClient = channel.networkClient;
  std::shared_ptr<RequestHandler> requestHandler = channel.requestHandler;
  // allocate memory for RMA read from client.
  rc.src_address = networkClient->get_dram_buffer(nullptr, rc.size);
  rc.src_rkey = networkClient->get_rkey();
#ifdef DEBUG
  std::cout << "[PmPoolClient::get] " << key << "-" << rc.size
    << ", hashkey is " << key_uint << std::endl;
#endif
  rc.key = key_uint;
  auto request = std::make_shared<Request>(rc);
  requestHandler->addTask(request);
  auto res = requestHandler->wait(request);
  memcpy(value, reinterpret_cast<char *>(rc.src_address), rc.size);
  networkClient->reclaim_dram_buffer(rc.src_address, rc.size);

#ifdef DEBUG
  fprintf(stderr, "[GET]key is %s, length is %ld, content is \n", key.c_str(),
      size);
  for (int i = 0; i < 100; i++) {
    fprintf(stderr, "%X ", *(value + i));
  }
  fprintf(stderr, " ...\n");
#endif
  return res;
}

vector<block_meta> PmPoolClient::getMeta(const string &key) {
  uint64_t key_uint;
  Digest::computeKeyHash(key, &key_uint);
  RequestContext rc = {};
  rc.type = GET_META;
  rc.rid = rid_++;
  rc.address = 0;
  rc.key = key_uint;

  ProxyRequestContext prc = {};
  prc.type = GET_HOSTS;
  prc.rid = rc.rid;
  prc.key = key_uint;
  auto pRequest = std::make_shared<ProxyRequest>(prc);
  proxyRequestHandler_->addTask(pRequest);
  string node = proxyRequestHandler_->get(pRequest).host;

  Channel channel = getChannel(node);
  std::shared_ptr<NetworkClient> networkClient = channel.networkClient;
  std::shared_ptr<RequestHandler> requestHandler = channel.requestHandler;

  auto request = std::make_shared<Request>(rc);
  requestHandler->addTask(request);
  auto rrc = requestHandler->get(request);
  return rrc.bml;
}

int PmPoolClient::del(const string &key) {
  uint64_t key_uint;
  Digest::computeKeyHash(key, &key_uint);
  RequestContext rc = {};
  rc.type = DELETE;
  rc.rid = rid_++;
  rc.key = key_uint;

  ProxyRequestContext prc = {};
  prc.type = GET_HOSTS;
  prc.rid = rc.rid;
  prc.key = key_uint;
  auto pRequest = std::make_shared<ProxyRequest>(prc);
  proxyRequestHandler_->addTask(pRequest);
  string node = proxyRequestHandler_->get(pRequest).host;
  Channel channel = getChannel(node);
  std::shared_ptr<NetworkClient> networkClient = channel.networkClient;
  std::shared_ptr<RequestHandler> requestHandler = channel.requestHandler;

  auto request = std::make_shared<Request>(rc);
  requestHandler->addTask(request);
  auto res = requestHandler->get(request).success;
  return res;
}
