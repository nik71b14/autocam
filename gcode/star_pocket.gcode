G21         ; Set units to millimeters
G90         ; Absolute positioning
G17         ; XY plane

G0 Z5       ; Move up to safe height
G0 X0 Y0    ; Move to origin

M3 S12000   ; Start spindle at 12000 RPM
F500        ; Set feed rate to 500 mm/min

G0 X25 Y0   ; Move to start point (outer point)

; Pocketing in steps: -2, -4, -6, -8, -10
G1 Z-2.0    ; First cutting depth
G1 X7.7 Y5.6
G1 X7.7 Y23.8
G1 X-3.0 Y9.1
G1 X-20.2 Y14.7
G1 X-9.5 Y0.0
G1 X-20.2 Y-14.7
G1 X-3.0 Y-9.1
G1 X7.7 Y-23.8
G1 X7.7 Y-5.6
G1 X25.0 Y0.0

G1 Z-4.0    ; Second cutting depth
G1 X7.7 Y5.6
G1 X7.7 Y23.8
G1 X-3.0 Y9.1
G1 X-20.2 Y14.7
G1 X-9.5 Y0.0
G1 X-20.2 Y-14.7
G1 X-3.0 Y-9.1
G1 X7.7 Y-23.8
G1 X7.7 Y-5.6
G1 X25.0 Y0.0

G1 Z-6.0    ; Third cutting depth
G1 X7.7 Y5.6
G1 X7.7 Y23.8
G1 X-3.0 Y9.1
G1 X-20.2 Y14.7
G1 X-9.5 Y0.0
G1 X-20.2 Y-14.7
G1 X-3.0 Y-9.1
G1 X7.7 Y-23.8
G1 X7.7 Y-5.6
G1 X25.0 Y0.0

G1 Z-8.0    ; Fourth cutting depth
G1 X7.7 Y5.6
G1 X7.7 Y23.8
G1 X-3.0 Y9.1
G1 X-20.2 Y14.7
G1 X-9.5 Y0.0
G1 X-20.2 Y-14.7
G1 X-3.0 Y-9.1
G1 X7.7 Y-23.8
G1 X7.7 Y-5.6
G1 X25.0 Y0.0

G1 Z-10.0   ; Fifth cutting depth
G1 X7.7 Y5.6
G1 X7.7 Y23.8
G1 X-3.0 Y9.1
G1 X-20.2 Y14.7
G1 X-9.5 Y0.0
G1 X-20.2 Y-14.7
G1 X-3.0 Y-9.1
G1 X7.7 Y-23.8
G1 X7.7 Y-5.6
G1 X25.0 Y0.0

G0 Z5       ; Retract
M5          ; Stop spindle
M30         ; End of program