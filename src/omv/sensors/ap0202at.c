/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2023 oclyke <oclyke@oclyke.dev>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * AP0202AT driver.
 */
#include "omv_boardconfig.h"
#if (OMV_ENABLE_AP0202AT == 1)

// Maximum time in milliseconds before host command
//   polling times out.
#define AP0202AT_HOST_COMMAND_POLL_TIMEOUT_MS (100)

// Maximum delay in milliseconds before SYSMGR_GET_STATE
//   polling times out during a reset.
#define AP0202AT_RESET_GET_STATE_TIMEOUT_MS (200)

/**
 * @brief Read a register from the AP0202AT ISP.
 * @details Uses the two-wire interface to read a 16-bit
 *   wide register from the AP0202AT ISP.
 * 
 * This function does not perform a NULL check on the
 *   output parameter.
 * 
 * @param sensor the sensor struct.
 * @param reg_addr the register address.
 * @param reg_data [output] pointer to the register data.
 * @return int 0 on success, -1 on error.
 */
static int read_reg_direct(sensor_t *sensor, uint16_t reg_addr, uint16_t* reg_data) {
    int ret = 0;

    // Translate OpenMV return code.
    if (omv_i2c_readw2(&sensor->i2c_bus, sensor->slv_addr, reg_addr, reg_data) != 0) {
        return -1;
    }

    return ret;
}

/**
 * @brief Write data to a register in the AP0202AT ISP.
 * @details Uses the two-wire interface to write a 16-bit
 *   wide register in the AP0202AT ISP.
 * 
 * @param sensor the sensor struct.
 * @param reg_addr the register address at which the data
 *   will be written.
 * @param data the data to be written to the register.
 * @return int 0 on success, -1 on error.
 */
static int write_reg_direct(sensor_t *sensor, uint16_t reg_addr, uint16_t data) {
    int ret = 0;

    // Translate OpenMV return code.
    if(omv_i2c_writew2(&sensor->i2c_bus, sensor->slv_addr, reg_addr, data) != 0) {
        ret = -1;
    }

    return ret;
}

/**
 * @brief Write data to a register field in the AP0202AT
 *   ISP.
 * @details Uses the two-wire interface to perform a
 *   read-modify-write operation on a 16-bit wide register
 *   according to a given mask.
 * 
 * The mask should be SET in the bits which are to be
 *   modified. Cleared bits will be left unchanged in the
 *   register.
 * 
 * Both the register value and the mask must be correctly
 *   aligned within the register. This function does not
 *   perform any shifting.
 * 
 * @param sensor the sensor struct.
 * @param reg_addr the register address at which the data
 *   will be written.
 * @param data the data to be written to the register.
 * @param mask the mask indicating portion of data to be
 *   written in the register.
 * @return int 0 on success, -1 on error.
 */
static int write_masked_reg(sensor_t *sensor, uint16_t reg_addr, uint16_t data, uint16_t mask) {
    int ret = 0;
    uint16_t reg_data;

    // Read the existing data from the register.
    ret = read_reg_direct(sensor, reg_addr, &reg_data);
    if (ret == -1) {
        return ret;
    }
    
    // Update the register data.
    reg_data &= ~mask;
    reg_data |= (data & mask);

    // Write the updated data to the register.
    // Translate OpenMV return code.
    if(write_reg_direct(sensor, reg_addr, reg_data) != 0) {
        ret = -1;
    }

    return ret;
}

/**
 * @brief Poll the Doorbell Bit until it is clear or a
 *   timeout occurs.
 * 
 * This function relies on the read_reg_direct function
 *   to be blocking in order to limit the rate of polling.
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
static int poll_doorbell_bit(sensor_t *sensor) {
    int ret = 0;
    for (mp_uint_t start = mp_hal_ticks_ms();;) {
        uint16_t reg_data;

        // Read the COMMAND_REGISTER.
        if(read_reg_direct(sensor, AP0202AT_REG_COMMAND_REGISTER, &reg_data) != 0) {
            return -1;
        }

        // Return success if the Doorbell Bit is clear.
        if ((reg_data & AP0202AT_SYSCTL_COMMAND_REGISTER_DOORBELL_BIT_MASK) == 0) {
            return 0;
        }

        // Return error if the timeout has elapsed.
        if ((mp_hal_ticks_ms() - start) >= AP0202AT_HOST_COMMAND_POLL_TIMEOUT_MS) {
            return -1;
        }
    }

    return ret;
}


/**
 * @brief Issue a command to the AP0202AT ISP via the host
 *   command interface.
 * @details Polls the Doorbell Bit of the COMMAND_REGISTER
 *   until either it is clear or a timeout occurs. If the
 *   Doorbell Bit is clear, the command is issued.
 * 
 * Note: This function does not check the return value of
 *   the command. Use the read_host_command_result function
 *   to check the result of the previous command.
 * 
 * @param sensor the sensor struct.
 * @param command the command to be issued.
 * @return int 0 on success, -1 on error.
 */
