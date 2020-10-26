/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2013-2019 Ibrahim Abdelkader <iabdalkader@openmv.io>
 * Copyright (c) 2013-2019 Kwabena W. Agyeman <kwagyeman@openmv.io>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * LCD Python module.
 */
#include STM32_HAL_H
#include <mp.h>
#include <objstr.h>
#include <spi.h>
#include <systick.h>
#include "imlib.h"
#include "fb_alloc.h"
#include "ff_wrapper.h"
#include "py_assert.h"
#include "py_helper.h"
#include "py_image.h"

#define RST_PORT            GPIOD
#define RST_PIN             GPIO_PIN_12
#define RST_PIN_WRITE(bit)  HAL_GPIO_WritePin(RST_PORT, RST_PIN, bit);

#define RS_PORT             GPIOD
#define RS_PIN              GPIO_PIN_13
#define RS_PIN_WRITE(bit)   HAL_GPIO_WritePin(RS_PORT, RS_PIN, bit);

#define CS_PORT             GPIOB
#define CS_PIN              GPIO_PIN_12
#define CS_PIN_WRITE(bit)   HAL_GPIO_WritePin(CS_PORT, CS_PIN, bit);

#define LED_PORT            GPIOA
#define LED_PIN             GPIO_PIN_5
#define LED_PIN_WRITE(bit)  HAL_GPIO_WritePin(LED_PORT, LED_PIN, bit);

