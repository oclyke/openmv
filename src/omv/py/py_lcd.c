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
#include "py_helper.h"
#include "omv_boardconfig.h"

#define FRAMEBUFFER_COUNT 3
static int framebuffer_head = 0;
static volatile int framebuffer_tail = 0;
static uint16_t *framebuffers[FRAMEBUFFER_COUNT] = {};

static int lcd_width = 0;
static int lcd_height = 0;

static enum {
    LCD_NONE,
    LCD_SHIELD,
    LCD_DISPLAY
} lcd_type = LCD_NONE;

static bool lcd_triple_buffer = false;
static bool lcd_bgr = false;

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
} lcd_resolution = LCD_DISPLAY_VGA;

static int lcd_refresh = 0;
static int lcd_intensity = 0;

#ifdef OMV_SPI_LCD_CONTROLLER
static SPI_HandleTypeDef spi_handle = {};
static DMA_HandleTypeDef dma_handle = {};

static volatile enum {
    SPI_TX_CB_IDLE,
    SPI_TX_CB_MEMORY_WRITE_CMD,
    SPI_TX_CB_MEMORY_WRITE,
    SPI_TX_CB_DISPLAY_ON,
    SPI_TX_CB_DISPLAY_OFF
} spi_tx_cb_state = SPI_TX_CB_IDLE;

static void spi_config_deinit()
{
    if (lcd_triple_buffer) {
        HAL_SPI_Abort(&spi_handle);
        HAL_DMA_Deinit(&dma_handle);
        spi_tx_cb_state = SPI_TX_CB_IDLE;
        fb_alloc_free_till_mark_past_mark_permanent();
    }

    HAL_SPI_DeInit(&spi_handle);
}

static void spi_config_init(int w, int h, int refresh_rate, bool triple_buffer, bool bgr)
{
    spi_handle.Mode = SPI_MODE_MASTER;
    spi_handle.Direction = SPI_DIRECTION_1LINE;
    spi_handle.DataSize = SPI_DATASIZE_16BIT;
    spi_handle.CLKPolarity = SPI_POLARITY_LOW;
    spi_handle.CLKPhase = SPI_PHASE_1EDGE;
    spi_handle.NSS = SPI_NSS_SOFT;
    spi_handle.FirstBit = SPI_FIRSTBIT_MSB;
    spi_handle.TIMode = SPI_TIMODE_DISABLE;
    spi_handle.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    spi_handle.CRCPolynomial = 0;

    long baudrate = w * h * refresh_rate * 16;
    int prescaler = (OMV_SPI_LCD_PCLK_FREQ() + baudrate - 1) / baudrate;
    if (prescaler <= 2) { spi_handle.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2; }
    else if (prescaler <= 4) { spi_handle.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4; }
    else if (prescaler <= 8) { spi_handle.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; }
    else if (prescaler <= 16) { spi_handle.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; }
    else if (prescaler <= 32) { spi_handle.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32; }
    else if (prescaler <= 64) { spi_handle.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64; }
    else if (prescaler <= 128) { spi_handle.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128; }
    else { spi_handle.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256; }

    HAL_SPI_Init(&spi_handle);

    OMV_SPI_LCD_RST_ON();
    HAL_Delay(100);

    OMV_SPI_LCD_RST_OFF();
    HAL_Delay(100);

    OMV_SPI_LCD_RS_ON();
    OMV_SPI_LCD_CS_LOW();
    HAL_SPI_Transmit(&spi_handle, (uint8_t *) {0x00, 0x11}, 1, HAL_MAX_DELAY); // sleep out
    OMV_SPI_LCD_CS_HIGH();
    OMV_SPI_LCD_RS_OFF();
    HAL_Delay(120);

    OMV_SPI_LCD_RS_ON();
    OMV_SPI_LCD_CS_LOW();
    HAL_SPI_Transmit(&spi_handle, (uint8_t *) {0x00, 0x36}, 1, HAL_MAX_DELAY); // memory data access control
    OMV_SPI_LCD_CS_HIGH();
    OMV_SPI_LCD_RS_OFF();
    OMV_SPI_LCD_CS_LOW();
    HAL_SPI_Transmit(&spi_handle, (uint8_t *) {0x00, bgr ? 0xC8 : 0xC0}, 1, HAL_MAX_DELAY); // argument
    OMV_SPI_LCD_CS_HIGH();

    OMV_SPI_LCD_RS_ON();
    OMV_SPI_LCD_CS_LOW();
    HAL_SPI_Transmit(&spi_handle, (uint8_t *) {0x00, 0x3A}, 1, HAL_MAX_DELAY); // interface pixel format
    OMV_SPI_LCD_CS_HIGH();
    OMV_SPI_LCD_RS_OFF();
    OMV_SPI_LCD_CS_LOW();
    HAL_SPI_Transmit(&spi_handle, (uint8_t *) {0x00, 0x05}, 1, HAL_MAX_DELAY); // argument
    OMV_SPI_LCD_CS_HIGH();

    if (triple_buffer) {
        fb_alloc_mark();

        framebuffer_head = 0;
        framebuffer_tail = 0;

        for (int i = 0; i < FRAMEBUFFER_COUNT; i++) {
            framebuffers[i] = (uint16_t *) fb_alloc0(w * h * sizeof(uint16_t), FB_ALLOC_NO_HINT);
        }

        dma_handle.Instance                 = OMV_SPI_LCD_DMA;
#if defined(MCU_SERIES_F4) || defined(MCU_SERIES_F7)
        dma_handle.Init.Channel             = OMV_SPI_LCD_DMA_REQUEST;
#else
        dma_handle.Init.Request             = OMV_SPI_LCD_DMA_REQUEST;
#endif
        dma_handle.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        dma_handle.Init.PeriphInc           = DMA_PINC_DISABLE;
        dma_handle.Init.MemInc              = DMA_MINC_ENABLE;
        dma_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        dma_handle.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
        dma_handle.Init.Mode                = DMA_NORMAL;
        dma_handle.Init.Priority            = DMA_PRIORITY_HIGH;
        dma_handle.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        dma_handle.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_1QUARTERFULL;
        dma_handle.Init.MemBurst            = DMA_MBURST_SINGLE;
        dma_handle.Init.PeriphBurst         = DMA_PBURST_SINGLE;

        HAL_DMA_Init(&dma_handle);

        __HAL_LINKDMA(&spi_handle, hdmatx, dma_handle);

        NVIC_SetPriority(OMV_SPI_LCD_IRQN, OMV_SPI_LCD_IRQN_PRI);
        HAL_NVIC_EnableIRQ(OMV_SPI_LCD_IRQN);

        NVIC_SetPriority(OMV_SPI_LCD_DMA_IRQN, OMV_SPI_LCD_DMA_IRQN_PRI);
        HAL_NVIC_DisableIRQ(OMV_SPI_LCD_DMA_IRQN);

        fb_alloc_mark_permanent();
    }
}

