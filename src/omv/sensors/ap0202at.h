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

int ap0202at_init(sensor_t *sensor);

#endif // __AP0202AT_H__
