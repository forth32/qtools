#!/bin/sh
./qcommand -c "c 3a" >/dev/null
while [ -c /dev/ttyUSB0 ]
 do
  true
 done
echo diagnostic port removed;
while [ ! -c /dev/ttyUSB0 ]
 do
  true
 done
echo download mode entered
sleep 2
./qdload -i NPRGpatched.bin
