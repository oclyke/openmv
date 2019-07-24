/*
 * This file is part of the OpenMV project.
 * Copyright (c) 2013/2014 Ibrahim Abdelkader <i.abdalkader@gmail.com>
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * OV5640 driver.
 *
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include STM32_HAL_H
#include "cambus.h"
#include "ov5640.h"
#include "ov5640_regs.h"
#include "systick.h"
#include "omv_boardconfig.h"

#define NUM_BRIGHTNESS_LEVELS (9)

#define NUM_CONTRAST_LEVELS (7)
static const uint8_t contrast_regs[NUM_CONTRAST_LEVELS][1] = {
    {0x14}, /* -3 */
    {0x18}, /* -2 */
    {0x1C}, /* -1 */
    {0x00}, /* +0 */
    {0x10}, /* +1 */
    {0x18}, /* +2 */
    {0x1C}, /* +3 */
};

#define NUM_SATURATION_LEVELS (7)
static const uint8_t saturation_regs[NUM_SATURATION_LEVELS][6] = {
    {0x0c, 0x30, 0x3d, 0x3e, 0x3d, 0x01}, /* -3 */
    {0x10, 0x3d, 0x4d, 0x4e, 0x4d, 0x01}, /* -2 */
    {0x15, 0x52, 0x66, 0x68, 0x66, 0x02}, /* -1 */
    {0x1a, 0x66, 0x80, 0x82, 0x80, 0x02}, /* +0 */
    {0x1f, 0x7a, 0x9a, 0x9c, 0x9a, 0x02}, /* +1 */
    {0x24, 0x8f, 0xb3, 0xb6, 0xb3, 0x03}, /* +2 */
    {0x2b, 0xab, 0xd6, 0xda, 0xd6, 0x04}, /* +3 */
};

static int reset(sensor_t *sensor)
{
    int i=0;
    const uint16_t (*regs)[2];
    // Reset all registers
    cambus_writeb2(sensor->slv_addr, 0x3008, 0x42);
    // Delay 10 ms
    systick_sleep(10);
    // Write default regsiters
    for (i=0, regs = default_regs; regs[i][0]; i++) {
        cambus_writeb2(sensor->slv_addr, regs[i][0], regs[i][1]);
    }
    cambus_writeb2(sensor->slv_addr, 0x3008, 0x02);
    systick_sleep(30);
    // Write auto focus firmware
    for (i=0, regs = OV5640_AF_REG; regs[i][0]; i++) {
        cambus_writeb2(sensor->slv_addr, regs[i][0], regs[i][1]);
    }
    // Delay
    systick_sleep(10);
    // Enable auto focus
    cambus_writeb2(sensor->slv_addr, 0x3023, 0x01);
    cambus_writeb2(sensor->slv_addr, 0x3022, 0x04);

    systick_sleep(30);
    return 0;
}

static int sleep(sensor_t *sensor, int enable)
{
    uint8_t reg;
    int ret = cambus_readb2(sensor->slv_addr, 0x3008, &reg);

    if (enable) {
        reg |= 0x40;
    } else {
        reg &= ~0x40;
    }

    // Write back register
    return cambus_writeb2(sensor->slv_addr, 0x3008, reg) | ret;
}

static int read_reg(sensor_t *sensor, uint16_t reg_addr)
{
    uint8_t reg_data;
    if (cambus_readb2(sensor->slv_addr, reg_addr, &reg_data) != 0) {
        return -1;
    }
    return reg_data;
}

static int write_reg(sensor_t *sensor, uint16_t reg_addr, uint16_t reg_data)
{
    return cambus_writeb2(sensor->slv_addr, reg_addr, reg_data);
}

static int set_pixformat(sensor_t *sensor, pixformat_t pixformat)
{
//    uint8_t reg;
    int ret=0;
    switch (pixformat) {
        case PIXFORMAT_RGB565:
            cambus_writeb2(sensor->slv_addr, 0x4300, 0x61);//RGB565
            cambus_writeb2(sensor->slv_addr, 0x501f, 0x01);//ISP RGB
            break;
        case PIXFORMAT_YUV422:
        case PIXFORMAT_GRAYSCALE:
            cambus_writeb2(sensor->slv_addr, 0x4300, 0x10);//Y8
            cambus_writeb2(sensor->slv_addr, 0x501f, 0x00);//ISP YUV
            break;
        case PIXFORMAT_BAYER:
            //reg = 0x00;//TODO: fix order
            break;
        default:
            return -1;
    }
    return ret;
}

