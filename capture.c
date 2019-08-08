/* Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "capture.h"
#include "save.h"
#include "os_common.h"

/* each 4bit 0 or 1 --> 1bit 0 or 1. eg. 0x1101 --> 0xD */
#define CAMMAP_4BITSTO_1BIT(a) \
               ((a & 0x01) + ((a >> 3) & 0x02) + ((a >> 6) & 0x04) + ((a >> 9) & 0x08))
/* each 4bit 0/1/2/3 --> 2bit 0/1/2/3. eg. 0x3210 --> 0xE4 */
#define CAMMAP_4BITSTO_2BITS(a) \
               ((a & 0x03) + ((a >> 2) & 0x0c) + ((a >> 4) & 0x30) + ((a >> 6) & 0xc0))
/* link i is enabled or not */
#define MAP_LINK_ENABLED(enable, i) ((enable >> 4*i) & 0x1)
/* link i out: 0 or 1 or 2 or 3 */
#define MAP_LINK_CSIOUT(csiOut, i) ((csiOut >> 4*i) & 0x3)
/* check if 4 links has any duplicated out number */
#define MAP_CHECK_DUPLICATED_OUT(csiOut) \
           (((MAP_LINK_CSIOUT(csiOut, 0) == MAP_LINK_CSIOUT(csiOut, 1)) || \
             (MAP_LINK_CSIOUT(csiOut, 0) == MAP_LINK_CSIOUT(csiOut, 2)) || \
             (MAP_LINK_CSIOUT(csiOut, 0) == MAP_LINK_CSIOUT(csiOut, 3)) || \
             (MAP_LINK_CSIOUT(csiOut, 1) == MAP_LINK_CSIOUT(csiOut, 2)) || \
             (MAP_LINK_CSIOUT(csiOut, 1) == MAP_LINK_CSIOUT(csiOut, 3)) || \
             (MAP_LINK_CSIOUT(csiOut, 2) == MAP_LINK_CSIOUT(csiOut, 3))) ? 1 : 0)

static NvMediaStatus
_DetermineCaptureStatus(NvMediaBool *captureFlag,
                        uint32_t frameNumber,
                        uint32_t numFramesToSkip,
                        uint32_t numFramesToWait,
                        uint32_t numMiniburstFrames)
{
    uint32_t offsetFrameNumber;
    uint32_t i;

    if (!captureFlag)
        return NVMEDIA_STATUS_BAD_PARAMETER;

    /* Frames in the beginning need to be skipped */
    if (frameNumber < numFramesToSkip) {
        *captureFlag = NVMEDIA_FALSE;
        return NVMEDIA_STATUS_OK;
    }

    /* Always capture frames if frame wait is 0 */
    if (!numFramesToWait) {
        *captureFlag = NVMEDIA_TRUE;
        return NVMEDIA_STATUS_OK;
    }

    offsetFrameNumber = frameNumber - numFramesToSkip;
    i = offsetFrameNumber % (numMiniburstFrames + numFramesToWait);

    /* Capturing mini burst frames */
    if (i < numMiniburstFrames)
        *captureFlag = NVMEDIA_TRUE;
    /* Waiting for frames to be captured */
    else
        *captureFlag = NVMEDIA_FALSE;

    return NVMEDIA_STATUS_OK;
}

static NvMediaStatus
_WriteCommandsToFile(FILE *fp,
                     I2cCommands *allCommands)
{
    uint32_t i = 0;

    for (i = 0; i < allCommands->numCommands; i++) {
        Command *cmd = &allCommands->commands[i];
        switch (cmd->commandType) {
            case DELAY:
            case I2C_DEVICE:
            case I2C_ERR:
            case SECTION_START:
            case SECTION_STOP:
                /* Do nothing */
                break;
            case WRITE_REG_1:
            case READ_REG_1:
                fprintf(fp, "%02x %02x %02x\n", cmd->deviceAddress << 1,
                        cmd->buffer[0], cmd->buffer[1]);
                break;
            case WRITE_REG_2:
            case READ_REG_2:
                fprintf(fp, "%02x %02x%02x %02x\n",
                        cmd->deviceAddress << 1, cmd->buffer[0],
                        cmd->buffer[1], cmd->buffer[2]);
                break;
            default:
                LOG_ERR("%s: Unknown command type encountered\n",
                        __func__);
                fclose(fp);
                return NVMEDIA_STATUS_ERROR;
        }
    }
    return NVMEDIA_STATUS_OK;
}

static NvMediaStatus
_ReadSensorRegisters(NvCaptureContext *ctx,
                     char *fileName)
{
    NvMediaStatus status;
    FILE *fp = fopen(fileName, "w");

    if (!fp) {
        LOG_ERR("%s: Failed to open file \"%s\"\n",__func__, fileName);
        return NVMEDIA_STATUS_ERROR;
    }

    /* Get register values from I2C for parsed commands */
    status = I2cProcessCommands(&ctx->parsedCommands,
                                I2C_READ,
                                ctx->i2cDeviceNum);
    if (status != NVMEDIA_STATUS_OK) {
        LOG_ERR("%s: Failed to read to registers over I2C\n", __func__);
        return NVMEDIA_STATUS_ERROR;
    }

    /* Get register values from I2C for sensor settings */
    status = I2cProcessCommands(&ctx->settingsCommands,
                                I2C_READ,
                                ctx->i2cDeviceNum);
    if (status != NVMEDIA_STATUS_OK) {
        LOG_ERR("%s: Failed to read to registers over I2C\n", __func__);
        return NVMEDIA_STATUS_ERROR;
    }

    /* Store values to file */

    fprintf(fp, "%s\n", "#All Registers");
    status = _WriteCommandsToFile(fp, &ctx->parsedCommands);
    if (status != NVMEDIA_STATUS_OK) {
        LOG_ERR("%s: Failed to write parsed registers to file.\n", __func__);
        return NVMEDIA_STATUS_ERROR;
    }

    fprintf(fp, "%s\n", "#Sensor Settings");
    status = _WriteCommandsToFile(fp, &ctx->settingsCommands);
    if (status != NVMEDIA_STATUS_OK) {
        LOG_ERR("%s: Failed to write settings registers to file.\n", __func__);
        return NVMEDIA_STATUS_ERROR;
    }

    fclose(fp);
    return NVMEDIA_STATUS_OK;
}

