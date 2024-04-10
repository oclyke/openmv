/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2023-2024 oclyke <oclyke@oclyke.dev>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * AR0147 w/ AP0202AT ISP driver.
 */
#include "omv_boardconfig.h"
#if (OMV_ENABLE_AP0202AT_AR0147 == 1)

#include <stdio.h>

#include "sensor.h"

#include "py/mphal.h"

#include "ap0202at.h"
#include "ap0202at_regs.h"
#include "ap0202at_ar0147_patches.h"

// Maximum time in milliseconds before host command
//   polling times out.
#define AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS (100)
#define AP0202AT_HOST_COMMAND_READ_POLL_TIMEOUT_MS (100)

#define APPLY_PATCH_TIMEOUT_MS (15000)

/**
 * @brief Blocking delay in milliseconds.
 * 
 * Relies on mp_hal_ticks_ms.
 * 
 * @param ms the number of milliseconds to delay.
 * @return int 0 on success, -1 on error.
 */
static int delay_ms(uint32_t ms) {
    int ret = 0;
    mp_uint_t start = mp_hal_ticks_ms();

    while ((mp_hal_ticks_ms() - start) < ms) {
        // Do nothing.
    }

    return ret;
}

/**
 * @brief Installs the sensor register write workaround.
 * @ref AP0202AT-REV2.ini lines 331, 370
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
static int install_sensor_reg_write_workaround(sensor_t *sensor) {
    int ret = 0;

    // AP0202AT-REV2.ini line 372
    static uint16_t reg_data0[] = {
        0x0016, 0x6039, // Enable clock to sensor
        0x0032, 0xC0F0, // Bring sensor out of reset
        0x3B00, 0x2083,
        0x3B02, 0x3500,
        0x3B04, 0x0100,
        0x3B88, 0x0005,
    };

    // AP0202AT-REV2.ini line 379
    static uint16_t reg_data1[] = {
        0x3B00, 0x2083,
        0x3B02, 0x3006,
        0x3B04, 0x0008,
        0x3B88, 0x0005,
        0x3B00, 0x2083,
        0x3B02, 0x3F66,
        0x3B04, 0x00C0,
        0x3B88, 0x0005,
    };

    // Loop over the first set of register writes.
    const size_t reg_data0_len = sizeof(reg_data0) / sizeof(reg_data0[0]);
    for (uint16_t i = 0; i < reg_data0_len; i += 2) {
        if (ap0202at_write_reg_direct(sensor, reg_data0[i], reg_data0[i + 1]) != 0) {
            return -1;
        }
    }

    // AP0202AT-REV2.ini line 378
    delay_ms(1);

    // Loop over the second set of register writes.
    const size_t reg_data1_len = sizeof(reg_data1) / sizeof(reg_data1[0]);
    for (uint16_t i = 0; i < reg_data1_len; i += 2) {
        if (ap0202at_write_reg_direct(sensor, reg_data1[i], reg_data1[i + 1]) != 0) {
            return -1;
        }
    }

    // AP0202AT-REV2.ini line 387
    delay_ms(50);

    return ret;
}

/**
 * @brief Loads and applies patch 28d4.
 * 
 * APA0202AT-REV2_AR0147-REV3.ini line 1108
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
static int load_apply_patch_28d4(sensor_t *sensor) {
    ap0202at_status_t status = STATUS_SUCCESS;
    uint16_t host_command_result;

    // APA0202AT-REV2_AR0147-REV3.ini line 1114
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x5d8);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x2364);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_RESERVE_RAM, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 1121
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x4d28);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_patch(sensor, patch_28d4_data, sizeof(patch_28d4_data) / sizeof(patch_28d4_data[0]));
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000);
    if (STATUS_SUCCESS != status) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 1316
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x2504);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x28d4);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x2364);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_APPLY_PATCH, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        // printf("Error: host command result not AP0202AT_HC_RESP_ENOERR. Line: %d\n", __LINE__);
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 1327
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_STATUS, &host_command_result, APPLY_PATCH_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        // printf("Error: host command status not STATUS_SUCCESS. Line: %d\n", __LINE__);
        // printf("Status: %d\n", status);
        ap0202at_print_status(status);
        // printf(" \n"); // space here avoids "undefined reference to putchar" error
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        // printf("Error: host command result not AP0202AT_HC_RESP_ENOERR. Line: %d\n", __LINE__);
        // printf("Result: %d\n", host_command_result);
        return -1;
    }

    return 0;
}

/**
 * @brief Loads and applies patch 01d4.
 * 
 * APA0202AT-REV2_AR0147-REV3.ini line 822
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
static int load_apply_patch_01d4(sensor_t *sensor) {
    ap0202at_status_t status = STATUS_SUCCESS;
    uint16_t host_command_result;

    // APA0202AT-REV2_AR0147-REV3.ini line 829
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x0);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x54);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_RESERVE_RAM, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 836
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x4750);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_patch(sensor, patch_01d4_data, sizeof(patch_01d4_data) / sizeof(patch_01d4_data[0]));
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000);
    if (STATUS_SUCCESS != status) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 843
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x0030);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x01d4);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x0054);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_APPLY_PATCH, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 854
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_STATUS, &host_command_result, APPLY_PATCH_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return 0;
}

/**
 * @brief Loads and applies patch 03d4.
 * 
 * APA0202AT-REV2_AR0147-REV3.ini line 860
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
static int load_apply_patch_03d4(sensor_t *sensor) {
    ap0202at_status_t status = STATUS_SUCCESS;
    uint16_t host_command_result;

    // APA0202AT-REV2_AR0147-REV3.ini line 867
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x54);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x98);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_RESERVE_RAM, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 874
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x47a4);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_patch(sensor, patch_03d4_data, sizeof(patch_03d4_data) / sizeof(patch_03d4_data[0]));
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000);
    if (STATUS_SUCCESS != status) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 884
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x00c8);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x03d4);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x0098);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_APPLY_PATCH, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 894
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_STATUS, &host_command_result, APPLY_PATCH_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return 0;
}


/**
 * @brief Loads and applies patch 11d4.
 * 
 * APA0202AT-REV2_AR0147-REV3.ini line 900
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
static int load_apply_patch_11d4(sensor_t *sensor) {
    ap0202at_status_t status = STATUS_SUCCESS;
    uint16_t host_command_result;

    // APA0202AT-REV2_AR0147-REV3.ini line 907
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x0ec);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x118);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_RESERVE_RAM, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 914
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x483c);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_patch(sensor, patch_11d4_data, sizeof(patch_11d4_data) / sizeof(patch_11d4_data[0]));
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000);
    if (STATUS_SUCCESS != status) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 925
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x019c);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x11d4);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x0118);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_APPLY_PATCH, &host_command_result, APPLY_PATCH_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 936
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_STATUS, &host_command_result, APPLY_PATCH_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return 0;
}


/**
 * @brief Loads and applies patch 21d4.
 * 
 * APA0202AT-REV2_AR0147-REV3.ini line 900
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
static int load_apply_patch_21d4(sensor_t *sensor) {
    ap0202at_status_t status = STATUS_SUCCESS;
    uint16_t host_command_result;

    // APA0202AT-REV2_AR0147-REV3.ini line 1036
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x4e0);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0xb8);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_RESERVE_RAM, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 1044
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x4c30);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_patch(sensor, patch_21d4_data, sizeof(patch_21d4_data) / sizeof(patch_21d4_data[0]));
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000);
    if (STATUS_SUCCESS != status) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 1054
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x056c);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x21d4);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x00b8);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_APPLY_PATCH, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 1064
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_STATUS, &host_command_result, APPLY_PATCH_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return 0;
}

/**
 * @brief Loads and applies patch 37d4.
 * 
 * APA0202AT-REV2_AR0147-REV3.ini line 1432
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
static int load_apply_patch_37d4(sensor_t *sensor) {
    ap0202at_status_t status = STATUS_SUCCESS;
    uint16_t host_command_result;

    // APA0202AT-REV2_AR0147-REV3.ini line 1434
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x293c);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x218);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_RESERVE_RAM, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 1441
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x708c);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_patch(sensor, patch_37d4_data, sizeof(patch_37d4_data) / sizeof(patch_37d4_data[0]));
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000);
    if (STATUS_SUCCESS != status) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 1459
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x2b30);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x37d4);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x0218);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_APPLY_PATCH, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 1469
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_STATUS, &host_command_result, APPLY_PATCH_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return 0;
}

/**
 * @brief Loads and applies patch 39d4.
 * 
 * APA0202AT-REV2_AR0147-REV3.ini line 
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
static int load_apply_patch_39d4(sensor_t *sensor) {
    ap0202at_status_t status = STATUS_SUCCESS;
    uint16_t host_command_result;

    // APA0202AT-REV2_AR0147-REV3.ini line
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x2b04);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0xac);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_RESERVE_RAM, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x72a4);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_patch(sensor, patch_39d4_data, sizeof(patch_39d4_data) / sizeof(patch_39d4_data[0]));
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000);
    if (STATUS_SUCCESS != status) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x2bd8);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x39d4);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x00ac);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_APPLY_PATCH, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // APA0202AT-REV2_AR0147-REV3.ini line 
    status = ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_PATCHLDR_STATUS, &host_command_result, APPLY_PATCH_TIMEOUT_MS);
    if (STATUS_SUCCESS != status) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return 0;
}

/**
 * @brief Loads all applicable patches for the combination
 *   of the AP0202AT ISP and the image sensor.
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
static int load_patches(sensor_t *sensor) {
    int ret = 0;

    // Patch 28d4
    ret = load_apply_patch_28d4(sensor);
    if (ret != 0) {
        // printf("patch 28d4 failed\n");
        return ret;
    }

    // Patch 01d4
    ret = load_apply_patch_01d4(sensor);
    if (ret != 0) {
        // printf("patch 01d4 failed\n");
        return ret;
    }

    // Patch 03d4
    ret = load_apply_patch_03d4(sensor);
    if (ret != 0) {
        // printf("patch 03d4 failed\n");
        return ret;
    }

    // Patch 11d4
    ret = load_apply_patch_11d4(sensor);
    if (ret != 0) {
        // printf("patch 11d4 failed\n");
        return ret;
    }

    // Patch 21d4
    ret = load_apply_patch_21d4(sensor);
    if (ret != 0) {
        // printf("patch 21d4 failed\n");
        return ret;
    }

    // Patch 37d4
    ret = load_apply_patch_37d4(sensor);
    if (ret != 0) {
        // printf("patch 37d4 failed\n");
        return ret;
    }

    // Patch 39d4
    ret = load_apply_patch_39d4(sensor);
    if (ret != 0) {
        // printf("patch 39d4 failed\n");
        return ret;
    }

    return ret;
}

/**
 * @brief Initialization phase for the AP0202AT ISP and the
 *  AR0147 image sensor.
 * 
 * Assumptions:
 * - The AP0202AT ISP was already reset into host configuration
 *  mode.
 * 
 * In this phase:
 * - The sensor reg write workaround is applied.
 * - Patches required for sensor discovery are applied.
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
int ap0202at_ar0147_init0(sensor_t *sensor) {
    int ret = 0;

    ret = install_sensor_reg_write_workaround(sensor);
    if (ret != 0) {
        // printf("install_sensor_reg_write_workaround failed\n");
        return ret;
    }

    ret = load_patches(sensor);
    if (ret != 0) {
        // printf("load_patches failed\n");
        return ret;
    }

    return ret;
}

// /**
//  * @brief Reset the image sensor into the default state.
//  * 
//  * @param sensor the sensor struct.
//  * @return int 0 on success, -1 on error.
//  */
// static int reset_image_sensor(sensor_t *sensor) {
//     int ret = 0;
//     uint16_t host_command_result;

