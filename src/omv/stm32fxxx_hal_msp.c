/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2013-2019 Ibrahim Abdelkader <iabdalkader@openmv.io>
 * Copyright (c) 2013-2019 Kwabena W. Agyeman <kwagyeman@openmv.io>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * HAL MSP.
 */
#include STM32_HAL_H
#include "omv_boardconfig.h"

/* GPIO struct */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} gpio_t;

/* DCMI GPIOs */
static const gpio_t dcmi_pins[] = {
    {DCMI_D0_PORT, DCMI_D0_PIN},
    {DCMI_D1_PORT, DCMI_D1_PIN},
    {DCMI_D2_PORT, DCMI_D2_PIN},
    {DCMI_D3_PORT, DCMI_D3_PIN},
    {DCMI_D4_PORT, DCMI_D4_PIN},
    {DCMI_D5_PORT, DCMI_D5_PIN},
    {DCMI_D6_PORT, DCMI_D6_PIN},
    {DCMI_D7_PORT, DCMI_D7_PIN},
    {DCMI_HSYNC_PORT, DCMI_HSYNC_PIN},
    {DCMI_VSYNC_PORT, DCMI_VSYNC_PIN},
    {DCMI_PXCLK_PORT, DCMI_PXCLK_PIN},
};

#define NUM_DCMI_PINS   (sizeof(dcmi_pins)/sizeof(dcmi_pins[0]))

void SystemClock_Config(void);

