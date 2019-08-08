/* Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef _SENSOR_INFO_H_
#define _SENSOR_INFO_H_

#include <stdlib.h>
#include "nvmedia_core.h"
#include "nvmedia_surface.h"
#include "nvmedia_image.h"
#include "i2cCommands.h"
#include "nvrawfile_interface.h"

#define BAYER_ORDERING(a,b,c,d) ((a) << 24 | (b) << 16 | (c) << 8 | (d))
#define NVRAWDUMP_BAYER_ORDERING_RGGB BAYER_ORDERING('R', 'G', 'G', 'B')
#define NVRAWDUMP_BAYER_ORDERING_BGGR BAYER_ORDERING('B', 'G', 'G', 'R')
#define NVRAWDUMP_BAYER_ORDERING_GRBG BAYER_ORDERING('G', 'R', 'B', 'G')
#define NVRAWDUMP_BAYER_ORDERING_GBRG BAYER_ORDERING('G', 'B', 'R', 'G')

typedef struct {
    uint16_t            *rgstrVal;
    uint32_t             rgstrAddr;
} RegisterSetup;

typedef void SensorProperties;

typedef struct {
    int32_t i2cDevice;
    uint32_t sensorAddress;
    uint32_t crystalFrequency;
} CalibrationParameters;

typedef struct {
    char *name;
    char **supportedArgs;
    uint32_t numSupportedArgs;
    uint32_t sizeOfSensorProperties;
    NvMediaStatus (*CalibrateSensor)(I2cCommands *settings, CalibrationParameters *calParam, SensorProperties *properties);
    NvMediaStatus (*ProcessCmdline)(int argc, char *argv[], SensorProperties *properties);
    NvMediaStatus (*AppendOutputFilename)(char *filename, SensorProperties *properties);
    NvMediaStatus (*WriteNvRawImage)(I2cCommands *settings, CalibrationParameters *calParam,
                                     NvMediaImage *image, int32_t frameNumber, char *fileName);
    void (*PrintSensorCaliUsage)(void);
} SensorInfo;

SensorInfo *GetSensorInfo(char *sensorName);

#endif /* _SENSOR_INFO_H_ */