static NvMediaStatus
_ReadDeserRegisters(NvCaptureContext *captureCtx)
{
    I2cHandle i2cHandle;
    int i;
    uint8_t dbg_val[6] = "";

    if (testutil_i2c_open(captureCtx->i2cDeviceNum,
                         &i2cHandle) < 0) {
        printf("%s: i2c_open() failed\n", __func__);
        return NVMEDIA_STATUS_ERROR;
    }

    printf("\nDeserializer registers:\n-------------------\n");
    for (i = 0; i < 256; i++)
    {
        dbg_val[0] = (uint8_t)i;
        dbg_val[1] = 0;

        if (testutil_i2c_read_subaddr(i2cHandle,
                                     captureCtx->captureParams.deserAddress.uIntValue,
                                     dbg_val, 1, &dbg_val[1], 1) < 0) {
            printf("DEBUG: testutil_i2c_read_subaddr() failed to \
                    read register %d\n", i);
            return NVMEDIA_STATUS_ERROR;
        }
        else
            printf("DEBUG: %d = \t%02X\n", i, dbg_val[1]);
    }
    return NVMEDIA_STATUS_OK;
}

static NvMediaStatus
_CheckValidMap(MapInfo *camMap)
{
    unsigned int i, j, n;

    // cam_enable correct?
    if (camMap->enable != (camMap->enable & 0x1111)) {
       LOG_ERR("%s: WRONG command! cam_enable for each cam can only be 0 or 1! 0001 to 1111\n", __func__);
       return NVMEDIA_STATUS_ERROR;
    }

    // cam_mask correct?
    if (camMap->mask != (camMap->mask & 0x1111)) {
       LOG_ERR("%s: WRONG command! cam_mask for each cam can only be 0 or 1! 0000 to 1110\n", __func__);
       return NVMEDIA_STATUS_ERROR;
    }

    // cam_enable correct?
    if (camMap->csiOut != (camMap->csiOut & 0x3333)) {
       LOG_ERR("%s: WRONG command! csi_outmap for each cam can only be 0, 1, 2 or 3! For example: 3210\n", __func__);
       return NVMEDIA_STATUS_ERROR;
    }

    // To same csi out?
    if (MAP_CHECK_DUPLICATED_OUT(camMap->csiOut)) {
       LOG_ERR("%s: WRONG command! csi_outmap has same out number?\n", __func__);
       return NVMEDIA_STATUS_ERROR;
    }

    // If mask all enabled links?
    if (camMap->mask == camMap->enable) {
       LOG_ERR("%s: WRONG command! can not mask all enabled link(s)!\n", __func__);
       return NVMEDIA_STATUS_ERROR;
    }

    // Check enabled links csi_out, should start from 0, then 1, 2, 3
    n = MAP_COUNT_ENABLED_LINKS(camMap->enable);
    for (j = 0; j < n; j++) {
        for (i = 0; i < 4; i++) {
           if (MAP_LINK_ENABLED(camMap->enable, i)) {
               if ((MAP_LINK_CSIOUT(camMap->csiOut, i)) == j) {
                   break;
               }
           }
        }
        if (i == 4) {
           LOG_ERR("%s: WRONG command! csi_outmap didn't match cam_enable or aggregate!\n", __func__);
           return NVMEDIA_STATUS_ERROR;
        }
    }

   return NVMEDIA_STATUS_OK;
}

static NvMediaStatus
_CheckCamReMap(NvCaptureContext *captureCtx,
               MapInfo *camMap)
{
    NvMediaStatus status = NVMEDIA_STATUS_OK;
    I2cHandle i2cHandle = NULL;
    uint8_t address[2]={0};

    /* Check Deserializer device ID */
    if (testutil_i2c_open(captureCtx->i2cDeviceNum,
                          &i2cHandle) < 0) {
        LOG_ERR("%s: i2c_open() failed\n", __func__);
        return NVMEDIA_STATUS_ERROR;
    }

    address[0] = 0x1E;  /* MAX9286_DEVICE_ID_REGISTER: 0x1E */
    testutil_i2c_read_subaddr(i2cHandle,
                              captureCtx->captureParams.deserAddress.uIntValue,
                              address,
                              1,
                              &address[1],
                              1);

    if (address[1] == 0x40) {  /* MAX9286_DEVICE_ID: 0x40 */
        /* Check if camMap valid */
        status = _CheckValidMap(camMap);
        if (status != NVMEDIA_STATUS_OK)
            goto done;

        /* Set camera enable */
        address[0] = 0x00;  /* register 0x00 bit[3:0] for camera enable */
        address[1] = 0xE0 + CAMMAP_4BITSTO_1BIT(camMap->enable);
        testutil_i2c_write_subaddr(i2cHandle,
                                   captureCtx->captureParams.deserAddress.uIntValue,
                                   address,
                                   2);

        if (camMap->mask != CAM_MASK_DEFAULT) {
            /* Set camera mask */
            address[0] = 0x69;  /* register 0x69 bit[3:0] for camera mask */
            address[1] = CAMMAP_4BITSTO_1BIT(camMap->mask);
            testutil_i2c_write_subaddr(i2cHandle,
                                       captureCtx->captureParams.deserAddress.uIntValue,
                                       address,
                                       2);
        }

        if (camMap->csiOut != CSI_OUT_DEFAULT) {
            /* Set camera csi out map */
            address[0] = 0x0B;  /* register 0x0B bit[7:0] for camera csi out order */
            address[1] = CAMMAP_4BITSTO_2BITS(camMap->csiOut);
            testutil_i2c_write_subaddr(i2cHandle,
                                       captureCtx->captureParams.deserAddress.uIntValue,
                                       address,
                                       2);
        }
    } else
        LOG_DBG("%s: Camera mapping is not supported\n", __func__);
done:
    if (i2cHandle)
            testutil_i2c_close(i2cHandle);
    return status;
}