extern mp_obj_t pyb_spi_send(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
extern mp_obj_t pyb_spi_make_new(mp_obj_t type_in, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args);
extern mp_obj_t pyb_spi_deinit(mp_obj_t self_in);

static mp_obj_t spi_port = NULL;
static int width = 0;
static int height = 0;
static enum { LCD_NONE, LCD_SHIELD, LCD_DISPLAY } type = LCD_NONE;
static bool backlight_init = false;

#define LTDC_FRAMEBUFFER_COUNT 3

static LTDC_HandleTypeDef ltdc_handle;
static uint16_t *ltdc_framebuffers[LTDC_FRAMEBUFFER_COUNT] = {};
static LTDC_LayerCfgTypeDef ltdc_framebuffer_layers[LTDC_FRAMEBUFFER_COUNT] = {};
static int ltdc_framebuffer_head = 0;
static volatile int ltdc_framebuffer_tail = 0;

static enum {
    LCD_DISPLAY_VGA,
    LCD_DISPLAY_WVGA,
    LCD_DISPLAY_SVGA,
    LCD_DISPLAY_XGA,
    LCD_DISPLAY_SXGA,
    LCD_DISPLAY_UXGA,
    LCD_DISPLAY_HD,
    LCD_DISPLAY_FHD,
    LCD_DISPLAY_MAX
} resolution = LCD_DISPLAY_VGA;

static int refresh = 0;

static TIM_HandleTypeDef bl_tim_handle;
static int bl_intensity = 255;

static const uint16_t resolution_w_h[][2] {
    {640,  480}, // VGA
    {800,  480}, // WVGA
    {800,  600}, // SVGA
    {1024, 768}, // XGA
    {1280, 1024}, // SXGA
    {1600, 1200}, // UXGA
    {1280, 720}, // HD
    {1920, 1080} // FHD
}

static const uint32_t resolution_clock[] { // CVT-RB ver 2 @ 60 FPS
    21363, // VGA
    26110, // WVGA
    32597, // SVGA
    52277, // XGA
    85920, // SXGA
    124364, // UXGA
    60405, // HD
    133187 // FHD
};

static const LTDC_InitTypeDef resolution_cfg[] { // CVT-RB ver 2
    { // VGA
        .HSPolarity = LTDC_HSPOLARITY_AH,
        .VSPolarity = LTDC_VSPOLARITY_AL,
        .DEPolarity = LTDC_DEPOLARITY_AH,
        .PCPolarity = LTDC_PCPOLARITY_IPC,
        .HorizontalSync = 32,
        .VerticalSync = 8,
        .AccumulatedHBP = 32 + 40,
        .AccumulatedVBP = 8 + 6,
        .AccumulatedActiveW = 32 + 40 + 640,
        .AccumulatedActiveH = 8 + 6 + 480,
        .TotalWidth = 32 + 40 + 640 + 8,
        .TotalHeigh = 8 + 6 + 480 + 1,
        .Backcolor = {.Blue=0, .Green=0, .Red=0}
    },
    { // WVGA
        .HSPolarity = LTDC_HSPOLARITY_AH,
        .VSPolarity = LTDC_VSPOLARITY_AL,
        .DEPolarity = LTDC_DEPOLARITY_AH,
        .PCPolarity = LTDC_PCPOLARITY_IPC,
        .HorizontalSync = 32,
        .VerticalSync = 8,
        .AccumulatedHBP = 32 + 40,
        .AccumulatedVBP = 8 + 6,
        .AccumulatedActiveW = 32 + 40 + 800,
        .AccumulatedActiveH = 8 + 6 + 480,
        .TotalWidth = 32 + 40 + 800 + 8,
        .TotalHeigh = 8 + 6 + 480 + 1,
        .Backcolor = {.Blue=0, .Green=0, .Red=0}
    },
    { // SVGA
        .HSPolarity = LTDC_HSPOLARITY_AH,
        .VSPolarity = LTDC_VSPOLARITY_AL,
        .DEPolarity = LTDC_DEPOLARITY_AH,
        .PCPolarity = LTDC_PCPOLARITY_IPC,
        .HorizontalSync = 32,
        .VerticalSync = 8,
        .AccumulatedHBP = 32 + 40,
        .AccumulatedVBP = 8 + 6,
        .AccumulatedActiveW = 32 + 40 + 800,
        .AccumulatedActiveH = 8 + 6 + 600,
        .TotalWidth = 32 + 40 + 800 + 8,
        .TotalHeigh = 8 + 6 + 600 + 4,
        .Backcolor = {.Blue=0, .Green=0, .Red=0}
    },
    { // XGA
        .HSPolarity = LTDC_HSPOLARITY_AH,
        .VSPolarity = LTDC_VSPOLARITY_AL,
        .DEPolarity = LTDC_DEPOLARITY_AH,
        .PCPolarity = LTDC_PCPOLARITY_IPC,
        .HorizontalSync = 32,
        .VerticalSync = 8,
        .AccumulatedHBP = 32 + 40,
        .AccumulatedVBP = 8 + 6,
        .AccumulatedActiveW = 32 + 40 + 1920,
        .AccumulatedActiveH = 8 + 6 + 1080,
        .TotalWidth = 32 + 40 + 1920 + 8,
        .TotalHeigh = 8 + 6 + 1080 + 17,
        .Backcolor = {.Blue=0, .Green=0, .Red=0}
    },
    { // SXGA
        .HSPolarity = LTDC_HSPOLARITY_AH,
        .VSPolarity = LTDC_VSPOLARITY_AL,
        .DEPolarity = LTDC_DEPOLARITY_AH,
        .PCPolarity = LTDC_PCPOLARITY_IPC,
        .HorizontalSync = 32,
        .VerticalSync = 8,
        .AccumulatedHBP = 32 + 40,
        .AccumulatedVBP = 8 + 6,
        .AccumulatedActiveW = 32 + 40 + 1024,
        .AccumulatedActiveH = 8 + 6 + 768,
        .TotalWidth = 32 + 40 + 1024 + 8,
        .TotalHeigh = 8 + 6 + 768 + 8,
        .Backcolor = {.Blue=0, .Green=0, .Red=0}
    },
    { // UXGA
        .HSPolarity = LTDC_HSPOLARITY_AH,
        .VSPolarity = LTDC_VSPOLARITY_AL,
        .DEPolarity = LTDC_DEPOLARITY_AH,
        .PCPolarity = LTDC_PCPOLARITY_IPC,
        .HorizontalSync = 32,
        .VerticalSync = 8,
        .AccumulatedHBP = 32 + 40,
        .AccumulatedVBP = 8 + 6,
        .AccumulatedActiveW = 32 + 40 + 1600,
        .AccumulatedActiveH = 8 + 6 + 1200,
        .TotalWidth = 32 + 40 + 1600 + 8,
        .TotalHeigh = 8 + 6 + 1200 + 21,
        .Backcolor = {.Blue=0, .Green=0, .Red=0}
    },
    { // HD
        .HSPolarity = LTDC_HSPOLARITY_AH,
        .VSPolarity = LTDC_VSPOLARITY_AL,
        .DEPolarity = LTDC_DEPOLARITY_AH,
        .PCPolarity = LTDC_PCPOLARITY_IPC,
        .HorizontalSync = 32,
        .VerticalSync = 8,
        .AccumulatedHBP = 32 + 40,
        .AccumulatedVBP = 8 + 6,
        .AccumulatedActiveW = 32 + 40 + 1280,
        .AccumulatedActiveH = 8 + 6 + 720,
        .TotalWidth = 32 + 40 + 1280 + 8,
        .TotalHeigh = 8 + 6 + 720 + 7,
        .Backcolor = {.Blue=0, .Green=0, .Red=0}
    },
    { // FHD
        .HSPolarity = LTDC_HSPOLARITY_AH,
        .VSPolarity = LTDC_VSPOLARITY_AL,
        .DEPolarity = LTDC_DEPOLARITY_AH,
        .PCPolarity = LTDC_PCPOLARITY_IPC,
        .HorizontalSync = 32,
        .VerticalSync = 8,
        .AccumulatedHBP = 32 + 40,
        .AccumulatedVBP = 8 + 6,
        .AccumulatedActiveW = 32 + 40 + 1920,
        .AccumulatedActiveH = 8 + 6 + 1080,
        .TotalWidth = 32 + 40 + 1920 + 8,
        .TotalHeigh = 8 + 6 + 1080 + 17,
        .Backcolor = {.Blue=0, .Green=0, .Red=0}
    }
};

static void ltdc_pll_config_deinit()
{
    __HAL_RCC_PLL3_DISABLE();

    uint32_t tickstart = HAL_GetTick();

    while (__HAL_RCC_GET_FLAG(RCC_FLAG_PLL3RDY)) {
        if ((HAL_GetTick() - tickstart) > PLL_TIMEOUT_VALUE) break;
    }
}

static void ltdc_pll_config_init(uint32_t frame_size, uint32_t refresh_rate)
{
    uint32_t pixel_clock = (resolution_clock[frame_size] * refresh_rate) / 60;

    for (uint32_t divm = 1; divm <= 63; divm++) {
        for (uint32_t divr = 1; divr <= 128; divr++) {

            uint32_t ref_clk = (OMV_OSC_HSE_FREQ / 1000) / divm;

            uint32_t vci = 0;
            if (1000 <= ref_clk && ref_clk <= 2000) vci = RCC_PLL3VCIRANGE_0;
            else if (2000 <= ref_clk && ref_clk <= 4000) vci = RCC_PLL3VCIRANGE_1;
            else if (4000 <= ref_clk && ref_clk <= 8000) vci = RCC_PLL3VCIRANGE_2;
            else if (8000 <= ref_clk && ref_clk <= 16000) vci = RCC_PLL3VCIRANGE_3;
            else continue;

            uint32_t pll_clk = pixel_clock * divr;

            uint32_t vco = 0;
            if (150000 <= pll_clk && pll_clk <= 42000) vco = RCC_PLL3VCOMEDIUM;
            else if (192000 <= pll_clk && pll_clk <= 836000) vco = RCC_PLL3VCOWIDE;
            else continue;

            uint32_t divn = pll_clk / ref_clk;
            if (divn < 4 || 512 < divn) continue;

            uint32_t frac = ((pll_clk % ref_clk) * 8192) / ref_clk;

            RCC_PeriphCLKInitTypeDef init;
            init.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
            init.PLL3.PLL3M = divm;
            init.PLL3.PLL3N = divn;
            init.PLL3.PLL3P = 128;
            init.PLL3.PLL3Q = 128;
            init.PLL3.PLL3R = divr;
            init.PLL3.PLL3RGE = vci;
            init.PLL3.PLL3VCOSEL = vco;
            init.PLL3.PLL3FRACN = frac;

            if (HAL_RCCEx_PeriphCLKConfig(&init) == HAL_OK) return;
        }
    }

    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Unable to initialize LTDC PLL!"));
}

static void ltdc_config_deinit()
{
    HAL_LTDC_DeInit(&ltdc_handle);
    ltdc_pll_config_deinit();

    for (int i = 0; i < LTDC_FRAMEBUFFER_COUNT; i++) {
        if (ltdc_framebuffers[i]) {
            ltdc_framebuffers[i] = NULL;
            fb_free();
        }
    }
}

static void ltdc_config_init(uint32_t frame_size, uint32_t refresh_rate)
{
    for (int i = 0; i < LTDC_FRAMEBUFFER_COUNT; i++) {
        ltdc_framebuffer_layers[i].WindowX0 = 0;
        ltdc_framebuffer_layers[i].WindowX1 = width;
        ltdc_framebuffer_layers[i].WindowY0 = 0;
        ltdc_framebuffer_layers[i].WindowY1 = height;
        ltdc_framebuffer_layers[i].PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
        ltdc_framebuffer_layers[i].Alpha = 0;
        ltdc_framebuffer_layers[i].Alpha0 = 0;
        ltdc_framebuffer_layers[i].BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
        ltdc_framebuffer_layers[i].BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
        ltdc_framebuffer_layers[i].FBStartAdress = ltdc_framebuffers[i] = (uint16_t *) fb_alloc0(width * height * sizeof(uint16_t), FB_ALLOC_NO_HINT);
        ltdc_framebuffer_layers[i].ImageWidth = width;
        ltdc_framebuffer_layers[i].ImageHeight = height;
        ltdc_framebuffer_layers[i].Backcolor.Blue = 0;
        ltdc_framebuffer_layers[i].Backcolor.Green = 0;
        ltdc_framebuffer_layers[i].Backcolor.Red = 0;
    }

    ltdc_pll_config_init(frame_size, refresh_rate);

    ltdc_handle.Instance = LTDC;
    memcpy(&ltdc_handle.Init, &resolution_cfg[frame_size], sizeof(LTDC_InitTypeDef));

    HAL_LTDC_Init(&ltdc_handle);

    NVIC_SetPriority(LTDC_IRQn, IRQ_PRI_LTDC);
    HAL_NVIC_EnableIRQ(LTDC_IRQn);
}

// Set the output to be equal to whatever head is and update tail to point to head. We never want
// to draw in any buffer where head or tail is.
void HAL_LTDC_ReloadEventCallback(LTDC_HandleTypeDef *hltdc)
{
    // If Alpha is zero then disable the layer to save bandwidth.
    if (ltdc_framebuffer_layers[ltdc_framebuffer_head].Alpha) {
        HAL_LTDC_ConfigLayer(&ltdc_handle, &ltdc_framebuffer_layers[ltdc_framebuffer_head], LTDC_LAYER_1);
    } else {
        __HAL_LTDC_LAYER_DISABLE(&ltdc_handle, LTDC_LAYER_1);
    }

    ltdc_framebuffer_tail = ltdc_framebuffer_head;
}

static void ltdc_display(image_t *src_img, int dst_x_start, int dst_y_start, float x_scale, float y_scale, rectangle_t *roi,
                         int rgb_channel, int alpha, const uint16_t *color_palette, const uint8_t *alpha_palette, image_hint_t hint)
{
    image_t dst_img;
    dst_img.w = width;
    dst_img.h = height;
    dst_img.bpp = IMAGE_BPP_RGB565;

    // For triple buffering we are never drawing where head or tail (which may instantly update to
    // to be equal to head) is.
    int new_ltdc_framebuffer_head = (ltdc_framebuffer_head + 1) % LTDC_FRAMEBUFFER_COUNT;
    if (new_ltdc_framebuffer_head == ltdc_framebuffer_tail) new_ltdc_framebuffer_head = (new_ltdc_framebuffer_head + 1) % LTDC_FRAMEBUFFER_COUNT;
    dst_img.data = ltdc_framebuffers[new_ltdc_framebuffer_head];

    // Set default values for the layer to display the whole framebuffer.
    ltdc_framebuffer_layers[new_ltdc_framebuffer_head].WindowX0 = 0;
    ltdc_framebuffer_layers[new_ltdc_framebuffer_head].WindowX1 = dst_img.w;
    ltdc_framebuffer_layers[new_ltdc_framebuffer_head].WindowY0 = 0;
    ltdc_framebuffer_layers[new_ltdc_framebuffer_head].WindowY1 = dst_img.h;
    ltdc_framebuffer_layers[new_ltdc_framebuffer_head].Alpha = fast_roundf((alpha * 255) / 256.f);
    ltdc_framebuffer_layers[new_ltdc_framebuffer_head].FBStartAdress = dst_img.data;
    ltdc_framebuffer_layers[new_ltdc_framebuffer_head].ImageWidth = dst_img.w;
    ltdc_framebuffer_layers[new_ltdc_framebuffer_head].ImageHeight = dst_img.h;

    // If alpha was initially black we don't need to do anything. Just display a black layer.
    while (ltdc_framebuffer_layers[new_ltdc_framebuffer_head].Alpha) {

        if (alpha_palette) {
            int i = 0;
            while ((i < 256) && (!alpha_palette[i])) i++;
            if (i == 256) { // zero alpha palette
                ltdc_framebuffer_layers[new_ltdc_framebuffer_head].Alpha = 0;
                break;
            }
        }

        // Set alpha to 256 here as we will use the layer alpha to blend the image into the background
        // color of black for free.
        imlib_draw_image(&dst_img, src_img, dst_x_start, dst_y_start, x_scale, y_scale, roi,
                         rgb_channel, 256, color_palette, alpha_palette, hint | IMAGE_HINT_BACKGROUND_IS_BLACK);
        // The IMAGE_HINT_BACKGROUND_IS_BLACK flag saves having to clear the area the image is being
        // drawn on and speeds up alpha palette blending.

        // To save time clearing the framebuffer before drawing on it per frame we compute the extent of
        // what was drawn and then adjust the layer size to only display that area.
        //
        // The code below mimicks how draw_image computes the area to draw in.

        int src_width_scaled = fast_floorf(fast_fabsf(x_scale) * (roi ? roi->w : src_img->w));
        int src_height_scaled = fast_floorf(fast_fabsf(y_scale) * (roi ? roi->h : src_img->h));

        // Center src if hint is set.
        if (hint & IMAGE_HINT_CENTER) {
            dst_x_start -= src_width_scaled / 2;
            dst_y_start -= src_height_scaled / 2;
        }

        // Clamp start x to image bounds.
        int src_x_start = 0;
        if (dst_x_start < 0) {
            src_x_start -= dst_x_start; // this is an add because dst_x_start is negative
            dst_x_start = 0;
        }

        if (dst_x_start >= dst_img.w) {
            ltdc_framebuffer_layers[new_ltdc_framebuffer_head].Alpha = 0;
            break;
        }

        int src_x_dst_width = src_width_scaled - src_x_start;

        if (src_x_dst_width <= 0) {
            ltdc_framebuffer_layers[new_ltdc_framebuffer_head].Alpha = 0;
            break;
        }

        // Clamp start y to image bounds.
        int src_y_start = 0;
        if (dst_y_start < 0) {
            src_y_start -= dst_y_start; // this is an add because dst_y_start is negative
            dst_y_start = 0;
        }

        if (dst_y_start >= dst_img.h) {
            ltdc_framebuffer_layers[new_ltdc_framebuffer_head].Alpha = 0;
            break;
        }

        int src_y_dst_height = src_height_scaled - src_y_start;

        if (src_y_dst_height <= 0) {
            ltdc_framebuffer_layers[new_ltdc_framebuffer_head].Alpha = 0;
            break;
        }

        // Clamp end x to image bounds.
        int dst_x_end = dst_x_start + src_x_dst_width;
        if (dst_x_end > dst_img.w) dst_x_end = dst_img.w;

        // Clamp end y to image bounds.
        int dst_y_end = dst_y_start + src_y_dst_height;
        if (dst_y_end > dst_img.h) dst_y_end = dst_img.h;

        // Optimize the layer size turning everything black that wasn't drawn on and saving RAM bandwidth.
        ltdc_framebuffer_layers[new_ltdc_framebuffer_head].WindowX0 = dst_x_start;
        ltdc_framebuffer_layers[new_ltdc_framebuffer_head].WindowX1 = dst_x_end;
        ltdc_framebuffer_layers[new_ltdc_framebuffer_head].WindowY0 = dst_y_start;
        ltdc_framebuffer_layers[new_ltdc_framebuffer_head].WindowY1 = dst_y_end;
        ltdc_framebuffer_layers[new_ltdc_framebuffer_head].FBStartAdress = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&dst_img, dst_y_start) + dst_x_start;
        ltdc_framebuffer_layers[new_ltdc_framebuffer_head].ImageWidth = dst_img.w;
        ltdc_framebuffer_layers[new_ltdc_framebuffer_head].ImageHeight = dst_y_end - dst_y_start;
        break;
    }

    // Update head which means a new image is ready.
    ltdc_framebuffer_head = new_ltdc_framebuffer_head;

    // Kick off an update of the display pointer.
    HAL_LTDC_Reload(&ltdc_handle, LTDC_RELOAD_VERTICAL_BLANKING);
}