void HAL_MspInit(void)
{
    /* Set the system clock */
    SystemClock_Config();

    #if defined(OMV_DMA_REGION_BASE)
    __DSB(); __ISB();
    HAL_MPU_Disable();

    /* Configure the MPU attributes to disable caching DMA buffers */
    MPU_Region_InitTypeDef MPU_InitStruct;
    MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress      = OMV_DMA_REGION_BASE;
    MPU_InitStruct.Size             = OMV_DMA_REGION_SIZE;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.Number           = MPU_REGION_NUMBER15;
    MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL1;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    /* Enable the MPU */
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
    __DSB(); __ISB();
    #endif

    /* Enable I/D cache */
    #if defined(MCU_SERIES_F7) ||\
        defined(MCU_SERIES_H7)
    if (SCB->CCR & (uint32_t)SCB_CCR_IC_Msk) {
        /* Disable and Invalidate I-Cache */
        SCB_DisableICache();
        SCB_InvalidateICache();
    }

    if (SCB->CCR & (uint32_t)SCB_CCR_DC_Msk) {
        /* Disable, Clean and Invalidate D-Cache */
        SCB_DisableDCache();
        SCB_CleanInvalidateDCache();
    }

    // Enable the CPU Caches
    SCB_EnableICache();
    SCB_EnableDCache();
    #endif

    /* Config Systick */
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);

    /* Enable GPIO clocks */
    __GPIOA_CLK_ENABLE();
    __GPIOB_CLK_ENABLE();
    __GPIOC_CLK_ENABLE();
    __GPIOD_CLK_ENABLE();
    __GPIOE_CLK_ENABLE();
    #ifdef OMV_ENABLE_GPIO_BANK_F
    __GPIOF_CLK_ENABLE();
    #endif
    #ifdef OMV_ENABLE_GPIO_BANK_G
    __GPIOG_CLK_ENABLE();
    #endif
    #ifdef OMV_ENABLE_GPIO_BANK_H
    __GPIOH_CLK_ENABLE();
    #endif
    #ifdef OMV_ENABLE_GPIO_BANK_I
    __GPIOI_CLK_ENABLE();
    #endif
    #ifdef OMV_ENABLE_GPIO_BANK_J
    __GPIOJ_CLK_ENABLE();
    #endif
    #ifdef OMV_ENABLE_GPIO_BANK_K
    __GPIOK_CLK_ENABLE();
    #endif

    #if defined (OMV_HARDWARE_JPEG)
    /* Enable JPEG clock */
    __HAL_RCC_JPEG_CLK_ENABLE();
    #endif

    /* Enable DMA clocks */
    __DMA1_CLK_ENABLE();
    __DMA2_CLK_ENABLE();

    #if defined(MCU_SERIES_H7)
    // MDMA clock
    __HAL_RCC_MDMA_CLK_ENABLE();
    #endif

    #if defined(OMV_HARDWARE_JPEG)
    // Enable JPEG clock
    __HAL_RCC_JPGDECEN_CLK_ENABLE();
    #endif

    /* Configure DCMI GPIO */
    GPIO_InitTypeDef  GPIO_InitStructure;
    GPIO_InitStructure.Pull  = GPIO_PULLDOWN;
    GPIO_InitStructure.Speed = GPIO_SPEED_LOW;
    GPIO_InitStructure.Mode  = GPIO_MODE_OUTPUT_PP;

    #if defined(DCMI_RESET_PIN)
    GPIO_InitStructure.Pin = DCMI_RESET_PIN;
    HAL_GPIO_Init(DCMI_RESET_PORT, &GPIO_InitStructure);
    #endif

    #if defined(DCMI_PWDN_PIN)
    GPIO_InitStructure.Pin = DCMI_PWDN_PIN;
    HAL_GPIO_Init(DCMI_PWDN_PORT, &GPIO_InitStructure);
    #endif

    #if defined(DCMI_FSYNC_PIN)
    GPIO_InitStructure.Pin = DCMI_FSYNC_PIN;
    HAL_GPIO_Init(DCMI_FSYNC_PORT, &GPIO_InitStructure);
    #endif
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == SCCB_I2C) {
        /* Enable I2C clock */
        SCCB_CLK_ENABLE();

        /* Configure SCCB GPIOs */
        GPIO_InitTypeDef GPIO_InitStructure;
        GPIO_InitStructure.Pull      = GPIO_NOPULL;
        GPIO_InitStructure.Speed     = GPIO_SPEED_LOW;
        GPIO_InitStructure.Mode      = GPIO_MODE_AF_OD;
        GPIO_InitStructure.Alternate = SCCB_AF;

        GPIO_InitStructure.Pin = SCCB_SCL_PIN;
        HAL_GPIO_Init(SCCB_PORT, &GPIO_InitStructure);

        GPIO_InitStructure.Pin = SCCB_SDA_PIN;
        HAL_GPIO_Init(SCCB_PORT, &GPIO_InitStructure);
    } else if (hi2c->Instance == FIR_I2C) {
        /* Enable I2C clock */
        FIR_I2C_CLK_ENABLE();

        /* Configure FIR I2C GPIOs */
        GPIO_InitTypeDef GPIO_InitStructure;
        GPIO_InitStructure.Pull      = GPIO_NOPULL;
        GPIO_InitStructure.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStructure.Mode      = GPIO_MODE_AF_OD;
        GPIO_InitStructure.Alternate = FIR_I2C_AF;

        GPIO_InitStructure.Pin = FIR_I2C_SCL_PIN;
        HAL_GPIO_Init(FIR_I2C_PORT, &GPIO_InitStructure);

        GPIO_InitStructure.Pin = FIR_I2C_SDA_PIN;
        HAL_GPIO_Init(FIR_I2C_PORT, &GPIO_InitStructure);
    }

}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == SCCB_I2C) {
        SCCB_FORCE_RESET();
        SCCB_RELEASE_RESET();
        SCCB_CLK_DISABLE();
    } else if (hi2c->Instance == FIR_I2C) {
        FIR_I2C_FORCE_RESET();
        FIR_I2C_RELEASE_RESET();
        FIR_I2C_CLK_DISABLE();
    }
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    #if (OMV_XCLK_SOURCE == OMV_XCLK_TIM)
    if (htim->Instance == DCMI_TIM) {
        /* Enable DCMI timer clock */
        DCMI_TIM_CLK_ENABLE();

        /* Timer GPIO configuration */
        GPIO_InitTypeDef  GPIO_InitStructure;
        GPIO_InitStructure.Pin       = DCMI_TIM_PIN;
        GPIO_InitStructure.Pull      = GPIO_NOPULL;
        GPIO_InitStructure.Speed     = GPIO_SPEED_HIGH;
        GPIO_InitStructure.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStructure.Alternate = DCMI_TIM_AF;
        HAL_GPIO_Init(DCMI_TIM_PORT, &GPIO_InitStructure);
    }
    #endif // (OMV_XCLK_SOURCE == OMV_XCLK_TIM)
}