static NvMediaStatus
_SetInterfaceType(CaptureConfigParams *captureParams,
                  NvMediaICPInterfaceType *interfaceType,
                  NvMediaICPCsiPhyMode *phyMode)
{
    char *interface = captureParams->interface.stringValue;
    *phyMode = NVMEDIA_ICP_CSI_DPHY_MODE;

    /* Set interface type */
    if (!strcasecmp(interface,"csi-a"))
        *interfaceType = NVMEDIA_IMAGE_CAPTURE_CSI_INTERFACE_TYPE_CSI_A;
    else if (!strcasecmp(interface,"csi-b"))
        *interfaceType = NVMEDIA_IMAGE_CAPTURE_CSI_INTERFACE_TYPE_CSI_B;
    else if (!strcasecmp(interface,"csi-c"))
        *interfaceType = NVMEDIA_IMAGE_CAPTURE_CSI_INTERFACE_TYPE_CSI_C;
    else if (!strcasecmp(interface,"csi-d"))
        *interfaceType = NVMEDIA_IMAGE_CAPTURE_CSI_INTERFACE_TYPE_CSI_D;
    else if (!strcasecmp(interface,"csi-e"))
        *interfaceType = NVMEDIA_IMAGE_CAPTURE_CSI_INTERFACE_TYPE_CSI_E;
    else if (!strcasecmp(interface,"csi-f"))
        *interfaceType = NVMEDIA_IMAGE_CAPTURE_CSI_INTERFACE_TYPE_CSI_F;
    else if (!strcasecmp(interface,"csi-g"))
        *interfaceType = NVMEDIA_IMAGE_CAPTURE_CSI_INTERFACE_TYPE_CSI_G;
    else if (!strcasecmp(interface,"csi-h"))
        *interfaceType = NVMEDIA_IMAGE_CAPTURE_CSI_INTERFACE_TYPE_CSI_H;
    else if (!strcasecmp(interface,"csi-ab"))
        *interfaceType = NVMEDIA_IMAGE_CAPTURE_CSI_INTERFACE_TYPE_CSI_AB;
    else if (!strcasecmp(interface,"csi-cd"))
        *interfaceType = NVMEDIA_IMAGE_CAPTURE_CSI_INTERFACE_TYPE_CSI_CD;
    else if (!strcasecmp(interface,"csi-ef"))
        *interfaceType = NVMEDIA_IMAGE_CAPTURE_CSI_INTERFACE_TYPE_CSI_EF;
    else if (!strcasecmp(interface,"csi-gh"))
        *interfaceType = NVMEDIA_IMAGE_CAPTURE_CSI_INTERFACE_TYPE_CSI_GH;
    else if (!strcasecmp(interface,"trio-ab")) {
        *interfaceType = NVMEDIA_IMAGE_CAPTURE_CSI_INTERFACE_TYPE_CSI_AB;
        *phyMode = NVMEDIA_ICP_CSI_CPHY_MODE;
    }
    else if (!strcasecmp(interface,"trio-cd")) {
        *interfaceType = NVMEDIA_IMAGE_CAPTURE_CSI_INTERFACE_TYPE_CSI_CD;
        *phyMode = NVMEDIA_ICP_CSI_CPHY_MODE;
    }
    else if (!strcasecmp(interface,"trio-ef")) {
        *interfaceType = NVMEDIA_IMAGE_CAPTURE_CSI_INTERFACE_TYPE_CSI_EF;
        *phyMode = NVMEDIA_ICP_CSI_CPHY_MODE;
    }
    else if (!strcasecmp(interface,"trio-gh")) {
        *interfaceType = NVMEDIA_IMAGE_CAPTURE_CSI_INTERFACE_TYPE_CSI_GH;
        *phyMode = NVMEDIA_ICP_CSI_CPHY_MODE;
    }
    else {
        LOG_ERR("%s: Bad interface type specified: %s \n", __func__, interface);
        return NVMEDIA_STATUS_ERROR;
    }
    return NVMEDIA_STATUS_OK;
}