static int issue_host_command(sensor_t *sensor, uint16_t command) {
    int ret = 0;

    if (poll_doorbell_bit(sensor) != 0) {
        return -1;
    }

    // Doorbell bit is clear, OK to issue command.
    command |= AP0202AT_SYSCTL_COMMAND_REGISTER_DOORBELL_BIT_MASK;
    if (write_reg_direct(sensor, AP0202AT_REG_COMMAND_REGISTER, command) != 0) {
        return -1;
    }

    return ret;
}

/**
 * @brief Read the result of the previous host command.
 * @details Polls the Doorbell Bit of the COMMAND_REGISTER
 *   to ensure that the previous command has completed.
 * 
 * This function is idempotent. It will always read the
 *   result of the last command issued.
 * 
 * In order to issue a command and read the result use the
 *   host_command function.
 * 
 * This function does not perform a NULL check on the
 *   output parameter.
 * 
 * @param sensor the sensor struct.
 * @param result [output] pointer for the result of the
 *   previous command.
 * @return int 0 on success, -1 on error.
 */
static int read_host_command_result(sensor_t *sensor, uint16_t *result) {
    int ret = 0;

    // Poll the Doorbell Bit.
    if (poll_doorbell_bit(sensor) != 0) {
        return -1;
    }

    // Read the result into the output parameter.
    if (read_reg_direct(sensor, AP0202AT_REG_COMMAND_REGISTER, result) != 0) {
        return -1;
    }

    return ret;
}

/**
 * @brief Perform a complete host command cycle.
 * @details Issues a command to the AP0202AT ISP via the
 *   host command interface and waits for the result.
 * 
 * This function does not perform a NULL check on the
 *   output parameter.
 * 
 * This function combines the issue_host_command and the
 *   read_host_command_result functions. Thusly, it has
 *   the opportunity to time out at two points.
 *   - Firstly: at the issue_host_command function. A
 *       timeout here indicates that a previous command
 *       is still in progress.
 *   - Secondly: at the read_host_command_result function.
 *       A timeout here indicates that the command that was
 *       issued was not able to complete in time.
 * 
 * @param sensor the sensor struct.
 * @param command 
 * @param result [output] pointer for the result of the
 *   previous command.
 * @return int 
 */
static int host_command(sensor_t sensor, uint16_t command, uint16_t* result) {
    int ret = 0;

    // Issue the command.
    if (issue_host_command(&sensor, command) != 0) {
        return -1;
    }

    // Read the result.
    if (read_host_command_result(&sensor, result) != 0) {
        return -1;
    }

    return ret;
}

/**
 * @brief Enter configuration mode via hardware reset.
 * @details Toggles the RESET_BAR pin for the required
 *   duration to perform a reset. Immediately exits
 *   reset state in order to enter configuration mode.
 * 
 * The state of the SPI_SDI pin will determine the
 *   flow of control through the configuration process
 *   after reset.
 * 
 * The minimum duration of the LOW RESET_BAR pulse is
 *   20 clock cycles. (Datasheet Table 7. HARD RESET)
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
static int enter_configuration_mode_hardware(sensor_t *sensor) {
    int ret = 0;

    // Hardware reset is not implemented.
    ret = -1;

    return ret;
}

/**
 * @brief Enter configuration mode via software reset.
 * @details Both enters and exits the software reset state
 *   in order to enter configuration mode.
 * 
 * The state of the SPI_SDI pin will determine the
 *   flow of control through the configuration process
 *   after reset.
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
static int enter_configuration_mode_software(sensor_t *sensor) {
    int ret = 0;

    // Enter software reset.
    if (write_masked_reg(
            sensor,
            AP0202AT_REG_SYSCTL_RESET_AND_MISC_CONTROL,
            AP0202AT_SYSCTL_RESET_AND_MISC_CONTROL_RESET_SOFT,
            AP0202AT_SYSCTL_RESET_AND_MISC_CONTROL_RESET_SOFT_MASK
        ) != 0) {
        ret = -1;
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

    // Patch loading is not implemented.
    ret = -1;

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

    // Resetting the image sensor is not implemented.
    ret = -1;

    return ret;
}

/**
 * @brief Reset the AP0202AT ISP into the default state.
 * 
 * This function also resets the attached image sensor.
 * 
 * @param sensor Pointer to sensor struct.
 * @return int 0 on success, -1 on error.
 */
