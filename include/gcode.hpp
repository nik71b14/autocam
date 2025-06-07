#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <glm/glm.hpp>

enum class Plane {
  XY, ZX, YZ
};

struct SimulationState {
  glm::vec3 position = glm::vec3(0.0f);
  double feedRate = 1000.0;
  double spindleSpeed = 0.0;
  int tool = 0;
  Plane currentPlane = Plane::XY;
  double speedFactor = 1.0;
};

#include <map>

class GCodeInterpreter {
public:
  GCodeInterpreter();
  ~GCodeInterpreter();

  bool loadFile(const std::string& filename);
  bool checkFile() const;

  void setSpeedFactor(double factor);
  void run();
  void stop();
  bool isRunning() const;

  glm::vec3 getCurrentPosition() const;
  double getCurrentFeedRate() const;
  double getCurrentSpindleSpeed() const;
  int getCurrentTool() const;
  Plane getCurrentPlane() const;

private:
  void executeCommand(const std::string& line);
  void parseLine(const std::string& line, std::map<char, double>& params, std::string& cmd);

  std::vector<std::string> gcodeLines;
  std::atomic<bool> running;
  std::thread simulationThread;

  mutable std::mutex stateMutex;
  SimulationState state;
};
