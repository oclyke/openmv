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

    // for OpenMV AR0147 sensor module
    const uint8_t ar0147_i2c_addr_w = 0x20;

    printf("Attempting to detect AR0147 sensor...\n");

    // 1. acquire the ccim lock (AP0202AT_HC_CMD_CCIMGR_GET_LOCK)
    // 2. poll until lock is acquired successfully (AP0202AT_HC_CMD_CCIMGR_LOCK_STATUS, AP0202AT_HC_RESP_ENOERR)
    // 3. set the proper i2c clock speed AP0202AT_HC_CMD_CCIMGR_CONFIG
    // 4. set the correct i2c address w/ AP0202AT_HC_CMD_CCIMGR_SET_DEVICE

    // acquire the ccim lock
    ret = ap0202at_cci_manager_get_lock(sensor, 100, 100);
    if (0 != ret) {
        printf("Error acquiring CCIM lock\n");
        return ret;
    }

    // ret = ap0202at_cci_manager_config(sensor, 400000, 100);
    // if (0 != ret) {
    //     printf("Error configuring CCIM\n");
    //     return ret;
    // }

    // set the i2c device
    ret = ap0202at_cci_manager_set_device(sensor, ar0147_i2c_addr_w, 100);
    if (0 != ret) {
        printf("Error setting CCIM device\n");
        return ret;
    }

    // read the sensor ID
    uint8_t data[2];
    ret = ap0202at_cci_manager_read(sensor, AR0147_REG_CHIP_VERSION_REG, data, 2, 100, 100);
    if (0 != ret) {
        printf("Error reading from CCIM\n");
        return ret;
    }
    uint16_t sensor_id = (data[0] << 8) | data[1];
    printf("Sensor ID: 0x%04X\n", sensor_id);

    // check whether the correct sensor was detected
    if (sensor_id == AR0147_CHIP_VERSION_REG_DEFAULT) {
        *detected = true;
    }

    // release the ccim lock
    ret = ap0202at_cci_manager_release_lock(sensor, 100);
    if (0 != ret) {
        printf("Error releasing CCIM lock\n");
        return ret;
    }

    return ret;
}

int ap0202at_detect_sensor_ar0231at(sensor_t *sensor, bool *detected) {
    int ret = 0;
    if (NULL == detected) {
        return -1;
    }

    printf("Attempting to detect AR0231AT sensor...\n");

    *detected = false;

    return ret;
}

#endif // (OMV_ENABLE_AP0202AT == 1)