static void ltdc_clear()
{
    // For triple buffering we are never drawing where head or tail (which may instantly update to
    // to be equal to head) is.
    int new_ltdc_framebuffer_head = (ltdc_framebuffer_head + 1) % LTDC_FRAMEBUFFER_COUNT;
    if (new_ltdc_framebuffer_head == ltdc_framebuffer_tail) new_ltdc_framebuffer_head = (new_ltdc_framebuffer_head + 1) % LTDC_FRAMEBUFFER_COUNT;

    // Set default values for the layer to display the whole framebuffer.
    ltdc_framebuffer_layers[new_ltdc_framebuffer_head].WindowX0 = 0;
    ltdc_framebuffer_layers[new_ltdc_framebuffer_head].WindowX1 = width;
    ltdc_framebuffer_layers[new_ltdc_framebuffer_head].WindowY0 = 0;
    ltdc_framebuffer_layers[new_ltdc_framebuffer_head].WindowY1 = height;
    ltdc_framebuffer_layers[new_ltdc_framebuffer_head].Alpha = 0;
    ltdc_framebuffer_layers[new_ltdc_framebuffer_head].FBStartAdress = ltdc_framebuffers[new_ltdc_framebuffer_head];
    ltdc_framebuffer_layers[new_ltdc_framebuffer_head].ImageWidth = width;
    ltdc_framebuffer_layers[new_ltdc_framebuffer_head].ImageHeight = height;

    // Update head which means a new image is ready.
    ltdc_framebuffer_head = new_ltdc_framebuffer_head;

    // Kick off an update of the display pointer.
    HAL_LTDC_Reload(&ltdc_handle, LTDC_RELOAD_VERTICAL_BLANKING);
}

