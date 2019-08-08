/* Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "check_version.h"
#include "log_utils.h"

NvMediaStatus
CheckModulesVersion(void)
{
    NvMediaVersion version;
    NvMediaStatus status = NVMEDIA_STATUS_OK;

    memset(&version, 0, sizeof(NvMediaVersion));
    status = NvMediaCoreGetVersion(&version);
    if (status != NVMEDIA_STATUS_OK)
        return status;

    if((version.major != NVMEDIA_CORE_VERSION_MAJOR) ||
       (version.minor != NVMEDIA_CORE_VERSION_MINOR)) {
        LOG_ERR("%s: Incompatible core version found \n", __func__);
        LOG_ERR("%s: Client version: %d.%d\n", __func__,
            NVMEDIA_CORE_VERSION_MAJOR, NVMEDIA_CORE_VERSION_MINOR);
        LOG_ERR("%s: Core version: %d.%d\n", __func__,
            version.major, version.minor);
        return NVMEDIA_STATUS_INCOMPATIBLE_VERSION;
    }

    memset(&version, 0, sizeof(NvMediaVersion));
    status = NvMediaImageGetVersion(&version);
    if (status != NVMEDIA_STATUS_OK)
        return status;

    if((version.major != NVMEDIA_IMAGE_VERSION_MAJOR) ||
       (version.minor != NVMEDIA_IMAGE_VERSION_MINOR)) {
        LOG_ERR("%s: Incompatible image version found \n", __func__);
        LOG_ERR("%s: Client version: %d.%d\n", __func__,
            NVMEDIA_IMAGE_VERSION_MAJOR, NVMEDIA_IMAGE_VERSION_MINOR);
        LOG_ERR("%s: Core version: %d.%d\n", __func__,
            version.major, version.minor);
        return NVMEDIA_STATUS_INCOMPATIBLE_VERSION;
    }

    memset(&version, 0, sizeof(NvMediaVersion));
    status = NvMediaISCGetVersion(&version);
    if (status != NVMEDIA_STATUS_OK)
        return status;

    if (version.major != NVMEDIA_ISC_VERSION_MAJOR ||
        version.minor != NVMEDIA_ISC_VERSION_MINOR) {
        LOG_ERR("%s: Incompatible ISC version found \n", __func__);
        LOG_ERR("%s: Client version: %d.%d\n", __func__,
                NVMEDIA_ISC_VERSION_MAJOR, NVMEDIA_ISC_VERSION_MINOR);
        LOG_ERR("%s: Core version: %d.%d\n", __func__,
                version.major, version.minor);
        return NVMEDIA_STATUS_INCOMPATIBLE_VERSION;
    }

    memset(&version, 0, sizeof(NvMediaVersion));
    status = NvMediaICPGetVersion(&version);
    if (status != NVMEDIA_STATUS_OK)
        return status;

    if (version.major != NVMEDIA_ICP_VERSION_MAJOR ||
        version.minor != NVMEDIA_ICP_VERSION_MINOR) {
        LOG_ERR("%s: Incompatible ICP version found \n", __func__);
        LOG_ERR("%s: Client version: %d.%d\n", __func__,
                NVMEDIA_ICP_VERSION_MAJOR, NVMEDIA_ICP_VERSION_MINOR);
        LOG_ERR("%s: Core version: %d.%d\n", __func__,
                version.major, version.minor);
        return NVMEDIA_STATUS_INCOMPATIBLE_VERSION;
    }

    memset(&version, 0, sizeof(NvMediaVersion));
    status = NvMedia2DGetVersion(&version);
    if (status != NVMEDIA_STATUS_OK)
        return status;

    if ((version.major != NVMEDIA_2D_VERSION_MAJOR) ||
        (version.minor != NVMEDIA_2D_VERSION_MINOR)) {
        LOG_ERR("%s: Incompatible 2D version found \n", __func__);
        LOG_ERR("%s: Client version: %d.%d\n", __func__,
            NVMEDIA_2D_VERSION_MAJOR, NVMEDIA_2D_VERSION_MINOR);
        LOG_ERR("%s: Core version: %d.%d\n", __func__,
            version.major, version.minor);
        return NVMEDIA_STATUS_INCOMPATIBLE_VERSION;
    }

    memset(&version, 0, sizeof(NvMediaVersion));
    status = NvMediaIDPGetVersion(&version);
    if (status != NVMEDIA_STATUS_OK)
        return status;

    if (version.major != NVMEDIA_IDP_VERSION_MAJOR ||
        version.minor != NVMEDIA_IDP_VERSION_MINOR) {
        LOG_ERR("%s: Incompatible IDP version found \n", __func__);
        LOG_ERR("%s: Client version: %d.%d\n", __func__,
                NVMEDIA_IDP_VERSION_MAJOR, NVMEDIA_IDP_VERSION_MINOR);
        LOG_ERR("%s: Core version: %d.%d\n", __func__,
                version.major, version.minor);
        return NVMEDIA_STATUS_INCOMPATIBLE_VERSION;
    }

    return status;
}