static bool spi_tx_cb_state_on[FRAMEBUFFER_COUNT] = {};

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == OMV_SPI_LCD_CONTROLLER) {
        static uint16_t *spi_tx_cb_state_memory_write_addr = NULL;
        static size_t spi_tx_cb_state_memory_write_count = 0;
        static bool spi_tx_cb_state_memory_write_first = false;

        switch (spi_tx_cb_state) {
            case SPI_TX_CB_MEMORY_WRITE_CMD: {
                if (!spi_tx_cb_state_on[framebuffer_head]) {
                    spi_tx_cb_state = SPI_TX_CB_DISPLAY_OFF;
                    framebuffer_tail = framebuffer_head;
                    OMV_SPI_LCD_CS_HIGH();
                    OMV_SPI_LCD_RS_ON();
                    OMV_SPI_LCD_CS_LOW();
                    HAL_SPI_Transmit_DMA(&spi_handle, (uint8_t *) {0x00, 0x28, 0x00, 0x00}, 1); // display off
                } else {
                    spi_tx_cb_state = SPI_TX_CB_MEMORY_WRITE;
                    spi_tx_cb_state_memory_write_addr = framebuffers[framebuffer_head]
                    spi_tx_cb_state_memory_write_count = lcd_width * lcd_height;
                    spi_tx_cb_state_memory_write_first = true;
                    framebuffer_tail = framebuffer_head;
                    OMV_SPI_LCD_CS_HIGH();
                    OMV_SPI_LCD_RS_ON();
                    OMV_SPI_LCD_CS_LOW();
                    HAL_SPI_Transmit_DMA(&spi_handle, (uint8_t *) {0x00, 0x2C, 0x00, 0x00}, 1); // memory write
                }
                break;
            }
            case SPI_TX_CB_MEMORY_WRITE: {
                uint16_t *addr = spi_tx_cb_state_memory_write_addr;
                size_t count = IM_MIN(spi_tx_cb_state_memory_write_count, 65535);
                spi_tx_cb_state = (spi_tx_cb_state_memory_write_count > 65535) ? SPI_TX_CB_MEMORY_WRITE : SPI_TX_CB_DISPLAY_ON;
                spi_tx_cb_state_memory_write_addr += count;
                spi_tx_cb_state_memory_write_count -= count;
                if (spi_tx_cb_state_memory_write_first) {
                    spi_tx_cb_state_memory_write_first = false;
                    OMV_SPI_LCD_CS_HIGH();
                    OMV_SPI_LCD_RS_OFF();
                    OMV_SPI_LCD_CS_LOW();
                }
                HAL_SPI_Transmit_DMA(&spi_handle, addr, count);
                break;
            }
            case SPI_TX_CB_DISPLAY_ON: {
                spi_tx_cb_state = SPI_TX_CB_MEMORY_WRITE_CMD;
                OMV_SPI_LCD_CS_HIGH();
                OMV_SPI_LCD_RS_ON();
                OMV_SPI_LCD_CS_LOW();
                HAL_SPI_Transmit_DMA(&spi_handle, (uint8_t *) {0x00, 0x29, 0x00, 0x00}, 1); // display on
                break;
            }
            case SPI_TX_CB_DISPLAY_OFF: {
                spi_tx_cb_state = SPI_TX_CB_IDLE;
                OMV_SPI_LCD_CS_HIGH();
                break;
            }
            default: {
                break;
            }
        }
    }
}

