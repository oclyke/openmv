/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2023-2024 oclyke <oclyke@oclyke.dev>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * AP0202AT driver library.
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

#include "py/mphal.h"

#include "ap0202at.h"
#include "ap0202at_regs.h"

#include <stdint.h>

#include <stdio.h>

// Maximum time in milliseconds before host command
//   polling times out.
#define AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS (100)
#define AP0202AT_HOST_COMMAND_READ_POLL_TIMEOUT_MS (100)

// Maximum delay in milliseconds before SYSMGR_GET_STATE
//   polling times out during a reset.
#define AP0202AT_RESET_GET_STATE_TIMEOUT_MS (200)

/**
 * @brief Get the address of a variable in the AP0202AT
 *  given the page and offset.
 * 
 * @param address_out 
 * @param page 
 * @param offset 
 * @return int 0 on success, -1 on error.
 */
int ap0202at_variable_address(uint16_t* address_out, uint8_t page, uint8_t offset) {
  int ret = 0;
  if (address_out != NULL) {
    *address_out = (0x8000 | ((page) << 10) | (offset));
  }
  return ret;
}

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
int ap0202at_read_reg_direct(sensor_t *sensor, uint16_t reg_addr, uint16_t* reg_data) {
    int ret = 0;

    // Translate OpenMV return code.
    int omv_return_code = omv_i2c_readw2(&sensor->i2c_bus, sensor->slv_addr, reg_addr, reg_data);
    if (omv_return_code != 0) {
        printf("OMV I2C READW2 RETURN CODE: %d\n", omv_return_code);
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
int ap0202at_write_reg_direct(sensor_t *sensor, uint16_t reg_addr, uint16_t data) {
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
int ap0202at_write_reg_masked(sensor_t *sensor, uint16_t reg_addr, uint16_t data, uint16_t mask) {
    int ret = 0;
    uint16_t reg_data;

    // Read the existing data from the register.
    ret = ap0202at_read_reg_direct(sensor, reg_addr, &reg_data);
    if (ret == -1) {
        return ret;
    }
    
    // Update the register data.
    reg_data &= ~mask;
    reg_data |= (data & mask);

    // Write the updated data to the register.
    // Translate OpenMV return code.
    if(ap0202at_write_reg_direct(sensor, reg_addr, reg_data) != 0) {
        ret = -1;
    }

    return ret;
}

/**
 * @brief Writes a burst of data to the AP0202AT ISP.
 * @details The burst data format is very specific. It must
 *  be a sequence of 16-bit words. The words are organized
 *  into groups of 25 laid end-to-end. The last group may
 *  be shorter than 25 words. The total number of words is
 *  given in the data_len parameter.
 * 
 *  The first word of each group is the register address
 *  at which the data will be written. The remaining 24
 *  words are the data to be written. The register address
 *  will auto-increment along with the data provided.
 * 
 *  This process will be repeated until all the groups of
 *  data have been written. The last group may be shorter
 *  than 25 words.
 * 
 * @param sensor the sensor struct.
 * @param data pointer to the burst data.
 * @param data_words the length of the burst data in words.
 * @return int 0 on success, -1 on error.
 */
int ap0202at_write_reg_burst_addr_24(sensor_t* sensor, uint16_t *data, uint16_t data_words) {
    int ret = 0;
    const size_t group_words = 25;
    uint16_t groups = data_words / group_words;
    uint16_t remainder = data_words % group_words;

    // Don't allow a burst of zero words.
    if ((groups == 0) && (remainder < 2)) {
        return -1;
    }

    // Send full groups of 25 words.
    for (uint16_t i = 0; i < groups; i++) {
        size_t index = i * group_words;
        ret |= omv_i2c_write_bytes(&sensor->i2c_bus, sensor->slv_addr, (uint8_t*)&data[index], 2, OMV_I2C_XFER_SUSPEND);
        ret |= omv_i2c_write_bytes(&sensor->i2c_bus, sensor->slv_addr, (uint8_t*)&data[index + 1], 2*(group_words - 1), OMV_I2C_XFER_NO_FLAGS);
        if (ret != 0) {
            return -1;
        }
    }

    // Send the remaining words, if any.
    if (remainder != 0) {
        ret |= omv_i2c_write_bytes(&sensor->i2c_bus, sensor->slv_addr, (uint8_t*)&data[groups * group_words], 2, OMV_I2C_XFER_SUSPEND);
        ret |= omv_i2c_write_bytes(&sensor->i2c_bus, sensor->slv_addr, (uint8_t*)&data[groups * group_words + 1], 2*remainder, OMV_I2C_XFER_NO_FLAGS);
        if (ret != 0) {
            return -1;
        }
    }

    return ret;
}


/**
 * @brief Writes a burst of data to the AP0202AT ISP.
 * @details The burst data format is very specific. It must
 *  be a sequence of 16-bit words. The words are organized
 *  into groups of 25 laid end-to-end. The last group may
 *  be shorter than 25 words. The total number of words is
 *  given in the data_len parameter.
 * 
 *  The first word of each group is the register address
 *  at which the data will be written. The remaining 24
 *  words are the data to be written. The register address
 *  will auto-increment along with the data provided.
 * 
 *  This process will be repeated until all the groups of
 *  data have been written. The last group may be shorter
 *  than 25 words.
 * 
 * @param sensor the sensor struct.
 * @param data pointer to the burst data.
 * @param data_words the length of the burst data in words.
 * @return int 0 on success, -1 on error.
 */
int ap0202at_write_patch(sensor_t* sensor, const uint16_t *data, uint16_t data_words) {
    return ap0202at_write_reg_burst_addr_24(sensor, (uint16_t*)data, data_words);
}

/**
 * @brief Write a register in the sensor attached to the
 *  AP0202AT ISP.
 * 
 * @param sensor the sensor struct.
 * @param addr the register address at which the data
 *   will be written in the attached sensor.
 * @param data the data to be written to the register.
 * @return int 0 on success, -1 on error.
 */
int ap0202at_write_sensor_u16(sensor_t *sensor, uint16_t addr, uint16_t data) {
    // this function is not yet implemented.
    // please see ap0202at_generic.c for an example of using the CCIM
    // interface.
    return -1;
}

/**
 * @brief 
 * 
 * APA0202AT-REV2_AR0147-REV3.ini line 1430
 * 
 * @param sensor the sensor struct.
 * @param port_address
 * @param sequencer_data 
 * @param sequencer_words the length of the sequencer data
 *  in words. 
 * @return int 0 on success, -1 on error.
 */
int ap0202at_write_sensor_sequencer(sensor_t* sensor, uint16_t port_address, const uint16_t* sequencer_data, size_t sequencer_words) {
    int ret = 0;
    uint16_t host_command_result;

    size_t burst_words = 6;
    size_t num_bursts = sequencer_words / burst_words;
    size_t leftover_words = sequencer_words % burst_words;

    // The full params pool contains the port address
    //   followed by the legnth (u16) followed by the
    //   burst data.
    size_t param_pool_words = 1 + 1 + burst_words;
    size_t leftover_pool_words = 1 + 1 + leftover_words;
    uint16_t params_pool[param_pool_words];

    // Loop over all complete bursts of 6 words.
    for (size_t idx = 0; idx < num_bursts; idx++) {

        // Prepare the params pool.
        params_pool[0] = port_address;
        params_pool[1] = (burst_words << 9);
        for (size_t i = 0; i < burst_words; i++) {
            params_pool[2 + i] = sequencer_data[idx * burst_words + i];
        }

        // Load the parameters pool into the AP0202AT.
        if (omv_i2c_write_bytes(&sensor->i2c_bus, sensor->slv_addr, (uint8_t*)&params_pool[0], 2*param_pool_words, OMV_I2C_XFER_NO_FLAGS) != 0) {
            return -1;
        }

        // Write the data through the CCIMGR interface
        if  (ap0202at_host_command(sensor, AP0202AT_HC_CMD_CCIMGR_WRITE, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_READ_POLL_TIMEOUT_MS) != 0) {
            return -1;
        }
        if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
            return -1;
        }

        // Poll the CCIMGR_STATUS command until the write is
        //   complete as indicated by a response of ENOERR.
        for (;;) {
            if  (ap0202at_host_command(sensor, AP0202AT_HC_CMD_CCIMGR_STATUS, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_READ_POLL_TIMEOUT_MS) != 0) {
                return -1;
            }
            if (host_command_result == AP0202AT_HC_RESP_ENOERR) {
                break;
            }
        }

    }

    // Handle the remaining data, if any.
    if (leftover_words != 0) {

        // Prepare the params pool.
        params_pool[0] = port_address;
        params_pool[1] = (leftover_words << 9);
        for (size_t i = 0; i < leftover_words; i++) {
            params_pool[2 + i] = sequencer_data[sequencer_words - leftover_words + i];
        }

        // Load the parameters pool into the AP0202AT.
        if (omv_i2c_write_bytes(&sensor->i2c_bus, sensor->slv_addr, (uint8_t*)&params_pool[0], 2*leftover_pool_words, OMV_I2C_XFER_NO_FLAGS) != 0) {
            return -1;
        }

        // Write the data through the CCIMGR interface
        if  (ap0202at_host_command(sensor, AP0202AT_HC_CMD_CCIMGR_WRITE, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_READ_POLL_TIMEOUT_MS) != 0) {
            return -1;
        }
        if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
            return -1;
        }

        // Poll the CCIMGR_STATUS command until the write is
        //   complete as indicated by a response of ENOERR.
        for (;;) {
            if  (ap0202at_host_command(sensor, AP0202AT_HC_CMD_CCIMGR_STATUS, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_READ_POLL_TIMEOUT_MS) != 0) {
                return -1;
            }
            if (host_command_result == AP0202AT_HC_RESP_ENOERR) {
                break;
            }
        }
    }

    return ret;
}

/**
 * @brief Poll the Doorbell Bit until it is clear or a
 *   timeout occurs.
 * 
 * This function relies on the ap0202at_read_reg_direct function
 *   to be blocking in order to limit the rate of polling.
 * 
 * @param sensor the sensor struct.
 * @param data [output] pointer to the data read from the
 *  COMMAND_REGISTER.
 * @param timeout_ms the maximum time to wait for the
 *   Doorbell Bit to clear in milliseconds.
 * @return int 0 on success, -1 on error.
 */
int ap0202at_poll_doorbell_bit(sensor_t *sensor, uint16_t* data, uint16_t timeout_ms) {
    int ret = 0;
    for (mp_uint_t start = mp_hal_ticks_ms();;) {
        uint16_t reg_data;

        // Read the COMMAND_REGISTER.
        if(ap0202at_read_reg_direct(sensor, AP0202AT_REG_SYSCTL_COMMAND_REGISTER, &reg_data) != 0) {
            return -1;
        }

        // Return the data if the output parameter is not NULL.
        if (data != NULL) {
            *data = reg_data;
        }

        // Return success if the Doorbell Bit is clear.
        if ((reg_data & AP0202AT_SYSCTL_COMMAND_REGISTER_DOORBELL_BIT_MASK) == 0) {
            return 0;
        }

        // Return error if the timeout has elapsed.
        mp_uint_t now = mp_hal_ticks_ms();
        mp_uint_t delta = (now - start);
        if (delta >= timeout_ms) {
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
 *   the command. Use the ap0202at_read_host_command_result function
 *   to check the result of the previous command.
 * 
 * @param sensor the sensor struct.
 * @param command the command to be issued.
 * @return int 0 on success, -1 on error.
 */
int ap0202at_issue_host_command(sensor_t *sensor, uint16_t command, uint16_t timeout_ms) {
    int ret = 0;

    printf("issue host command: 0x%04X\n", command);

    if (ap0202at_poll_doorbell_bit(sensor, NULL, timeout_ms) != 0) {
        printf("DOORBELL BIT FAILED TO CLEAR\n");
        return -1;
    }

    // Doorbell bit is clear, OK to issue command.
    command |= AP0202AT_SYSCTL_COMMAND_REGISTER_DOORBELL_BIT_MASK;
    if (ap0202at_write_reg_direct(sensor, AP0202AT_REG_SYSCTL_COMMAND_REGISTER, command) != 0) {
        printf("FAILED TO WRITE COMMAND REGISTER\n");
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
int ap0202at_read_host_command_result(sensor_t *sensor, uint16_t *result, uint16_t timeout_ms) {
    int ret = 0;

    // Poll the Doorbell Bit and read the result.
    if (ap0202at_poll_doorbell_bit(sensor, result, timeout_ms) != 0) {
        return -1;
    }

    return ret;
}

/**
 * @brief Issues a host command after writing the parameters
 *  to the parameter pool. The parameters array is written
 *  sequentially beginning at PARAM_POOL_0.
 * 
 * AND9930 Figure 4.
 * 
 * @param sensor the sensor struct.
 * @param command the command to be issued.
 * @param params pointer to the parameters array. data is
 *  stored in big-endian format. e.g. the 16-bit value 0x1234
 *  is stored as {0x12, 0x34}.
 * @param params_len length of the parameters array. must be
 *  even to ensure that the parameters are written in pairs.
 * @return int 0 on success, -1 on error.
 */
int ap0202at_issue_host_command_with_params(sensor_t *sensor, uint16_t command, uint8_t* params, size_t params_len, uint16_t timeout_ms) {
    if (params_len > 255) {
        return -1;
    }
    if (params_len % 2 != 0) {
        return -1;
    }

    // Ensure that the Doorbell Bit is clear.
    if (ap0202at_poll_doorbell_bit(sensor, NULL, timeout_ms) != 0) {
        return -1;
    }
    
    // Write the parameters to the parameter pool.
    size_t pairs = params_len / 2;
    for (size_t i = 0; i < pairs; i++) {
        if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0 + i, (params[2*i] << 8) | params[2*i + 1]) != 0) {
            return -1;
        }
    }

    // Issue the command.
    return ap0202at_issue_host_command(sensor, command, timeout_ms);
}



/**
 * @brief Read the result of the previous host command along
 *  with the parameters written to the parameter pool.
 * 
 * @param sensor the sensor struct.
 * @param result [output] pointer for the result of the
 * @param params [output] pointer to the parameters array.
 * @param params_len length of the parameters array. must be
 *  even to ensure that the parameters are read in pairs.
 * @return int 0 on success, -1 on error.
 */
int ap0202at_read_host_command_result_with_params(sensor_t *sensor, uint16_t *result, uint8_t* params, size_t params_len, uint16_t timeout_ms) {
    int ret = 0;

    if (params_len > 255) {
        return -1;
    }
    if (params_len % 2 != 0) {
        return -1;
    }

    // Poll the Doorbell Bit.
    if (ap0202at_poll_doorbell_bit(sensor, result, timeout_ms) != 0) {
        return -1;
    }

    size_t pairs = params_len / 2;
    for (size_t i = 0; i < pairs; i++) {
        uint16_t param;
        if (ap0202at_read_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0 + i, &param) != 0) {
            return -1;
        }
        params[2*i] = param >> 8;
        params[2*i + 1] = param & 0xFF;
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
 * This function combines the ap0202at_issue_host_command and the
 *   ap0202at_read_host_command_result functions. Thusly, it has
 *   the opportunity to time out at two points.
 *   - Firstly: at the ap0202at_issue_host_command function. A
 *       timeout here indicates that a previous command
 *       is still in progress.
 *   - Secondly: at the ap0202at_read_host_command_result function.
 *       A timeout here indicates that the command that was
 *       issued was not able to complete in time.
 * 
 * @param sensor the sensor struct.
 * @param command 
 * @param result [output] pointer for the result of the
 *   previous command.
 * @return int 
 */
int ap0202at_host_command(sensor_t *sensor, uint16_t command, uint16_t* result, uint16_t timeout_issue_ms, uint16_t timeout_read_ms) {
    int ret = 0;

    // Issue the command.
    if (ap0202at_issue_host_command(sensor, command, timeout_issue_ms) != 0) {
        return -1;
    }

    // Read the result.
    if (ap0202at_read_host_command_result(sensor, result, timeout_read_ms) != 0) {
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
int ap0202at_enter_configuration_mode_hardware(sensor_t *sensor) {
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
int ap0202at_enter_configuration_mode_software(sensor_t *sensor) {
    int ret = 0;

    // Enter software reset.
    if (ap0202at_write_reg_masked(
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
 * @brief Reset the AP0202AT ISP into the default state.
 * 
 * This function also resets the attached image sensor.
 * 
 * @param sensor Pointer to sensor struct.
 * @return int 0 on success, -1 on error.
 */
int ap0202at_reset(sensor_t *sensor) {
    int ret = 0;
    uint16_t host_command_result;

    // Use software reset to enter configuration mode.
    if (ap0202at_enter_configuration_mode_software(sensor) != 0) {
        return -1;
    }

    // Wait for the Doorbell Bit to clear.
    // This is standard practice when entering
    //   configuration mode and is particularly called
    //   out if the Flash-Config mode is used to
    //   indicate that the flash download is complete.
    if (ap0202at_poll_doorbell_bit(sensor, NULL, 250) != 0) {
        return -1;
    }

    // Issue SYSMGR_GET_STATE repeatedly until the result
    //   is not EBUSY.
    for (mp_uint_t start = mp_hal_ticks_ms();;) {
        if  (ap0202at_host_command(sensor, AP0202AT_HC_CMD_SYSMGR_GET_STATE, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_READ_POLL_TIMEOUT_MS) != 0) {
            return -1;
        }
        if (host_command_result != AP0202AT_HC_RESP_EBUSY) {
            break;
        }

        // Return error if the timeout has elapsed.
        if ((mp_hal_ticks_ms() - start) >= AP0202AT_RESET_GET_STATE_TIMEOUT_MS) {
            return -1;
        }
    }

    return ret;
}

/**
 * @brief Perform sensor discovery.
 * 
 * Uses the host command sensor manager interface to
 *  discover the attached image sensor.
 * 
 * If the response is NO_SENSOR, then the function
 *  will report an error.
 * 
 * @param sensor the sensor struct.
 * @param sensor_id optional output for the sensor ID.
 * @return int 0 on success, -1 on error.
 */
int ap0202at_sensor_discovery(sensor_t *sensor, uint16_t *sensor_id) {
    int ret = 0;
    uint16_t host_command_result;

    // Perform sensor discovery and fail if no sensor is
    //   found.
    if  (ap0202at_host_command(sensor, AP0202AT_HC_CMD_SENSOR_MGR_DISCOVER_SENSOR, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_READ_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (host_command_result == AP0202AT_HC_CMD_SENSOR_MGR_DISCOVER_RESP_NO_SENSOR) {
        return -1;
    }

    // NOTE: figure out whether this is the fn to run when detecting the sensor

    if (sensor_id != NULL) {
        *sensor_id = host_command_result;
    }

    return ret;
}

/**
 * @brief Issue the SYSMGR SET_STATE command to enter 'state'.
 * 
 * @param sensor 
 * @return int 0 on success, -1 on error.
 */
int ap0202at_sysmgr_set_state(sensor_t *sensor, uint8_t state) {
    int ret = 0;
    uint16_t host_command_result;

    // Issue the SET_STATE command to enter streaming.
    if(ap0202at_poll_doorbell_bit(sensor, NULL, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_write_reg_direct(sensor, AP0202AT_VAR_CMD_HANDLER_PARAMS_POOL_0, state) != 0) {
        return -1;
    }
    if (ap0202at_host_command(sensor, AP0202AT_HC_CMD_SYSMGR_SET_STATE, &host_command_result, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS, AP0202AT_HOST_COMMAND_READ_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        return -1;
    }

    return ret;
}

/**
 * @brief Issue the SYSMGR GET_STATE command to read the current state.
 * 
 * @param sensor the sensor struct.
 * @param state [output] the current state.
 * @return int 0 on success, -1 on error.
 */
int ap0202at_sysmgr_get_state(sensor_t *sensor, uint8_t *state) {
    int ret = 0;
    uint16_t host_command_result;
    if (NULL == state) {
        return -1;
    }

    uint8_t pool[2];
    if  (ap0202at_issue_host_command(sensor, AP0202AT_HC_CMD_SYSMGR_GET_STATE, AP0202AT_HOST_COMMAND_ISSUE_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (ap0202at_read_host_command_result_with_params(sensor, &host_command_result, pool, sizeof(pool), AP0202AT_HOST_COMMAND_READ_POLL_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (host_command_result != AP0202AT_HC_RESP_ENOERR) {
        printf("Failed to get state: (err %d)\n", host_command_result);
        return -1;
    }

    // Return the state.
    *state = pool[0];

    return ret;
}

/**
 * @brief Enter the streaming state.
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
int ap0202at_sysmgr_enter_state_streaming(sensor_t *sensor) {
    int ret = 0;
    uint8_t state;

    ret = ap0202at_sysmgr_set_state(sensor, AP0202AT_HCI_SYS_STATE_STREAMING);
    if (ret != 0) {
        return -1;
    }

    ret = ap0202at_sysmgr_get_state(sensor, &state);
    if (ret != 0) {
        return -1;
    }
    if (state != AP0202AT_HCI_SYS_STATE_STREAMING) {
        return -1;
    }

    return ret;
}

/**
 * @brief Read a register from the AP0202AT ISP.
 * @details Uses the two-wire interface to read a 16-bit
 *   wide register from the AP0202AT ISP.
 *   This function is meant to be used in the `read_reg`
 *   field of the sensor_t struct.
 * 
 * @param sensor the sensor struct.
 * @param reg_addr the register address.
 * @return int -1 in case of error, else register contents.
 */
static int read_reg(sensor_t *sensor, uint16_t reg_addr) {
    uint16_t reg_data;

    // Translate OpenMV return code.
    if (ap0202at_read_reg_direct(sensor, reg_addr, &reg_data) != 0) {
        return -1;
    }

    return reg_data;
}

/**
 * @brief Write data to a register in the AP0202AT ISP.
 * @details Uses the two-wire interface to write a 16-bit
 *   wide register in the AP0202AT ISP.
 *   This function is meant to be used in the `write_reg`
 *   field of the sensor_t struct.
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
    if(ap0202at_write_reg_direct(sensor, reg_addr, data) != 0) {
        ret = -1;
    }

    return ret;
}

/**
 * @brief Initialize the sensor structure for the AP0202AT.
 * @details Additional fields may be set on the sensor
 *  struct but these fields should be kept.
 * 
 * @param sensor the sensor struct.
 * @return int 0 on success, -1 on error.
 */
int ap0202at_init(sensor_t *sensor) {
    sensor->read_reg = read_reg;
    sensor->write_reg = write_reg;

    return 0;
}

#endif // (OMV_ENABLE_AP0202AT == 1)
