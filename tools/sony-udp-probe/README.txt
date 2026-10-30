Sony SRG-300H UDP Probe v0.0.14
Extract over SonyTest, run Build-Windows.cmd, then SonyUdpProbe.exe.
DEFAULT MODE IS NOW zoom-test: THIS TEST MOVES THE ZOOM LENS.
It leaves zoom at its NEW position. No preset recall or automatic return.

RESET -> ACK -> 100 ms -> power inquiry sequence 1 (0110)
power reply -> 100 ms -> initial zoom inquiry sequence 2 (0110)
initial zoom reply -> 100 ms -> lowest-speed TELE sequence 3 (0100)
250 ms -> STOP sequence 4 (0100), without waiting for the TELE reply
STOP completion -> 200 ms settling + 100 ms -> final zoom inquiry sequence 5 (0110)

PASS requires final zoom position greater than initial position.
Unchanged/decreased position is INCONCLUSIVE. No motion if baseline fails,
camera is in standby, or baseline is at/above optical telephoto limit 0x4000.
STOP errors/timeouts cause one additional STOP attempt and exit.
UDP delivery is not guaranteed; keep camera power accessible during the test.
Source/listener ports remain 52381. Replies accepted from camera IP on any port.
CLI:
SonyUdpProbe.exe 192.168.0.100 52381 3000 52381 1 zoom-test
Send SonyUdpProbe-*.log; confirm v0.0.14 and mode zoom-test.
Other previous read-only modes remain available explicitly.
Windows compilation and actual hardware behavior require your local test.
