/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2024 oclyke <oclyke@oclyke.dev>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * AP0202AT driver generic sensor common utilities.
 * The AP0202AT ISP is used in combination with other
 * sensors. The methods provided in this file are
 * applicable to the AP0202AT and may be used in
 * combination with drivers for other sensors.
 * 
 * When calling AP0202AT methods it is expected that the
 * sensor struct I2C bus and slave address target the ISP
 * itself. The ISP communicates with the attached sensor
 * on a separate I2C bus.
 */
#include "omv_boardconfig.h"
#if (OMV_ENABLE_AP0202AT == 1)

#include "py/mphal.h"

#include "sensor.h"
#include "ap0202at.h"
#include "ap0202at_regs.h"
#include "ap0202at_ar0147.h"
#include "ap0202at_ar0231at.h"

#include "ar0147_regs.h"
#include "ar0231at_regs.h"

#include <stdio.h>

// Maximum time in milliseconds before host command
//   polling times out.
#define AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS (100)
#define AP0202AT_HOST_COMMAND_READ_POLL_TIMEOUT_MS (100)

int ap0202at_detect_self(sensor_t *sensor, bool *detected) {
    int ret = 0;
    uint16_t reg = 0;
    if (NULL == detected) {
        return -1;
    }

    // check chip version
    ret = ap0202at_read_reg_direct(sensor, AP0202AT_REG_SYSCTL_CHIP_VERSION_REG, &reg);
    if (0 != ret) {
        return -1;
    }
    if (reg != AP0202AT_SYSCTL_CHIP_VERSION_REG_DEFAULT_VALUE) {
        return -1;
    }

    // check user defined device address ID
    ret = ap0202at_read_reg_direct(sensor, AP0202AT_REG_SYSCTL_USER_DEFINED_DEVICE_ADDRESS_ID, &reg);
    if (0 != ret) {
        return -1;
    }
    if (reg != AP0202AT_SYSCTL_USER_DEFINED_DEVICE_ADDRESS_ID_DEFAULT_VALUE) {
        return -1;
    }

    *detected = true;

    return ret;
}

int ap0202at_detect_sensor_ar0147(sensor_t *sensor, bool *detected) {
    int ret = 0;
    if (NULL == detected) {
        return -1;
    }

    ret = ap0202at_ar0147_init0(sensor);
    if (0 != ret) {
        // printf("Error during init0: ");
        ap0202at_print_status(ret);
        // printf(" \n"); // space here avoids "undefined reference to putchar" error
        return ret;
    }

    // printf("Waiting for doorbell clear\n");
    ret = ap0202at_host_command_poll_doorbell_bit_clear(sensor, NULL, 500);
    if (0 != ret) {
        // printf("Error polling doorbell bit clear: ");
        ap0202at_print_status(ret);
        // printf(" \n"); // space here avoids "undefined reference to putchar" error
        return ret;
    }

    uint8_t cci_address;
    uint8_t revision;
    uint16_t model_id;
    ret = ap0202at_sensor_manager_discover_sensor(sensor, &cci_address, &revision, &model_id, 3000);
    if (0 != ret) {
        // printf("Error discovering sensor: ");
        ap0202at_print_status(ret);
        // printf(" \n"); // space here avoids "undefined reference to putchar" error
        return ret;
    }
    // printf("Discovered sensor at 0x%0X\n", cci_address);
    // printf("Sensor revision: 0x%0X\n", revision);
    // printf("Sensor model ID: 0x%04X\n", model_id);

    return ret;
}

int ap0202at_detect_sensor_ar0231at(sensor_t *sensor, bool *detected) {
    int ret = 0;
    if (NULL == detected) {
        return -1;
    }

    // printf("Attempting to detect AR0231AT sensor...\n");

    *detected = false;

    return ret;
}

#endif // (OMV_ENABLE_AP0202AT == 1)