void HAL_DCMI_MspInit(DCMI_HandleTypeDef* hdcmi)
{
    /* DCMI clock enable */
    __DCMI_CLK_ENABLE();

    /* DCMI GPIOs configuration */
    GPIO_InitTypeDef  GPIO_InitStructure;
    GPIO_InitStructure.Pull      = GPIO_PULLDOWN;
    GPIO_InitStructure.Speed     = GPIO_SPEED_HIGH;
    GPIO_InitStructure.Alternate = GPIO_AF13_DCMI;

    /* Enable VSYNC EXTI */
    GPIO_InitStructure.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStructure.Pin  = DCMI_VSYNC_PIN;
    HAL_GPIO_Init(DCMI_VSYNC_PORT, &GPIO_InitStructure);

    /* Configure DCMI pins */
    GPIO_InitStructure.Mode      = GPIO_MODE_AF_PP;
    for (int i=0; i<NUM_DCMI_PINS; i++) {
        GPIO_InitStructure.Pin = dcmi_pins[i].pin;
        HAL_GPIO_Init(dcmi_pins[i].port, &GPIO_InitStructure);
    }
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    #if defined(IMU_SPI)
    if (hspi->Instance == IMU_SPI) {
        IMU_SPI_CLK_ENABLE();

        GPIO_InitTypeDef GPIO_InitStructure;
        GPIO_InitStructure.Pull      = GPIO_PULLUP;
        GPIO_InitStructure.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStructure.Alternate = IMU_SPI_AF;
        GPIO_InitStructure.Speed     = GPIO_SPEED_FREQ_LOW;

        GPIO_InitStructure.Pin       = IMU_SPI_SCLK_PIN;
        HAL_GPIO_Init(IMU_SPI_SCLK_PORT, &GPIO_InitStructure);

        GPIO_InitStructure.Pin       = IMU_SPI_MISO_PIN;
        HAL_GPIO_Init(IMU_SPI_MISO_PORT, &GPIO_InitStructure);

        GPIO_InitStructure.Pin       = IMU_SPI_MOSI_PIN;
        HAL_GPIO_Init(IMU_SPI_MOSI_PORT, &GPIO_InitStructure);

        GPIO_InitStructure.Mode      = GPIO_MODE_OUTPUT_PP;

        GPIO_InitStructure.Pin       = IMU_SPI_SSEL_PIN;
        HAL_GPIO_Init(IMU_SPI_SSEL_PORT, &GPIO_InitStructure);
    }
    #endif

    #if defined(LEPTON_SPI)
    if (hspi->Instance == LEPTON_SPI) {
        LEPTON_SPI_CLK_ENABLE();

        GPIO_InitTypeDef GPIO_InitStructure;
        GPIO_InitStructure.Pull      = GPIO_PULLUP;
        GPIO_InitStructure.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStructure.Speed     = GPIO_SPEED_FREQ_LOW;

        GPIO_InitStructure.Alternate = LEPTON_SPI_SCLK_AF;
        GPIO_InitStructure.Pin       = LEPTON_SPI_SCLK_PIN;
        HAL_GPIO_Init(LEPTON_SPI_SCLK_PORT, &GPIO_InitStructure);

        GPIO_InitStructure.Alternate = LEPTON_SPI_MISO_AF;
        GPIO_InitStructure.Pin       = LEPTON_SPI_MISO_PIN;
        HAL_GPIO_Init(LEPTON_SPI_MISO_PORT, &GPIO_InitStructure);

        GPIO_InitStructure.Alternate = LEPTON_SPI_MOSI_AF;
        GPIO_InitStructure.Pin       = LEPTON_SPI_MOSI_PIN;
        HAL_GPIO_Init(LEPTON_SPI_MOSI_PORT, &GPIO_InitStructure);

        GPIO_InitStructure.Alternate = LEPTON_SPI_SSEL_AF;
        GPIO_InitStructure.Pin       = LEPTON_SPI_SSEL_PIN;
        HAL_GPIO_Init(LEPTON_SPI_SSEL_PORT, &GPIO_InitStructure);
    }
    #endif
}