// If the callback chain is not running restart it. Display off may have been called so we need wait
// for that operation to complete before restarting the process.
static void spi_lcd_kick()
{
    int spi_tx_cb_state_sampled = spi_tx_cb_state; // volatile

    if ((spi_tx_cb_state_sampled == SPI_TX_CB_IDLE) || (spi_tx_cb_state_sampled == SPI_TX_CB_DISPLAY_OFF)) {
        uint32_t tick = HAL_GetTick();

        while (spi_tx_cb_state != SPI_TX_CB_IDLE) { // volatile
            if ((HAL_GetTick() - tick) >= 1000) return; // give up (should not happen)
        }

        spi_tx_cb_state = SPI_TX_CB_MEMORY_WRITE_CMD;
        HAL_SPI_TxCpltCallback(&spi_handle);
    }
}

static void spi_lcd_draw_image_cb(int x_start, int x_end, int y_row, imlib_draw_row_data_t *data)
{
    HAL_SPI_Transmit(&spi_handle, data->dst_row_override, lcd_width, HAL_MAX_DELAY);
}

static void spi_lcd_display(image_t *src_img, int dst_x_start, int dst_y_start, float x_scale, float y_scale, rectangle_t *roi,
                            int rgb_channel, int alpha, const uint16_t *color_palette, const uint8_t *alpha_palette, image_hint_t hint)
{
    image_t dst_img;
    dst_img.w = lcd_width;
    dst_img.h = lcd_height;
    dst_img.bpp = IMAGE_BPP_RGB565;

    if (!lcd_triple_buffer) {
        dst_img.data = fb_alloc0(lcd_width * sizeof(uint16_t), FB_ALLOC_NO_HINT);

        OMV_SPI_LCD_RS_ON();
        OMV_SPI_LCD_CS_LOW();
        HAL_SPI_Transmit(&spi_handle, (uint8_t *) {0x00, 0x2C}, 1, HAL_MAX_DELAY); // memory write
        OMV_SPI_LCD_CS_HIGH();
        OMV_SPI_LCD_RS_OFF();

        OMV_SPI_LCD_CS_LOW();
        imlib_draw_image(&dst_img, src_img, dst_x_start, dst_y_start, x_scale, y_scale, roi,
                         rgb_channel, alpha, color_palette, alpha_palette, hint | IMAGE_HINT_BLACK_BACKGROUND, spi_lcd_draw_image_cb, dst_img.data);
        OMV_SPI_LCD_CS_HIGH();

        OMV_SPI_LCD_RS_ON();
        OMV_SPI_LCD_CS_LOW();
        HAL_SPI_Transmit(&spi_handle, (uint8_t *) {0x00, 0x29}, 1, HAL_MAX_DELAY); // display on
        OMV_SPI_LCD_CS_HIGH();
        OMV_SPI_LCD_RS_OFF();

        fb_free();
    } else {
        // For triple buffering we are never drawing where head or tail (which may instantly update to
        // to be equal to head) is.
        int new_framebuffer_head = (framebuffer_head + 1) % FRAMEBUFFER_COUNT;
        if (new_framebuffer_head == framebuffer_tail) new_framebuffer_head = (new_framebuffer_head + 1) % FRAMEBUFFER_COUNT;
        dst_img.data = framebuffers[new_framebuffer_head];

        memset(dst_img.data, 0, lcd_width * lcd_height * sizeof(uint16_t)); // TODO: improve
        imlib_draw_image(&dst_img, src_img, dst_x_start, dst_y_start, x_scale, y_scale, roi,
                         rgb_channel, alpha, color_palette, alpha_palette, hint | IMAGE_HINT_BLACK_BACKGROUND, NULL, NULL);

        // Tell the call back FSM that we want to turn the display on.
        spi_tx_cb_state_on[new_framebuffer_head] = true;

        // Update head which means a new image is ready.
        framebuffer_head = new_framebuffer_head;

        // Kick off an update of the display.
        spi_lcd_kick();
    }
}

static void spi_lcd_clear()
{
    if (!lcd_triple_buffer) {
        OMV_SPI_LCD_RS_ON();
        OMV_SPI_LCD_CS_LOW();
        HAL_SPI_Transmit(&spi_handle, (uint8_t *) {0x00, 0x28}, 1, HAL_MAX_DELAY); // display off
        OMV_SPI_LCD_CS_HIGH();
        OMV_SPI_LCD_RS_OFF();
    } else {
        // For triple buffering we are never drawing where head or tail (which may instantly update to
        // to be equal to head) is.
        int new_framebuffer_head = (framebuffer_head + 1) % FRAMEBUFFER_COUNT;
        if (new_framebuffer_head == framebuffer_tail) new_framebuffer_head = (new_framebuffer_head + 1) % FRAMEBUFFER_COUNT;

        // Tell the call back FSM that we want to turn the display off.
        spi_tx_cb_state_on[new_framebuffer_head] = false;

        // Update head which means a new image is ready.
        framebuffer_head = new_framebuffer_head;

        // Kick off an update of the display.
        spi_lcd_kick();
    }
}