static void ltdc_set_backlight(int intensity)
{
    if ((bl_intensity < 255) && (255 <= intensity)) {
        GPIO_InitTypeDef GPIO_InitStructure;
        GPIO_InitStructure.Pull = GPIO_NOPULL;
        GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStructure.Pin = OMV_LCD_BL_PIN;
        HAL_GPIO_Init(OMV_LCD_BL_PORT, &GPIO_InitStructure);
        OMV_LCD_BL_ON();
    } else if ((0 < bl_intensity) && (intensity <= 0)) {
        OMV_LCD_BL_OFF();
        HAL_GPIO_Deinit(OMV_LCD_BL_PORT, OMV_LCD_BL_PIN);
    }

    int tclk = OMV_LCD_BL_TIM_PCLK_FREQ() * 2;
    int period = (tclk / OMV_LCD_BL_FREQ) - 1;

    if (((bl_intensity <= 0) || (255 <= bl_intensity)) && (0 < intensity) && (intensity < 255)) {
        GPIO_InitTypeDef GPIO_InitStructure;
        GPIO_InitStructure.Pull = GPIO_NOPULL;
        GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStructure.Alternate = OMV_LCD_BL_ALT;
        GPIO_InitStructure.Pin = OMV_LCD_BL_PIN;
        HAL_GPIO_Init(OMV_LCD_BL_PORT, &GPIO_InitStructure);

        bl_tim_handle.Instance = OMV_LCD_BL_TIM;
        bl_tim_handle.Init.Period = period;
        bl_tim_handle.Init.Prescaler = TIM_ETRPRESCALER_DIV1;
        bl_tim_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
        bl_tim_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

        TIM_OC_InitTypeDef bl_tim_oc_handle;
        bl_tim_oc_handle.Pulse = (period * intensity) / 255;
        bl_tim_oc_handle.OCMode = TIM_OCMODE_PWM1;
        bl_tim_oc_handle.OCPolarity = TIM_OCPOLARITY_HIGH;
        bl_tim_oc_handle.OCFastMode = TIM_OCFAST_DISABLE;
        bl_tim_oc_handle.OCIdleState = TIM_OCIDLESTATE_RESET;

        HAL_TIM_PWM_Init(&bl_tim_handle);
        HAL_TIM_PWM_ConfigChannel(&bl_tim_handle, &bl_tim_oc_handle, OMV_LCD_BL_TIM_CHANNEL);
        HAL_TIM_PWM_Start(&bl_tim_handle, OMV_LCD_BL_TIM_CHANNEL);
    } else if ((0 < bl_intensity) && (bl_intensity < 255) && ((intensity <= 0) || (255 <= intensity))) {
        HAL_TIM_PWM_Stop(&bl_tim_handle, OMV_LCD_BL_TIM_CHANNEL);
        HAL_TIM_PWM_Deinit(&bl_tim_handle);
    } else if ((0 < bl_intensity) && (bl_intensity < 255) && (0 < intensity) && (intensity < 255)) {
        __HAL_TIM_SET_COMPARE(&bl_tim_handle, OMV_LCD_BL_TIM_CHANNEL, (period * intensity) / 255);
    }

    bl_intensity = intensity;
}