static NvMediaStatus
_SetICPSettings(CaptureThreadCtx *ctx,
                NvMediaICPSettings *icpSettings,
                CaptureConfigParams *captureParams,
                NvMediaICPInterfaceType interfaceType,
                NvMediaICPCsiPhyMode phyMode,
                TestArgs *testArgs)
{
    uint32_t width = 0 , height = 0;
    char *inputFormat = NULL;

    if (sscanf(captureParams->resolution.stringValue, "%ux%u",
               &width,
               &height) != 2) {
        LOG_ERR("%s: Invalid input resolution %s\n", __func__,
                captureParams->resolution.stringValue);
        return NVMEDIA_STATUS_BAD_PARAMETER;
    }
    NVM_SURF_FMT_DEFINE_ATTR(surfFormatAttrs);

    /* Set input and surface format */
    inputFormat = captureParams->inputFormat.stringValue;
    if (!strcasecmp(inputFormat, "422p")) {
        ctx->inputFormat.inputFormatType = NVMEDIA_IMAGE_CAPTURE_INPUT_FORMAT_TYPE_YUV422;
        NVM_SURF_FMT_SET_ATTR_YUV(surfFormatAttrs,YUV,422,PLANAR,UINT,8,PL);
        ctx->rawBytesPerPixel = 0;
    } else if (!strcasecmp(inputFormat, "rgb")) {
        ctx->inputFormat.inputFormatType = NVMEDIA_IMAGE_CAPTURE_INPUT_FORMAT_TYPE_RGB888;
        NVM_SURF_FMT_SET_ATTR_RGBA(surfFormatAttrs,RGBA,UINT,8,PL);
        ctx->rawBytesPerPixel = 0;
    } else if (!strcasecmp(inputFormat, "raw8")) {
        ctx->inputFormat.inputFormatType = NVMEDIA_IMAGE_CAPTURE_INPUT_FORMAT_TYPE_RAW8;
        ctx->inputFormat.bitsPerPixel = NVMEDIA_BITS_PER_PIXEL_8;
        NVM_SURF_FMT_SET_ATTR_RAW(surfFormatAttrs,RGGB,UINT,8,PL);
        surfFormatAttrs[NVM_SURF_ATTR_COMPONENT_ORDER].value +=  captureParams->pixelOrder.uIntValue;
        ctx->rawBytesPerPixel = 1;
        ctx->pixelOrder = captureParams->pixelOrder.uIntValue;
    } else if  (!strcasecmp(inputFormat, "raw10")) {
        ctx->inputFormat.inputFormatType = NVMEDIA_IMAGE_CAPTURE_INPUT_FORMAT_TYPE_RAW10;
        ctx->inputFormat.bitsPerPixel = NVMEDIA_BITS_PER_PIXEL_10;
        NVM_SURF_FMT_SET_ATTR_RAW(surfFormatAttrs,RGGB,UINT,10,PL);
        surfFormatAttrs[NVM_SURF_ATTR_COMPONENT_ORDER].value +=  captureParams->pixelOrder.uIntValue;
        ctx->rawBytesPerPixel = 2;
        ctx->pixelOrder =  captureParams->pixelOrder.uIntValue;
    } else if (!strcasecmp(inputFormat, "raw12")) {
        ctx->inputFormat.inputFormatType = NVMEDIA_IMAGE_CAPTURE_INPUT_FORMAT_TYPE_RAW12;
        ctx->inputFormat.bitsPerPixel = NVMEDIA_BITS_PER_PIXEL_12;
        NVM_SURF_FMT_SET_ATTR_RAW(surfFormatAttrs,RGGB,UINT,12,PL);
        surfFormatAttrs[NVM_SURF_ATTR_COMPONENT_ORDER].value +=  captureParams->pixelOrder.uIntValue;
        ctx->rawBytesPerPixel = 2;
        ctx->pixelOrder =  captureParams->pixelOrder.uIntValue;
    } else if (!strcasecmp(inputFormat, "raw16")) {
        ctx->inputFormat.inputFormatType = NVMEDIA_IMAGE_CAPTURE_INPUT_FORMAT_TYPE_RAW16;
        ctx->inputFormat.bitsPerPixel = NVMEDIA_BITS_PER_PIXEL_16;
        NVM_SURF_FMT_SET_ATTR_RAW(surfFormatAttrs,RGGB,UINT,16,PL);
        surfFormatAttrs[NVM_SURF_ATTR_COMPONENT_ORDER].value += captureParams->pixelOrder.uIntValue;
        ctx->rawBytesPerPixel = 2;
        ctx->pixelOrder =  captureParams->pixelOrder.uIntValue;
    /* @@@@ ----------------- FLIR BOSON ONLY ------------------ */
    } else if (!strcasecmp(inputFormat, "raw14")) {
        ctx->inputFormat.inputFormatType = NVMEDIA_IMAGE_CAPTURE_INPUT_FORMAT_TYPE_RAW14;

        NVM_SURF_FMT_SET_ATTR_RAW(surfFormatAttrs,RGGB,UINT,14,PL);
        surfFormatAttrs[NVM_SURF_ATTR_COMPONENT_ORDER].value +=  captureParams->pixelOrder.uIntValue;


        // ctx->inputFormat.bitsPerPixel = NVMEDIA_BITS_PER_PIXEL_14;   // @@@@ Boson is 14 bits
        // ctx->pixelOrder = captureParams->pixelOrder.uIntValue;
        // ctx->surfFormat.surfaceFormatType = NVMEDIA_IMAGE_CAPTURE_SURFACE_FORMAT_TYPE_RAW;
        // ctx->surfFormat.bitsPerPixel = NVMEDIA_BITS_PER_PIXEL_14;    // @@@@ Boson is 14 bits
        // ctx->surfFormat.pixelOrder = captureParams->pixelOrder.uIntValue;
        // ctx->surfType = NvMediaSurfaceType_Image_RAW;
        // ctx->surfAttributes |= NVMEDIA_IMAGE_ATTRIBUTE_BITS_PER_PIXEL;
        // ctx->surfAttributes |= NVMEDIA_IMAGE_ATTRIBUTE_RAW_PIXEL_ORDER;
        // ctx->surfAdvConfig.bitsPerPixel = NVMEDIA_BITS_PER_PIXEL_14;    // @@@@ Boson is 14 bits
        // ctx->surfAdvConfig.pixelOrder = captureParams->pixelOrder.uIntValue;

    /* @@@@ END -------------- FLIR BOSON ONLY ------------------ */
    } else if (!strcasecmp(inputFormat, "raw20")) {
        ctx->inputFormat.inputFormatType = NVMEDIA_IMAGE_CAPTURE_INPUT_FORMAT_TYPE_RAW20;
        ctx->inputFormat.bitsPerPixel = NVMEDIA_BITS_PER_PIXEL_20;
        NVM_SURF_FMT_SET_ATTR_RAW(surfFormatAttrs,RGGB,UINT,20,PL);
        surfFormatAttrs[NVM_SURF_ATTR_COMPONENT_ORDER].value +=  captureParams->pixelOrder.uIntValue;
        ctx->rawBytesPerPixel = 4;
        ctx->pixelOrder =  captureParams->pixelOrder.uIntValue;
    } else {
        LOG_ERR("%s: Bad input format specified: %s \n",
                __func__, inputFormat);
        return NVMEDIA_STATUS_BAD_PARAMETER;
    }
    ctx->surfType = NvMediaSurfaceFormatGetType(surfFormatAttrs, NVM_SURF_FMT_ATTR_MAX);

    /* Set NvMediaICPSettings */
    icpSettings->interfaceType = interfaceType;
    memcpy(&icpSettings->inputFormat, &ctx->inputFormat, sizeof(NvMediaICPInputFormat));
    icpSettings->width = (testArgs->useVirtualChannels)?
                          width : width * testArgs->numSensors;

    icpSettings->height = height;
    icpSettings->startX = 0;
    icpSettings->startY = 0;
    icpSettings->embeddedDataType = captureParams->emb.isUsed;
    icpSettings->embeddedDataLines = (captureParams->emb.isUsed == NVMEDIA_TRUE) ?
                                        captureParams->emb.uIntValue : 0;
    icpSettings->interfaceLanes = captureParams->csiLanes.uIntValue;
    icpSettings->surfaceType = ctx->surfType;
    icpSettings->phyMode = phyMode;

    /* Set SurfaceAllocAttrs */
    ctx->surfAllocAttrs[0].type = NVM_SURF_ATTR_WIDTH;
    ctx->surfAllocAttrs[0].value = icpSettings->width;
    ctx->surfAllocAttrs[1].type = NVM_SURF_ATTR_HEIGHT;
    ctx->surfAllocAttrs[1].value = icpSettings->height;
    ctx->surfAllocAttrs[2].type = NVM_SURF_ATTR_EMB_LINES_TOP;
    ctx->surfAllocAttrs[2].value = icpSettings->embeddedDataLines;
    ctx->surfAllocAttrs[3].type = NVM_SURF_ATTR_EMB_LINES_BOTTOM;
    ctx->surfAllocAttrs[3].value = 0;
    ctx->surfAllocAttrs[4].type = NVM_SURF_ATTR_CPU_ACCESS;
    ctx->surfAllocAttrs[4].value = NVM_SURF_ATTR_CPU_ACCESS_CACHED;
    ctx->surfAllocAttrs[5].type = NVM_SURF_ATTR_ALLOC_TYPE;
    ctx->surfAllocAttrs[5].value = NVM_SURF_ATTR_ALLOC_ISOCHRONOUS;
    ctx->numSurfAllocAttrs = 6;
    return NVMEDIA_STATUS_OK;
}