#ifdef OMV_SPI_LCD_BL_DAC
static DAC_HandleTypeDef lcd_dac_handle = {};
#endif

static void spi_lcd_set_backlight(int intensity)
{
#ifdef OMV_SPI_LCD_BL_DAC
    if ((lcd_intensity < 255) && (255 <= intensity)) {
#else
    if ((lcd_intensity < 1) && (1 <= intensity)) {
#endif
        OMV_SPI_LCD_BL_ON();
        HAL_GPIO_Deinit(OMV_SPI_LCD_BL_PORT, OMV_SPI_LCD_BL_PIN);
    } else if ((0 < lcd_intensity) && (intensity <= 0)) {
        GPIO_InitTypeDef GPIO_InitStructure;
        GPIO_InitStructure.Pull = GPIO_NOPULL;
        GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStructure.Pin = OMV_SPI_LCD_BL_PIN;
        HAL_GPIO_Init(OMV_SPI_LCD_BL_PORT, &GPIO_InitStructure);
        OMV_SPI_LCD_BL_OFF();
    }

#ifdef OMV_SPI_LCD_BL_DAC
    if (((lcd_intensity <= 0) || (255 <= lcd_intensity)) && (0 < intensity) && (intensity < 255)) {
        GPIO_InitTypeDef GPIO_InitStructure;
        GPIO_InitStructure.Pull = GPIO_NOPULL;
        GPIO_InitStructure.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStructure.Pin = OMV_SPI_LCD_BL_PIN;
        HAL_GPIO_Init(OMV_SPI_LCD_BL_PORT, &GPIO_InitStructure);

        lcd_dac_handle.Instance = OMV_SPI_LCD_BL_DAC;

        DAC_ChannelConfTypeDef lcd_dac_channel_handle;
        lcd_dac_channel_handle.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
        lcd_dac_channel_handle.DAC_Trigger = DAC_TRIGGER_NONE;
        lcd_dac_channel_handle.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
        lcd_dac_channel_handle.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_DISABLE;
        lcd_dac_channel_handle.DAC_UserTrimming = DAC_TRIMMING_FACTORY;

        HAL_DAC_Init(&lcd_dac_handle);
        HAL_DAC_ConfigChannel(&lcd_dac_handle, &lcd_dac_channel_handle, OMV_SPI_LCD_BL_DAC_CHANNEL);
        HAL_DAC_Start(&lcd_dac_handle, OMV_SPI_LCD_BL_DAC_CHANNEL);
        HAL_DAC_SetValue(&lcd_dac_handle, OMV_SPI_LCD_BL_DAC_CHANNEL, DAC_ALIGN_8B_R, intensity);
    } else if ((0 < lcd_intensity) && (lcd_intensity < 255) && ((intensity <= 0) || (255 <= intensity))) {
        HAL_DAC_Stop(&lcd_dac_handle, OMV_SPI_LCD_BL_DAC_CHANNEL);
        HAL_DAC_Deinit(&lcd_dac_handle);
    } else if ((0 < lcd_intensity) && (lcd_intensity < 255) && (0 < intensity) && (intensity < 255)) {
        HAL_DAC_SetValue(&lcd_dac_handle, OMV_SPI_LCD_BL_DAC_CHANNEL, DAC_ALIGN_8B_R, intensity);
    }
#endif

    lcd_intensity = intensity;
}
#endif // OMV_SPI_LCD_CONTROLLER

#ifdef OMV_LCD_CONTROLLER
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

static LTDC_HandleTypeDef ltdc_handle = {};
static LTDC_LayerCfgTypeDef ltdc_framebuffer_layers[FRAMEBUFFER_COUNT] = {};

static void ltdc_pll_config_deinit()
{
    __HAL_RCC_PLL3_DISABLE();

    uint32_t tickstart = HAL_GetTick();

    while (__HAL_RCC_GET_FLAG(RCC_FLAG_PLL3RDY)) {
        if ((HAL_GetTick() - tickstart) > PLL_TIMEOUT_VALUE) break;
    }
}

static void ltdc_pll_config_init(int frame_size, int refresh_rate)
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
    fb_alloc_free_till_mark_past_mark_permanent();
}

