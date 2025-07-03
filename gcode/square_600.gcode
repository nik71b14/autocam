%
(Move to safe height)
G0 Z1200.000

(Move to start corner above material)
G0 X-300.000 Y-300.000
G0 Z500.000

(Begin cutting square)
G1 Z500.000 F500.000
G1 X300.000 Y-300.000 F500.000
G1 X300.000 Y300.000 F500.000
G1 X-300.000 Y300.000 F500.000
G1 X-300.000 Y-300.000 F500.000

(Retract and finish)
G0 Z1200.000
G0 X0 Y0
%
