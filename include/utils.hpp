#pragma once

#include <string>

inline std::string getFileNameFromPath(const std::string& path) {
  size_t pos = path.find_last_of("/\\");
  if (pos == std::string::npos) {
    return path;
  }
  return path.substr(pos + 1);
}

inline std::string stlToBinName(const std::string& stlFileName) {
  std::string binFileName = stlFileName;
  size_t extPos = binFileName.rfind(".stl");
  if (extPos != std::string::npos && extPos == binFileName.length() - 4) {
    binFileName = binFileName.substr(0, extPos);
  }
  binFileName += ".bin";
  return binFileName;
}