// =============================================================================
//  autocam - 3-axis CNC milling voxel simulator.
//
//  main() is a thin dispatcher: it parses the command line and hands off to the
//  selected sub-command (see include/modes.hpp and src/modes/*.cpp). Modes are
//  chosen at RUNTIME via a sub-command argument (git/docker style) instead of
//  the old compile-time #define switches, so:
//    - no recompilation is needed to change mode or parameters;
//    - every mode is always compiled (the compiler checks them all);
//    - the tool is scriptable for batch/headless data generation.
//
//  See DOCS/MANUAL.md for the full command reference.
// =============================================================================

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include "cli.hpp"
#include "modes.hpp"

void printUsage() {
  std::cout <<
      "autocam - 3-axis CNC milling voxel simulator\n\n"
      "Usage:\n"
      "  autocam <command> [options]\n\n"
      "Commands:\n"
      "  voxelize <input.stl> [--out <file.bin>] [--res <float>] [--mem-mb <int>]\n"
      "      Voxelize an STL mesh and save it as a .bin voxel object.\n"
      "      Default output: test/<stlname>.bin\n\n"
      "  simulate --gcode <f.gcode> --workpiece <w.bin> --tool <t.bin>\n"
      "           [--out <r.bin>] [--step <float>] [--perspective] [--no-view] [--legacy]\n"
      "      Carve the workpiece along the G-code toolpath with the tool.\n"
      "      --no-view runs headless (no window); --out saves the carved result.\n"
      "      --legacy uses per-step stamping instead of the swept subtraction.\n\n"
      "  view <file.bin> [--ortho]\n"
      "      Raymarch-view a .bin voxel object.\n\n"
      "  help, --help\n"
      "      Show this message.\n";
}

int main(int argc, char** argv) {
  // Valueless flags: tokens the parser must NOT treat as "--key <value>".
  const std::unordered_set<std::string> valuelessFlags = {
      "--ortho", "--perspective", "--no-view", "--verbose", "--legacy", "--help"};

  try {
    CliArgs args = parseCli(argc, argv, valuelessFlags);

    // No command, or an explicit help request -> print usage.
    if (args.command.empty() || args.command == "help" || args.command == "--help" || args.has("--help")) {
      printUsage();
      return EXIT_SUCCESS;
    }

    // Dispatch to the selected sub-command.
    if (args.command == "voxelize") return runVoxelize(args);
    if (args.command == "simulate") return runSimulate(args);
    if (args.command == "view") return runView(args);

    std::cerr << "Unknown command: '" << args.command << "'\n\n";
    printUsage();
    return EXIT_FAILURE;
  } catch (const std::exception& e) {
    std::cerr << "[Error] " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
