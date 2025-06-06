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
// At the moment, only commands from group 1 (Tool Movement Commands) and group 5 will be implemented.