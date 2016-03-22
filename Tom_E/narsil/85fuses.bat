avrdude -p t85 -c usbasp -Ulfuse:w:0xe2:m -Uhfuse:w:0xdf:m -Uefuse:w:0xff:m
rem C3 for low byte is 6.4 Mhz