typedef struct {
    GPIO_TypeDef *port;
    uint16_t af, pin;
} ltdc_gpio_t;

static const ltdc_gpio_t ltdc_pins[] = {
    {OMV_LCD_R0_PORT, OMV_LCD_R0_ALT, OMV_LCD_R0_PIN},
    {OMV_LCD_R1_PORT, OMV_LCD_R1_ALT, OMV_LCD_R1_PIN},
    {OMV_LCD_R2_PORT, OMV_LCD_R2_ALT, OMV_LCD_R2_PIN},
    {OMV_LCD_R3_PORT, OMV_LCD_R3_ALT, OMV_LCD_R3_PIN},
    {OMV_LCD_R4_PORT, OMV_LCD_R4_ALT, OMV_LCD_R4_PIN},
    {OMV_LCD_R5_PORT, OMV_LCD_R5_ALT, OMV_LCD_R5_PIN},
    {OMV_LCD_R6_PORT, OMV_LCD_R6_ALT, OMV_LCD_R6_PIN},
    {OMV_LCD_R7_PORT, OMV_LCD_R7_ALT, OMV_LCD_R7_PIN},
    {OMV_LCD_G0_PORT, OMV_LCD_G0_ALT, OMV_LCD_G0_PIN},
    {OMV_LCD_G1_PORT, OMV_LCD_G1_ALT, OMV_LCD_G1_PIN},
    {OMV_LCD_G2_PORT, OMV_LCD_G2_ALT, OMV_LCD_G2_PIN},
    {OMV_LCD_G3_PORT, OMV_LCD_G3_ALT, OMV_LCD_G3_PIN},
    {OMV_LCD_G4_PORT, OMV_LCD_G4_ALT, OMV_LCD_G4_PIN},
    {OMV_LCD_G5_PORT, OMV_LCD_G5_ALT, OMV_LCD_G5_PIN},
    {OMV_LCD_G6_PORT, OMV_LCD_G6_ALT, OMV_LCD_G6_PIN},
    {OMV_LCD_G7_PORT, OMV_LCD_G7_ALT, OMV_LCD_G7_PIN},
    {OMV_LCD_B0_PORT, OMV_LCD_B0_ALT, OMV_LCD_B0_PIN},
    {OMV_LCD_B1_PORT, OMV_LCD_B1_ALT, OMV_LCD_B1_PIN},
    {OMV_LCD_B2_PORT, OMV_LCD_B2_ALT, OMV_LCD_B2_PIN},
    {OMV_LCD_B3_PORT, OMV_LCD_B3_ALT, OMV_LCD_B3_PIN},
    {OMV_LCD_B4_PORT, OMV_LCD_B4_ALT, OMV_LCD_B4_PIN},
    {OMV_LCD_B5_PORT, OMV_LCD_B5_ALT, OMV_LCD_B5_PIN},
    {OMV_LCD_B6_PORT, OMV_LCD_B6_ALT, OMV_LCD_B6_PIN},
    {OMV_LCD_B7_PORT, OMV_LCD_B7_ALT, OMV_LCD_B7_PIN},
    {OMV_LCD_CLK_PORT, OMV_LCD_CLK_ALT, OMV_LCD_CLK_PIN},
    {OMV_LCD_DE_PORT, OMV_LCD_DE_ALT, OMV_LCD_DE_PIN},
    {OMV_LCD_HSYNC_PORT, OMV_LCD_HSYNC_ALT, OMV_LCD_HSYNC_PIN},
    {OMV_LCD_VSYNC_PORT, OMV_LCD_VSYNC_ALT, OMV_LCD_VSYNC_PIN},
};

