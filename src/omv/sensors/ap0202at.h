/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2023-2024 oclyke <oclyke@oclyke.dev>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * AP0202AT driver.
 */
#ifndef __AP0202AT_H__
#define __AP0202AT_H__

// #include <stdint.h>

#include "sensor.h"

// AP0202AT EXTCLK frequency.
// Range 24 to 30 MHz does not require PLL reconfiguration.
#define AP0202AT_XCLK_FREQ    (24000000)

// register access
int ap0202at_read_reg_direct(sensor_t *sensor, uint16_t reg_addr, uint16_t* reg_data);
int ap0202at_write_reg_direct(sensor_t *sensor, uint16_t reg_addr, uint16_t data);
int ap0202at_write_reg_masked(sensor_t *sensor, uint16_t reg_addr, uint16_t data, uint16_t mask);
int ap0202at_write_reg_burst_addr_24(sensor_t* sensor, uint16_t *data, uint16_t data_words);
int ap0202at_write_patch(sensor_t* sensor, const uint16_t *data, uint16_t data_words);

// host command interface (HCI)
int ap0202at_host_command_emplace_parameter_offset_u8(uint8_t* pool, size_t pool_len, size_t offset, uint8_t param);
int ap0202at_host_command_emplace_parameter_offset_u16(uint8_t* pool, size_t pool_len, size_t offset, uint16_t param);
int ap0202at_host_command_emplace_parameter_offset_u32(uint8_t* pool, size_t pool_len, size_t offset, uint32_t param);
int ap0202at_host_command_get_doorbell_bit(sensor_t *sensor, uint16_t* result, bool* doorbell);
int ap0202at_host_command_poll_doorbell_bit_clear(sensor_t *sensor, uint16_t* result, uint16_t timeout_ms);
int ap0202at_host_command_load_parameter_pool(sensor_t *sensor, size_t offset, uint8_t* params, size_t params_len);
int ap0202at_host_command_unload_parameter_pool(sensor_t *sensor, size_t offset, uint8_t* params, size_t params_len);
int ap0202at_host_command_execute_command_synchronous(sensor_t *sensor, uint16_t command, uint16_t* result, uint16_t timeout_ms);
int ap0202at_host_command_start_command_asynchronous(sensor_t *sensor, uint16_t command, uint16_t* result, uint16_t timeout_ms);
int ap0202at_host_command_finish_command_asynchronous(sensor_t *sensor, uint16_t command, uint16_t* result, uint16_t timeout_ms);

// camera control interface (CCI)
int ap0202at_cci_manager_get_lock(sensor_t *sensor, uint16_t timeout_start_ms, uint16_t timeout_finish_ms);
int ap0202at_cci_manager_release_lock(sensor_t *sensor, uint16_t timeout_ms);
int ap0202at_cci_manager_config(sensor_t *sensor, uint32_t cci_speed_hz, uint16_t timeout_ms);
int ap0202at_cci_manager_set_device(sensor_t *sensor, uint8_t device_address, uint16_t timeout_ms);
int ap0202at_cci_manager_read(sensor_t *sensor, uint16_t register_address, uint8_t* data, uint8_t data_len, uint16_t timeout_start_ms, uint16_t timeout_finish_ms);
int ap0202at_cci_manager_write(sensor_t *sensor, uint16_t register_address, uint8_t* data, uint8_t data_len, uint16_t timeout_start_ms, uint16_t timeout_finish_ms);

// sysmgr commands
int ap0202at_sysmgr_set_state(sensor_t *sensor, uint8_t state, uint16_t timeout_ms);
int ap0202at_sysmgr_get_state(sensor_t *sensor, uint8_t *state, uint16_t timeout_ms);

// not sure what this function really is.
// what is a "sensor sequencer"?
int ap0202at_write_sensor_u16(sensor_t *sensor, uint16_t port_address, uint16_t data, uint16_t timeout_start_ms, uint16_t timeout_finish_ms);
int ap0202at_write_sensor_sequencer(sensor_t *sensor, uint16_t port_address, const uint16_t* sequencer_data, size_t sequencer_words);

// configuration mode
int ap0202at_enter_configuration_mode_hardware(sensor_t *sensor);
int ap0202at_enter_configuration_mode_software(sensor_t *sensor);

// reset process
int ap0202at_reset(sensor_t *sensor);
int ap0202at_sensor_discovery(sensor_t *sensor, uint16_t *sensor_id);



// sensor_t struct initialization
int ap0202at_init(sensor_t *sensor);

#endif // __AP0202AT_H__
