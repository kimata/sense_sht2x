# Sensirion SHT-25 Utility for Linux

Measure temperature and humidity by using SHT-25 via I2C bus.

## Usage

$ make
gcc -Wall -std=c99 -o sense_sht2x sense_sht2x.c
$ ./sense_sht2x 1 # `1' is I2C bus ID. if you use BagleBone Black and use P9_19-20, I2C_BUS is `1'.
TEMP:35.29
HUMI: 38.80