void HAL_LTDC_MspInit(LTDC_HandleTypeDef *hltdc)
{
    #if defined(OMV_DSI_CONTROLLER)
    if (htldc->Instance = OMV_LCD_CONTROLLER) {
        OMV_LCD_CLK_ENABLE();
    }
    #elif defined(OMV_LCD_CONTROLLER)
    if (htldc->Instance = OMV_LCD_CONTROLLER) {
        OMV_LCD_CLK_ENABLE();

        GPIO_InitTypeDef GPIO_InitStructure;
        GPIO_InitStructure.Pull      = GPIO_NOPULL;
        GPIO_InitStructure.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStructure.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;

        for (int i = 0, ii = sizeof(ltdc_pins) / sizeof(ltdc_gpio_t); i < ii; i++) {
            GPIO_InitStructure.Alternate = ltdc_pins[i].af;
            GPIO_InitStructure.Pin       = ltdc_pins[i].pin;
            HAL_GPIO_Init(ltdc_pins[i].port, &GPIO_InitStructure);
        }

        GPIO_InitStructure.Mode      = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStructure.Speed     = GPIO_SPEED_FREQ_LOW;

        #if defined(OMV_LCD_DISP_PIN)
        GPIO_InitStructure.Pin       = OMV_LCD_DISP_PIN;
        HAL_GPIO_Init(OMV_LCD_DISP_PORT, &GPIO_InitStructure);
        OMV_LCD_DISP_ON();
        #endif

        #if defined(OMV_LCD_BL_PIN)
        GPIO_InitStructure.Pin       = OMV_LCD_BL_PIN;
        HAL_GPIO_Init(OMV_LCD_BL_PORT, &GPIO_InitStructure);
        OMV_LCD_BL_ON();
        #endif

    #endif
}

void HAL_LTDC_MspDeInit(LTDC_HandleTypeDef *hltdc)
{
    #if defined(OMV_DSI_CONTROLLER)
    if (htldc->Instance = OMV_LCD_CONTROLLER) {
        OMV_LCD_FORCE_RESET();
        OMV_LCD_RELEASE_RESET();
        OMV_LCD_CLK_DISABLE();
    }
    #elif defined(OMV_LCD_CONTROLLER)
    if (htldc->Instance = OMV_LCD_CONTROLLER) {
        OMV_LCD_FORCE_RESET();
        OMV_LCD_RELEASE_RESET();
        OMV_LCD_CLK_DISABLE();

        for (int i = 0, ii = sizeof(ltdc_pins) / sizeof(ltdc_gpio_t); i < ii; i++) {
            HAL_GPIO_Deinit(ltdc_pins[i].port, ltdc_pins[i].pin);
        }

        #if defined(OMV_LCD_DISP_PIN)
        OMV_LCD_DISP_OFF();
        HAL_GPIO_Deinit(OMV_LCD_DISP_PORT, OMV_LCD_DISP_PIN);
        #endif

        #if defined(OMV_LCD_BL_PIN)
        OMV_LCD_BL_OFF();
        HAL_GPIO_Deinit(OMV_LCD_BL_PORT, OMV_LCD_BL_PIN);
        #endif
    }
    #endif
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    #if defined(OMV_LCD_BL_TIM)
    if (htim->Instance == OMV_LCD_BL_TIM) {
        OMV_LCD_BL_TIM_CLK_ENABLE();
    }
    #endif
}

void HAL_TIM_PWM_MspDeInit(TIM_HandleTypeDef *htim)
{
    #if defined(OMV_LCD_BL_TIM)
    if (htim->Instance == OMV_LCD_BL_TIM) {
        OMV_LCD_BL_TIM_FORCE_RESET();
        OMV_LCD_BL_TIM_RELEASE_RESET();
        OMV_LCD_BL_TIM_CLK_DISABLE();
    }
    #endif
}

void HAL_MspDeInit(void)
{

}