static int set_framesize(sensor_t *sensor, framesize_t framesize)
{
    int ret=0;
    uint16_t w = resolution[framesize][0];
    uint16_t h = resolution[framesize][1];

    ret |= cambus_writeb2(sensor->slv_addr, 0x3808, w>>8);
    ret |= cambus_writeb2(sensor->slv_addr, 0x3809,  w);
    ret |= cambus_writeb2(sensor->slv_addr, 0x380a, h>>8);
    ret |= cambus_writeb2(sensor->slv_addr, 0x380b,  h);

    return ret;
}

static int set_framerate(sensor_t *sensor, framerate_t framerate)
{
    return 0;
}

static int set_contrast(sensor_t *sensor, int level)
{
    int ret = 0;

    int new_level = level + (NUM_CONTRAST_LEVELS / 2) + 1;
    if (new_level < 0 || new_level > NUM_CONTRAST_LEVELS) {
        return -1;
    }

    ret |= cambus_writeb2(sensor->slv_addr, 0x3212, 0x03); // start group 3
    ret |= cambus_writeb2(sensor->slv_addr, 0x5586, (new_level + 4) << 2);
    ret |= cambus_writeb2(sensor->slv_addr, 0x5585, contrast_regs[new_level-1][0]);
    ret |= cambus_writeb2(sensor->slv_addr, 0x3212, 0x13); // end group 3
    ret |= cambus_writeb2(sensor->slv_addr, 0x3212, 0xa3); // launch group 3

    return ret;
}

static int set_brightness(sensor_t *sensor, int level)
{
    int ret = 0;

    int new_level = level + (NUM_BRIGHTNESS_LEVELS / 2) + 1;
    if (new_level < 0 || new_level > NUM_BRIGHTNESS_LEVELS) {
        return -1;
    }

    ret |= cambus_writeb2(sensor->slv_addr, 0x3212, 0x03); // start group 3
    ret |= cambus_writeb2(sensor->slv_addr, 0x5587, abs(level) << 4);
    ret |= cambus_writeb2(sensor->slv_addr, 0x5588, (level < 0) ? 0x09 : 0x01);
    ret |= cambus_writeb2(sensor->slv_addr, 0x3212, 0x13); // end group 3
    ret |= cambus_writeb2(sensor->slv_addr, 0x3212, 0xa3); // launch group 3

    return ret;
}

static int set_saturation(sensor_t *sensor, int level)
{
    int ret = 0;

    int new_level = level + (NUM_SATURATION_LEVELS / 2) + 1;
    if (new_level < 0 || new_level > NUM_SATURATION_LEVELS) {
        return -1;
    }

    ret |= cambus_writeb2(sensor->slv_addr, 0x3212, 0x03); // start group 3
    ret |= cambus_writeb2(sensor->slv_addr, 0x5581, 0x1c);
    ret |= cambus_writeb2(sensor->slv_addr, 0x5582, 0x5a);
    ret |= cambus_writeb2(sensor->slv_addr, 0x5583, 0x06);
    ret |= cambus_writeb2(sensor->slv_addr, 0x5584, saturation_regs[new_level-1][0]);
    ret |= cambus_writeb2(sensor->slv_addr, 0x5585, saturation_regs[new_level-1][1]);
    ret |= cambus_writeb2(sensor->slv_addr, 0x5586, saturation_regs[new_level-1][2]);
    ret |= cambus_writeb2(sensor->slv_addr, 0x5587, saturation_regs[new_level-1][3]);
    ret |= cambus_writeb2(sensor->slv_addr, 0x5588, saturation_regs[new_level-1][4]);
    ret |= cambus_writeb2(sensor->slv_addr, 0x5589, saturation_regs[new_level-1][5]);
    ret |= cambus_writeb2(sensor->slv_addr, 0x558b, 0x98);
    ret |= cambus_writeb2(sensor->slv_addr, 0x558a, 0x01);
    ret |= cambus_writeb2(sensor->slv_addr, 0x3212, 0x13); // end group 3
    ret |= cambus_writeb2(sensor->slv_addr, 0x3212, 0xa3); // launch group 3

    return ret;
}

static int set_gainceiling(sensor_t *sensor, gainceiling_t gainceiling)
{
    return 0;
}

static int set_colorbar(sensor_t *sensor, int enable)
{
    return cambus_writeb2(sensor->slv_addr, 0x503D, enable ? 0x80 : 0x00);
}

