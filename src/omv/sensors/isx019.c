/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2013-2023 Ibrahim Abdelkader <iabdalkader@openmv.io>
 * Copyright (c) 2013-2023 Kwabena W. Agyeman <kwagyeman@openmv.io>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * ISX019 driver.
 */
#include "omv_boardconfig.h"
#if (OMV_ENABLE_ISX019 == 1)

#include <stdlib.h>

#include "cambus.h"
#include "sensor.h"
#include "py/mphal.h"

#define ACTIVE_SENSOR_WIDTH     (1280)
#define ACTIVE_SENSOR_HEIGHT    (960)

// https://github.com/sonydevworld/spresense-nuttx/blob/2dd207208a08b43d837f2cc22f0d048dcbabc420/drivers/video/isx019.c

/****************************************************************************
 * drivers/video/isx019.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "isx019_reg.h"
#include "isx019_range.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Wait time on power on sequence. */

#define TRANSITION_TIME_TO_STARTUP   (130 * 1000) /* unit : usec */
#define TRANSITION_TIME_TO_STREAMING (40 * 1000)  /* unit : usec */
#define DELAY_TIME_JPEGDQT_SWAP      (35 * 1000)  /* unit : usec */

/* Index of array for drive mode setting */

#define INDEX_SENS     (0)
#define INDEX_POST     (1)
#define INDEX_SENSPOST (2)
#define INDEX_IO       (3)

/* Timer value for power on control */

#define ISX019_ACCESSIBLE_WAIT_SEC    (0)
#define ISX019_ACCESSIBLE_WAIT_USEC   (200000)
#define FPGA_ACCESSIBLE_WAIT_SEC      (1)
#define FPGA_ACCESSIBLE_WAIT_USEC     (0)

/* Array size of DQT setting for JPEG quality */

#define JPEG_DQT_ARRAY_SIZE (64)

/* ISX019 standard master clock */

#define ISX019_STANDARD_MASTER_CLOCK (27000000)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct isx019_fpga_jpg_quality_s
{
    /* JPEG quality */

    int quality;

    /* DQT header setting for y component */

    uint8_t y_head[JPEG_DQT_ARRAY_SIZE];

    /* DQT calculation data for y component */

    uint8_t y_calc[JPEG_DQT_ARRAY_SIZE];

    /* DQT header setting for c component */

    uint8_t c_head[JPEG_DQT_ARRAY_SIZE];

    /* DQT calculation data for c component */

    uint8_t c_calc[JPEG_DQT_ARRAY_SIZE];
};

