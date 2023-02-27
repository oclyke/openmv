/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2013-2023 Ibrahim Abdelkader <iabdalkader@openmv.io>
 * Copyright (c) 2013-2023 Kwabena W. Agyeman <kwagyeman@openmv.io>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * ISX019 Driver - 1280x960 HDR Camera
 */
#ifndef __ISX019_H__
#define __ISX019_H__
#define ISX019_XCLK_FREQ 27000000
int omv_isx019_init(sensor_t *sensor);
#endif // __ISX019_H__