//     // The only image sensor supported is the AR0147.
//     if (false) {
//         return -1;
//     }

//     if (ap0202at_write_reg_masked(
//             sensor,
//             AP0202AT_VAR_SENSOR_MGR_MODE,
//             AP0202AT_VAR_SENSOR_MGR_SENSOR_DEFAULT_SEQUENCER_LOAD_INHIBIT_TRUE,
//             AP0202AT_VAR_SENSOR_MGR_SENSOR_DEFAULT_SEQUENCER_LOAD_INHIBIT_MASK
//         ) != 0) {
//         return -1;
//     }

//     // APA0202AT-REV2_AR0147-REV3.ini line 749
//     if  (ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_CCIMGR_GET_LOCK, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
//         return -1;
//     }
//     if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
//         return -1;
//     }

//     // APA0202AT-REV2_AR0147-REV3.ini line 750
//     if  (ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_CCIMGR_LOCK_STATUS, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
//         return -1;
//     }
//     printf("UNCHECKED RESULT host_command_result: %04x\n", host_command_result);
//     // if (host_command_result != AP0202AT_HC_CMD_CCIMGR_LOCK_STATUS_RESP_LOCKED) {
//     //     return -1;
//     // }

//     // APA0202AT-REV2_AR0147-REV3.ini line 754
//     if (ap0202at_write_sensor_u16(sensor, 0x2512, 0x8000, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
//         return -1;
//     }