static void ltdc_config_init(int frame_size, int refresh_rate)
{
    int w = resolution_w_h[frame_size][0];
    int h = resolution_w_h[frame_size][1];

    fb_alloc_mark();

    framebuffer_head = 0;
    framebuffer_tail = 0;

    for (int i = 0; i < FRAMEBUFFER_COUNT; i++) {
        framebuffers[i] = (uint16_t *) fb_alloc0(w * h * sizeof(uint16_t), FB_ALLOC_NO_HINT);
        ltdc_framebuffer_layers[i].WindowX0 = 0;
        ltdc_framebuffer_layers[i].WindowX1 = w;
        ltdc_framebuffer_layers[i].WindowY0 = 0;
        ltdc_framebuffer_layers[i].WindowY1 = h;
        ltdc_framebuffer_layers[i].PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
        ltdc_framebuffer_layers[i].Alpha = 0;
        ltdc_framebuffer_layers[i].Alpha0 = 0;
        ltdc_framebuffer_layers[i].BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
        ltdc_framebuffer_layers[i].BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
        ltdc_framebuffer_layers[i].FBStartAdress = framebuffers[i];
        ltdc_framebuffer_layers[i].ImageWidth = w;
        ltdc_framebuffer_layers[i].ImageHeight = h;
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

    fb_alloc_mark_permanent();
}

// Set the output to be equal to whatever head is and update tail to point to head. We never want
// to draw in any buffer where head or tail is.
void HAL_LTDC_ReloadEventCallback(LTDC_HandleTypeDef *hltdc)
{
    // If Alpha is zero then disable the layer to save bandwidth.
    if (ltdc_framebuffer_layers[framebuffer_head].Alpha) {
        HAL_LTDC_ConfigLayer(&ltdc_handle, &ltdc_framebuffer_layers[framebuffer_head], LTDC_LAYER_1);
    } else {
        __HAL_LTDC_LAYER_DISABLE(&ltdc_handle, LTDC_LAYER_1);
    }

    framebuffer_tail = framebuffer_head;
}

static void ltdc_display(image_t *src_img, int dst_x_start, int dst_y_start, float x_scale, float y_scale, rectangle_t *roi,
                         int rgb_channel, int alpha, const uint16_t *color_palette, const uint8_t *alpha_palette, image_hint_t hint)
{
    image_t dst_img;
    dst_img.w = lcd_width;
    dst_img.h = lcd_height;
    dst_img.bpp = IMAGE_BPP_RGB565;

    // For triple buffering we are never drawing where head or tail (which may instantly update to
    // to be equal to head) is.
    int new_framebuffer_head = (framebuffer_head + 1) % FRAMEBUFFER_COUNT;
    if (new_framebuffer_head == framebuffer_tail) new_framebuffer_head = (new_framebuffer_head + 1) % FRAMEBUFFER_COUNT;
    dst_img.data = framebuffers[new_framebuffer_head];

    // Set default values for the layer to display the whole framebuffer.
    ltdc_framebuffer_layers[new_framebuffer_head].WindowX0 = 0;
    ltdc_framebuffer_layers[new_framebuffer_head].WindowX1 = dst_img.w;
    ltdc_framebuffer_layers[new_framebuffer_head].WindowY0 = 0;
    ltdc_framebuffer_layers[new_framebuffer_head].WindowY1 = dst_img.h;
    ltdc_framebuffer_layers[new_framebuffer_head].Alpha = fast_roundf((alpha * 255) / 256.f);
    ltdc_framebuffer_layers[new_framebuffer_head].FBStartAdress = dst_img.data;
    ltdc_framebuffer_layers[new_framebuffer_head].ImageWidth = dst_img.w;
    ltdc_framebuffer_layers[new_framebuffer_head].ImageHeight = dst_img.h;

    // If alpha was initially black we don't need to do anything. Just display a black layer.
    while (ltdc_framebuffer_layers[new_framebuffer_head].Alpha) {

        if (alpha_palette) {
            int i = 0;
            while ((i < 256) && (!alpha_palette[i])) i++;
            if (i == 256) { // zero alpha palette
                ltdc_framebuffer_layers[new_framebuffer_head].Alpha = 0;
                break;
            }
        }

        // Set alpha to 256 here as we will use the layer alpha to blend the image into the background
        // color of black for free.
        imlib_draw_image(&dst_img, src_img, dst_x_start, dst_y_start, x_scale, y_scale, roi,
                         rgb_channel, 256, color_palette, alpha_palette, hint | IMAGE_HINT_BLACK_BACKGROUND, NULL, NULL);
        // The IMAGE_HINT_BLACK_BACKGROUND flag saves having to clear the area the image is being
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
            ltdc_framebuffer_layers[new_framebuffer_head].Alpha = 0;
            break;
        }

        int src_x_dst_width = src_width_scaled - src_x_start;

        if (src_x_dst_width <= 0) {
            ltdc_framebuffer_layers[new_framebuffer_head].Alpha = 0;
            break;
        }

        // Clamp start y to image bounds.
        int src_y_start = 0;
        if (dst_y_start < 0) {
            src_y_start -= dst_y_start; // this is an add because dst_y_start is negative
            dst_y_start = 0;
        }

        if (dst_y_start >= dst_img.h) {
            ltdc_framebuffer_layers[new_framebuffer_head].Alpha = 0;
            break;
        }

        int src_y_dst_height = src_height_scaled - src_y_start;

        if (src_y_dst_height <= 0) {
            ltdc_framebuffer_layers[new_framebuffer_head].Alpha = 0;
            break;
        }

        // Clamp end x to image bounds.
        int dst_x_end = dst_x_start + src_x_dst_width;
        if (dst_x_end > dst_img.w) dst_x_end = dst_img.w;

        // Clamp end y to image bounds.
        int dst_y_end = dst_y_start + src_y_dst_height;
        if (dst_y_end > dst_img.h) dst_y_end = dst_img.h;

        // Optimize the layer size turning everything black that wasn't drawn on and saving RAM bandwidth.
        ltdc_framebuffer_layers[new_framebuffer_head].WindowX0 = dst_x_start;
        ltdc_framebuffer_layers[new_framebuffer_head].WindowX1 = dst_x_end;
        ltdc_framebuffer_layers[new_framebuffer_head].WindowY0 = dst_y_start;
        ltdc_framebuffer_layers[new_framebuffer_head].WindowY1 = dst_y_end;
        ltdc_framebuffer_layers[new_framebuffer_head].FBStartAdress = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&dst_img, dst_y_start) + dst_x_start;
        ltdc_framebuffer_layers[new_framebuffer_head].ImageWidth = dst_img.w;
        ltdc_framebuffer_layers[new_framebuffer_head].ImageHeight = dst_y_end - dst_y_start;
        break;
    }

    // Update head which means a new image is ready.
    framebuffer_head = new_framebuffer_head;

    // Kick off an update of the display pointer.
    HAL_LTDC_Reload(&ltdc_handle, LTDC_RELOAD_VERTICAL_BLANKING);
}

