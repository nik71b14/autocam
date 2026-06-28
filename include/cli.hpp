#pragma once

// =============================================================================
//  cli.hpp - Minimal, zero-dependency command-line parser.
//
//  The application is invoked as:
//
//      voxelize <command> [positionals...] [--key value | --key=value | --flag]
//
//  - argv[1] is the COMMAND (e.g. "voxelize", "simulate", "view", "help").
//  - Tokens that start with "--" are either:
//        * FLAGS (valueless), if their name is listed in `valuelessFlags`
//          (e.g. --ortho, --no-view, --verbose, --help); or
//        * OPTIONS with a value, written as "--key value" or "--key=value".
//  - Everything else is a POSITIONAL argument (e.g. an input .stl / .bin path).
//
//  Header-only on purpose (same style as utils.hpp): no extra .cpp to add to
//  the build, and the parser is tiny.
// =============================================================================

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct CliArgs {
  std::string command;                                    // argv[1], "" if none
  std::vector<std::string> positionals;                   // non-option tokens
  std::unordered_map<std::string, std::string> opts;      // --key value
  std::unordered_set<std::string> flags;                  // --flag (valueless)

  // True if `name` was passed either as a flag or as an option.
  bool has(const std::string& name) const { return flags.count(name) || opts.count(name); }

  // String option lookup with a fallback default.
  std::string get(const std::string& key, const std::string& def) const {
    auto it = opts.find(key);
    return it == opts.end() ? def : it->second;
  }

  // Numeric option lookups with fallback defaults.
  float getFloat(const std::string& key, float def) const {
    auto it = opts.find(key);
    return it == opts.end() ? def : std::stof(it->second);
  }
  int getInt(const std::string& key, int def) const {
    auto it = opts.find(key);
    return it == opts.end() ? def : std::stoi(it->second);
  }
};

// Parse argc/argv into a CliArgs.
//
// `valuelessFlags` is the set of "--" tokens that must be treated as boolean
// flags (so the parser knows NOT to consume the following token as their
// value). Any "--" token not in this set is treated as an option that expects
// a value ("--key value") or carries one inline ("--key=value").
inline CliArgs parseCli(int argc, char** argv, const std::unordered_set<std::string>& valuelessFlags) {
  CliArgs args;
  if (argc > 1) args.command = argv[1];

  // Start parsing options/positionals after the command (index 2).
  for (int i = 2; i < argc; ++i) {
    std::string token = argv[i];

    if (token.rfind("--", 0) == 0) {
      // Inline form: --key=value
      size_t eq = token.find('=');
      if (eq != std::string::npos) {
        args.opts[token.substr(0, eq)] = token.substr(eq + 1);
        continue;
      }

      // Boolean flag (no value).
      if (valuelessFlags.count(token)) {
        args.flags.insert(token);
        continue;
      }

      // Option that expects the next token as its value: --key value
      if (i + 1 < argc) {
        args.opts[token] = argv[++i];
      } else {
        throw std::runtime_error("Missing value for option '" + token + "'");
      }
      continue;
    }

    // Plain positional argument.
    args.positionals.push_back(token);
  }

  return args;
}
