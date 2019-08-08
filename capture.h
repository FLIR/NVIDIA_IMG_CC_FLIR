/* Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef __CAPTURE_H__
#define __CAPTURE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "cmdline.h"
#include "thread_utils.h"
#include "parser.h"
#include "nvmedia_isc.h"
#include "nvmedia_icp.h"
#include "nvmedia_surface.h"

#define CAPTURE_INPUT_QUEUE_SIZE             5     /* min no. of buffers needed to capture without any frame drops */
#define CAPTURE_DEQUEUE_TIMEOUT              1000
#define CAPTURE_ENQUEUE_TIMEOUT              100
#define CAPTURE_FEED_FRAME_TIMEOUT           100
#define CAPTURE_GET_FRAME_TIMEOUT            500
#define CAPTURE_MAX_RETRY                    10

typedef struct {
    NvMediaICPEx               *icpExCtx;
    NvQueue                    *inputQueue;
    NvQueue                    *outputQueue;
    volatile NvMediaBool       *quit;
    NvMediaBool                 exitedFlag;
    NvMediaICPSettings         *settings;

    /* capture params */
    uint32_t                    width;
    uint32_t                    height;
    uint32_t                    virtualGroupIndex;
    uint32_t                    currentFrame;
    uint32_t                    numFramesToSkip;
    uint32_t                    numFramesToCapture;
    uint32_t                    numFramesToWait;
    uint32_t                    numMiniburstFrames;
    uint32_t                    numBuffers;

    /* input and surface params */
    NvMediaICPInputFormat       inputFormat;
    NvMediaSurfaceType          surfType;
    uint32_t                    rawBytesPerPixel;
    uint32_t                    pixelOrder;
    NvMediaSurfAllocAttr        surfAllocAttrs[8];
    uint32_t                    numSurfAllocAttrs;

} CaptureThreadCtx;

typedef struct {
    /* capture context */
    NvThread                   *captureThread[NVMEDIA_ICP_MAX_VIRTUAL_GROUPS];
    CaptureThreadCtx            threadCtx[NVMEDIA_ICP_MAX_VIRTUAL_GROUPS];
    NvMediaICPEx               *icpExCtx;
    NvMediaICPSettingsEx        icpSettingsEx;
    NvMediaDevice              *device;
    NvMediaISCRootDevice       *iscCtx;
    CaptureConfigParams         captureParams;
    SensorInfo                 *sensorInfo;
    MapInfo                    *camMap;

    /* General Variables */
    volatile NvMediaBool       *quit;
    TestArgs                   *testArgs;
    uint32_t                    numSensors;
    uint32_t                    numVirtualChannels;
    uint32_t                    i2cDeviceNum;
    uint32_t                    inputQueueSize;
    I2cCommands                 parsedCommands;
    I2cCommands                 settingsCommands;
    CalibrationParameters       calParams;
    NvMediaICPInterfaceType     interfaceType;
    NvMediaICPCsiPhyMode        phyMode;
    uint32_t                    crystalFrequency;
    NvMediaBool                 useNvRawFormat;
} NvCaptureContext;

NvMediaStatus
CaptureInit(NvMainContext *mainCtx);

NvMediaStatus
CaptureFini(NvMainContext *mainCtx);

NvMediaStatus
CaptureProc(NvMainContext *mainCtx);

#ifdef __cplusplus
}
#endif

#endif

