#pragma once
#include <WebServer.h>
#include "History.h"
#include "Controller.h"
class WebServerModule {
 public:
  using StatusProvider = ApplicationStatus (*)();
  using CommandHandler = ControlReply (*)(ControlAction);
  void begin(const History &history, const SensorSnapshot &snapshot, StatusProvider status, CommandHandler command);
  void tick() { server_.handleClient(); }
 private:
  WebServer server_{80};
  const History *history_ = nullptr;
  const SensorSnapshot *snapshot_ = nullptr;
  StatusProvider status_ = nullptr;
  CommandHandler command_ = nullptr;
  String boot_, token_;
  void data();
  void control();
  void reply(int code, bool accepted, const char *message);
};
