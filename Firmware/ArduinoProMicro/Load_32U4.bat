@echo off
del toload.hex
copy *.hex toload.hex
mode COM7 BAUD=57600
avrdude -p m32u4 -c avr109 -b 57600 -e -U flash:w:toload.hex -P com7 -v
pause
