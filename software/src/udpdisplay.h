#pragma once

// Network mode: shows 1-bit framebuffers (ROWS*COLS bits, LSB first) received over UDP.
//
// Example QR code
// qrencode -t ascii -m 2 'HOLY SHIT!' | perl -lne's/(.)(.)/$1/g; $_ .= "#" x (32 - length $_); tr/ #/10/; 0 and print $_; $x .= $_ }{ $x .= "1" x 32 for $. .. 31; 0 and print $x; 1 and print pack "b*", $x'  | nc -w0 -u 10.42.45.255 1337

void udpdisplaysetup();  // Shows the IP address on the display and starts listening
void udpdisplayloop();