// Send out 8-bit data using the SPI object.
STATIC void lcd_write_command_byte(uint8_t data_byte)
{
    mp_map_t arg_map;
    arg_map.all_keys_are_qstrs = true;
    arg_map.is_fixed = true;
    arg_map.is_ordered = true;
    arg_map.used = 0;
    arg_map.alloc = 0;
    arg_map.table = NULL;

    CS_PIN_WRITE(false);
    RS_PIN_WRITE(false); // command
    pyb_spi_send(
        2, (mp_obj_t []) {
            spi_port,
            mp_obj_new_int(data_byte)
        },
        &arg_map
    );
    CS_PIN_WRITE(true);
}

// Send out 8-bit data using the SPI object.
STATIC void lcd_write_data_byte(uint8_t data_byte)
{
    mp_map_t arg_map;
    arg_map.all_keys_are_qstrs = true;
    arg_map.is_fixed = true;
    arg_map.is_ordered = true;
    arg_map.used = 0;
    arg_map.alloc = 0;
    arg_map.table = NULL;

    CS_PIN_WRITE(false);
    RS_PIN_WRITE(true); // data
    pyb_spi_send(
        2, (mp_obj_t []) {
            spi_port,
            mp_obj_new_int(data_byte)
        },
        &arg_map
    );
    CS_PIN_WRITE(true);
}

// Send out 8-bit data using the SPI object.
STATIC void lcd_write_command(uint8_t data_byte, uint32_t len, uint8_t *dat)
{
    lcd_write_command_byte(data_byte);
    for (uint32_t i=0; i<len; i++) lcd_write_data_byte(dat[i]);
}

