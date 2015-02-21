#!/bin/sh
PORT=$1
if [ -z "$PORT" ]; then PORT=/dev/ttyUSB0; fi
./qcomand -p $PORT -c"c b"
