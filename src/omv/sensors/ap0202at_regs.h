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

// COMMAND_REGISTER SHORTCUTS
#define AP0202AT_REG_COMMAND_REGISTER                             (0x0000)

#define AP0202AT_HC_CHANGE_CONFIG                                 (0x8100)

#define AP0202AT_HC_SENSOR_DISCOVERY                              (0x8E00)
#define AP0202AT_HC_SENSOR_DISCOVERY_RETURN_NO_SENSOR             (0xFFFF)

// SYSCTL
#define AP0202AT_REG_SYSCTL_RESET_AND_MISC_CONTROL                (0x001A)
#define AP0202AT_SYSCTL_RESET_AND_MISC_CONTROL_RESET_SOFT         (0x0001)
#define AP0202AT_SYSCTL_RESET_AND_MISC_CONTROL_RESET_SOFT_MASK    (0x0001)

#define AP0202AT_REG_SYSCTL_COMMAND_REGISTER                      (0x0040)
#define AP0202AT_SYSCTL_COMMAND_REGISTER_DOORBELL_BIT             (0x8000)
#define AP0202AT_SYSCTL_COMMAND_REGISTER_DOORBELL_BIT_MASK        (0x8000)

#endif // __REG_REGS_AP0202AT_H__