// Send out 8-bit data using the SPI object.
STATIC void lcd_write_data(uint32_t len, uint8_t *dat)
{
    mp_obj_str_t arg_str;
    arg_str.base.type = &mp_type_bytes;
    arg_str.hash = 0;
    arg_str.len = len;
    arg_str.data = dat;

    mp_map_t arg_map;
    arg_map.all_keys_are_qstrs = true;
    arg_map.is_fixed = true;
    arg_map.is_ordered = true;
    arg_map.used = 0;
    arg_map.alloc = 0;
    arg_map.table = NULL;

    CS_PIN_WRITE(false);
    RS_PIN_WRITE(true); // data
    pyb_spi_send(
        2, (mp_obj_t []) {
            spi_port,
            &arg_str
        },
        &arg_map
    );
    CS_PIN_WRITE(true);
}

static void spi_lcd_display()
{
    lcd_write_command_byte(0x2C);
    fb_alloc_mark();
    uint8_t *zero = fb_alloc0(width*2, FB_ALLOC_NO_HINT);
    uint16_t *line = fb_alloc(width*2, FB_ALLOC_NO_HINT);
    for (int i=0; i<t_pad; i++) {
        lcd_write_data(width*2, zero);
    }
    for (int i=0; i<rect.h; i++) {
        if (l_pad) {
            lcd_write_data(l_pad*2, zero); // l_pad < width
        }
        if (IM_IS_GS(arg_img)) {
            for (int j=0; j<rect.w; j++) {
                uint8_t pixel = IM_GET_GS_PIXEL(arg_img, (rect.x + j), (rect.y + i));
                line[j] = COLOR_Y_TO_RGB565(pixel);
            }
        } else {
            for (int j=0; j<rect.w; j++) {
                uint16_t pixel = IM_GET_RGB565_PIXEL(arg_img, (rect.x + j), (rect.y + i));
                line[j] = __REV16(pixel);
            }
        }
        lcd_write_data(rect.w*2, (uint8_t *) line);
        if (r_pad) {
            lcd_write_data(r_pad*2, zero); // r_pad < width
        }
    }
    for (int i=0; i<b_pad; i++) {
        lcd_write_data(width*2, zero);
    }
    fb_alloc_free_till_mark();
}

static void spi_lcd_clear()
{
    lcd_write_command_byte(0x2C);
    fb_alloc_mark();
    uint8_t *zero = fb_alloc0(width*2, FB_ALLOC_NO_HINT);
    for (int i=0; i<height; i++) {
        lcd_write_data(width*2, zero);
    }
    fb_alloc_free_till_mark();
}

static void spi_lcd_set_backlight(int intensity)
{
    if (!backlight_init) {
        GPIO_InitTypeDef GPIO_InitStructure;
        GPIO_InitStructure.Pull  = GPIO_NOPULL;
        GPIO_InitStructure.Speed = GPIO_SPEED_LOW;
        GPIO_InitStructure.Mode  = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStructure.Pin = LED_PIN;
        LED_PIN_WRITE(bit); // Set first to prevent glitches.
        HAL_GPIO_Init(LED_PORT, &GPIO_InitStructure);
        backlight_init = true;
    }
    LED_PIN_WRITE(intensity);

    bl_intensity = intensity;
}

STATIC mp_obj_t py_lcd_deinit()
{
    switch (type) {
        case LCD_NONE:
            return mp_const_none;
        case LCD_SHIELD:
            HAL_GPIO_DeInit(RST_PORT, RST_PIN);
            HAL_GPIO_DeInit(RS_PORT, RS_PIN);
            HAL_GPIO_DeInit(CS_PORT, CS_PIN);
            pyb_spi_deinit(spi_port);
            spi_port = NULL;
            width = 0;
            height = 0;
            type = LCD_NONE;
            if (backlight_init) {
                HAL_GPIO_DeInit(LED_PORT, LED_PIN);
                backlight_init = false;
            }
            return mp_const_none;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_deinit_obj, py_lcd_deinit);

STATIC mp_obj_t py_lcd_init(uint n_args, const mp_obj_t *args, mp_map_t *kw_args)
{
    py_lcd_deinit();

    switch (py_helper_keyword_int(n_args, args, 0, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_type), LCD_SHIELD)) {
        case LCD_SHIELD: {
            GPIO_InitTypeDef GPIO_InitStructure;
            GPIO_InitStructure.Pull  = GPIO_NOPULL;
            GPIO_InitStructure.Speed = GPIO_SPEED_LOW;
            GPIO_InitStructure.Mode  = GPIO_MODE_OUTPUT_OD;

            GPIO_InitStructure.Pin = CS_PIN;
            CS_PIN_WRITE(true); // Set first to prevent glitches.
            HAL_GPIO_Init(CS_PORT, &GPIO_InitStructure);

            GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;

            GPIO_InitStructure.Pin = RST_PIN;
            RST_PIN_WRITE(true); // Set first to prevent glitches.
            HAL_GPIO_Init(RST_PORT, &GPIO_InitStructure);

            GPIO_InitStructure.Pin = RS_PIN;
            RS_PIN_WRITE(true); // Set first to prevent glitches.
            HAL_GPIO_Init(RS_PORT, &GPIO_InitStructure);

            spi_port = pyb_spi_make_new(NULL,
                2, // n_args
                3, // n_kw
                (mp_obj_t []) {
                    MP_OBJ_NEW_SMALL_INT(2), // SPI Port
                    MP_OBJ_NEW_SMALL_INT(SPI_MODE_MASTER),
                    MP_OBJ_NEW_QSTR(MP_QSTR_baudrate),
                    MP_OBJ_NEW_SMALL_INT(1000000000/66), // 66 ns clk period
                    MP_OBJ_NEW_QSTR(MP_QSTR_polarity),
                    MP_OBJ_NEW_SMALL_INT(0),
                    MP_OBJ_NEW_QSTR(MP_QSTR_phase),
                    MP_OBJ_NEW_SMALL_INT(0)
                }
            );
            width = 128;
            height = 160;
            type = LCD_SHIELD;
            backlight_init = false;

            RST_PIN_WRITE(false);
            systick_sleep(100);
            RST_PIN_WRITE(true);
            systick_sleep(100);
            lcd_write_command_byte(0x11); // Sleep Exit
            systick_sleep(120);

            // Memory Data Access Control
            uint8_t madctl = 0xC0;
            uint8_t bgr = py_helper_keyword_int(n_args, args, 0, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_bgr), 0);
            lcd_write_command(0x36, 1, (uint8_t []) {madctl | (bgr<<3)});

            // Interface Pixel Format
            lcd_write_command(0x3A, 1, (uint8_t []) {0x05});

            // Display on
            lcd_write_command_byte(0x29);

            break;
        }
        case LCD_DISPLAY: {
            int frame_size = py_helper_keyword_int(n_args, args, 1, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_framesize), LCD_DISPLAY_VGA);
            if ((frame_size < 0) || (LCD_DISPLAY_MAX <= frame_size)) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Invalid Frame Size!"));
            int refresh_rate = py_helper_keyword_int(n_args, args, 2, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_refresh), 60);
            if ((refresh_rate < 30) || (120 < refresh_rate)) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Invalid Refresh Rate!"));
            ltdc_config_init(frame_size, refresh_rate);
            width = resolution_w_h[resolution][0];
            height = resolution_w_h[resolution][1];
            type = LCD_DISPLAY;
            resolution = frame_size;
            refresh = refresh_rate;
            break;
        }
        default: {
            break;
        }
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(py_lcd_init_obj, 0, py_lcd_init);