//     // APA0202AT-REV2_AR0147-REV3.ini line 803
//     if (ap0202at_write_sensor_sequencer(sensor, 0x2510, ar0147_sequencer_data, sizeof(ar0147_sequencer_data)/ sizeof(uint16_t)) != 0) {
//         return -1;
//     }

//     // APA0202AT-REV2_AR0147-REV3.ini line 804
//     if  (ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_CCIMGR_RELEASE_LOCK, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
//         return -1;
//     }
//     if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
//         return -1;
//     }

//     // APA0202AT-REV2.ini line 338
//     if  (ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_SENSOR_MGR_INITIALIZE_SENSOR, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
//         return -1;
//     }
//     if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
//         return -1;
//     }

//     // APA0202AT-REV2_AR0147-REV3.ini line 733
//     if  (ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_CCIMGR_GET_LOCK, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
//         return -1;
//     }
//     if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
//         return -1;
//     }

//     // APA0202AT-REV2_AR0147-REV3.ini line 738
//     if (ap0202at_write_sensor_u16(sensor, 0x3754, 0x0000, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
//         return -1;
//     }
//     if (ap0202at_write_sensor_u16(sensor, 0x3F90, 0x0800, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
//         return -1;
//     }

//     // APA0202AT-REV2_AR0147-REV3.ini line 741
//     if  (ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_CCIMGR_RELEASE_LOCK, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
//         return -1;
//     }
//     if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
//         return -1;
//     }