static void ltdc_clear()
{
    // For triple buffering we are never drawing where head or tail (which may instantly update to
    // to be equal to head) is.
    int new_framebuffer_head = (framebuffer_head + 1) % FRAMEBUFFER_COUNT;
    if (new_framebuffer_head == framebuffer_tail) new_framebuffer_head = (new_framebuffer_head + 1) % FRAMEBUFFER_COUNT;

    // Set default values for the layer to display the whole framebuffer.
    ltdc_framebuffer_layers[new_framebuffer_head].WindowX0 = 0;
    ltdc_framebuffer_layers[new_framebuffer_head].WindowX1 = lcd_width;
    ltdc_framebuffer_layers[new_framebuffer_head].WindowY0 = 0;
    ltdc_framebuffer_layers[new_framebuffer_head].WindowY1 = lcd_height;
    ltdc_framebuffer_layers[new_framebuffer_head].Alpha = 0;
    ltdc_framebuffer_layers[new_framebuffer_head].FBStartAdress = framebuffers[new_framebuffer_head];
    ltdc_framebuffer_layers[new_framebuffer_head].ImageWidth = lcd_width;
    ltdc_framebuffer_layers[new_framebuffer_head].ImageHeight = lcd_height;

    // Update head which means a new image is ready.
    framebuffer_head = new_framebuffer_head;

    // Kick off an update of the display pointer.
    HAL_LTDC_Reload(&ltdc_handle, LTDC_RELOAD_VERTICAL_BLANKING);
}

#ifdef OMV_LCD_BL_TIM
static TIM_HandleTypeDef lcd_tim_handle = {};
#endif