STATIC mp_obj_t py_lcd_width()
{
    if (type == LCD_NONE) return mp_const_none;
    return mp_obj_new_int(width);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_width_obj, py_lcd_width);

STATIC mp_obj_t py_lcd_height()
{
    if (type == LCD_NONE) return mp_const_none;
    return mp_obj_new_int(height);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_height_obj, py_lcd_height);

STATIC mp_obj_t py_lcd_type()
{
    if (type == LCD_NONE) return mp_const_none;
    return mp_obj_new_int(type);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_type_obj, py_lcd_type);

STATIC mp_obj_t py_lcd_framesize()
{
    if (type == LCD_NONE) return mp_const_none;
    return mp_obj_new_int(resolution);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_framesize_obj, py_lcd_framesize);

STATIC mp_obj_t py_lcd_refresh()
{
    if (type == LCD_NONE) return mp_const_none;
    return mp_obj_new_int(refresh);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_refresh_obj, py_lcd_refresh);

STATIC mp_obj_t py_lcd_set_backlight(mp_obj_t intensity_obj)
{
    int intensity = mp_obj_get_int(intensity_obj);
    if ((intensity < 0) || (255 < intensity)) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "0 <= intensity <= 255!"));

    switch (type) {
        case LCD_SHIELD: {
            spi_lcd_set_backlight(intensity);
            break;
        }
        case LCD_DISPLAY: {
            ltdc_set_backlight(intensity);
            break;
        }
        default: {
            break;
        }
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_lcd_set_backlight_obj, py_lcd_set_backlight);

STATIC mp_obj_t py_lcd_get_backlight()
{
    if (type == LCD_NONE) return mp_const_none;
    return mp_obj_new_int(bl_intensity);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_get_backlight_obj, py_lcd_get_backlight);

