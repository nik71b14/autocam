// This class is a gcode interpreter, which opens a file like e.g. path.gcode or parh.nc, parses it following
// In subtractive process simulation like milling, the most relevant G-code commands are those defined in the ISO 6983 standard
// (commonly known as RS-274), which describes the programming of numerically controlled (NC) machines.
// These commands control tool movement, spindle functions, and toolpath behaviors.
// For simulation, especially in a virtual or digital twin context, the commands related to tool motion, tool change,
// spindle control, and feed rates are most important.
//
// Common G-code Commands for Subtractive Process Simulation (ISO 6983 / RS-274)
//
// 1. Tool Movement Commands (Linear & Circular)
//    Command   Meaning
//    -------   ------------------------------------------
//    G0        Rapid positioning (non-cutting fast move)
//    G1        Linear interpolation (cutting move)
//    G2        Circular interpolation clockwise
//    G3        Circular interpolation counterclockwise
//    G4        Dwell (pause for a set time)
//
// 2. Plane Selection (for arc/circle motion)
//    Command   Meaning
//    -------   -------------------------------
//    G17       Select XY plane
//    G18       Select ZX plane
//    G19       Select YZ plane
//
// 3. Units and Positioning Modes
//    Command   Meaning
//    -------   -------------------------------
//    G20       Inch units
//    G21       Millimeter units
//    G90       Absolute positioning
//    G91       Incremental positioning
//
// 4. Tool Control and Spindle Commands
//    Command   Meaning
//    -------   ------------------------------------------
//    M3        Spindle on clockwise
//    M4        Spindle on counterclockwise
//    M5        Spindle stop
//    M6        Tool change
//
// 5. Feedrate and Speed Control
//    Command   Meaning
//    -------   -------------------------------
//    F         Feed rate (units per minute)
//    S         Spindle speed (RPM)
//
// 6. Miscellaneous Commands
//    Command   Meaning
//    -------   ------------------------------------------
//    M0        Program stop (wait for user input)
//    M1        Optional stop
//    M2/M30    End of program / Reset
//
// 7. Coordinate System Setting
//    Command   Meaning
//    -------   ------------------------------------------
//    G54-G59   Work coordinate system selection
//    G92       Set position register (define current position)
//
// At the moment, only commands from group 1 (Tool Movement Commands) will be implemented.

// Please implement a G-code interpreter class that can read a G-code file, parse the commands, and execute them.
// The execution is intended as varying the parameters (e.g. x, y, z coordinates, feed rate etc.) of a toolpath in a simulation context.
// The execution should proceed in "real time", meaning that the simulation should reflect the time it would take to execute the commands in a real machine.
// The execution speed should be adjustable, allowing for faster or slower simulation speeds (global speed factor override).
// The class should implements the following methods:
// - `loadFile(const std::string &filename)`: Load a G-code file.
// - `checkFile()`: Checks a loaded file (if any) for formal correctness.
// - `parseLine(const std::string &line)`: Parse a single line of G-code.
// - `executeCommand(const std::string &command)`: Execute a parsed G-code command.
// - `setSpeedFactor(double factor)`: Set the global speed factor for the simulation.
// - `run()`: Start the simulation, processing commands in real time according to the speed factor, scrolling through the G-code file.
// - `stop()`: Stop the simulation.
// - `isRunning() const`: Check if the simulation is currently running.
// - `getCurrentPosition() const`: Get the current position of the tool in the simulation.
// - `getCurrentFeedRate() const`: Get the current feed rate of the tool in the simulation.
// - `getCurrentSpindleSpeed() const`: Get the current spindle speed of the tool in the simulation.
// - `getCurrentTool() const`: Get the current tool being used in the simulation.
// - `getCurrentPlane() const`: Get the current plane being used in the simulation (XY, ZX, YZ).

#include "gcode.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <map>
#include <cmath>
#include <algorithm>
#include <thread>

GCodeInterpreter::GCodeInterpreter() : running(false) {}

GCodeInterpreter::~GCodeInterpreter() {
  stop();
}

bool GCodeInterpreter::loadFile(const std::string& filename) {
  std::ifstream file(filename);
  if (!file.is_open()) return false;
  std::string line;
  gcodeLines.clear();
  while (std::getline(file, line)) {
    gcodeLines.push_back(line);
  }
  file.close();
  return true;
}

