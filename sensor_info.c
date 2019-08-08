/* Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <stdlib.h>

#include "sensor_info.h"
#include "sensorInfo_ov10640.h"
#include "sensorInfo_ar0231.h"

SensorInfo *
GetSensorInfo(char *sensorName)
{
    uint32_t i;
    SensorInfo *sensorInfo[] = {
        GetSensorInfo_ov10640(),
        GetSensorInfo_ar0231()
    };

    if (!sensorName) {
        LOG_ERR("%s: Sensor name not provided\n", __func__);
        return NULL;
    }

    for (i = 0; i < sizeof(sensorInfo) / sizeof(sensorInfo[0]); i++) {
        if (!strncmp(sensorName, sensorInfo[i]->name,
           strlen(sensorInfo[i]->name))) {
            LOG_DBG("%s: Found sensor info for  %s\n", __func__, sensorName);
            return sensorInfo[i];
        }
    }

    LOG_ERR("%s: Can't find sensor info for %s\n", __func__, sensorName);
    return NULL;
}
