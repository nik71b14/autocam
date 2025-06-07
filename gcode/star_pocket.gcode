G21         ; Set units to millimeters
G90         ; Absolute positioning
G17         ; XY plane

G0 Z5       ; Move up to safe height
G0 X0 Y0    ; Move to origin

M3 S12000   ; Start spindle at 12000 RPM
F500        ; Set feed rate to 500 mm/min

G0 X25 Y0   ; Move to start point (one vertex of the star)

; Pocketing in steps: -2, -4, -6, -8, -10
G1 Z-2.0    ; First cutting depth

G1 X7.7 Y15.5
G1 X20.2 Y47.6
G1 X0 Y30.9
G1 X-20.2 Y47.6
G1 X-7.7 Y15.5
G1 X-25 Y0
G1 X-7.7 Y-15.5
G1 X-20.2 Y-47.6
G1 X0 Y-30.9
G1 X20.2 Y-47.6
G1 X7.7 Y-15.5
G1 X25 Y0

G1 Z-4.0
(repeat shape at lower depth)
G1 X7.7 Y15.5
G1 X20.2 Y47.6
G1 X0 Y30.9
G1 X-20.2 Y47.6
G1 X-7.7 Y15.5
G1 X-25 Y0
G1 X-7.7 Y-15.5
G1 X-20.2 Y-47.6
G1 X0 Y-30.9
G1 X20.2 Y-47.6
G1 X7.7 Y-15.5
G1 X25 Y0

G1 Z-6.0
; ... repeat
G1 X7.7 Y15.5
G1 X20.2 Y47.6
G1 X0 Y30.9
G1 X-20.2 Y47.6
G1 X-7.7 Y15.5
G1 X-25 Y0
G1 X-7.7 Y-15.5
G1 X-20.2 Y-47.6
G1 X0 Y-30.9
G1 X20.2 Y-47.6
G1 X7.7 Y-15.5
G1 X25 Y0

G1 Z-8.0
; ... repeat
G1 X7.7 Y15.5
G1 X20.2 Y47.6
G1 X0 Y30.9
G1 X-20.2 Y47.6
G1 X-7.7 Y15.5
G1 X-25 Y0
G1 X-7.7 Y-15.5
G1 X-20.2 Y-47.6
G1 X0 Y-30.9
G1 X20.2 Y-47.6
G1 X7.7 Y-15.5
G1 X25 Y0

G1 Z-10.0
; ... repeat
G1 X7.7 Y15.5
G1 X20.2 Y47.6
G1 X0 Y30.9
G1 X-20.2 Y47.6
G1 X-7.7 Y15.5
G1 X-25 Y0
G1 X-7.7 Y-15.5
G1 X-20.2 Y-47.6
G1 X0 Y-30.9
G1 X20.2 Y-47.6
G1 X7.7 Y-15.5
G1 X25 Y0

G0 Z5       ; Retract
M5          ; Stop spindle
M30         ; End of program