//     return ret;
// }

/**
 * @brief Reset the AP0202AT ISP and the AR0147 into the
 *   default state.
 * 
 * - 
 * 
 * @param sensor Pointer to sensor struct.
 * @return int 0 on success, -1 on error.
 */
static int reset(sensor_t *sensor) {
    return -1;

    // int ret = 0;
    // uint8_t state;

    // // Reset the AP0202AT
    // ret = ap0202at_reset(sensor);
    // if (ret != 0) {
    //     printf("Failed to reset AP0202AT\n");
    //     return -1;
    // }

    // // Install the sensor reg write workaround.
    // if (install_sensor_reg_write_workaround(sensor) != 0) {
    //     printf("Failed to install sensor reg write workaround\n");
    //     return -1;
    // }

    // // Load patches for the AP0202AT ISP.
    // if(load_patches(sensor) != 0) {
    //     printf("Failed to load patches\n");
    //     return -1;
    // }

    // // Perform sensor discovery and fail if no sensor is
    // //   found.
    // uint8_t cci_address;
    // uint8_t revision;
    // uint16_t model_id;
    // ret = ap0202at_sensor_manager_discover_sensor(sensor, &cci_address, &revision, &model_id, 100);
    // if (ret != 0) {
    //     printf("Failed to discover sensor\n");
    //     return -1;
    // }
    // printf("Discovered sensor at 0x%0X\n", cci_address);
    // printf("Sensor revision: 0x%0X\n", revision);
    // printf("Sensor model ID: 0x%04X\n", model_id);

    // // Perform sensor configuration.
    // if(reset_image_sensor(sensor) != 0) {
    //     printf("Failed to reset image sensor\n");
    //     return -1;
    // }

    // // Issue the Change Config command to enter streaming.
    // ret = ap0202at_sysmgr_set_state(sensor, AP0202AT_HCI_SYS_STATE_STREAMING, 100);
    // if (ret != 0) {
    //     printf("Failed to issue set state command.\n");
    //     return -1;
    // }

    // ret = ap0202at_sysmgr_get_state(sensor, &state, 100);
    // if (ret != 0) {
    //     printf("Failed to issue get state command\n");
    //     return -1;
    // }
    // if (state != AP0202AT_HCI_SYS_STATE_STREAMING) {
    //     printf("Failed to enter streaming state. Found state 0x%X instead\n", state);
    //     return -1;
    // }

    // return ret;
}

/**
 * @brief Initialize the sensor structure for the AP0202AT.
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
int ap0202at_ar0147_init(sensor_t *sensor) {
    int ret = 0;

    // Initialize the AP0202AT portions of the sensor
    //   structure.
    ret = ap0202at_init(sensor);
    if (ret != 0) {
        return -1;
    }

    sensor->reset = reset;

    // Set sensor flags
    sensor->hw_flags.vsync = 1; // ???
    sensor->hw_flags.hsync = 0; // ???
    sensor->hw_flags.pixck = 1; // ???
    sensor->hw_flags.gs_bpp = 1; // ??? 1 or 2
    sensor->hw_flags.rgb_swap = 0; // ??? (costs CPU time, so if you can configure the camera the right way)
    sensor->hw_flags.yuv_order = SENSOR_HW_FLAGS_YVU422; // ???

    return 0;
}

#endif // (OMV_ENABLE_AP0202AT_AR0147 == 1)