bool GCodeInterpreter::checkFile() const {
  for (const auto& line : gcodeLines) {
    if (line.find("G0") != std::string::npos || line.find("G1") != std::string::npos)
      return true;
  }
  return false;
}

void GCodeInterpreter::setSpeedFactor(double factor) {
  std::lock_guard<std::mutex> lock(stateMutex);
  state.speedFactor = factor;
}

void GCodeInterpreter::run() {
  if (running) return;
  running = true;
  simulationThread = std::thread([this]() {
    for (const auto& line : gcodeLines) {
      if (!running) break;
      executeCommand(line);
    }
    running = false;
  });
}

void GCodeInterpreter::stop() {
  running = false;
  if (simulationThread.joinable())
    simulationThread.join();
}

bool GCodeInterpreter::isRunning() const {
  return running;
}

glm::vec3 GCodeInterpreter::getCurrentPosition() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return state.position;
}

double GCodeInterpreter::getCurrentFeedRate() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return state.feedRate;
}

double GCodeInterpreter::getCurrentSpindleSpeed() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return state.spindleSpeed;
}

int GCodeInterpreter::getCurrentTool() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return state.tool;
}

Plane GCodeInterpreter::getCurrentPlane() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return state.currentPlane;
}

void GCodeInterpreter::parseLine(const std::string& line, std::map<char, double>& params, std::string& cmd) {
  std::istringstream ss(line);
  std::string token;
  while (ss >> token) {
    char letter = token[0];
    std::string valueStr = token.substr(1);
    if (letter == 'G' || letter == 'M') {
      cmd = token;
    } else {
      try {
        double value = std::stod(valueStr);
        params[letter] = value;
      } catch (...) {}
    }
  }
}

void GCodeInterpreter::executeCommand(const std::string& line) {
  std::map<char, double> params;
  std::string cmd;
  parseLine(line, params, cmd);

  if (cmd == "G0" || cmd == "G1") {
    glm::vec3 startPos;
    double currentFeedRate;
    double speedFactor;

    {
      std::lock_guard<std::mutex> lock(stateMutex);
      startPos = state.position;
      currentFeedRate = state.feedRate;
      speedFactor = state.speedFactor;
    }

    glm::vec3 targetPos = startPos;
    if (params.count('X')) targetPos.x = static_cast<float>(params['X']);
    if (params.count('Y')) targetPos.y = static_cast<float>(params['Y']);
    if (params.count('Z')) targetPos.z = static_cast<float>(params['Z']);

    if (params.count('F')) {
      std::lock_guard<std::mutex> lock(stateMutex);
      state.feedRate = params['F'];
      currentFeedRate = state.feedRate;
    }

    double distance = glm::length(targetPos - startPos);
    double timeSeconds = (cmd == "G0") ? distance / 5000.0 : distance / (currentFeedRate / 60.0);
    timeSeconds /= speedFactor;

    const double stepTime = 0.01;
    int steps = std::max(1, static_cast<int>(timeSeconds / stepTime));
    for (int i = 1; i <= steps && running; ++i) {
      double t = static_cast<double>(i) / steps;
      glm::vec3 interpolated = glm::mix(startPos, targetPos, static_cast<float>(t));
      {
        std::lock_guard<std::mutex> lock(stateMutex);
        state.position = interpolated;
      }
      std::this_thread::sleep_for(std::chrono::duration<double>(timeSeconds / steps));
    }

    {
      std::lock_guard<std::mutex> lock(stateMutex);
      state.position = targetPos;
    }

  } else if (cmd == "G4") {
    double dwell = params.count('P') ? params['P'] : 0;
    double localSpeed;
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      localSpeed = state.speedFactor;
    }
    std::this_thread::sleep_for(std::chrono::duration<double>(dwell / 1000.0 / localSpeed));
  } else if (cmd == "G17") {
    std::lock_guard<std::mutex> lock(stateMutex);
    state.currentPlane = Plane::XY;
  } else if (cmd == "G18") {
    std::lock_guard<std::mutex> lock(stateMutex);
    state.currentPlane = Plane::ZX;
  } else if (cmd == "G19") {
    std::lock_guard<std::mutex> lock(stateMutex);
    state.currentPlane = Plane::YZ;
  } else if (cmd == "M6" && params.count('T')) {
    std::lock_guard<std::mutex> lock(stateMutex);
    state.tool = static_cast<int>(params['T']);
  } else if (params.count('S')) {
    std::lock_guard<std::mutex> lock(stateMutex);
    state.spindleSpeed = params['S'];
  }
}