typedef struct isx019_fpga_jpg_quality_s isx019_fpga_jpg_quality_t;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int send_read_cmd(uint8_t cat,
                         uint16_t addr,
                         uint8_t size);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const isx019_fpga_jpg_quality_t g_isx019_jpg_quality[] =
{
  {
    10,
      {
         21,  16,  16,  26,  18,  26,  43,  21,
         21,  43,  43,  43,  32,  43,  43,  43,
         43,  43,  43,  43,  43,  64,  43,  43,
         43,  43,  43,  64,  64,  64,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
      },
      {
          3,   4, 133, 131, 131, 131,   1,   1,
          4, 135,   3, 131, 131, 131,   1,   1,
        133,   3,   2, 131, 131,   1,   1,   1,
        131, 131, 131, 131,   1,   1,   1,   1,
        131, 131, 131,   1,   1,   1,   1,   1,
        131, 131,   1,   1,   1,   1,   1,   1,
          1,   1,   1,   1,   1,   1,   1,   1,
          1,   1,   1,   1,   1,   1,   1,   1,
      },
      {
         21,  26,  26,  32,  26,  32,  43,  26,
         26,  43,  64,  43,  32,  43,  64,  64,
         64,  43,  43,  64,  64,  64,  64,  64,
         43,  64,  64,  64,  64,  64,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
      },
      {
          3, 133,   2, 131,   1,   1,   1,   1,
        133, 133, 133, 131,   1,   1,   1,   1,
          2, 133,   2, 131,   1,   1,   1,   1,
        131, 131, 131, 131,   1,   1,   1,   1,
          1,   1,   1,   1,   1,   1,   1,   1,
          1,   1,   1,   1,   1,   1,   1,   1,
          1,   1,   1,   1,   1,   1,   1,   1,
          1,   1,   1,   1,   1,   1,   1,   1,
      }
  },
  {
    20,
      {
         18,  14,  14,  14,  16,  14,  21,  16,
         16,  21,  32,  21,  16,  21,  32,  32,
         26,  21,  21,  26,  32,  32,  26,  26,
         26,  26,  26,  32,  43,  32,  32,  32,
         32,  32,  32,  43,  43,  43,  43,  43,
         43,  43,  43,  64,  64,  64,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
      },
      {
        135, 137, 137,   3,   2,   2,   2, 131,
        137,   4,   4,   3, 133, 133,   2, 131,
        137,   4,   4,   3, 133,   2, 131,   1,
          3,   3,   3, 133,   2, 131,   1,   1,
          2, 133, 133,   2, 131,   1,   1,   1,
          2, 133,   2, 131,   1,   1,   1,   1,
          2,   2, 131,   1,   1,   1,   1,   1,
        131, 131,   1,   1,   1,   1,   1,   1,
      },
      {
         21,  21,  21,  21,  26,  21,  26,  21,
         21,  26,  26,  21,  26,  21,  26,  32,
         26,  26,  26,  26,  32,  43,  32,  32,
         32,  32,  32,  43,  64,  43,  43,  43,
         43,  43,  43,  64,  64,  64,  43,  43,
         43,  64,  64,  64,  64,  64,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
      },
      {
          3,   3,   3, 133, 133,   2, 131,   1,
          3, 133,   3,   3, 133,   2, 131,   1,
          3,   3, 133, 133,   2, 131,   1,   1,
        133,   3, 133,   2, 131, 131,   1,   1,
        133, 133,   2, 131, 131,   1,   1,   1,
          2,   2, 131, 131,   1,   1,   1,   1,
        131, 131,   1,   1,   1,   1,   1,   1,
          1,   1,   1,   1,   1,   1,   1,   1,
      }
  },
  {
    30,
      {
         16,  11,  11,  11,  12,  11,  16,  12,
         12,  16,  21,  14,  13,  14,  21,  26,
         21,  16,  16,  21,  26,  32,  21,  21,
         21,  21,  21,  32,  32,  21,  26,  26,
         26,  26,  21,  32,  32,  32,  32,  43,
         32,  32,  32,  43,  43,  43,  43,  43,
         43,  64,  64,  64,  64,  64,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
      },
      {
          4,   6,   6,   4,   3, 133,   2,   2,
          6, 139, 139, 137,   3,   3,   3,   2,
          6, 139,   5,   4,   3, 133,   2, 131,
          4, 137,   4,   3, 133,   2, 131,   1,
          3,   3,   3, 133, 131, 131,   1,   1,
        133,   3, 133,   2, 131,   1,   1,   1,
          2,   3,   2, 131,   1,   1,   1,   1,
          2,   2, 131,   1,   1,   1,   1,   1,
      },
      {
         16,  14,  14,  16,  18,  16,  21,  18,
         18,  21,  21,  16,  21,  16,  21,  26,
         21,  21,  21,  21,  26,  43,  26,  26,
         26,  26,  26,  43,  43,  32,  32,  32,
         32,  32,  32,  43,  43,  43,  43,  43,
         43,  43,  43,  43,  43,  43,  43,  43,
         43,  64,  64,  64,  64,  64,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
      },
      {
          4, 137,   4,   3,   3, 133, 131, 131,
        137, 135, 135,   4,   3, 133,   2, 131,
          4, 135,   3,   3, 133,   2, 131, 131,
          3,   4,   3, 133,   2, 131, 131,   1,
          3,   3, 133,   2, 131, 131,   1,   1,
        133, 133,   2, 131, 131,   1,   1,   1,
        131,   2, 131, 131,   1,   1,   1,   1,
        131, 131, 131,   1,   1,   1,   1,   1,
      }
  },
  {
    40,
      {
         12,   8,   8,   8,   9,   8,  12,   9,
          9,  12,  16,  11,  10,  11,  16,  21,
         14,  12,  12,  14,  21,  26,  18,  18,
         21,  18,  18,  26,  21,  18,  21,  21,
         21,  21,  18,  21,  21,  26,  26,  32,
         26,  26,  21,  32,  32,  43,  43,  32,
         32,  43,  43,  43,  43,  43,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
      },
      {
        139,   8,   8, 139,   4,   3, 133,   3,
          8,   7,   7,   6, 137, 135, 135,   3,
          8,   7, 141, 139, 135,   3, 133,   2,
        139,   6, 139,   3,   3, 133,   2, 131,
          4, 137, 135,   3,   2, 131, 131,   1,
          3, 135,   3, 133, 131, 131,   1,   1,
        133, 135, 133,   2, 131,   1,   1,   1,
          3,   3,   2, 131,   1,   1,   1,   1,
      },
      {
         13,  11,  11,  13,  14,  13,  16,  14,
         14,  16,  21,  14,  14,  14,  21,  21,
         16,  16,  16,  16,  21,  26,  21,  21,
         21,  21,  21,  26,  32,  26,  21,  21,
         21,  21,  26,  32,  32,  32,  32,  32,
         32,  32,  32,  43,  43,  32,  32,  43,
         43,  43,  43,  43,  43,  43,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
      },
      {
          5,   6,   5,   4,   3,   3, 133,   2,
          6, 137, 137, 137,   4,   3, 133,   2,
          5, 137, 137,   4,   3,   3,   2, 131,
          4, 137,   4,   3,   3,   2, 131, 131,
          3,   4,   3,   3,   2,   2, 131,   1,
          3,   3,   3,   2,   2, 131,   1,   1,
        133, 133,   2, 131, 131,   1,   1,   1,
          2,   2, 131, 131,   1,   1,   1,   1,
      }
  },
  {
    50,
      {
          8,   6,   6,   6,   6,   6,   8,   6,
          6,   8,  12,   8,   7,   8,  12,  14,
         10,   8,   8,  10,  14,  16,  13,  13,
         14,  13,  13,  16,  16,  12,  14,  13,
         13,  14,  12,  16,  14,  18,  18,  21,
         18,  18,  14,  26,  26,  26,  26,  26,
         26,  32,  32,  32,  32,  32,  43,  43,
         43,  43,  43,  43,  43,  43,  43,  43,
      },
      {
          8,  11,  11,   8, 139, 137,   4,   4,
         11,  11,  11,   8, 141,   5, 139, 137,
         11,  11,   9,   8,   5, 137, 135, 133,
          8,   8,   8, 137,   5, 135, 133,   2,
        139, 141,   5,   5,   3, 133,   2, 131,
        137,   5, 137, 135, 133,   2, 131, 131,
          4, 139, 135, 133,   2, 131, 131, 131,
          4, 137, 133,   2, 131, 131, 131, 131,
      },
      {
          9,   8,   8,   9,  10,   9,  11,   9,
          9,  11,  14,  11,  13,  11,  14,  16,
         14,  14,  14,  14,  16,  18,  13,  13,
         14,  13,  13,  18,  26,  16,  14,  14,
         14,  14,  16,  26,  21,  21,  21,  21,
         21,  21,  21,  26,  26,  26,  26,  26,
         26,  32,  32,  32,  32,  32,  43,  43,
         43,  43,  43,  43,  43,  43,  43,  43,
      },
      {
          7,   8,   7,   6, 137,   4, 135, 133,
          8, 141,   7,   6, 137,   5,   4,   3,
          7,   7,   5, 137,   5, 137,   3, 133,
          6,   6, 137, 137, 137,   3, 133,   2,
        137, 137,   5, 137,   3, 133,   2, 131,
          4,   5, 137,   3, 133,   2, 131, 131,
        135,   4,   3, 133,   2, 131, 131, 131,
        133,   3, 133,   2, 131, 131, 131, 131,
      }
  },
  {
    60,
      {
          6,   4,   4,   4,   5,   4,   6,   5,
          5,   6,   9,   6,   5,   6,   9,  11,
          8,   6,   6,   8,  11,  12,  10,  10,
         11,  10,  10,  12,  16,  12,  12,  12,
         12,  12,  12,  16,  12,  14,  14,  16,
         14,  14,  12,  18,  18,  21,  21,  18,
         18,  26,  26,  26,  26,  26,  32,  32,
         32,  32,  32,  32,  32,  32,  32,  32,
      },
      {
         11,  16,  16,  11,   7,   6, 139,   4,
         16,  13,  13,  11,   8, 141, 139, 139,
         16,  13,  13,  11, 141, 139, 137, 135,
         11,  11,  11,   6, 139, 137, 135, 133,
          7,   8, 141, 139,   4,   3, 133,   2,
          6, 141, 139, 137,   3, 133,   2,   2,
        139, 139, 137, 135, 133,   2,   2,   2,
          4, 139, 135, 133,   2,   2,   2,   2,
      },
      {
          7,   7,   7,  13,  12,  13,  26,  16,
         16,  26,  26,  21,  16,  21,  26,  32,
         32,  32,  32,  32,  32,  32,  32,  32,
         32,  32,  32,  32,  32,  32,  32,  32,
         32,  32,  32,  32,  32,  32,  32,  32,
         32,  32,  32,  32,  32,  32,  32,  32,
         32,  32,  32,  32,  32,  32,  32,  32,
         32,  32,  32,  32,  32,  32,  32,  32,
      },
      {
          9,   9,   5, 133, 133,   2,   2,   2,
          9, 139,   4,   3,   2,   2,   2,   2,
          5,   4,   4,   2,   2,   2,   2,   2,
        133,   3,   2,   2,   2,   2,   2,   2,
        133,   2,   2,   2,   2,   2,   2,   2,
          2,   2,   2,   2,   2,   2,   2,   2,
          2,   2,   2,   2,   2,   2,   2,   2,
          2,   2,   2,   2,   2,   2,   2,   2,
      }
  },
  {
    70,
      {
          4,   3,   3,   3,   3,   3,   4,   3,
          3,   4,   6,   4,   3,   4,   6,   7,
          5,   4,   4,   5,   7,   8,   6,   6,
          7,   6,   6,   8,  10,   8,   9,   9,
          9,   9,   8,  10,  10,  12,  12,  12,
         12,  12,  10,  12,  12,  13,  13,  12,
         12,  16,  16,  16,  16,  16,  21,  21,
         21,  21,  21,  21,  21,  21,  21,  21,
      },
      {
         16,  21,  21,  16,  11,   9,   8, 141,
         21,  21,  21,  16,  13,  11,   8, 141,
         21,  21,  21,  16,  11,   7, 139, 139,
         16,  16,  16,   9,   7, 139, 139,   4,
         11,  13,  11,   7, 139,   5,   4,   3,
          9,  11,   7, 139,   5,   4,   3,   3,
          8,   8, 139, 139,   4,   3,   3,   3,
        141, 141, 139,   4,   3,   3,   3,   3,
      },
      {
          4,   5,   5,   8,   7,   8,  14,  10,
         10,  14,  21,  14,  14,  14,  21,  21,
         21,  21,  21,  21,  21,  21,  21,  21,
         21,  21,  21,  21,  21,  21,  21,  21,
         21,  21,  21,  21,  21,  21,  21,  21,
         21,  21,  21,  21,  21,  21,  21,  21,
         21,  21,  21,  21,  21,  21,  21,  21,
         21,  21,  21,  21,  21,  21,  21,  21,
      },
      {
         16,  13,   8, 137,   3,   3,   3,   3,
         13,   9, 141, 137,   3,   3,   3,   3,
          8, 141, 137,   3,   3,   3,   3,   3,
        137, 137,   3,   3,   3,   3,   3,   3,
          3,   3,   3,   3,   3,   3,   3,   3,
          3,   3,   3,   3,   3,   3,   3,   3,
          3,   3,   3,   3,   3,   3,   3,   3,
          3,   3,   3,   3,   3,   3,   3,   3,
      }
  },
  {
    80,
      {
          2,   2,   2,   2,   2,   2,   2,   2,
          2,   2,   3,   2,   2,   2,   3,   4,
          3,   2,   2,   3,   4,   5,   4,   4,
          4,   4,   4,   5,   6,   5,   5,   5,
          5,   5,   5,   6,   6,   7,   7,   8,
          7,   7,   6,   9,   9,  10,  10,   9,
          9,  12,  12,  12,  12,  12,  12,  12,
         12,  12,  12,  12,  12,  12,  12,  12,
      },
      {
         32,  32,  32,  32,  21,  16,  13,  11,
         32,  32,  32,  32,  21,  16,  13,  11,
         32,  32,  32,  32,  16,  13,   9,   7,
         32,  32,  32,  16,  13,   9,   7, 139,
         21,  21,  16,  13,   8, 141, 139, 139,
         16,  16,  13,   9, 141, 139, 139, 139,
         13,  13,   9,   7, 139, 139, 139, 139,
         11,  11,   7, 139, 139, 139, 139, 139,
      },
      {
          3,   3,   3,   5,   4,   5,   9,   6,
          6,   9,  13,  11,   9,  11,  13,  14,
         14,  14,  14,  14,  14,  14,  12,  12,
         12,  12,  12,  14,  14,  12,  12,  12,
         12,  12,  12,  14,  12,  12,  12,  12,
         12,  12,  12,  12,  12,  12,  12,  12,
         12,  12,  12,  12,  12,  12,  12,  12,
         12,  12,  12,  12,  12,  12,  12,  12,
      },
      {
         21,  21,  13,   7,   5, 137, 137, 137,
         21,  16,  11,   6, 137, 139, 139, 139,
         13,  11,   7, 137, 139, 139, 139, 139,
          7,   6, 137, 139, 139, 139, 139, 139,
          5, 137, 139, 139, 139, 139, 139, 139,
        137, 139, 139, 139, 139, 139, 139, 139,
        137, 139, 139, 139, 139, 139, 139, 139,
        137, 139, 139, 139, 139, 139, 139, 139,
      }
  },
  {
    90,
      {
          1,   1,   1,   1,   1,   1,   1,   1,
          1,   1,   2,   1,   1,   1,   2,   2,
          2,   1,   1,   2,   2,   2,   2,   2,
          2,   2,   2,   2,   3,   2,   3,   3,
          3,   3,   2,   3,   3,   4,   4,   4,
          4,   4,   3,   5,   5,   5,   5,   5,
          5,   7,   7,   7,   7,   7,   8,   8,
          8,   8,   8,   8,   8,   8,   8,   8,
      },
      {
         64,  64,  64,  64,  32,  32,  32,  21,
         64,  64,  64,  64,  32,  32,  32,  21,
         64,  64,  64,  64,  32,  21,  16,  13,
         64,  64,  64,  32,  21,  16,  13,   9,
         32,  32,  32,  21,  16,  13,   9,   8,
         32,  32,  21,  16,  13,   9,   8,   8,
         32,  32,  16,  13,   9,   8,   8,   8,
         21,  21,  13,   9,   8,   8,   8,   8,
      },
      {
          1,   1,   1,   2,   2,   2,   5,   3,
          3,   5,   7,   5,   4,   5,   7,   8,
          8,   8,   8,   8,   8,   8,   8,   8,
          8,   8,   8,   8,   8,   8,   8,   8,
          8,   8,   8,   8,   8,   8,   8,   8,
          8,   8,   8,   8,   8,   8,   8,   8,
          8,   8,   8,   8,   8,   8,   8,   8,
          8,   8,   8,   8,   8,   8,   8,   8,
      },
      {
         64,  64,  32,  13,   9,   8,   8,   8,
         64,  32,  21,  13,   8,   8,   8,   8,
         32,  21,  16,   8,   8,   8,   8,   8,
         13,  13,   8,   8,   8,   8,   8,   8,
          9,   8,   8,   8,   8,   8,   8,   8,
          8,   8,   8,   8,   8,   8,   8,   8,
          8,   8,   8,   8,   8,   8,   8,   8,
          8,   8,   8,   8,   8,   8,   8,   8,
      }
  },
  {
    100,
      {
          1,   1,   1,   1,   1,   1,   1,   1,
          1,   1,   1,   1,   1,   1,   1,   1,
          1,   1,   1,   1,   1,   1,   1,   1,
          1,   1,   1,   1,   1,   1,   1,   1,
          1,   1,   1,   1,   1,   1,   1,   1,
          1,   1,   1,   2,   2,   2,   2,   2,
          2,   2,   2,   2,   2,   2,   3,   3,
          3,   3,   3,   3,   3,   3,   3,   3,
      },
      {
         64,  64,  64,  64,  64,  64,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  64,
         64,  64,  64,  64,  64,  64,  64,  32,
         64,  64,  64,  64,  64,  64,  32,  32,
         64,  64,  64,  64,  64,  32,  32,  21,
         64,  64,  64,  64,  32,  32,  21,  21,
         64,  64,  64,  32,  32,  21,  21,  21,
         64,  64,  32,  32,  21,  21,  21,  21,
      },
      {
          1,   1,   1,   1,   1,   1,   1,   1,
          1,   1,   2,   2,   1,   2,   2,   3,
          3,   3,   3,   3,   3,   3,   3,   3,
          3,   3,   3,   3,   3,   3,   3,   3,
          3,   3,   3,   3,   3,   3,   3,   3,
          3,   3,   3,   3,   3,   3,   3,   3,
          3,   3,   3,   3,   3,   3,   3,   3,
          3,   3,   3,   3,   3,   3,   3,   3,
      },
      {
         64,  64,  64,  64,  32,  21,  21,  21,
         64,  64,  64,  32,  21,  21,  21,  21,
         64,  64,  64,  21,  21,  21,  21,  21,
         64,  32,  21,  21,  21,  21,  21,  21,
         32,  21,  21,  21,  21,  21,  21,  21,
         21,  21,  21,  21,  21,  21,  21,  21,
         21,  21,  21,  21,  21,  21,  21,  21,
         21,  21,  21,  21,  21,  21,  21,  21,
      }
  }
};

#define NR_JPGSETTING_TBL \
        (sizeof(g_isx019_jpg_quality) / sizeof(isx019_fpga_jpg_quality_t))

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int fpga_i2c_write(uint8_t addr, uint8_t *data, uint8_t size)
{
    static uint8_t buf[FPGA_I2C_REGSIZE_MAX + FPGA_I2C_REGADDR_LEN];
    int ret;

    /* ISX019 requires that send read command to ISX019 before FPGA access. */

    ret = send_read_cmd(CAT_VERSION, ROM_VERSION, 1);
    if (ret < 0) {
        return ret;
    }

    buf[FPGA_I2C_OFFSET_ADDR] = addr;
    memcpy(&buf[FPGA_I2C_OFFSET_WRITEDATA], data, size);
    ret = cambus_write_bytes(&sensor.bus,
                             FPGA_I2C_SLVADDR,
                             buf,
                             size + FPGA_I2C_REGADDR_LEN,
                             CAMBUS_XFER_NO_FLAGS);

    return ret;
}

static int fpga_i2c_read(uint8_t addr, uint8_t *data, uint8_t size)
{
    int ret;

    /* ISX019 requires that send read command to ISX019 before FPGA access. */

    ret = send_read_cmd(CAT_VERSION, ROM_VERSION, 1);
    if (ret < 0) {
        return ret;
    }

    ret = cambus_write_bytes(&sensor.bus,
                             FPGA_I2C_SLVADDR,
                             &addr,
                             FPGA_I2C_REGADDR_LEN,
                             CAMBUS_XFER_NO_FLAGS);
    if (ret >= 0) {
        ret = cambus_read_bytes(&sensor.bus, FPGA_I2C_SLVADDR, data, size, CAMBUS_XFER_NO_FLAGS);
    }

    return ret;
}

static int fpga_activate_setting()
{
    uint8_t regval = FPGA_ACTIVATE_REQUEST;
    int ret = fpga_i2c_write(FPGA_ACTIVATE, &regval, sizeof(uint8_t));

    for (mp_uint_t tick_start = mp_hal_ticks_ms(); ret == 0; ) {
        ret = fpga_i2c_read(FPGA_ACTIVATE, &regval, sizeof(uint8_t));
        if (regval == 0) {
            return 0;
        }
        if ((mp_hal_ticks_ms() - tick_start) > 1000) {
            return -1;
        }
    }

    return ret;
}

static uint8_t calc_isx019_chksum(uint8_t *data, uint8_t len)
{
    int i;
    uint8_t chksum = 0;

    for (i = 0; i < len; i++) {
        /* ISX019 checksum is lower 8bit of addition result.
        * So, no problem even if overflow occur.
        */

        chksum += data[i];
    }

    return chksum;
}

static bool validate_isx019_chksum(uint8_t *data, uint8_t len)
{
    uint8_t chksum;

    chksum = calc_isx019_chksum(data, len - 1);

    return (data[len - 1] == chksum) ? true : false;
}

static int recv_write_response()
{
    uint8_t buf[ISX019_I2C_WRRES_TOTALLEN];

    int ret = cambus_read_bytes(&sensor.bus, sensor.slv_addr, buf, sizeof(buf), CAMBUS_XFER_NO_FLAGS);
    if (ret < 0) {
        return ret;
    }

    if ((buf[ISX019_I2C_OFFSET_TOTALLEN] != ISX019_I2C_WRRES_TOTALLEN) ||
        (buf[ISX019_I2C_OFFSET_CMDNUM]   != 1) ||
        (buf[ISX019_I2C_OFFSET_CMDLEN]   != ISX019_I2C_WRRES_LEN) ||
        (buf[ISX019_I2C_OFFSET_STS]      != ISX019_I2C_STS_OK) ||
        !validate_isx019_chksum(buf, ISX019_I2C_WRRES_TOTALLEN)) {
        return -1;
    }

    return 0;
}

static int recv_read_response(uint8_t *data,
                              uint8_t size)
{
    uint8_t buf[ISX019_I2C_WRREQ_TOTALLEN(ISX019_I2C_REGSIZE_MAX)] = {};

    int ret = cambus_read_bytes(&sensor.bus, sensor.slv_addr,
                                buf, ISX019_I2C_RDRES_TOTALLEN(size), CAMBUS_XFER_NO_FLAGS);
    if (ret < 0) {
        return ret;
    }

    if ((buf[ISX019_I2C_OFFSET_TOTALLEN] != ISX019_I2C_RDRES_TOTALLEN(size)) ||
        (buf[ISX019_I2C_OFFSET_CMDNUM]   != 1) ||
        (buf[ISX019_I2C_OFFSET_CMDLEN]   != ISX019_I2C_RDRES_LEN(size)) ||
        (buf[ISX019_I2C_OFFSET_STS]      != ISX019_I2C_STS_OK) ||
        !validate_isx019_chksum(buf, ISX019_I2C_RDRES_TOTALLEN(size))) {
        return -1;
    }

    memcpy(data, &buf[ISX019_I2C_OFFSET_READDATA], size);

    return 0;
}

static int send_write_cmd(uint8_t cat,
                          uint16_t addr,
                          uint8_t *data,
                          uint8_t size)
{
  uint8_t buf[ISX019_I2C_WRREQ_TOTALLEN(ISX019_I2C_REGSIZE_MAX)] = {};

  buf[ISX019_I2C_OFFSET_TOTALLEN]  = ISX019_I2C_WRREQ_TOTALLEN(size);
  buf[ISX019_I2C_OFFSET_CMDNUM]    = 1;
  buf[ISX019_I2C_OFFSET_CMDLEN]    = ISX019_I2C_WRREQ_LEN(size);
  buf[ISX019_I2C_OFFSET_CMD]       = ISX019_I2C_CMD_WRITE;
  buf[ISX019_I2C_OFFSET_CATEGORY]  = cat;
  buf[ISX019_I2C_OFFSET_ADDRESS_H] = addr >> 8;
  buf[ISX019_I2C_OFFSET_ADDRESS_L] = addr & 0xff;

  memcpy(&buf[ISX019_I2C_OFFSET_WRITEDATA], data, size);

  int len = ISX019_I2C_OFFSET_WRITEDATA + size;
  buf[len] = calc_isx019_chksum(buf, len);
  len++;

  return cambus_write_bytes(&sensor.bus, sensor.slv_addr, buf, len, CAMBUS_XFER_NO_FLAGS);
}

static int isx019_i2c_write(uint8_t cat,
                            uint16_t addr,
                            uint8_t *data,
                            uint8_t size)
{
    int ret;

    ret = send_write_cmd(cat, addr, data, size);
    if (ret == 0) {
        ret = recv_write_response();
    }

    return ret;
}

static int send_read_cmd(uint8_t cat,
                         uint16_t addr,
                         uint8_t size)
{
    uint8_t buf[ISX019_I2C_RDREQ_TOTALLEN] = {};

    buf[ISX019_I2C_OFFSET_TOTALLEN]  = ISX019_I2C_RDREQ_TOTALLEN;
    buf[ISX019_I2C_OFFSET_CMDNUM]    = 1;
    buf[ISX019_I2C_OFFSET_CMDLEN]    = ISX019_I2C_RDREQ_LEN;
    buf[ISX019_I2C_OFFSET_CMD]       = ISX019_I2C_CMD_READ;
    buf[ISX019_I2C_OFFSET_CATEGORY]  = cat;
    buf[ISX019_I2C_OFFSET_ADDRESS_H] = addr >> 8;
    buf[ISX019_I2C_OFFSET_ADDRESS_L] = addr & 0xff;
    buf[ISX019_I2C_OFFSET_READSIZE]  = size;

    int len = ISX019_I2C_OFFSET_READSIZE + 1;
    buf[len] = calc_isx019_chksum(buf, len);
    len++;

    return cambus_write_bytes(&sensor.bus, sensor.slv_addr, buf, len, CAMBUS_XFER_NO_FLAGS);
}

static int isx019_i2c_read(uint8_t  cat,
                           uint16_t addr,
                           uint8_t  *data,
                           uint8_t  size)
{
    int ret;

    ret = send_read_cmd(cat, addr, size);
    if (ret == 0) {
        ret = recv_read_response(data, size);
    }

    return ret;
}

static void search_dqt_data(int32_t quality,
                            const uint8_t **y_head, const uint8_t **y_calc,
                            const uint8_t **c_head, const uint8_t **c_calc)
{
    int i;
    const isx019_fpga_jpg_quality_t *jpg = &g_isx019_jpg_quality[0];

    *y_head = NULL;
    *y_calc = NULL;
    *c_head = NULL;
    *c_calc = NULL;

    /* Search approximate DQT data from a table by rounding quality. */

    quality = ((quality + 5) / 10) * 10;
    if (quality == 0) {
        /* Set the minimum value of quality to 10. */

        quality = 10;
    }

    for (i = 0; i < NR_JPGSETTING_TBL; i++) {
        if (quality == jpg->quality) {
            *y_head = jpg->y_head;
            *y_calc = jpg->y_calc;
            *c_head = jpg->c_head;
            *c_calc = jpg->c_calc;
            break;
        }

        jpg++;
    }
}

int set_dqt(uint8_t component, uint8_t target, const uint8_t *buf)
{
    int i, ret;
    uint8_t addr;
    uint8_t select;
    uint8_t data;
    uint8_t regval;

    if (target == FPGA_DQT_DATA) {
        addr   = FPGA_DQT_ADDRESS;
        select = FPGA_DQT_SELECT;
        data   = FPGA_DQT_DATA;
    } else {
        addr   = FPGA_DQT_CALC_ADDRESS;
        select = FPGA_DQT_CALC_SELECT;
        data   = FPGA_DQT_CALC_DATA;
    }

    ret = fpga_i2c_write(select, &component, sizeof(uint8_t));
    for (i = 0; i < JPEG_DQT_ARRAY_SIZE; i++) {
        regval = i | FPGA_DQT_WRITE | FPGA_DQT_BUFFER;
        ret |= fpga_i2c_write(addr, &regval, sizeof(uint8_t));
        ret |= fpga_i2c_write(data, (uint8_t *) &buf[i], sizeof(uint8_t));
    }

    return ret;
}

static int set_jpg_quality(int val)
{
    const uint8_t *y_head;
    const uint8_t *y_calc;
    const uint8_t *c_head;
    const uint8_t *c_calc;

    /* Set JPEG quality by setting DQT information to FPGA. */

    search_dqt_data(val, &y_head, &y_calc, &c_head, &c_calc);
    if ((y_head == NULL) ||
        (y_calc == NULL) ||
        (c_head == NULL) ||
        (c_calc == NULL)) {
        return -1;
    }

    /* Update DQT data and activate them. */

    int ret = set_dqt(FPGA_DQT_LUMA,    FPGA_DQT_DATA, y_head);
    ret |= set_dqt(FPGA_DQT_CHROMA,     FPGA_DQT_DATA, c_head);
    ret |= set_dqt(FPGA_DQT_LUMA,       FPGA_DQT_CALC_DATA, y_calc);
    ret |= set_dqt(FPGA_DQT_CHROMA,     FPGA_DQT_CALC_DATA, c_calc);
    ret |= fpga_activate_setting();

    /* Wait for swap of non-active side and active side. */

    mp_hal_delay_ms(DELAY_TIME_JPEGDQT_SWAP / 1000);

    /* Update non-active side in preparation for other activation trigger. */

    ret |= set_dqt(FPGA_DQT_LUMA,   FPGA_DQT_DATA, y_head);
    ret |= set_dqt(FPGA_DQT_CHROMA, FPGA_DQT_DATA, c_head);
    ret |= set_dqt(FPGA_DQT_LUMA,   FPGA_DQT_CALC_DATA, y_calc);
    ret |= set_dqt(FPGA_DQT_CHROMA, FPGA_DQT_CALC_DATA, c_calc);

    return ret;
}

/****************************************************************************
 * OpenMV Integration
 ****************************************************************************/

static int reset(sensor_t *sensor)
{
    uint8_t drv[] = {
#if 0
      DOL2_30FPS_SENS, DOL2_30FPS_POST, DOL2_30FPS_SENSPOST, DOL2_30FPS_IO
#else
      DOL3_30FPS_SENS, DOL3_30FPS_POST, DOL3_30FPS_SENSPOST, DOL3_30FPS_IO
#endif
    };

    uint8_t regval;
    int ret = 0;

    mp_hal_delay_ms(TRANSITION_TIME_TO_STARTUP / 1000);

    ret |= isx019_i2c_write(CAT_CONFIG, MODE_SENSSEL,      &drv[INDEX_SENS], sizeof(uint8_t));
    ret |= isx019_i2c_write(CAT_CONFIG, MODE_POSTSEL,      &drv[INDEX_POST], sizeof(uint8_t));
    ret |= isx019_i2c_write(CAT_CONFIG, MODE_SENSPOST_SEL, &drv[INDEX_SENSPOST], sizeof(uint8_t));

    mp_hal_delay_ms(TRANSITION_TIME_TO_STREAMING / 1000);

    regval = FPGA_RESET_ENABLE;
    ret |= fpga_i2c_write(FPGA_RESET, &regval, 1);

    regval = FPGA_DATA_OUTPUT_STOP;
    ret |= fpga_i2c_write(FPGA_DATA_OUTPUT, &regval, 1);

    ret |= fpga_activate_setting();

    regval = FPGA_RESET_RELEASE;
    ret |= fpga_i2c_write(FPGA_RESET, &regval, 1);

    ret |= fpga_activate_setting();

    ret |= set_jpg_quality(90);

    return ret;
}

static int sleep(sensor_t *sensor, int enable)
{
    uint8_t regval = enable ? FPGA_DATA_OUTPUT_START : FPGA_DATA_OUTPUT_STOP;
    int ret = fpga_i2c_write(FPGA_DATA_OUTPUT, &regval, sizeof(uint8_t));
    ret |= fpga_activate_setting();
    return ret;
}

static int set_pixformat(sensor_t *sensor, pixformat_t pixformat)
{
    int format;

    switch (pixformat) {
        case PIXFORMAT_GRAYSCALE:
        case PIXFORMAT_YUV422:
            format = FPGA_FORMAT_YUV;
            break;
        case PIXFORMAT_RGB565:
            format = FPGA_FORMAT_RGB;
            break;
        case PIXFORMAT_JPEG:
            format = FPGA_FORMAT_JPEG;
            break;
        default:
            return -1;
    }

    uint8_t regval;
    int ret = fpga_i2c_read(FPGA_FORMAT_AND_SCALE, &regval, sizeof(uint8_t));
    regval = (regval & ~0x03) | format;
    ret |= fpga_i2c_write(FPGA_FORMAT_AND_SCALE, &regval, sizeof(uint8_t));
    ret |= fpga_activate_setting();
    return ret;
}

static int set_framesize(sensor_t *sensor, framesize_t framesize)
{
    uint16_t w = resolution[framesize][0];
    uint16_t h = resolution[framesize][1];

    uint8_t scale = FPGA_SCALE_1280_960, clip_size = FPGA_CLIP_NON, clip_top = 0, clip_left = 0;

    if ((w == 120) && (h == 120)) {
        scale = FPGA_SCALE_160_120;
    } else if ((w == 320) && (h == 240)) {
        scale = FPGA_SCALE_320_240;
    } else if ((w == 480) && (h == 360)) {
        scale = FPGA_SCALE_640_480;
        clip_size = FPGA_CLIP_640_360;
        clip_top = ((480 - 360) / 2) / FPGA_CLIP_UNIT;
        clip_left = ((640 - 480) / 2) / FPGA_CLIP_UNIT;
    } else if ((w == 640) && (h == 360)) {
        scale = FPGA_SCALE_640_480;
        clip_size = FPGA_CLIP_640_360;
        clip_top = ((480 - 360) / 2) / FPGA_CLIP_UNIT;
    } else if ((w == 640) && (h == 480)) {
        scale = FPGA_SCALE_640_480;
    } else if ((w == 1280) && (h == 720)) {
        clip_size = FPGA_CLIP_1280_720;
        clip_top = ((960 - 720) / 2) / FPGA_CLIP_UNIT;
    } else if ((w != 1280) || (h != 960)) {
        return -1; 
    }

    int ret = fpga_i2c_write(FPGA_CLIP_SIZE, &clip_size, sizeof(uint8_t));
    ret |= fpga_i2c_write(FPGA_CLIP_TOP,  &clip_top, sizeof(uint8_t));
    ret |= fpga_i2c_write(FPGA_CLIP_LEFT, &clip_left, sizeof(uint8_t));

    uint8_t regval;
    ret |= fpga_i2c_read(FPGA_FORMAT_AND_SCALE, &regval, sizeof(uint8_t));
    regval = (regval & ~0x30) | scale;
    ret |= fpga_i2c_write(FPGA_FORMAT_AND_SCALE, &regval, sizeof(uint8_t));

    regval = FPGA_FPS_1_1;
    ret |= fpga_i2c_write(FPGA_FPS_AND_THUMBNAIL, &regval, sizeof(uint8_t));

    regval = FPGA_DATA_OUTPUT_START;
    ret |= fpga_i2c_write(FPGA_DATA_OUTPUT, &regval, sizeof(uint8_t));

    ret |= fpga_activate_setting();
    return ret;
}

static int set_contrast(sensor_t *sensor, int level)
{
    if ((level < MIN_CONTRAST) || (MAX_CONTRAST < level)) {
        return -1;
    }
  
    return isx019_i2c_write(CAT_PICTTUNE, UICONTRAST, (uint8_t *) &level, sizeof(uint8_t));
}

static int set_brightness(sensor_t *sensor, int level)
{
    if ((level < MIN_BRIGHTNESS) || (MAX_BRIGHTNESS < level)) {
        return -1;
    }

    int brightness = level << 2;
    return isx019_i2c_write(CAT_PICTTUNE, UIBRIGHTNESS, (uint8_t *) &brightness, sizeof(uint16_t));    
}

static int set_saturation(sensor_t *sensor, int level)
{
    if ((level < MIN_SATURATION) || (MAX_SATURATION < level)) {
        return -1;
    }
  
    return isx019_i2c_write(CAT_PICTTUNE, UISATURATION, (uint8_t *) &level, sizeof(uint8_t));
}
/*
static int set_hue(sensor_t *sensor, int level)
{
    if ((level < MIN_HUE) || (MAX_HUE < level)) {
        return -1;
    }

    int hue = (level * 90) / 128;
    return isx019_i2c_write(CAT_PICTTUNE, UIHUE, (uint8_t *) &hue, sizeof(uint8_t));
}
*/
static int set_quality(sensor_t *sensor, int qs)
{
    return set_jpg_quality(qs);
}

static int set_auto_gain(sensor_t *sensor, int enable, float gain_db, float gain_db_ceiling)
{
    int ret = 0;
    uint16_t gain = 0;
    (void) gain_db_ceiling;

    if (enable == 0) {
        if ((!isnanf(gain_db)) && (!isnanf(gain_db))) {
            gain = IM_MAX(IM_MIN(fast_roundf(gain_db / 0.1f), 8191), 0);
        } else {
            uint8_t buf;
            ret |= isx019_i2c_read(CAT_AECOM, GAIN_LEVEL, &buf, sizeof(uint8_t));
            gain = buf / 0.3f;
        }
    }

    ret |= isx019_i2c_write(CAT_CATAE, GAIN_PRIMODE, (uint8_t *) &gain, sizeof(uint16_t));
    return ret;
}

static int get_gain_db(sensor_t *sensor, float *gain_db)
{
    int ret = 0;
    uint8_t gain;
    ret |= isx019_i2c_read(CAT_AECOM, GAIN_LEVEL, &gain, sizeof(uint8_t));
    *gain_db = gain * 0.3f;
    return ret;
}

static int set_auto_exposure(sensor_t *sensor, int enable, int exposure_us)
{
    int ret = 0;
    uint32_t regval = 0;

    if (enable == 0) {
      if (exposure_us >= 0) {
          float clock_ratio = ((float) sensor_get_xclk_frequency()) / ISX019_STANDARD_MASTER_CLOCK;
          regval = fast_floorf(exposure_us * clock_ratio);
      } else {
          ret |= isx019_i2c_read(CAT_AESOUT, SHT_TIME, (uint8_t *) &regval, sizeof(uint32_t));
      }
    }
    
    ret |= isx019_i2c_write(CAT_CATAE, SHT_PRIMODE, (uint8_t *) &regval, sizeof(uint32_t));
    return ret;
}

static int get_exposure_us(sensor_t *sensor, int *exposure_us)
{
    uint32_t regval;
    int ret = isx019_i2c_read(CAT_AESOUT, SHT_TIME, (uint8_t *) &regval, sizeof(uint32_t));
    float clock_ratio = ((float) sensor_get_xclk_frequency()) / ISX019_STANDARD_MASTER_CLOCK;
    *exposure_us = fast_ceilf(regval / clock_ratio);
    return ret;
}

static int set_auto_whitebal(sensor_t *sensor, int enable, float r_gain_db, float g_gain_db, float b_gain_db)
{
    int ret = 0;
    uint8_t mode = AWBMODE_AUTO;
    (void) g_gain_db;

    if (enable == 0) {
        if ((!isnanf(r_gain_db)) && (!isnanf(b_gain_db)) && (!isinff(r_gain_db)) && (!isinff(b_gain_db))) {
            uint16_t r_addr, b_addr;
            uint16_t r = IM_MAX(IM_MIN(fast_roundf(r_gain_db / 0.3f), 8191), 0);
            uint16_t b = IM_MAX(IM_MIN(fast_roundf(r_gain_db / 0.3f), 8191), 0);
            static bool toggle = false;

            if (toggle) {
                r_addr = USER0_R;
                b_addr = USER0_B;
                toggle = false;
            } else {
                r_addr = USER1_R;
                b_addr = USER1_B;
                toggle = true;
            }

            ret |= isx019_i2c_write(CAT_AWB_USERTYPE, r_addr, (uint8_t *) &r, sizeof(uint16_t));
            ret |= isx019_i2c_write(CAT_AWB_USERTYPE, b_addr, (uint8_t *) &b, sizeof(uint16_t));
            ret |= isx019_i2c_write(CAT_CATAWB, AWBUSER_NO, (uint8_t *) &toggle, sizeof(uint8_t));
            mode = AWBMODE_MANUAL;
        } else {
            mode = AWBMODE_HOLD;
        }
    }

    ret |= isx019_i2c_write(CAT_CATAWB, AWBMODE, &mode, sizeof(uint8_t));
    return ret;
}

static int get_rgb_gain_db(sensor_t *sensor, float *r_gain_db, float *g_gain_db, float *b_gain_db)
{
    int ret = 0;
    uint16_t r, b;
    ret |= isx019_i2c_read(CAT_AWBSOUT, CONT_R, (uint8_t *) &r, sizeof(uint16_t));
    ret |= isx019_i2c_read(CAT_AWBSOUT, CONT_B, (uint8_t *) &b, sizeof(uint16_t));
    *r_gain_db = r * 0.3f;
    *g_gain_db = 1.0f;
    *b_gain_db = b * 0.3f;
    return ret;
}

static int set_hmirror(sensor_t *sensor, int enable)
{
    uint8_t regval;
    int ret = isx019_i2c_read(CAT_CONFIG, REVERSE, &regval, sizeof(uint8_t));
    regval = (enable == 0) ? (regval & ~H_REVERSE) : (regval | H_REVERSE);
    ret |= isx019_i2c_write(CAT_CONFIG, REVERSE, &regval, sizeof(uint8_t));
    return ret;
}

static int set_vflip(sensor_t *sensor, int enable)
{
    uint8_t regval;
    int ret = isx019_i2c_read(CAT_CONFIG, REVERSE, &regval, sizeof(uint8_t));
    regval = (enable == 0) ? (regval & ~V_REVERSE) : (regval | V_REVERSE);
    ret |= isx019_i2c_write(CAT_CONFIG, REVERSE, &regval, sizeof(uint8_t));
    return ret;
}

int omv_isx019_init(sensor_t *sensor)
{
    // Initialize sensor structure.
    sensor->reset               = reset;
    sensor->sleep               = sleep;
    sensor->set_pixformat       = set_pixformat;
    sensor->set_framesize       = set_framesize;
    sensor->set_contrast        = set_contrast;
    sensor->set_brightness      = set_brightness;
    sensor->set_saturation      = set_saturation;
    sensor->set_quality         = set_quality;
    sensor->set_auto_gain       = set_auto_gain;
    sensor->get_gain_db         = get_gain_db;
    sensor->set_auto_exposure   = set_auto_exposure;
    sensor->get_exposure_us     = get_exposure_us;
    sensor->set_auto_whitebal   = set_auto_whitebal;
    sensor->get_rgb_gain_db     = get_rgb_gain_db;
    sensor->set_hmirror         = set_hmirror;
    sensor->set_vflip           = set_vflip;

    // Set sensor flags
    sensor->hw_flags.vsync      = 0;
    sensor->hw_flags.hsync      = 0;
    sensor->hw_flags.pixck      = 1;
    sensor->hw_flags.fsync      = 0;
    sensor->hw_flags.jpege      = 1;
    sensor->hw_flags.jpeg_mode  = 3;
    sensor->hw_flags.gs_bpp     = 2;

    return 0;
}
#endif // (OMV_ENABLE_ISX019 == 1)