static NvMediaStatus
_CreateImageQueue(NvMediaDevice *device,
                  NvQueue **queue,
                  uint32_t queueSize,
                  uint32_t width,
                  uint32_t height,
                  NvMediaSurfaceType surfType,
                  NvMediaSurfAllocAttr *surfAllocAttrs,
                  uint32_t numSurfAllocAttrs)
{
    uint32_t j = 0;
    NvMediaImage *image = NULL;
    NvMediaStatus status = NVMEDIA_STATUS_OK;

    if (NvQueueCreate(queue,
                      queueSize,
                      sizeof(NvMediaImage *)) != NVMEDIA_STATUS_OK) {
       LOG_ERR("%s: Failed to create image Queue \n", __func__);
       goto failed;
    }

    for (j = 0; j < queueSize; j++) {
        LOG_DBG("%s: NvMediaImageCreateNew\n", __func__);
        image =  NvMediaImageCreateNew(device,           // device
                                    surfType,           // NvMediaSurfaceType type
                                    surfAllocAttrs,     // surf allocation attrs
                                    numSurfAllocAttrs,  // num attrs
                                    0);                 // flags
        if (!image) {
            LOG_ERR("%s: NvMediaImageCreate failed for image %d",
                        __func__, j);
            status = NVMEDIA_STATUS_ERROR;
            goto failed;
        }

        image->tag = *queue;

        if (IsFailed(NvQueuePut(*queue,
                                (void *)&image,
                                NV_TIMEOUT_INFINITE))) {
            LOG_ERR("%s: Pushing image to image queue failed\n", __func__);
            status = NVMEDIA_STATUS_ERROR;
            goto failed;
        }
    }

    return NVMEDIA_STATUS_OK;
failed:
    return status;
}