static void ltdc_set_backlight(int intensity)
{
#ifdef OMV_LCD_BL_TIM
    if ((lcd_intensity < 255) && (255 <= intensity)) {
#else
    if ((lcd_intensity < 1) && (1 <= intensity)) {
#endif
        GPIO_InitTypeDef GPIO_InitStructure;
        GPIO_InitStructure.Pull = GPIO_NOPULL;
        GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStructure.Pin = OMV_LCD_BL_PIN;
        HAL_GPIO_Init(OMV_LCD_BL_PORT, &GPIO_InitStructure);
        OMV_LCD_BL_ON();
    } else if ((0 < lcd_intensity) && (intensity <= 0)) {
        OMV_LCD_BL_OFF();
        HAL_GPIO_Deinit(OMV_LCD_BL_PORT, OMV_LCD_BL_PIN);
    }

#ifdef OMV_LCD_BL_TIM
    int tclk = OMV_LCD_BL_TIM_PCLK_FREQ() * 2;
    int period = (tclk / OMV_LCD_BL_FREQ) - 1;

    if (((lcd_intensity <= 0) || (255 <= lcd_intensity)) && (0 < intensity) && (intensity < 255)) {
        GPIO_InitTypeDef GPIO_InitStructure;
        GPIO_InitStructure.Pull = GPIO_NOPULL;
        GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStructure.Alternate = OMV_LCD_BL_ALT;
        GPIO_InitStructure.Pin = OMV_LCD_BL_PIN;
        HAL_GPIO_Init(OMV_LCD_BL_PORT, &GPIO_InitStructure);

        lcd_tim_handle.Instance = OMV_LCD_BL_TIM;
        lcd_tim_handle.Init.Period = period;
        lcd_tim_handle.Init.Prescaler = TIM_ETRPRESCALER_DIV1;
        lcd_tim_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
        lcd_tim_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

        TIM_OC_InitTypeDef lcd_tim_oc_handle;
        lcd_tim_oc_handle.Pulse = (period * intensity) / 255;
        lcd_tim_oc_handle.OCMode = TIM_OCMODE_PWM1;
        lcd_tim_oc_handle.OCPolarity = TIM_OCPOLARITY_HIGH;
        lcd_tim_oc_handle.OCFastMode = TIM_OCFAST_DISABLE;
        lcd_tim_oc_handle.OCIdleState = TIM_OCIDLESTATE_RESET;

        HAL_TIM_PWM_Init(&lcd_tim_handle);
        HAL_TIM_PWM_ConfigChannel(&lcd_tim_handle, &lcd_tim_oc_handle, OMV_LCD_BL_TIM_CHANNEL);
        HAL_TIM_PWM_Start(&lcd_tim_handle, OMV_LCD_BL_TIM_CHANNEL);
    } else if ((0 < lcd_intensity) && (lcd_intensity < 255) && ((intensity <= 0) || (255 <= intensity))) {
        HAL_TIM_PWM_Stop(&lcd_tim_handle, OMV_LCD_BL_TIM_CHANNEL);
        HAL_TIM_PWM_Deinit(&lcd_tim_handle);
    } else if ((0 < lcd_intensity) && (lcd_intensity < 255) && (0 < intensity) && (intensity < 255)) {
        __HAL_TIM_SET_COMPARE(&lcd_tim_handle, OMV_LCD_BL_TIM_CHANNEL, (period * intensity) / 255);
    }
#endif

    lcd_intensity = intensity;
}
#endif // OMV_LCD_CONTROLLER

STATIC mp_obj_t py_lcd_deinit()
{
    switch (lcd_type) {
        #ifdef OMV_SPI_LCD_CONTROLLER
        case LCD_SHIELD: {
            spi_config_deinit();
            spi_lcd_set_backlight(255); // back to default state
            break;
        }
        #endif
        #ifdef OMV_LCD_CONTROLLER
        case LCD_DISPLAY: {
            ltdc_config_deinit();
            ltdc_set_backlight(0); // back to default state
            break;
        }
        #endif
        default: {
            break;
        }
    }

    lcd_width = 0;
    lcd_height = 0;
    lcd_type = LCD_NONE;
    lcd_triple_buffer = false;
    lcd_bgr = false;
    lcd_resolution = 0;
    lcd_refresh = 0;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_deinit_obj, py_lcd_deinit);

STATIC mp_obj_t py_lcd_init(uint n_args, const mp_obj_t *args, mp_map_t *kw_args)
{
    py_lcd_deinit();

    switch (py_helper_keyword_int(n_args, args, 0, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_type), LCD_SHIELD)) {
        #ifdef OMV_SPI_LCD_CONTROLLER
        case LCD_SHIELD: {
            int w = py_helper_keyword_int(n_args, args, 1, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_width), 128);
            if ((w <= 0) || (32767 < w) || (w % 2)) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Invalid Width!"));
            int h = py_helper_keyword_int(n_args, args, 2, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_height), 160);
            if ((h <= 0) || (32767 < h) || (h % 2)) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Invalid Height!"));
            int refresh_rate = py_helper_keyword_int(n_args, args, 3, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_refresh), 60);
            if ((refresh_rate < 30) || (120 < refresh_rate)) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Invalid Refresh Rate!"));
            bool triple_buffer = py_helper_keyword_int(n_args, args, 4, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_triple_buffer), false);
            bool bgr = py_helper_keyword_int(n_args, args, 5, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_bgr), false);
            spi_config_init(w, h, refresh_rate, triple_buffer, bgr);
            spi_lcd_set_backlight(255); // to on state
            lcd_width = w;
            lcd_height = h;
            lcd_type = LCD_SHIELD;
            lcd_triple_buffer = triple_buffer;
            lcd_bgr = bgr;
            lcd_resolution = 0;
            lcd_refresh = refresh_rate;
            break;
        }
        #endif
        #ifdef OMV_LCD_CONTROLLER
        case LCD_DISPLAY: {
            int frame_size = py_helper_keyword_int(n_args, args, 1, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_framesize), LCD_DISPLAY_WVGA);
            if ((frame_size < 0) || (LCD_DISPLAY_MAX <= frame_size)) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Invalid Frame Size!"));
            int refresh_rate = py_helper_keyword_int(n_args, args, 2, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_refresh), 60);
            if ((refresh_rate < 30) || (120 < refresh_rate)) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Invalid Refresh Rate!"));
            ltdc_config_init(frame_size, refresh_rate);
            ltdc_set_backlight(255); // to on state
            lcd_width = resolution_w_h[frame_size][0];
            lcd_height = resolution_w_h[frame_size][1];
            lcd_type = LCD_DISPLAY;
            lcd_triple_buffer = true;
            lcd_bgr = false;
            lcd_resolution = frame_size;
            lcd_refresh = refresh_rate;
            break;
        }
        #endif
        default: {
            break;
        }
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(py_lcd_init_obj, 0, py_lcd_init);

