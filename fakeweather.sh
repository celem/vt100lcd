#!/bin/bash
#
# Send simulated weather to vt100lcd on /dev/ttyUSB0
# change windspeed every few seconds, limited between 30-to-75
#
SLEEPTIME=3
PORT=/dev/ttyUSB0
#PORT=x
stty -F /dev/ttyUSB0 9600 cs8 cread clocal
echo "Assumes that vt100lcd is on /dev/ttyUSB0"
echo
echo "Press [CTRL+C] to stop.."
	echo -ne \\033c >$PORT
	sleep 1
	echo -n "Temperature: 71f" >$PORT
	sleep 1
	echo -ne \\033E >$PORT
	sleep 1
	echo -n "Wind Speed:33mph" >$PORT
	sleep $SLEEPTIME
while :
do
	echo -ne \\033[2\;12H >$PORT
	sleep 1
	echo -n "$((RANDOM%65+10))" >$PORT
	sleep $SLEEPTIME
done