static uint32_t
_CaptureThreadFunc(void *data)
{
    CaptureThreadCtx *threadCtx = (CaptureThreadCtx *)data;
    uint32_t i = 0, totalCapturedFrames = 0, lastCapturedFrame = 0;
    NvMediaBool startCapture = NVMEDIA_FALSE;
    NvMediaImage *capturedImage = NULL;
    NvMediaImage *feedImage = NULL;
    NvMediaStatus status;
    uint64_t tbegin = 0, tend = 0, fps;
    NvMediaICP *icpInst = NULL;
    uint32_t retry = 0;

    for (i = 0; i < threadCtx->icpExCtx->numVirtualGroups; i++) {
        if (threadCtx->icpExCtx->icp[i].virtualGroupId == threadCtx->virtualGroupIndex) {
            icpInst = NVMEDIA_ICP_HANDLER(threadCtx->icpExCtx,i);
            break;
        }
    }
    if (!icpInst) {
        LOG_ERR("%s: Failed to get icpInst for virtual channel %d\n", __func__,
                threadCtx->virtualGroupIndex);
        goto done;
    }

    while (!(*threadCtx->quit)) {
        status = _DetermineCaptureStatus(&startCapture,
                                         i,
                                         threadCtx->numFramesToSkip,
                                         threadCtx->numFramesToWait,
                                         threadCtx->numMiniburstFrames);
        if (status != NVMEDIA_STATUS_OK) {
            LOG_ERR("%s: CaptureDetermineStatus failed\n", __func__);
            *threadCtx->quit = NVMEDIA_TRUE;
            goto done;
        }

        /* Set current frame to be an offset by frames to skip */
        if (startCapture)
            threadCtx->currentFrame = i - threadCtx->numFramesToSkip;

        /* Feed all images to image capture object from the input Queue */
        while (NvQueueGet(threadCtx->inputQueue,
                          &feedImage,
                          0) == NVMEDIA_STATUS_OK) {

            status = NvMediaICPFeedFrame(icpInst,
                                         feedImage,
                                         CAPTURE_FEED_FRAME_TIMEOUT);
            if (status != NVMEDIA_STATUS_OK) {
                LOG_ERR("%s: %d: NvMediaICPFeedFrame failed\n", __func__, __LINE__);
                if (NvQueuePut((NvQueue *)feedImage->tag,
                               (void *)&feedImage,
                               0) != NVMEDIA_STATUS_OK) {
                    LOG_ERR("%s: Failed to put image back into capture input queue", __func__);
                    *threadCtx->quit = NVMEDIA_TRUE;
                    status = NVMEDIA_STATUS_ERROR;
                    goto done;
                }
                feedImage = NULL;
                *threadCtx->quit = NVMEDIA_TRUE;
                goto done;
            }
            feedImage = NULL;
        }

        /* Get captured frame */
        status = NvMediaICPGetFrameEx(icpInst,
                                      CAPTURE_GET_FRAME_TIMEOUT,
                                      &capturedImage);
        switch (status) {
            case NVMEDIA_STATUS_OK:
                retry = 0;
                break;
            case NVMEDIA_STATUS_TIMED_OUT:
                LOG_WARN("%s: NvMediaICPGetFrameEx timed out\n", __func__);
                if (++retry > CAPTURE_MAX_RETRY) {
                    LOG_ERR("%s: keep failing at NvMediaICPGetFrameEx for %d times\n", __func__, retry);
                    /* @@@@ ----------------- FLIR BOSON ONLY ------------------ */
                    // This needs to be addresed. Main issue is that Surface is a quarter of the image.
                    // This could be causing these errors? Requires further investigation. Capturing and not Displaying works.
    
                    /* Stop ICP to release all the buffer fed so far */
                    //@@@@ NvMediaICPStop(icpInst);
                    //@@@@ *threadCtx->quit = NVMEDIA_TRUE;
                    //@@@@ goto done;

                    /* @@@@ END -------------- FLIR BOSON ONLY ------------------ */
                    retry=0;
                }
                continue;
            case NVMEDIA_STATUS_INSUFFICIENT_BUFFERING:
                LOG_WARN("%s: NvMediaICPGetFrameEx failed as no frame buffers are available for capture."
                         "Please feed frames using NvMediaICPFeedFrame\n", __func__);
                continue;
            case NVMEDIA_STATUS_ERROR:
            default:
                LOG_ERR("%s: NvMediaICPGetFrameEx failed\n", __func__);
                *threadCtx->quit = NVMEDIA_TRUE;
                goto done;
        }

        GetTimeMicroSec(&tend);
        uint64_t td = tend - tbegin;
        if (td > 3000000) {
            fps = (int)(totalCapturedFrames-lastCapturedFrame)*(1000000.0/td);

            tbegin = tend;
            lastCapturedFrame = totalCapturedFrames;
            LOG_INFO("%s: VC:%d FPS=%d delta=%lld", __func__,
                     threadCtx->virtualGroupIndex, fps, td);
        }

        /* push the captured image onto output queue */
        if (startCapture) {
            status = NvQueuePut(threadCtx->outputQueue,
                                (void *)&capturedImage,
                                CAPTURE_ENQUEUE_TIMEOUT);
            if (status != NVMEDIA_STATUS_OK) {
                LOG_INFO("%s: Failed to put image onto capture output queue", __func__);
                goto done;
            }

            totalCapturedFrames++;
        } else {
            status = NvQueuePut((NvQueue *)capturedImage->tag,
                                (void *)&capturedImage,
                                0);
            if (status != NVMEDIA_STATUS_OK) {
                LOG_ERR("%s: Failed to put image back into capture input queue", __func__);
                *threadCtx->quit = NVMEDIA_TRUE;
                goto done;
            }

        }
        capturedImage = NULL;
done:
        if (capturedImage) {
            status = NvQueuePut((NvQueue *)capturedImage->tag,
                                (void *)&capturedImage,
                                0);
            if (status != NVMEDIA_STATUS_OK) {
                LOG_ERR("%s: Failed to put image back into capture input queue", __func__);
                *threadCtx->quit = NVMEDIA_TRUE;
            }
            capturedImage = NULL;
        }
        i++;

        /* To stop capturing if specified number of frames are captured */
        if (threadCtx->numFramesToCapture && startCapture &&
           (totalCapturedFrames == threadCtx->numFramesToCapture))
            break;
    }

    /* Release all the frames which are fed */
    while (NvMediaICPReleaseFrame(icpInst, &capturedImage) == NVMEDIA_STATUS_OK) {
        if (capturedImage) {
            status = NvQueuePut((NvQueue *)capturedImage->tag,
                                (void *)&capturedImage,
                                0);
            if (status != NVMEDIA_STATUS_OK) {
                LOG_ERR("%s: Failed to put image back into input queue", __func__);
                break;
            }
        }
        capturedImage = NULL;
    }
    NvMediaICPStop(icpInst);

    LOG_INFO("%s: Capture thread exited\n", __func__);
    threadCtx->exitedFlag = NVMEDIA_TRUE;
    return NVMEDIA_STATUS_OK;
}