STATIC mp_obj_t py_lcd_width()
{
    if (lcd_type == LCD_NONE) return mp_const_none;
    return mp_obj_new_int(lcd_width);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_width_obj, py_lcd_width);

STATIC mp_obj_t py_lcd_height()
{
    if (lcd_type == LCD_NONE) return mp_const_none;
    return mp_obj_new_int(lcd_height);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_height_obj, py_lcd_height);

STATIC mp_obj_t py_lcd_type()
{
    if (lcd_type == LCD_NONE) return mp_const_none;
    return mp_obj_new_int(lcd_type);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_type_obj, py_lcd_type);

STATIC mp_obj_t py_lcd_triple_buffer()
{
    if (lcd_type == LCD_NONE) return mp_const_none;
    return mp_obj_new_int(lcd_triple_buffer);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_triple_buffer_obj, py_lcd_triple_buffer);

STATIC mp_obj_t py_lcd_bgr()
{
    if (lcd_type == LCD_NONE) return mp_const_none;
    return mp_obj_new_int(lcd_bgr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_bgr_obj, py_lcd_bgr);

STATIC mp_obj_t py_lcd_framesize()
{
    if (lcd_type != LCD_DISPLAY) return mp_const_none;
    return mp_obj_new_int(lcd_resolution);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_framesize_obj, py_lcd_framesize);

STATIC mp_obj_t py_lcd_refresh()
{
    if (lcd_type == LCD_NONE) return mp_const_none;
    return mp_obj_new_int(lcd_refresh);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_refresh_obj, py_lcd_refresh);

STATIC mp_obj_t py_lcd_set_backlight(mp_obj_t intensity_obj)
{
    int intensity = mp_obj_get_int(intensity_obj);
    if ((intensity < 0) || (255 < intensity)) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "0 <= intensity <= 255!"));

    switch (lcd_type) {
        #ifdef OMV_SPI_LCD_CONTROLLER
        case LCD_SHIELD: {
            spi_lcd_set_backlight(intensity);
            break;
        }
        #endif
        #ifdef OMV_LCD_CONTROLLER
        case LCD_DISPLAY: {
            ltdc_set_backlight(intensity);
            break;
        }
        #endif
        default: {
            break;
        }
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_lcd_set_backlight_obj, py_lcd_set_backlight);

STATIC mp_obj_t py_lcd_get_backlight()
{
    if (lcd_type == LCD_NONE) return mp_const_none;
    return mp_obj_new_int(lcd_intensity);
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

    switch (lcd_type) {
        #ifdef OMV_SPI_LCD_CONTROLLER
        case LCD_SHIELD: {
            fb_alloc_mark();
            spi_lcd_display(arg_img, arg_x_off, arg_y_off, arg_x_scale, arg_y_scale, &arg_roi,
                            arg_rgb_channel, arg_alpha, color_palette, alpha_palette, hint);
            fb_alloc_free_till_mark();
            break;
        }
        #endif
        #ifdef OMV_LCD_CONTROLLER
        case LCD_DISPLAY: {
            fb_alloc_mark();
            ltdc_display(arg_img, arg_x_off, arg_y_off, arg_x_scale, arg_y_scale, &arg_roi,
                         arg_rgb_channel, arg_alpha, color_palette, alpha_palette, hint);
            fb_alloc_free_till_mark();
            break;
        }
        #endif
        default: {
            break;
        }
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(py_lcd_display_obj, 1, py_lcd_display);

STATIC mp_obj_t py_lcd_clear()
{
    switch (lcd_type) {
        #ifdef OMV_SPI_LCD_CONTROLLER
        case LCD_SHIELD: {
            spi_lcd_clear();
            break;
        }
        #endif
        #ifdef OMV_LCD_CONTROLLER
        case LCD_DISPLAY: {
            ltdc_clear();
            break;
        }
        #endif
        default: {
            break;
        }
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_lcd_clear_obj, py_lcd_clear);

STATIC const mp_rom_map_elem_t globals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_OBJ_NEW_QSTR(MP_QSTR_lcd)          },
    { MP_ROM_QSTR(MP_QSTR_LCD_NONE),        MP_OBJ_NEW_QSTR(LCD_NONE)             },
    { MP_ROM_QSTR(MP_QSTR_LCD_SHIELD),      MP_OBJ_NEW_QSTR(LCD_SHIELD)           },
    { MP_ROM_QSTR(MP_QSTR_LCD_DISPLAY),     MP_OBJ_NEW_QSTR(LCD_DISPLAY)          },
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
    { MP_ROM_QSTR(MP_QSTR_triple_buffer),   MP_ROM_PTR(&py_lcd_triple_buffer_obj) },
    { MP_ROM_QSTR(MP_QSTR_bgr),             MP_ROM_PTR(&py_lcd_bgr_obj)           },
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
