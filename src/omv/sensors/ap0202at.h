/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2023 oclyke <oclyke@oclyke.dev>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * AP0202AT driver.
 */
#ifndef __AP0202AT_H__
#define __AP0202AT_H__

// AP0202AT EXTCLK frequency.
// Range 24 to 30 MHz does not require PLL reconfiguration.
#define AP0202AT_XCLK_FREQ    (24000000)

// register and variable location
int ap0202at_variable_address(uint16_t* address_out, uint8_t page, uint8_t offset);

// register access
int ap0202at_read_reg_direct(sensor_t *sensor, uint16_t reg_addr, uint16_t* reg_data);
int ap0202at_write_reg_direct(sensor_t *sensor, uint16_t reg_addr, uint16_t data);
int ap0202at_write_reg_masked(sensor_t *sensor, uint16_t reg_addr, uint16_t data, uint16_t mask);
int ap0202at_write_reg_burst_addr_24(sensor_t* sensor, uint16_t *data, uint16_t data_words);

// sensor register passthrough access
int ap0202at_write_sensor_u16(sensor_t *sensor, uint16_t addr, uint16_t data);
int ap0202at_write_sensor_sequencer(sensor_t* sensor, uint16_t port_address, uint16_t* sequencer_data, size_t sequencer_words);

// host command interface
int ap0202at_poll_doorbell_bit(sensor_t *sensor, uint16_t timeout_ms);
int ap0202at_issue_host_command(sensor_t *sensor, uint16_t command);
int ap0202at_read_host_command_result(sensor_t *sensor, uint16_t *result);
int ap0202at_host_command(sensor_t sensor, uint16_t command, uint16_t* result);

// configuration mode
int ap0202at_enter_configuration_mode_hardware(sensor_t *sensor);
int ap0202at_enter_configuration_mode_software(sensor_t *sensor);

// reset process
int ap0202at_reset(sensor_t *sensor);
int ap0202at_sensor_discovery(sensor_t *sensor, uint16_t *sensor_id);
int ap0202at_enter_streaming(sensor_t *sensor);

// sensor_t struct initialization
int ap0202at_init(sensor_t *sensor);

#endif // __AP0202AT_H__