STATIC mp_obj_t py_lcd_display(uint n_args, const mp_obj_t *args, mp_map_t *kw_args)
{
    image_t *arg_img = py_helper_arg_to_image_mutable(args[0]);

    const mp_obj_t *arg_vec;
    uint offset = py_helper_consume_array(n_args, args, 1, 2, &arg_vec);
    int arg_x_off = mp_obj_get_int(arg_vec[0]);
    int arg_y_off = mp_obj_get_int(arg_vec[1]);

    float arg_x_scale = 1.f;
    bool got_x_scale = py_helper_keyword_float_maybe(n_args, args, offset + 0, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_x_scale), &arg_x_scale);

    float arg_y_scale = 1.f;
    bool got_y_scale = py_helper_keyword_float_maybe(n_args, args, offset + 1, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_y_scale), &arg_y_scale);

    rectangle_t arg_roi;
    py_helper_keyword_rectangle_roi(arg_img, n_args, args, offset + 2, kw_args, &arg_roi);

    int arg_rgb_channel = py_helper_keyword_int(n_args, args, offset + 3, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_rgb_channel), -1);
    if ((arg_rgb_channel < -1) || (2 < arg_rgb_channel)) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "-1 <= rgb_channel <= 2!"));

    int arg_alpha = py_helper_keyword_int(n_args, args, offset + 4, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_alpha), 256);
    if ((arg_alpha < 0) || (256 < arg_alpha)) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "0 <= alpha <= 256!"));

    const uint16_t *color_palette = NULL;
    {
        int palette;

        if (py_helper_keyword_int_maybe(n_args, args, offset + 5, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_color_palette), &palette)) {
            if (palette == COLOR_PALETTE_RAINBOW) color_palette = rainbow_table;
            else if (palette == COLOR_PALETTE_IRONBOW) color_palette = ironbow_table;
            else nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Invalid pre-defined color palette!"));
        } else {
            image_t *arg_color_palette = py_helper_keyword_to_image_mutable_color_palette(n_args, args, offset + 5, kw_args);

            if (arg_color_palette) {
                if (arg_color_palette->bpp != IMAGE_BPP_RGB565) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Color palette must be RGB565!"));
                if ((arg_color_palette->w * arg_color_palette->h) != 256) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Color palette must be 256 pixels!"));
                color_palette = (uint16_t *) arg_color_palette->data;
            }
        }
    }

    const uint8_t *alpha_palette = NULL;
    {
        image_t *arg_alpha_palette = py_helper_keyword_to_image_mutable_alpha_palette(n_args, args, offset + 6, kw_args);

        if (arg_alpha_palette) {
            if (arg_alpha_palette->bpp != IMAGE_BPP_GRAYSCALE) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Alpha palette must be GRAYSCALE!"));
            if ((arg_alpha_palette->w * arg_alpha_palette->h) != 256) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Alpha palette must be 256 pixels!"));
            alpha_palette = (uint8_t *) arg_alpha_palette->data;
        }
    }

    image_hint_t hint = py_helper_keyword_int(n_args, args, offset + 7, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_hint), 0);

    int arg_x_size;
    bool got_x_size = py_helper_keyword_int_maybe(n_args, args, offset + 8, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_x_size), &arg_x_size);

    int arg_y_size;
    bool got_y_size = py_helper_keyword_int_maybe(n_args, args, offset + 9, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_y_size), &arg_y_size);

    if (got_x_scale && got_x_size) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Choose either x_scale or x_size not both!"));
    if (got_y_scale && got_y_size) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Choose either y_scale or y_size not both!"));

    if (got_x_size) arg_x_scale = arg_x_size / ((float) arg_img->w);
    if (got_y_size) arg_y_scale = arg_y_size / ((float) arg_img->h);

    if ((!got_x_scale) && (!got_x_size) && got_y_size) arg_x_scale = arg_y_scale;
    if ((!got_y_scale) && (!got_y_size) && got_x_size) arg_y_scale = arg_x_scale;

    switch (type) {
        case LCD_SHIELD: {
            fb_alloc_mark();
            spi_lcd_display(arg_img, arg_x_off, arg_y_off, arg_x_scale, arg_y_scale, &arg_roi,
                            arg_rgb_channel, arg_alpha, color_palette, alpha_palette, hint);
            fb_alloc_free_till_mark();
            break;
        }
        case LCD_DISPLAY: {
            fb_alloc_mark();
            ltdc_display(arg_img, arg_x_off, arg_y_off, arg_x_scale, arg_y_scale, &arg_roi,
                         arg_rgb_channel, arg_alpha, color_palette, alpha_palette, hint);
            fb_alloc_free_till_mark();
            break;
        }
        default: {
            break;
        }
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(py_lcd_display_obj, 1, py_lcd_display);

STATIC mp_obj_t py_lcd_clear()
{
    switch (type) {
        case LCD_SHIELD: {
            fb_alloc_mark();
            spi_lcd_clear();
            fb_alloc_free_till_mark();
            break;
        }
        case LCD_DISPLAY: {
            ltdc_clear();
            break;
        }
        default: {
            break;
        }
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_clear_obj, py_lcd_clear);

STATIC const mp_rom_map_elem_t globals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_OBJ_NEW_QSTR(MP_QSTR_lcd)          },
    { MP_ROM_QSTR(MP_QSTR_VGA),             MP_OBJ_NEW_QSTR(LCD_DISPLAY_VGA)      },
    { MP_ROM_QSTR(MP_QSTR_WVGA),            MP_OBJ_NEW_QSTR(LCD_DISPLAY_WVGA)     },
    { MP_ROM_QSTR(MP_QSTR_SVGA),            MP_OBJ_NEW_QSTR(LCD_DISPLAY_SVGA)     },
    { MP_ROM_QSTR(MP_QSTR_XGA),             MP_OBJ_NEW_QSTR(LCD_DISPLAY_XGA)      },
    { MP_ROM_QSTR(MP_QSTR_SXGA),            MP_OBJ_NEW_QSTR(LCD_DISPLAY_SXGA)     },
    { MP_ROM_QSTR(MP_QSTR_UXGA),            MP_OBJ_NEW_QSTR(LCD_DISPLAY_UXGA)     },
    { MP_ROM_QSTR(MP_QSTR_HD),              MP_OBJ_NEW_QSTR(LCD_DISPLAY_HD)       },
    { MP_ROM_QSTR(MP_QSTR_FHD),             MP_OBJ_NEW_QSTR(LCD_DISPLAY_FHD)      },
    { MP_ROM_QSTR(MP_QSTR_init),            MP_ROM_PTR(&py_lcd_init_obj)          },
    { MP_ROM_QSTR(MP_QSTR_deinit),          MP_ROM_PTR(&py_lcd_deinit_obj)        },
    { MP_ROM_QSTR(MP_QSTR_width),           MP_ROM_PTR(&py_lcd_width_obj)         },
    { MP_ROM_QSTR(MP_QSTR_height),          MP_ROM_PTR(&py_lcd_height_obj)        },
    { MP_ROM_QSTR(MP_QSTR_type),            MP_ROM_PTR(&py_lcd_type_obj)          },
    { MP_ROM_QSTR(MP_QSTR_framesize),       MP_ROM_PTR(&py_lcd_framesize_obj)     },
    { MP_ROM_QSTR(MP_QSTR_refresh),         MP_ROM_PTR(&py_lcd_refresh_obj)       },
    { MP_ROM_QSTR(MP_QSTR_get_backlight),   MP_ROM_PTR(&py_lcd_get_backlight_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_backlight),   MP_ROM_PTR(&py_lcd_set_backlight_obj) },
    { MP_ROM_QSTR(MP_QSTR_display),         MP_ROM_PTR(&py_lcd_display_obj)       },
    { MP_ROM_QSTR(MP_QSTR_clear),           MP_ROM_PTR(&py_lcd_clear_obj)         },
};

STATIC MP_DEFINE_CONST_DICT(globals_dict, globals_dict_table);

const mp_obj_module_t lcd_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_t) &globals_dict,
};

void py_lcd_init0()
{
    py_lcd_deinit();
}
