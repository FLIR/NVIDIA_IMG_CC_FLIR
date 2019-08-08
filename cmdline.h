/* Copyright (c) 2014-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef _CMDLINE_H_
#define _CMDLINE_H_

#include "nvmedia_icp.h"
#include "nvmedia_idp.h"
#include "nvmedia_core.h"
#include "nvmedia_surface.h"
#include "nvmedia_common.h"
#include "misc_utils.h"
#include "sensor_info.h"

#define MIN_BUFFER_POOL_SIZE    5
#define MAX_BUFFER_POOL_SIZE    NVMEDIA_MAX_CAPTURE_FRAME_BUFFERS
#define MAX_STRING_SIZE         256

#define CAM_ENABLE_DEFAULT 0x0001  // only enable cam link 0
#define CAM_MASK_DEFAULT   0x0000  // do not mask any link
#define CSI_OUT_DEFAULT    0x3210  // cam link i -> csiout i

// count enabled camera links
#define MAP_COUNT_ENABLED_LINKS(enable) \
             ((enable & 0x1) + ((enable >> 4) & 0x1) + \
             ((enable >> 8) & 0x1) + ((enable >> 12) & 0x1))

// convert aggegate number to cam_enable
#define MAP_N_TO_ENABLE(n) \
             ((((1 << n) - 1) & 0x0001) + ((((1 << n) - 1) << 3) &0x0010) + \
              ((((1 << n) - 1) << 6) & 0x0100) + ((((1 << n) - 1) << 9) & 0x1000))

typedef struct {
    NvMediaBool                 isUsed;
    union {
        uint32_t                uIntValue;
        float                   floatValue;
        char                    stringValue[MAX_STRING_SIZE];
        struct {
            float               R;
            float               G1;
            float               G2;
            float               B;
        };
        struct {
            uint32_t            kp[12]; // 12 knee points
        };
    };
} CmdlineParameter;

typedef struct {
    unsigned int                enable; // camera[3..0] enable, value:0/1. eg 0x1111
    unsigned int                mask;   // camera[3..0] mask,   value:0/1. eg 0x0001
    unsigned int                csiOut; // camera[3..0] csi outmap, value:0/1/2/3. eg. 0x3210
} MapInfo;

typedef struct {
    SensorInfo                  *sensorInfo;
    SensorProperties            *sensorProperties;
    NvMediaBool                 calibrateSensorFlag;

    uint32_t                    logLevel;
    CmdlineParameter            wrregs;
    CmdlineParameter            rdregs;
    CmdlineParameter            frames;
    CmdlineParameter            rtSettings;
    NvMediaBool                 displayEnabled;
    NvMediaBool                 displayIdUsed;
    uint32_t                    displayId;
    uint32_t                    windowId;
    uint32_t                    depth;
    NvMediaBool                 positionSpecifiedFlag;
    NvMediaRect                 position;
    NvMediaBool                 useFilePrefix;
    NvMediaBool                 useNvRawFormat;
    char                        filePrefix[MAX_STRING_SIZE];
    uint32_t                    crystalFrequency;
    uint32_t                    numFramesToSkip;
    uint32_t                    numFramesToWait;
    uint32_t                    numMiniburstFrames;
    uint32_t                    bufferPoolSize;
    uint32_t                    numSensors;
    uint32_t                    numLinks;
    uint32_t                    numVirtualChannels;
    NvMediaBool                 useAggregationFlag;
    NvMediaBool                 useVirtualChannels;
    MapInfo                     camMap;
    NvMediaBool                 disablePwrCtrl;
    CmdlineParameter            config[NVMEDIA_ICP_MAX_VIRTUAL_CHANNELS];
} TestArgs;

NvMediaStatus
ParseArgs(int argc,
          char *argv[],
          TestArgs *allArgs);

#endif
