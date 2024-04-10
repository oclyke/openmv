/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2023 oclyke <oclyke@oclyke.dev>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * AP0202AT register definitions.
 */
#ifndef __REG_REGS_AP0202AT_H__
#define __REG_REGS_AP0202AT_H__

// VARIABLE SHORTCUTS
#define AP0202AT_VARIABLE(page, offset)                          (0x8000 | ((page) << 10) | (offset))

// COMMAND_REGISTER SHORTCUTS
#define AP0202AT_REG_COMMAND_REGISTER                             (0x0000)

#define AP0202AT_HC_CHANGE_CONFIG                                 (0x8100)

#define AP0202AT_HC_CMD_SYSMGR_SET_STATE                          (0x8100)
#define AP0202AT_HC_CMD_SYSMGR_GET_STATE                          (0x8101)

#define AP0202AT_HC_CMD_SEQ_REFRESH                               (0x8606)
#define AP0202AT_HC_CMD_SEQ_REFRESH_STATUS                        (0x8607)

#define AP0202AT_HC_CMD_GPIO_SET_PROP                             (0x8400)
#define AP0202AT_HC_CMD_GPIO_GET_PROP                             (0x8401)
#define AP0202AT_HC_CMD_GPIO_SET_STATE                            (0x8402)
#define AP0202AT_HC_CMD_GPIO_GET_STATE                            (0x8403)

#define AP0202AT_HC_CMD_SENSOR_MGR_DISCOVER_SENSOR                (0x8E00)
#define AP0202AT_HC_CMD_SENSOR_MGR_INITIALIZE_SENSOR              (0x8E01)
#define AP0202AT_HC_CMD_SENSOR_MGR_DISCOVER_RESP_NO_SENSOR        (0xFFFF)

#define AP0202AT_HC_CMD_CCIMGR_GET_LOCK                           (0x8D00)
#define AP0202AT_HC_CMD_CCIMGR_LOCK_STATUS                        (0x8D01)
#define AP0202AT_HC_CMD_CCIMGR_RELEASE_LOCK                       (0x8D02)
#define AP0202AT_HC_CMD_CCIMGR_WRITE                              (0x8D06)
#define AP0202AT_HC_CMD_CCIMGR_READ                               (0x8D05)
#define AP0202AT_HC_CMD_CCIMGR_WRITE_BITFIELD                     (0x8D07)
#define AP0202AT_HC_CMD_CCIMGR_STATUS                             (0x8D08)

#define AP0202AT_HC_CMD_STEMGR_CONFIG                             (0x8310)
#define AP0202AT_HC_CMD_STEMGR_WRITE_CONFIG                       (0x8313)

#define AP0202AT_HC_RESP_ENOERR   (0)
#define AP0202AT_HC_RESP_ENOENT   (1)
#define AP0202AT_HC_RESP_EINTR    (2)
#define AP0202AT_HC_RESP_EIO      (3)
#define AP0202AT_HC_RESP_E2BIG    (4)
#define AP0202AT_HC_RESP_EBADF    (5)
#define AP0202AT_HC_RESP_EAGAIN   (6)
#define AP0202AT_HC_RESP_ENOMEM   (7)
#define AP0202AT_HC_RESP_EACCES   (8)
#define AP0202AT_HC_RESP_EBUSY    (9)
#define AP0202AT_HC_RESP_EEXIST   (10)
#define AP0202AT_HC_RESP_ENODEV   (11)
#define AP0202AT_HC_RESP_EINVAL   (12)
#define AP0202AT_HC_RESP_ENOSPC   (13)
#define AP0202AT_HC_RESP_ERANGE   (14)
#define AP0202AT_HC_RESP_ENOSYS   (15)
#define AP0202AT_HC_RESP_EALREADY (16)

// SYSCTL
#define AP0202AT_REG_SYSCTL_RESET_AND_MISC_CONTROL                (0x001A)
#define AP0202AT_SYSCTL_RESET_AND_MISC_CONTROL_RESET_SOFT         (0x0001)
#define AP0202AT_SYSCTL_RESET_AND_MISC_CONTROL_RESET_SOFT_MASK    (0x0001)

#define AP0202AT_REG_SYSCTL_COMMAND_REGISTER                      (0x0040)
#define AP0202AT_SYSCTL_COMMAND_REGISTER_DOORBELL_BIT             (0x8000)
#define AP0202AT_SYSCTL_COMMAND_REGISTER_DOORBELL_BIT_MASK        (0x8000)

// XDMA 
#define AP0202AT_REG_ACCESS_CTL_STAT                              (0x0982)
#define AP0202AT_PHYSICAL_ADDRESS_ACCESS                          (0x098A)

// SENSOR MANAGER VARIABLES
#define AP0202AT_VAR_SENSOR_MGR_MODE                              (AP0202AT_VARIABLE(0x13, 0x02))

#define AP0202AT_VAR_SENSOR_MGR_SENSOR_DEFAULT_SEQUENCER_LOAD_INHIBIT_MASK  (0x0010)
#define AP0202AT_VAR_SENSOR_MGR_SENSOR_DEFAULT_SEQUENCER_LOAD_INHIBIT_TRUE  (0x0010)
#define AP0202AT_VAR_SENSOR_MGR_SENSOR_DEFAULT_SEQUENCER_LOAD_INHIBIT_FALSE (0x0000)


// COMMAND HANDLER VARIABLES
#define AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0                    (AP0202AT_VARIABLE(0x1F, 0x00))
#define AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_1                    (AP0202AT_VARIABLE(0x1F, 0x02))
#define AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_2                    (AP0202AT_VARIABLE(0x1F, 0x04))
#define AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_3                    (AP0202AT_VARIABLE(0x1F, 0x06))
#define AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_4                    (AP0202AT_VARIABLE(0x1F, 0x08))

#endif // __REG_REGS_AP0202AT_H__