static int reset(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    // Use software reset to enter configuration mode.
    if (enter_configuration_mode_software(sensor) != 0) {
        return -1;
    }

    // Wait for the Doorbell Bit to clear.
    // This is standard practice when entering
    //   configuration mode and is particularly called
    //   out if the Flash-Config mode is used to
    //   indicate that the flash download is complete.
    if (poll_doorbell_bit(sensor) != 0) {
        return -1;
    }

    // Issue SYSMGR_GET_STATE repeatedly until the result
    //   is not EBUSY.
    for (mp_uint_t start = mp_hal_ticks_ms();;) {
        uint16_t reg_data;

        if (host_command(sensor, AP0202AT_HC_SYSMGR_GET_STATE, &host_command_result) != 0) {
            return -1;
        }
        if (host_command_result != AP0202AT_HC_SYSMGR_GET_STATE_RETURN_EBUSY) {
            break;
        }

        // Return error if the timeout has elapsed.
        if ((mp_hal_ticks_ms() - start) >= AP0202AT_RESET_GET_STATE_TIMEOUT_MS) {
            return -1;
        }
    }

    // Load patches for the AP0202AT ISP.
    if(load_patches(sensor) != 0) {
        return -1;
    }

    // Perform sensor discovery and fail if no sensor is
    //   found.
    if (host_command(sensor, AP0202AT_HC_SENSOR_DISCOVERY, &host_command_result) != 0) {
        return -1;
    }
    if (host_command_result == AP0202AT_HC_SENSOR_DISCOVERY_RETURN_NO_SENSOR) {
        return -1;
    }

    // Perform sensor configuration.
    if(reset_image_sensor(sensor) != 0) {
        return -1;
    }

    // Issue the Change Config command to enter streaming.
    if (host_command(sensor, AP0202AT_HC_CHANGE_CONFIG, &host_command_result) != 0) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_CHANGE_CONFIG_RETURN_ENOERR) {
        return -1;
    }

    // Ensure that the system is now in the streaming
    //   state.
    if (host_command(sensor, AP0202AT_HC_SYSMGR_GET_STATE, &host_command_result) != 0) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_SYSMGR_GET_STATE_RETURN_SYS_STATE_STREAMING) {
        return -1;
    }

    return ret;
}

/**
 * @brief Read a register from the AP0202AT ISP.
 * @details Uses the two-wire interface to read a 16-bit
 *   wide register from the AP0202AT ISP.
 * 
 * @param sensor the sensor struct.
 * @param reg_addr the register address.
 * @return int -1 in case of error, else register contents.
 */
static int read_reg(sensor_t *sensor, uint16_t reg_addr) {
    uint16_t reg_data;

    // Translate OpenMV return code.
    if (read_reg_direct(sensor, reg_addr, &reg_data) != 0) {
        return -1;
    }

    return reg_data;
}

/**
 * @brief Write data to a register in the AP0202AT ISP.
 * @details Uses the two-wire interface to write a 16-bit
 *   wide register in the AP0202AT ISP.
 * 
 * @param sensor the sensor struct.
 * @param reg_addr the register address at which the data
 *   will be written.
 * @param data the data to be written to the register.
 * @return int 0 on success, -1 on error.
 */
static int write_reg(sensor_t *sensor, uint16_t reg_addr, uint16_t data) {
    int ret = 0;

    // Translate OpenMV return code.
    if(write_reg_direct(sensor, reg_addr, data) != 0) {
        ret = -1;
    }

    return ret;
}

/**
 * @brief Initialize the sensor structure for the AP0202AT.
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
int mt9m114_init(sensor_t *sensor) {
    sensor->reset = reset;
    sensor->read_reg = read_reg;
    sensor->write_reg = write_reg;

    return 0;
}

#endif // (OMV_ENABLE_AP0202AT == 1)
