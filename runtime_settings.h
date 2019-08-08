/* Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef __RUNTIME_SETTINGS_H__
#define __RUNTIME_SETTINGS_H__

#include "main.h"
#include "sensor_info.h"
#include "thread_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int                         argc;
    char                        *argv[50];
    uint32_t                    numFrames;
    I2cCommands                 *cmds;
    char                        outputFileName[MAX_STRING_SIZE];
} RuntimeSettings;

typedef struct {
    NvThread                   *runtimeSettingsThread;
    NvMediaBool                 exitedFlag;
    volatile NvMediaBool       *quit;
    RuntimeSettings            *rtSettings;
    uint32_t                    numRtSettings;
    uint32_t                    currentRtSettings;
    uint32_t                   *currentFrame;
    CalibrationParameters      *calParam;
} NvRuntimeSettingsContext;

NvMediaStatus
RuntimeSettingsInit(NvMainContext *mainCtx);

NvMediaStatus
RuntimeSettingsFini(NvMainContext *mainCtx);

NvMediaStatus
RuntimeSettingsProc(NvMainContext *mainCtx);

#ifdef __cplusplus
}
#endif

#endif

