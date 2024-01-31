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

#include "py/mphal.h"

#include "sensor.h"
#include "ap0202at.h"
#include "ap0202at_regs.h"
#include "ap0202at_ar0231at_patches.h"

// Maximum time in milliseconds before host command
//   polling times out.
#define AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS (100)
#define AP0202AT_HOST_COMMAND_READ_POLL_TIMEOUT_MS (100)

/**
 * @brief Loads and applies patch 0056.
 * 
 * AP0202AT-REV2_AR0231AT-REV7.ini line 1135
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
static int load_apply_patch_0056(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    // AP0202AT-REV2_AR0231AT-REV7.ini line 1141
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x0) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x614) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8706, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // AP0202AT-REV2_AR0231AT-REV7.ini line 1149
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x4750) != 0) {
        return -1;
    }
    if (ap0202at_write_patch(sensor, patch_0056_data, sizeof(patch_0056_data) / sizeof(uint16_t)) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000) != 0) {
        return -1;
    }

    // AP0202AT-REV2_AR0231AT-REV7.ini line 1188
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x049c) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x0056) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x0614) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8702, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // AP0202AT-REV2_AR0231AT-REV7.ini line 1198
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8701, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return ret;
}

static int load_apply_patch_0156(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x614) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x54) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8706, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x4d64) != 0) {
        return -1;
    }
    if (ap0202at_write_patch(sensor, patch_0156_data, sizeof(patch_0156_data) / sizeof(uint16_t)) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000) != 0) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x049c) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x0156) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x0054) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8702, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8701, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return ret;
}

static int load_apply_patch_0256(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x668) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x620) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8706, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x4db8) != 0) {
        return -1;
    }
    if (ap0202at_write_patch(sensor, patch_0256_data, sizeof(patch_0256_data) / sizeof(uint16_t)) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000) != 0) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x0c20) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x0256) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x0620) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8702, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }

    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8701, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return ret;
}

static int load_apply_patch_0356(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0xc88) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x98) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8706, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x53d8) != 0) {
        return -1;
    }
    if (ap0202at_write_patch(sensor, patch_0356_data, sizeof(patch_0356_data) / sizeof(uint16_t)) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000) != 0) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x0cfc) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x0356) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x0098) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8702, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }

    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8701, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return ret;
}

static int load_apply_patch_0456(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0xd20) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x12d0) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8706, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x5470) != 0) {
        return -1;
    }
    if (ap0202at_write_patch(sensor, patch_0456_data, sizeof(patch_0456_data) / sizeof(uint16_t)) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000) != 0) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x1d40) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x0456) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x12d0) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8702, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }

    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8701, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return ret;
}

static int load_apply_patch_1156(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x1ff0) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x118) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8706, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x6740) != 0) {
        return -1;
    }
    if (ap0202at_write_patch(sensor, patch_1156_data, sizeof(patch_1156_data) / sizeof(uint16_t)) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000) != 0) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x20a0) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x1156) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x0118) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8702, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }

    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8701, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return ret;
}

static int load_apply_patch_1356(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x2108) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x414) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8706, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x6858) != 0) {
        return -1;
    }
    if (ap0202at_write_patch(sensor, patch_1356_data, sizeof(patch_1356_data) / sizeof(uint16_t)) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000) != 0) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x2108) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x1356) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x0414) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8702, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }

    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8701, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }
    if (!host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return ret;
}

static int load_apply_patch_1456(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x25c1) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0xdc) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8706, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x6c6c) != 0) {
        return -1;
    }
    if (ap0202at_write_patch(sensor, patch_1456_data, sizeof(patch_1456_data) / sizeof(uint16_t)) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000) != 0) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x25cc) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x1456) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x00dc) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8702, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }

    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8701, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }
    if (!host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return ret;
}

static int load_apply_patch_1556(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x25f8) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x1f8) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8706, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x6d48) != 0) {
        return -1;
    }
    if (ap0202at_write_patch(sensor, patch_1556_data, sizeof(patch_1556_data) / sizeof(uint16_t)) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000) != 0) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x2750) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x1556) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x01f8) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8702, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }

    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8701, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }
    if (!host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return ret;
}

static int load_apply_patch_1756(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x27f0) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x60) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8706, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x6f40) != 0) {
        return -1;
    }
    if (ap0202at_write_patch(sensor, patch_1756_data, sizeof(patch_1756_data) / sizeof(uint16_t)) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000) != 0) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x283c) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x1756) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x0060) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8702, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }

    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8701, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
      return -1;
    }
    if (!host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return ret;
}

static int load_apply_patch_1956(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x2850) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0xa0) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8706, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x6fa0) != 0) {
        return -1;
    }
    if (ap0202at_write_patch(sensor, patch_1956_data, sizeof(patch_1956_data) / sizeof(uint16_t)) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000) != 0) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x28cc) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x1956) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x00a0) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8702, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }

    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8701, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (!host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return ret;
}

static int load_apply_patch_2156(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x28f0) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0xb8) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8706, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_ACCESS_CTL_STAT, 0x0001) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_PHYSICAL_ADDRESS_ACCESS, 0x7040) != 0) {
        return -1;
    }
    if (ap0202at_write_patch(sensor, patch_2156_data, sizeof(patch_2156_data) / sizeof(uint16_t)) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_LOGICAL_ADDRESS_ACCESS, 0x0000) != 0) {
        return -1;
    }

    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, 0x297c) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1, 0x2156) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2, 0xa103) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3, 0x0204) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4, 0x00b8) != 0) {
        return -1;
    }
    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8702, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }

    if  (ap0202at_host_command_execute_command_synchronous(sensor, 0x8701, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (!host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return ret;
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

    // AP0202AT-REV2_AR0231AT-REV7.ini line 1118

    if (load_apply_patch_0056(sensor) != 0) {
        return -1;
    }
    if (load_apply_patch_0156(sensor) != 0) {
        return -1;
    }
    if (load_apply_patch_0256(sensor) != 0) {
        return -1;
    }
    if (load_apply_patch_0356(sensor) != 0) {
        return -1;
    }
    if (load_apply_patch_0456(sensor) != 0) {
        return -1;
    }
    if (load_apply_patch_1156(sensor) != 0) {
        return -1;
    }
    if (load_apply_patch_1356(sensor) != 0) {
        return -1;
    }
    if (load_apply_patch_1456(sensor) != 0) {
        return -1;
    }
    if (load_apply_patch_1556(sensor) != 0) {
        return -1;
    }
    if (load_apply_patch_1756(sensor) != 0) {
        return -1;
    }
    if (load_apply_patch_1956(sensor) != 0) {
        return -1;
    }
    if (load_apply_patch_2156(sensor) != 0) {
        return -1;
    }

    return ret;
}

static int sensor_do_sequencer(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    // AP0202AT-REV2_AR0231AT-REV7.ini line 1042
    if  (ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_CCIMGR_GET_LOCK, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // AP0202AT-REV2_AR0231AT-REV7.ini line 1043
    if  (ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_CCIMGR_LOCK_STATUS, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    printf("UNCHECKED HOST COMMAND RESULT: host_command_result: %d\n", host_command_result);
    // if (host_command_result != AP0202AT_HC_CMD_CCIMGR_LOCK_STATUS_RESP_LOCKED) {
    //     return -1;
    // }

    // AP0202AT-REV2_AR0231AT-REV7.ini line 1047
    if (ap0202at_write_sensor_u16(sensor, 0x2512,0x8000, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }

    // AP0202AT-REV2_AR0231AT-REV7.ini line 1066
    if (ap0202at_write_sensor_sequencer(sensor, 0x2510, ar0231at_sequencer_data, sizeof(ar0231at_sequencer_data)/ sizeof(uint16_t)) != 0) {
        return -1;
    }

    // AP0202AT-REV2_AR0231AT-REV7.ini line 1068
    if  (ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_CCIMGR_RELEASE_LOCK, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return ret;
}

static int sensor_post_initialization(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    // AP0202AT-REV2_AR0231AT-REV7.ini line 83

    // Get the CCIM lock
    if  (ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_CCIMGR_GET_LOCK, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    // AP0202AT-REV2_AR0231AT-REV7.ini line 985
    if (ap0202at_write_sensor_u16(sensor, 0x318E, 0x0200, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3092, 0x0C24, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x337A, 0x0C80, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3520, 0x1288, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3522, 0x880C, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3524, 0x0C12, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x352C, 0x1212, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x354A, 0x007F, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x350C, 0x055C, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    } 
    if (ap0202at_write_sensor_u16(sensor, 0x3506, 0x3333, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3508, 0x3333, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3100, 0x4000, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3280, 0x0FA0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3282, 0x0FA0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3284, 0x0FA0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3286, 0x0FA0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3288, 0x0FA0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x328A, 0x0FA0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x328C, 0x0FA0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x328E, 0x0FA0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3290, 0x0FA0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3292, 0x0FA0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3294, 0x0FA0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3296, 0x0FA0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3298, 0x0FA0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x329A, 0x0FA0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x329C, 0x0FA0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x329E, 0x0FA0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS,  AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }

    if (ap0202at_write_sensor_u16(sensor, 0x32E6, 0xE0, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x1008, 0x036F, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x100C, 0x058F, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x100E, 0x07AF, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x1010, 0x014F, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3230, 0x0312, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3232, 0x0532, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3234, 0x0752, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x3236, 0x0F2, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }

    if (ap0202at_write_sensor_u16(sensor, 0x32D0, 0x3A02, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x32D2, 0x3508, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x32D4, 0x3702, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x32D6, 0x3C04, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x32DC, 0x370A, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_sensor_u16(sensor, 0x0566, 0x3328, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }

    // Release the CCIM lock
    if  (ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_CCIMGR_RELEASE_LOCK, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return ret;
}

/**
 * @brief Reset the image sensor into the default state.
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
static int reset_image_sensor(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    if (ap0202at_write_reg_masked(
            sensor,
            AP0202AT_VAR_SENSOR_MGR_MODE,
            AP0202AT_VAR_SENSOR_MGR_SENSOR_DEFAULT_SEQUENCER_LOAD_INHIBIT_TRUE,
            AP0202AT_VAR_SENSOR_MGR_SENSOR_DEFAULT_SEQUENCER_LOAD_INHIBIT_MASK
        ) != 0) {
        return -1;
    }

    if (sensor_do_sequencer(sensor) != 0) {
        return -1;
    }

    // Sensor initialization
    // APA0202AT-REV2.ini line 304
    if  (ap0202at_host_command_execute_command_synchronous(sensor, AP0202AT_HC_CMD_SENSOR_MGR_INITIALIZE_SENSOR, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    if (sensor_post_initialization(sensor) != 0) {
        return -1;
    }

    return ret;
}

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
    int ret = 0;
    uint8_t state;
    uint16_t sensor_id;

    // Reset the AP0202AT
    // APA0202AT-REV2.ini line 298
    ret = ap0202at_reset(sensor);
    if (ret != 0) {
        return -1;
    }

    // Load patches for the AP0202AT ISP.
    if(load_patches(sensor) != 0) {
        return -1;
    }

    // Perform sensor discovery and fail if no sensor is
    //   found.
    ret = ap0202at_sensor_discovery(sensor, &sensor_id);
    if (ret != 0) {
        return -1;
    }
    printf("UNUSED SENSOR ID: sensor_id: %d\n", sensor_id);
    // if (sensor_id != AR0231AT_SENSOR_ID) {
    //     return -1;
    // }

    // Perform sensor configuration.
    if(reset_image_sensor(sensor) != 0) {
        return -1;
    }

    // Issue the Change Config command to enter streaming.
    ret = ap0202at_sysmgr_set_state(sensor, AP0202AT_HCI_SYS_STATE_STREAMING, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS);
    if (ret != 0) {
        printf("Failed to issue set state command.\n");
        return -1;
    }

    ret = ap0202at_sysmgr_get_state(sensor, &state, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS);
    if (ret != 0) {
        printf("Failed to issue get state command\n");
        return -1;
    }
    if (state != AP0202AT_HCI_SYS_STATE_STREAMING) {
        printf("Failed to enter streaming state. Found state 0x%X instead\n", state);
        return -1;
    }

    return ret;
}

/**
 * @brief Initialize the sensor structure for the AP0202AT.
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
int ap0202at_ar0231_init(sensor_t *sensor) {
    int ret = 0;

    // Initialize the AP0202AT portions of the sensor
    //   structure.
    ret = ap0202at_init(sensor);
    if (ret != 0) {
        return -1;
    }

    sensor->reset = reset;

    return 0;
}

#endif // (OMV_ENABLE_AP0202AT_AR0147 == 1)