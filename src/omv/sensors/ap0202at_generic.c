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

    ret = ap0202at_read_reg_direct(sensor, AP0202AT_REG_SYSCTL_CHIP_VERSION_REG, &reg);
    if (0 != ret) {
        printf("ap0202at read chip version failed.\n");
        return -1;
    }
    printf("ap0202at chip version: 0x%X\n", reg);
    if (reg != AP0202AT_SYSCTL_CHIP_VERSION_REG_DEFAULT_VALUE) {
        printf("ap0202at chip version mismatch.\n");
        return -1;
    }

    ret = ap0202at_read_reg_direct(sensor, AP0202AT_REG_SYSCTL_USER_DEFINED_DEVICE_ADDRESS_ID, &reg);
    if (0 != ret) {
        printf("ap0202at read user defined device address id failed.\n");
        return -1;
    }
    printf("ap0202at user defined device address id: 0x%X\n", reg);
    if (reg != AP0202AT_SYSCTL_USER_DEFINED_DEVICE_ADDRESS_ID_DEFAULT_VALUE) {
        printf("ap0202at user defined device address id mismatch.\n");
        return -1;
    }

    *detected = true;

    return ret;
}

int ap0202at_detect_sensor_ar0147(sensor_t *sensor, bool *detected) {
    int ret = 0;
    uint16_t host_command_result;
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
    ret = ap0202at_host_command(sensor, AP0202AT_HC_CMD_CCIMGR_GET_LOCK, &host_command_result, 100, 3000);
    if (0 != ret) {
        printf("Error acquiring CCIM lock\n");
        return ret;
    }
    if ((host_command_result != AP0202AT_HC_RESP_ENOERR)
        && (host_command_result != AP0202AT_HC_RESP_EALREADY)) {
        printf("Failed to acquire CCIM lock\n");
        printf("host_command_result: 0x%X\n", host_command_result);
        return -1;
    }

    do {
        ret = ap0202at_host_command(sensor, AP0202AT_HC_CMD_CCIMGR_LOCK_STATUS, &host_command_result, 100, 100);
        if (0 != ret) {
            printf("Failed to get CCIM lock status\n");
            printf("host_command_result: 0x%X\n", host_command_result);
            return ret;
        }
        printf("CCIM lock status: %d\n", host_command_result);
    } while (host_command_result != AP0202AT_HC_RESP_ENOERR);

    // // set the i2c clock speed to 400kHz (AP0202AT_HC_CMD_CCIMGR_CONFIG)
    // if (ap0202at_poll_doorbell_bit(sensor, NULL, 100) != 0) {
    //     printf("Failed to ensure clear doorbell bit\n");
    //     return -1;
    // }
    // if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x0006) != 0) {
    //     return -1;
    // }
    // if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x1A80) != 0) {
    //     return -1;
    // }
    // if (ap0202at_host_command(sensor, AP0202AT_HC_CMD_CCIMGR_CONFIG, &host_command_result) != 0) {
    //     printf("Failed to set CCIM config\n");
    //     return -1;
    // }

    // set the i2c device
    if(ap0202at_poll_doorbell_bit(sensor, NULL, 100) != 0) {
        printf("Failed to ensure clear doorbell bit\n");
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, ar0147_i2c_addr_w) != 0) {
        printf("Error writing to CCIM params pool\n");
        return -1;
    }
    if (ap0202at_host_command(sensor, AP0202AT_HC_CMD_CCIMGR_SET_DEVICE, &host_command_result, 100, 100) != 0) {
        printf("Error setting CCIM device\n");
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        printf("Failed to set CCIM device: (err %d)\n", host_command_result);
        return -1;
    }

    // read the sensor ID
    if (ap0202at_poll_doorbell_bit(sensor, NULL, 100) != 0) {
        printf("Failed to ensure clear doorbell bit\n");
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, AR0147_REG_CHIP_VERSION_REG) != 0) {
        printf("Error writing to CCIM params pool\n");
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x0002) != 0) {
        printf("Error writing to CCIM params pool\n");
        return -1;
    }
    if (ap0202at_host_command(sensor, AP0202AT_HC_CMD_CCIMGR_READ, &host_command_result, 100, 100) != 0) {
        printf("Error issuing read command\n");
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        printf("Failed issue read command: (err %d)\n", host_command_result);
        return -1;
    }

    // wait for the read to be completed
    do {
        ret = ap0202at_host_command(sensor, AP0202AT_HC_CMD_CCIMGR_STATUS, &host_command_result, 100, 100);
        if (0 != ret) {
            printf("Failed to get CCI status\n");
            return ret;
        }
        printf("CCI status: %d\n", host_command_result);
    } while (host_command_result != AP0202AT_HC_RESP_ENOERR);

    // read the sensor ID from the params pool
    uint16_t sensor_id;
    if (ap0202at_read_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, &sensor_id) != 0) {
        printf("Error reading from CCIM params pool\n");
        return -1;
    }
    printf("Sensor ID: 0x%04X\n", sensor_id);

    // check whether the correct sensor was detected
    if (sensor_id == AR0147_CHIP_VERSION_REG_DEFAULT) {
        *detected = true;
    }

    // release the ccim lock
    ret = ap0202at_host_command(sensor, AP0202AT_HC_CMD_CCIMGR_RELEASE_LOCK, &host_command_result, 100, 100);
    if (0 != ret) {
        printf("Error releasing CCIM lock\n");
        return ret;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        printf("Failed to release CCIM lock\n");
        return -1;
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