NvMediaStatus
CaptureInit(NvMainContext *mainCtx)
{
    NvCaptureContext *captureCtx = NULL;
    NvMediaStatus status;
    TestArgs *testArgs = mainCtx->testArgs;
    uint32_t i = 0;

    if (!mainCtx) {
        LOG_ERR("%s: Bad parameter", __func__);
        return NVMEDIA_STATUS_BAD_PARAMETER;
    }

    mainCtx->ctxs[CAPTURE_ELEMENT]= malloc(sizeof(NvCaptureContext));
    if (!mainCtx->ctxs[CAPTURE_ELEMENT]) {
        LOG_ERR("%s: Failed to allocate memory for capture context\n", __func__);
        return NVMEDIA_STATUS_OUT_OF_MEMORY;
    }

    captureCtx = mainCtx->ctxs[CAPTURE_ELEMENT];
    memset(captureCtx, 0, sizeof(NvCaptureContext));

    /* initialize capture context */
    captureCtx->quit = &mainCtx->quit;
    captureCtx->testArgs  = testArgs;
    captureCtx->numSensors = testArgs->numSensors;
    captureCtx->numVirtualChannels = testArgs->numVirtualChannels;
    captureCtx->sensorInfo = testArgs->sensorInfo;
    captureCtx->inputQueueSize = testArgs->bufferPoolSize;
    captureCtx->crystalFrequency = testArgs->crystalFrequency;
    captureCtx->useNvRawFormat = testArgs->useNvRawFormat;
    captureCtx->sensorInfo = testArgs->sensorInfo;

    /* Parse registers file */
    if (testArgs->wrregs.isUsed) {
        status = ParseRegistersFile(testArgs->wrregs.stringValue,
                                    &captureCtx->captureParams,
                                    &captureCtx->parsedCommands);
        if (status != NVMEDIA_STATUS_OK) {
            LOG_ERR("%s: Failed to parse register file\n",__func__);
            goto failed;
        }
    }
    captureCtx->i2cDeviceNum = captureCtx->captureParams.i2cDevice.uIntValue;
    captureCtx->calParams.i2cDevice = captureCtx->i2cDeviceNum;
    captureCtx->calParams.sensorAddress = captureCtx->captureParams.sensorAddress.uIntValue;
    captureCtx->calParams.crystalFrequency = captureCtx->crystalFrequency;

    /* Create NvMedia Device */
    captureCtx->device = NvMediaDeviceCreate();
    if (!captureCtx->device) {
        status = NVMEDIA_STATUS_ERROR;
        LOG_ERR("%s: Failed to create NvMedia device\n", __func__);
        goto failed;
    }

    /* Set NvMediaICPSettingsEx */

    status = _SetInterfaceType(&captureCtx->captureParams,
                               &captureCtx->interfaceType,
                               &captureCtx->phyMode);
    if (status != NVMEDIA_STATUS_OK) {
        LOG_ERR("%s: Failed to set interface type \n", __func__);
        goto failed;
    }

    captureCtx->icpSettingsEx.numVirtualGroups = testArgs->numVirtualChannels;
    captureCtx->icpSettingsEx.interfaceType = captureCtx->interfaceType;
    captureCtx->icpSettingsEx.interfaceLanes = captureCtx->captureParams.csiLanes.uIntValue;
    captureCtx->icpSettingsEx.phyMode = captureCtx->phyMode;

    for (i = 0; i < captureCtx->icpSettingsEx.numVirtualGroups; i++) {
        captureCtx->icpSettingsEx.virtualGroups[i].numVirtualChannels = 1;
        captureCtx->icpSettingsEx.virtualGroups[i].virtualChannels[0].virtualChannelIndex = i;
        status = _SetICPSettings(&captureCtx->threadCtx[i],
                                 NVMEDIA_ICP_SETTINGS_HANDLER(captureCtx->icpSettingsEx, i, 0),
                                 &captureCtx->captureParams,
                                 captureCtx->interfaceType,
                                 captureCtx->phyMode,
                                 testArgs);
        if (status != NVMEDIA_STATUS_OK) {
            LOG_ERR("%s: Failed to set ICP settings\n", __func__);
            goto failed;
        }
    }

    /* Power up SER-DES and Cameras, it makes CSI lanes to LP mode */
    if (!testArgs->disablePwrCtrl) {
        /* Create NvMediaISC object to power on cameras */
        captureCtx->iscCtx =
            NvMediaISCRootDeviceCreate(
                ISC_RDEV_CFG(captureCtx->interfaceType,
                             captureCtx->i2cDeviceNum)); /* port */
        if (!captureCtx->iscCtx) {
            LOG_ERR("%s: Failed to create NvMedia ISC root device\n", __func__);
            status = NVMEDIA_STATUS_ERROR;
            goto failed;
        }

        /* Delay for 50ms in order to let sensor power on*/
        nvsleep(50000);
    }

    /* Write pre-requsite registers over i2c */
    status = I2cProcessInitialRegisters(&captureCtx->parsedCommands,
                                        captureCtx->i2cDeviceNum);
    if (status != NVMEDIA_STATUS_OK) {
        LOG_ERR("%s: Failed to write to initial registers over I2C\n", __func__);
        goto failed;
    }

    LOG_DBG("%s: Creating ICP context\n", __func__);

    /* Create NvMediaICPEx object */
    captureCtx->icpExCtx = NvMediaICPCreateEx(&captureCtx->icpSettingsEx);
    if (!captureCtx->icpExCtx) {
        LOG_ERR("%s: NvMediaICPCreateEx failed\n", __func__);
        status = NVMEDIA_STATUS_ERROR;
        goto failed;
    }

    /* Write registers from script file over i2c */
    status = I2cProcessCommands(&captureCtx->parsedCommands,
                                I2C_WRITE,
                                captureCtx->i2cDeviceNum);
    if (status != NVMEDIA_STATUS_OK) {
        LOG_ERR("%s: Failed to write to registers over I2C\n", __func__);
        goto failed;
    }

    /* Camera enable, mask or re-map if needed */
    if ((testArgs->camMap.enable != CAM_ENABLE_DEFAULT) ||
       (testArgs->camMap.mask != CAM_MASK_DEFAULT) ||
       (testArgs->camMap.csiOut != CSI_OUT_DEFAULT)) {
        status = _CheckCamReMap(captureCtx,
                                &testArgs->camMap);
        if (status != NVMEDIA_STATUS_OK) {
            LOG_ERR("%s: Failed to re-map, mask or change csi-out order of the cameras\n", __func__);
            goto failed;
        }
    }

    /* Calibrate sensor if needed */
    if (testArgs->calibrateSensorFlag && captureCtx->sensorInfo) {
        /* Populate sensor properties */
        status = captureCtx->sensorInfo->CalibrateSensor(&captureCtx->settingsCommands,
                                                         &captureCtx->calParams,
                                                         testArgs->sensorProperties);
        if (status != NVMEDIA_STATUS_OK) {
            LOG_ERR("%s: Failed to CalibrateSensor, check calibration values \n", __func__);
            goto failed;
        }
        /* Apply calibration settings */
        status = I2cProcessCommands(&captureCtx->settingsCommands,
                                    I2C_WRITE,
                                    captureCtx->calParams.i2cDevice);
        if (status != NVMEDIA_STATUS_OK) {
            LOG_ERR("%s: Failed to process register settings\n", __func__);
            goto failed;
        }
    }

    /* Create Input Queues and set data for capture threads */
    for (i = 0; i < captureCtx->numVirtualChannels; i++) {

        captureCtx->threadCtx[i].icpExCtx = captureCtx->icpExCtx;
        captureCtx->threadCtx[i].quit = captureCtx->quit;
        captureCtx->threadCtx[i].exitedFlag = NVMEDIA_TRUE;
        captureCtx->threadCtx[i].virtualGroupIndex = i;
        captureCtx->threadCtx[i].numFramesToCapture = (testArgs->frames.isUsed)?
                                                       testArgs->frames.uIntValue : 0;
        captureCtx->threadCtx[i].numFramesToSkip = testArgs->numFramesToSkip;
        captureCtx->threadCtx[i].numFramesToWait = testArgs->numFramesToWait;
        captureCtx->threadCtx[i].numMiniburstFrames = 1;
        captureCtx->threadCtx[i].width  = NVMEDIA_ICP_SETTINGS_HANDLER(captureCtx->icpSettingsEx, i, 0)->width;
        captureCtx->threadCtx[i].height = NVMEDIA_ICP_SETTINGS_HANDLER(captureCtx->icpSettingsEx, i, 0)->height;
        captureCtx->threadCtx[i].settings = NVMEDIA_ICP_SETTINGS_HANDLER(captureCtx->icpSettingsEx, i, 0);
        captureCtx->threadCtx[i].numBuffers = captureCtx->inputQueueSize;

        /* Create inputQueue for storing captured Images */
        status = _CreateImageQueue(captureCtx->device,
                                   &captureCtx->threadCtx[i].inputQueue,
                                   captureCtx->inputQueueSize,
                                   captureCtx->threadCtx[i].width,
                                   captureCtx->threadCtx[i].height,
                                   captureCtx->threadCtx[i].surfType,
                                   captureCtx->threadCtx[i].surfAllocAttrs,
                                   captureCtx->threadCtx[i].numSurfAllocAttrs);
        if (status != NVMEDIA_STATUS_OK) {
            LOG_ERR("%s: capture InputQueue %d creation failed\n", __func__, i);
            goto failed;
        }

        LOG_DBG("%s: Capture Input Queue %d: %ux%u, images: %u \n",
                __func__, i, captureCtx->threadCtx[i].width,
                captureCtx->threadCtx[i].height,
                captureCtx->inputQueueSize);
    }

    return NVMEDIA_STATUS_OK;
failed:
    LOG_ERR("%s: Failed to initialize Capture\n", __func__);
    return status;
}

