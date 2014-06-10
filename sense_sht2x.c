/*
 * Sensirion SHT-25 Utility
 *
 * Copyright (C) 2014 Tetsuya Kimata <kimata@green-rabbit.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 *
 * USAGE:
 * $ make
 * $ ./sense_sht2x I2C_BUS
 *
 * if you use BagleBone Black and use P9_19-20, I2C_BUS is `1'.
 *
 */

#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#define VERSION				"0.0.1"

#define I2C_DEV_ADDR        0x40
#define CRC_POLY            0x131   // P(x)=x^8+x^5+x^4+1 = 100110001
#define MEASURE_RETRY_WAIT  85000   // 85ms = maximum measurement time

typedef enum {
    CMD_TRIG_TEMP_POLL      = 0xF3, // command trig. temp meas. no hold master
    CMD_TRIG_HUMI_POLL      = 0xF5, // command trig. humidity meas. no hold master
    CMD_USER_REG_WRITE	    = 0xE6, // command writing user register
    CMD_USER_REG_READ       = 0xE7, // command reading user register
    CMD_SOFT_RESET          = 0xFE, // command soft reset
    CMD_MEASURE_READ        = 0x00, // read measured value (for software convenience)
} SHT2x_COMMAND;

typedef enum {
    OPT_RES_12_14BIT        = 0x00, // RH=12bit, T=14bit
    OPT_RES_8_12BIT         = 0x01, // RH= 8bit, T=12bit
    OPT_RES_10_13BIT        = 0x80, // RH=10bit, T=13bit
    OPT_RES_11_11BIT        = 0x81  // RH=11bit, T=11bit
} SHT2x_OPTION;

typedef struct {
    
} USER_REGISTER;

uint8_t calc_crc(uint8_t data[], uint8_t size)
{
    uint16_t crc = 0;

    for (uint8_t i = 0; i < size; i++) {
	crc ^= (data[i]);
	for (uint8_t j = 0; j < 8; j++) {
	    crc = (crc & 0x80) ? ((crc << 1) ^ CRC_POLY) : (crc << 1);
	}
    }
    return crc;
}

int check_crc(uint8_t data[], uint8_t size, uint8_t crc)
{
    return (calc_crc(data, size) == crc) ? 0 : -1;
}

int exec_measure_read(int fd, SHT2x_COMMAND cmd, uint16_t *value)
{
    uint8_t buf[3];

    for (uint8_t i = 0; i < 2; i++) {
	if (read(fd, buf, 3) != 3) { 
	    usleep(MEASURE_RETRY_WAIT);
	    continue;
	}
	if (check_crc(buf, 2, buf[2])) { 
	    fprintf(stderr, "ERROR: CRC validation\n");
	    return -1;
	}
	if (value == NULL) {
	    fprintf(stderr, "ERROR: invalid function call\n");
	    return -1;
	}
	*value = buf[0] << 8 | buf[1];
	return 0;
    }
    fprintf(stderr, "ERROR: i2c read\n");
    return -1;
}

int exec_command(int fd, SHT2x_COMMAND cmd, uint16_t *value)
{
    switch (cmd) {
    case CMD_TRIG_TEMP_POLL:
    case CMD_TRIG_HUMI_POLL:
    case CMD_SOFT_RESET:
        if ((write(fd, &cmd, 1)) != 1) {
            fprintf(stderr, "ERROR: i2c write\n");
            exit(EXIT_FAILURE);
        }
        break;
    case CMD_MEASURE_READ:
        if (exec_measure_read(fd, cmd, value) == -1) {
            exit(EXIT_FAILURE);

        }
	break;
    case CMD_USER_REG_WRITE:
    case CMD_USER_REG_READ:
        fprintf(stderr, "ERROR: NOT implemented\n");
        exit(EXIT_FAILURE);
        break;
    }
    usleep(10000); // wait 10ms

    return 0;
}

float calc_temp(uint16_t value)
{
    if ((value & 0x2) != 0) {
        fprintf(stderr, "ERROR: invalid value\n");
        exit(EXIT_FAILURE);
    }
    return -46.85 + (175.72 * (value & 0xFFFC)) / (1 << 16);
}

float calc_humi(uint16_t value)
{
    if ((value & 0x2) == 0) {
        fprintf(stderr, "ERROR: invalid value\n");
        exit(EXIT_FAILURE);
    }
    
    return -6 + (125.0 * (value & 0xFFFC)) / (1 << 16);
}

int exec_sense(uint8_t bus, uint8_t show_temp, uint8_t show_humi)
{
    int fd;
    char i2c_dev_path[64];
    uint16_t temp, humi;

    sprintf(i2c_dev_path, "/dev/i2c-%d", bus);
    if ((fd = open(i2c_dev_path, O_RDWR)) < 0) {
        printf("ERROR: Faild to open i2c port\n");
        return EXIT_FAILURE;
    }

    if (ioctl(fd, I2C_SLAVE, I2C_DEV_ADDR) < 0) {
        printf("ERROR: Unable to get bus access\n");
        return EXIT_FAILURE;
    }

    exec_command(fd, CMD_SOFT_RESET, NULL);

    exec_command(fd, CMD_TRIG_TEMP_POLL, NULL);
    exec_command(fd, CMD_MEASURE_READ, &temp);

    exec_command(fd, CMD_TRIG_HUMI_POLL, NULL);
    exec_command(fd, CMD_MEASURE_READ, &humi);

    if ((show_temp == 1) && (show_humi == 0)) {
        printf("%.2f\n", calc_temp(temp));
    } else if ((show_temp == 0) && (show_humi == 1)) {
        printf("%.2f\n", calc_humi(humi));
    } else {
        printf("TEMP: %.2f\n", calc_temp(temp));
        printf("HUMI: %.2f\n", calc_humi(humi));
    }
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    struct option long_opts[] = {
        { "bus",      		1, NULL, 'b'  },
        { "temperature",    0, NULL, 'T' },
        { "humidity",    	0, NULL, 'H' },
        { "version", 		0, NULL, 'v' },
        {0, 0, 0, 0}
    };

    int opt_index = 0;
    int result = 0;
    uint8_t bus = 1;
    uint8_t show_temp = 0;
    uint8_t show_humi = 0;

    while ((result = getopt_long(argc, argv, "b:THv",
                                 long_opts, &opt_index)) != -1) {
        switch (result) {
        case 'b':
            bus = (uint8_t)(atoi(optarg) & 0xFF);
            break;
        case 'T':
            show_temp = 1;
            break;
        case 'H':
            show_humi = 1;
            break;
        case 'v':
            printf("sense_sht2x version %s. (build: %s)\n", VERSION,  __TIMESTAMP__);
            return EXIT_SUCCESS;
        }
    }

    return exec_sense(bus, show_temp, show_humi);
}

// Local Variables:
// mode: c
// c-basic-offset: 4
// tab-width: 4
// indent-tabs-mode: nil
// End:
