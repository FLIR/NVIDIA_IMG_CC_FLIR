/* Copyright (c) 2014-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef _PARSER_H_
#define _PARSER_H_

#include "nvmedia_core.h"
#include "nvmedia_surface.h"
#include "misc_utils.h"

#include "i2cCommands.h"

#define MAX_STRING_SIZE         256

typedef struct {
    NvMediaBool                 isUsed;
    union {
        int                     intValue;
        uint32_t                uIntValue;
        char                    stringValue[MAX_STRING_SIZE];
    };
} InputParameter;

typedef struct {
    InputParameter               inputFormat;
    InputParameter               surfaceFormat;
    InputParameter               resolution;
    InputParameter               interface;
    InputParameter               i2cDevice;
    InputParameter               csiLanes;
    InputParameter               sensorAddress;
    InputParameter               deserAddress;
    InputParameter               pixelOrder;
    InputParameter               emb;
} CaptureConfigParams;

NvMediaStatus
ParseRegistersFile(char *filename,
                   CaptureConfigParams *params,
                   I2cCommands *allCommands);

#endif