static int set_auto_gain(sensor_t *sensor, int enable, float gain_db, float gain_db_ceiling)
{
    uint8_t reg;
    int ret = cambus_readb2(sensor->slv_addr, 0x3503, &reg);
    ret |= cambus_writeb2(sensor->slv_addr, 0x3503, (reg & 0xFD) | ((enable == 0) << 1));

    if ((enable == 0) && (!isnanf(gain_db)) && (!isinff(gain_db))) {
        int gain = IM_MAX(IM_MIN(fast_expf((gain_db / 20.0) * fast_log(10.0)), 1023), 0);

        ret |= cambus_writeb2(sensor->slv_addr, 0x350a, gain >> 8);
        ret |= cambus_writeb2(sensor->slv_addr, 0x350b, gain);
    } else if ((enable != 0) && (!isnanf(gain_db_ceiling)) && (!isinff(gain_db_ceiling))) {
        int gain_ceiling = IM_MAX(IM_MIN(fast_expf((gain_db_ceiling / 20.0) * fast_log(10.0)), 1023), 0);

        ret |= cambus_readb2(sensor->slv_addr, 0x3A18, &reg);
        ret |= cambus_writeb2(sensor->slv_addr, 0x3A18, (reg & 0xFC) | (gain_ceiling >> 8));
        ret |= cambus_writeb2(sensor->slv_addr, 0x3A19, gain_ceiling);
    }

    return ret;
}

static int get_gain_db(sensor_t *sensor, float *gain_db)
{
    uint8_t gainh, gainl;

    int ret = cambus_readb2(sensor->slv_addr, 0x350a, &gainh);
    ret |= cambus_readb2(sensor->slv_addr, 0x350b, &gainl);

    *gain_db = 20.0 * (fast_log(((gainh & 0x3) << 8) | gainl) / fast_log(10.0));

    return ret;
}

static int set_auto_exposure(sensor_t *sensor, int enable, int exposure_us)
{
    return 0;
}

static int get_exposure_us(sensor_t *sensor, int *exposure_us)
{
    return 0;
}

static int set_auto_whitebal(sensor_t *sensor, int enable, float r_gain_db, float g_gain_db, float b_gain_db)
{
    uint8_t reg;
    int ret = cambus_readb2(sensor->slv_addr, 0x3406, &reg);
    ret |= cambus_writeb2(sensor->slv_addr, 0x3406, (reg & 0xFE) | (enable == 0));

    if ((enable == 0) && (!isnanf(r_gain_db)) && (!isnanf(g_gain_db)) && (!isnanf(b_gain_db))
                      && (!isinff(r_gain_db)) && (!isinff(g_gain_db)) && (!isinff(b_gain_db))) {

        int r_gain = IM_MAX(IM_MIN(fast_roundf(fast_expf((r_gain_db / 20.0) * fast_log(10.0))), 4095), 0);
        int g_gain = IM_MAX(IM_MIN(fast_roundf(fast_expf((g_gain_db / 20.0) * fast_log(10.0))), 4095), 0);
        int b_gain = IM_MAX(IM_MIN(fast_roundf(fast_expf((b_gain_db / 20.0) * fast_log(10.0))), 4095), 0);

        ret |= cambus_writeb2(sensor->slv_addr, 0x3400, r_gain >> 8);
        ret |= cambus_writeb2(sensor->slv_addr, 0x3401, r_gain);
        ret |= cambus_writeb2(sensor->slv_addr, 0x3402, g_gain >> 8);
        ret |= cambus_writeb2(sensor->slv_addr, 0x3403, g_gain);
        ret |= cambus_writeb2(sensor->slv_addr, 0x3404, b_gain >> 8);
        ret |= cambus_writeb2(sensor->slv_addr, 0x3405, b_gain);
    }

    return ret;
}

static int get_rgb_gain_db(sensor_t *sensor, float *r_gain_db, float *g_gain_db, float *b_gain_db)
{
    uint8_t redh, redl, greenh, greenl, blueh, bluel;

    int ret = cambus_readb2(sensor->slv_addr, 0x3400, &redh);
    ret |= cambus_readb2(sensor->slv_addr, 0x3401, &redl);
    ret |= cambus_readb2(sensor->slv_addr, 0x3402, &greenh);
    ret |= cambus_readb2(sensor->slv_addr, 0x3403, &greenl);
    ret |= cambus_readb2(sensor->slv_addr, 0x3404, &blueh);
    ret |= cambus_readb2(sensor->slv_addr, 0x3405, &bluel);

    *r_gain_db = 20.0 * (fast_log(((redh & 0xF) << 8) | redl) / fast_log(10.0));
    *g_gain_db = 20.0 * (fast_log(((greenh & 0xF) << 8) | greenl) / fast_log(10.0));
    *b_gain_db = 20.0 * (fast_log(((blueh & 0xF) << 8) | bluel) / fast_log(10.0));

    return ret;
}

