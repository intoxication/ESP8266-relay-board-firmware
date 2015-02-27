#!/bin/bash
cd html/
find | ../mkespfsimage/mkespfsimage -c 1 > new.espfs
cd ../
python et.py --port /dev/ttyUSB0 write_flash 0x12000 html/new.espfs
rm html/new.espfs