NvMediaStatus
CaptureFini(NvMainContext *mainCtx)
{
    NvCaptureContext *captureCtx = NULL;
    NvMediaImage *image = NULL;
    NvMediaStatus status;
    uint32_t i = 0;

    if (!mainCtx)
        return NVMEDIA_STATUS_OK;

    captureCtx = mainCtx->ctxs[CAPTURE_ELEMENT];
    if (!captureCtx)
        return NVMEDIA_STATUS_OK;

    /* Wait for threads to exit */
    for (i = 0; i < captureCtx->numVirtualChannels; i++) {
        if (captureCtx->captureThread[i]) {
            while (!captureCtx->threadCtx[i].exitedFlag) {
                LOG_DBG("%s: Waiting for capture thread %d to quit\n",
                        __func__, i);
            }
        }
    }

    *captureCtx->quit = NVMEDIA_TRUE;

    /* Destroy threads */
    for (i = 0; i < captureCtx->numVirtualChannels; i++) {
        if (captureCtx->captureThread[i]) {
            status = NvThreadDestroy(captureCtx->captureThread[i]);
            if (status != NVMEDIA_STATUS_OK)
                LOG_ERR("%s: Failed to destroy capture thread %d\n",
                        __func__, i);
        }
    }

    /* Destroy input queues */
    for (i = 0; i < captureCtx->numVirtualChannels; i++) {
        if (captureCtx->threadCtx[i].inputQueue) {
            while ((NvQueueGet(captureCtx->threadCtx[i].inputQueue, &image,
                        0)) == NVMEDIA_STATUS_OK) {
                if (image) {
                    NvMediaImageDestroy(image);
                    image = NULL;
                }
            }
            LOG_DBG("%s: Destroying capture input queue %d \n", __func__, i);
            NvQueueDestroy(captureCtx->threadCtx[i].inputQueue);
        }
    }

    /* Destroy sensor properties */
    if(captureCtx->testArgs->sensorProperties)
        free(captureCtx->testArgs->sensorProperties);

    /* Read Sensor Registers */
    if (captureCtx->testArgs->rdregs.isUsed) {
        status = _ReadSensorRegisters(captureCtx, captureCtx->testArgs->rdregs.stringValue);
        if (status != NVMEDIA_STATUS_OK) {
            LOG_ERR("%s: Failed to read sensor registers\n", __func__);
        }
    }

    /* Print the deserializer registers and debug status registers*/
    if (captureCtx->testArgs->logLevel == LEVEL_DBG && captureCtx->captureParams.deserAddress.isUsed) {
        _ReadDeserRegisters(captureCtx);
    }

    /* Destroy contexts */
    if (captureCtx->icpExCtx)
        NvMediaICPDestroyEx(captureCtx->icpExCtx);

    if (captureCtx->iscCtx)
        NvMediaISCRootDeviceDestroy(captureCtx->iscCtx);

    if (captureCtx->device)
        NvMediaDeviceDestroy(captureCtx->device);

    if (captureCtx)
        free(captureCtx);

    LOG_INFO("%s: CaptureFini done\n", __func__);

    return NVMEDIA_STATUS_OK;
}

NvMediaStatus
CaptureProc(NvMainContext *mainCtx)
{
    NvMediaStatus status;
    uint32_t i=0;

    if (!mainCtx) {
        LOG_ERR("%s: Bad parameter\n", __func__);
        return NVMEDIA_STATUS_ERROR;
    }

    NvCaptureContext *captureCtx  = mainCtx->ctxs[CAPTURE_ELEMENT];
    NvSaveContext    *saveCtx     = mainCtx->ctxs[SAVE_ELEMENT];

    /* Setting the queues */
    for (i = 0; i < captureCtx->numVirtualChannels; i++) {
        CaptureThreadCtx *threadCtx = &captureCtx->threadCtx[i];
        if (threadCtx)
            threadCtx->outputQueue = saveCtx->threadCtx[i].inputQueue;
    }

    /* Create capture threads */
    for (i = 0; i < captureCtx->numVirtualChannels; i++) {
        CaptureThreadCtx *threadCtx = &captureCtx->threadCtx[i];
        if (threadCtx) {
            threadCtx->exitedFlag = NVMEDIA_FALSE;
            status = NvThreadCreate(&captureCtx->captureThread[i],
                                    &_CaptureThreadFunc,
                                    (void *)threadCtx,
                                    NV_THREAD_PRIORITY_NORMAL);
            if (status != NVMEDIA_STATUS_OK) {
                LOG_ERR("%s: Failed to create captureThread %d\n",
                        __func__, i);
                threadCtx->exitedFlag = NVMEDIA_TRUE;
                goto failed;
            }
        }
    }
failed:
    return status;
}