static int set_hmirror(sensor_t *sensor, int enable)
{
    uint8_t reg;
    int ret = cambus_readb2(sensor->slv_addr, 0x3821, &reg);
    if (enable){
        ret |= cambus_writeb2(sensor->slv_addr, 0x3821, reg|0x06);
    } else {
        ret |= cambus_writeb2(sensor->slv_addr, 0x3821, reg&0xF9);
    }
    return ret;
}

static int set_vflip(sensor_t *sensor, int enable)
{
    uint8_t reg;
    int ret = cambus_readb2(sensor->slv_addr, 0x3820, &reg);
    if (enable){
        ret |= cambus_writeb2(sensor->slv_addr, 0x3820, reg|0x06);
    } else {
        ret |= cambus_writeb2(sensor->slv_addr, 0x3820, reg&0xF9);
    }
    return ret;
}

static int set_special_effect(sensor_t *sensor, sde_t sde)
{
    int ret = 0;

    switch (sde) {
        case SDE_NEGATIVE:
            ret |= cambus_writeb2(sensor->slv_addr, 0x3212, 0x03); // start group 3
            ret |= cambus_writeb2(sensor->slv_addr, 0x5580, 0x40);
            ret |= cambus_writeb2(sensor->slv_addr, 0x5003, 0x08);
            ret |= cambus_writeb2(sensor->slv_addr, 0x5583, 0x40); // sat U
            ret |= cambus_writeb2(sensor->slv_addr, 0x5584, 0x10); // sat V
            ret |= cambus_writeb2(sensor->slv_addr, 0x3212, 0x13); // end group 3
            ret |= cambus_writeb2(sensor->slv_addr, 0x3212, 0xa3); // latch group 3
            break;
        case SDE_NORMAL:
            ret |= cambus_writeb2(sensor->slv_addr, 0x3212, 0x03); // start group 3
            ret |= cambus_writeb2(sensor->slv_addr, 0x5580, 0x06);
            ret |= cambus_writeb2(sensor->slv_addr, 0x5583, 0x40); // sat U
            ret |= cambus_writeb2(sensor->slv_addr, 0x5584, 0x10); // sat V
            ret |= cambus_writeb2(sensor->slv_addr, 0x5003, 0x08);
            ret |= cambus_writeb2(sensor->slv_addr, 0x3212, 0x13); // end group 3
            ret |= cambus_writeb2(sensor->slv_addr, 0x3212, 0xa3); // latch group 3
            break;
        default:
            return -1;
    }

    return ret;
}

static int set_lens_correction(sensor_t *sensor, int enable, int radi, int coef)
{
    return cambus_writeb2(sensor->slv_addr, 0x5000, enable ? 0x86 : 0x06);
}

int ov5640_init(sensor_t *sensor)
{
    // Initialize sensor structure.
    sensor->gs_bpp              = 1;
    sensor->reset               = reset;
    sensor->sleep               = sleep;
    sensor->read_reg            = read_reg;
    sensor->write_reg           = write_reg;
    sensor->set_pixformat       = set_pixformat;
    sensor->set_framesize       = set_framesize;
    sensor->set_framerate       = set_framerate;
    sensor->set_contrast        = set_contrast;
    sensor->set_brightness      = set_brightness;
    sensor->set_saturation      = set_saturation;
    sensor->set_gainceiling     = set_gainceiling;
    sensor->set_colorbar        = set_colorbar;
    sensor->set_auto_gain       = set_auto_gain;
    sensor->get_gain_db         = get_gain_db;
    sensor->set_auto_exposure   = set_auto_exposure;
    sensor->get_exposure_us     = get_exposure_us;
    sensor->set_auto_whitebal   = set_auto_whitebal;
    sensor->get_rgb_gain_db     = get_rgb_gain_db;
    sensor->set_hmirror         = set_hmirror;
    sensor->set_vflip           = set_vflip;
    sensor->set_special_effect  = set_special_effect;
    sensor->set_lens_correction = set_lens_correction;

    // Set sensor flags
    SENSOR_HW_FLAGS_SET(sensor, SENSOR_HW_FLAGS_VSYNC, 0);
    SENSOR_HW_FLAGS_SET(sensor, SENSOR_HW_FLAGS_HSYNC, 0);
    SENSOR_HW_FLAGS_SET(sensor, SENSOR_HW_FLAGS_PIXCK, 1);
    SENSOR_HW_FLAGS_SET(sensor, SENSOR_HW_FLAGS_FSYNC, 1);
    SENSOR_HW_FLAGS_SET(sensor, SENSOR_HW_FLAGS_JPEGE, 1);

    return 0;
}
