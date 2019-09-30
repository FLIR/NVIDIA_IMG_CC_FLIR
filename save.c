/* Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <limits.h>
#include <math.h>

#include "capture.h"
#include "save.h"
#include "composite.h"

#define CONV_GET_X_OFFSET(xoffsets, red, green1, green2, blue) \
            xoffsets[red] = 0;\
            xoffsets[green1] = 1;\
            xoffsets[green2] = 0;\
            xoffsets[blue] = 1;

#define CONV_GET_Y_OFFSET(yoffsets, red, green1, green2, blue) \
            yoffsets[red] = 0;\
            yoffsets[green1] = 0;\
            yoffsets[green2] = 1;\
            yoffsets[blue] = 1;

#define CONV_CALCULATE_PIXEL(pSrcBuff, srcPitch, x, y, xOffset, yOffset) \
            (pSrcBuff[srcPitch*(y + yOffset) + 2*(x + xOffset) + 1] << 2) | \
            (pSrcBuff[srcPitch*(y + yOffset) + 2*(x + xOffset)] >> 6)

#define CONV_CALCULATE_PIXEL_UINT(pSrcBuff, srcPitch, x, y, xOffset, yOffset) \
            pSrcBuff[srcPitch*(y + yOffset) + 2*(x + xOffset) + 1]

enum PixelColor {
    RED,
    GREEN1,
    GREEN2,
    BLUE,
    NUM_PIXEL_COLORS
};

/* @@@@ ----------------- FLIR BOSON ONLY ------------------ */

// We are  reordering the bits
// Boson Data: x.x.N4.N4:N3.N3.N3.N3 - N2.N2.N2.N2:N1.N1.N1.N1
// byteH -> N2.N2.N3.N3:N3.N3.N4.N4  (has the most significant bits)
// byteL -> x.x.N1.N1:N1.N1.N2.N2  (has the less significant bits)
short int reverse_16bits(unsigned char byteH, unsigned char byteL) {
    short int valor;
    int x=0;

    valor = 0;

    for (x=0; x<8; x++) {
        valor   = ( valor << 1) + ( byteH & 0x01 ) ;
        byteH = ( byteH >> 1) ;
    }
    for (x=0; x<6; x++) {
        valor   = ( valor << 1) + ( byteL & 0x01 ) ;
        byteL = ( byteL >> 1) ;
    }

    return valor;

}

void write_raw_to_file(uint8_t *data, int stride, int height, 
    char *filename)
{
    FILE *f = fopen(filename, "w+");
    fwrite(data, sizeof(uint8_t), stride * height, f);
}

uint8_t raw12_to_byte(uint8_t *data, int idx) {
    uint16_t pixel = (data[idx + 1] << 8) + data[idx];
    return (pixel >> 4) & 0xFF;
}
/* @@@@ END -------------- FLIR BOSON ONLY ------------------ */


static NvMediaStatus
_ConvGetPixelOffsets(NvMediaRawPixelOrder pixelOrder,
                     uint32_t *xOffsets,
                     uint32_t *yOffsets)
{
    if (!xOffsets || !yOffsets)
        return NVMEDIA_STATUS_BAD_PARAMETER;

    switch (pixelOrder) {
        case NVMEDIA_RAW_PIXEL_ORDER_RGGB:
            CONV_GET_X_OFFSET(xOffsets, RED, GREEN1, GREEN2, BLUE);
            CONV_GET_Y_OFFSET(yOffsets, RED, GREEN1, GREEN2, BLUE);
            break;
        case NVMEDIA_RAW_PIXEL_ORDER_GRBG:
            CONV_GET_X_OFFSET(xOffsets, GREEN1, RED, BLUE, GREEN2);
            CONV_GET_Y_OFFSET(yOffsets, GREEN1, RED, BLUE, GREEN2);
            break;
        case NVMEDIA_RAW_PIXEL_ORDER_GBRG:
            CONV_GET_X_OFFSET(xOffsets, GREEN1, BLUE, RED, GREEN2);
            CONV_GET_Y_OFFSET(yOffsets, GREEN1, BLUE, RED, GREEN2);
            break;
        case NVMEDIA_RAW_PIXEL_ORDER_BGGR:
        default:
            CONV_GET_X_OFFSET(xOffsets, BLUE, GREEN1, GREEN2, RED);
            CONV_GET_Y_OFFSET(yOffsets, BLUE, GREEN1, GREEN2, RED);
            break;
    }

    return NVMEDIA_STATUS_OK;
}

static NvMediaStatus
_ConvRawToRgba(NvMediaImage *imgSrc,
               NvMediaImage *imgDst,
               uint32_t rawBytesPerPixel,
               uint32_t pixelOrder)
{
    NvMediaImageSurfaceMap surfaceMap;
    uint32_t srcImageSize = 0, srcWidth, srcHeight;
    uint32_t dstImageSize = 0, dstWidth, dstHeight;
    uint8_t *pSrcBuff = NULL, *pDstBuff = NULL, *pTmp = NULL;
    uint32_t srcPitch = 0, dstPitch = 0;
    NvMediaStatus status;
    uint8_t alpha = 0xFF;
    uint32_t x = 0, y = 0;
    uint32_t xOffsets[NUM_PIXEL_COLORS] = {0}, yOffsets[NUM_PIXEL_COLORS] = {0};

    /* @@@@ ----------------- FLIR BOSON ONLY ------------------ */
    /* @@@@ Auxiliary vars to re-order bits and perform simple AGC */
    unsigned short value;
    int min=0xFFFF;
    int max=0;
    unsigned char tmpH, tmpL;
    /* @@@@ END -------------- FLIR BOSON ONLY ------------------ */
    

    NVM_SURF_FMT_DEFINE_ATTR(srcAttr);
    NVM_SURF_FMT_DEFINE_ATTR(dstAttr);

    status = NvMediaSurfaceFormatGetAttrs(imgSrc->type,
                                    srcAttr,
                                    NVM_SURF_FMT_ATTR_MAX);
    if (status != NVMEDIA_STATUS_OK) {
        LOG_ERR("%s:NvMediaSurfaceFormatGetAttrs failed\n", __func__);
        return NVMEDIA_STATUS_ERROR;
    }

    status = NvMediaSurfaceFormatGetAttrs(imgDst->type,
                                    dstAttr,
                                    NVM_SURF_FMT_ATTR_MAX);
    if (status != NVMEDIA_STATUS_OK) {
        LOG_ERR("%s:NvMediaSurfaceFormatGetAttrs failed\n", __func__);
        return NVMEDIA_STATUS_ERROR;
    }

    if (NvMediaImageLock(imgSrc, NVMEDIA_IMAGE_ACCESS_WRITE, &surfaceMap) !=
        NVMEDIA_STATUS_OK) {
        LOG_ERR("%s: NvMediaImageLock failed\n", __func__);
        return NVMEDIA_STATUS_ERROR;
    }

    srcHeight = surfaceMap.height;
    srcWidth  = surfaceMap.width;

    if (srcAttr[NVM_SURF_ATTR_SURF_TYPE].value == NVM_SURF_ATTR_SURF_TYPE_RAW) {
        srcPitch = srcWidth * rawBytesPerPixel;
        srcImageSize = srcPitch * srcHeight;
        srcImageSize += imgSrc->embeddedDataTopSize;
        srcImageSize += imgSrc->embeddedDataBottomSize;
    } else {
        LOG_ERR("%s: Unsupported source surface type\n", __func__);
        return NVMEDIA_STATUS_ERROR;
    }

    if (!(pSrcBuff = malloc(srcImageSize))) {
        LOG_ERR("%s: Out of memory\n", __func__);
        return NVMEDIA_STATUS_OUT_OF_MEMORY;
    }

    status = NvMediaImageGetBits(imgSrc, NULL, (void **)&pSrcBuff, &srcPitch);
    NvMediaImageUnlock(imgSrc);
    if (status != NVMEDIA_STATUS_OK) {
        LOG_ERR("%s: NvMediaImageGetBits() failed\n", __func__);
        goto done;
    }

    // set height skip telemetry line
    dstHeight = srcHeight - 1;
    dstWidth  = srcWidth;

    if (dstAttr[NVM_SURF_ATTR_SURF_TYPE].value == NVM_SURF_ATTR_SURF_TYPE_RGBA) {
        dstPitch = dstWidth;
        dstImageSize = dstHeight * dstPitch;
    } else {
        LOG_ERR("%s: Unsupported destination surface type\n", __func__);
        status = NVMEDIA_STATUS_ERROR;
        goto done;
    }

    if (!(pDstBuff = calloc(1, dstImageSize))) {
        LOG_ERR("%s: Out of memory\n", __func__);
        status = NVMEDIA_STATUS_OUT_OF_MEMORY;
        goto done;
    }

    pTmp = pDstBuff;
    /* Convert to grayscale */

    /* Y is starting at valid pixel, skipping embedded lines from top */
    y = imgSrc->embeddedDataTopSize / srcPitch;

    /* Get offsets for each pixel color */
    status = _ConvGetPixelOffsets(pixelOrder, xOffsets, yOffsets);
    if (status != NVMEDIA_STATUS_OK) {
        LOG_ERR("%s: Failed to get PixelOffsets\n", __func__);
        return status;
    }

    if ((srcAttr[NVM_SURF_ATTR_BITS_PER_COMPONENT].value == NVM_SURF_ATTR_BITS_PER_COMPONENT_12) &&
        (srcAttr[NVM_SURF_ATTR_DATA_TYPE].value == NVM_SURF_ATTR_DATA_TYPE_INT)) {
        for (; y < srcHeight; y += 2) {
            for (x = 0; x < srcWidth; x += 2) {
                /* R */
                *pTmp = CONV_CALCULATE_PIXEL(pSrcBuff, srcPitch, x, y, xOffsets[RED], yOffsets[RED]);
                pTmp++;
                /* G (average of green in BGGR) */
                *pTmp = ((CONV_CALCULATE_PIXEL(pSrcBuff, srcPitch, x, y, xOffsets[GREEN1], yOffsets[GREEN1])) +
                         (CONV_CALCULATE_PIXEL(pSrcBuff, srcPitch, x, y, xOffsets[GREEN2], yOffsets[GREEN2]))) /2 ;
                pTmp++;
                /* B */
                *pTmp = CONV_CALCULATE_PIXEL(pSrcBuff, srcPitch, x, y, xOffsets[BLUE], yOffsets[BLUE]);
                pTmp++;
                /* A */
                *pTmp = alpha;
                pTmp++;
            }
        }
    }
    // @@@@ ------------------ 12 Bit grayscale ----------------- 
    else if (((srcAttr[NVM_SURF_ATTR_BITS_PER_COMPONENT].value == NVM_SURF_ATTR_BITS_PER_COMPONENT_10) ||
                (srcAttr[NVM_SURF_ATTR_BITS_PER_COMPONENT].value == NVM_SURF_ATTR_BITS_PER_COMPONENT_12) ||
                (srcAttr[NVM_SURF_ATTR_BITS_PER_COMPONENT].value == NVM_SURF_ATTR_BITS_PER_COMPONENT_16)) &&
               (srcAttr[NVM_SURF_ATTR_DATA_TYPE].value == NVM_SURF_ATTR_DATA_TYPE_UINT)) {
        
        // ANIL EDIT: start from 1st row (skip telemetry line)
        for (y = 1; y < srcHeight; y++) {
            for (x = 0; x < srcPitch; x += 2) {
                *pTmp = raw12_to_byte(pSrcBuff, y * srcPitch + x);
                pTmp++;
            }
        }
    }
    // @@@@ ------------ 8 bit grayscale -----------------
    else if (srcAttr[NVM_SURF_ATTR_BITS_PER_COMPONENT].value == NVM_SURF_ATTR_BITS_PER_COMPONENT_8 &&
               srcAttr[NVM_SURF_ATTR_DATA_TYPE].value == NVM_SURF_ATTR_DATA_TYPE_UINT) {
        
        // ANIL EDIT: start from 1st row (skip telemetry line)
        for (y = 1; y < srcHeight; y++) {
            for (x = 0; x < srcWidth; x++) {
                *pTmp = pSrcBuff[y * srcPitch + x];
                pTmp++;
            }
        }
    }
    /* @@@@ ----------------- FLIR BOSON ONLY ------------------ */
    else if ((srcAttr[NVM_SURF_ATTR_BITS_PER_COMPONENT].value == NVM_SURF_ATTR_BITS_PER_COMPONENT_14) && 
            (srcAttr[NVM_SURF_ATTR_DATA_TYPE].value == NVM_SURF_ATTR_DATA_TYPE_INT)) {
    
        // reverse and order bits
  
        // Boson Data: x.x.N4.N4:N3.N3.N3.N3 - N2.N2.N2.N2:N1.N1.N1.N1
        // At reception:
        // byteH ( Buff[ n   ] ) -> N2.N2.N3.N3:N3.N3.N4.N4  (has the most significant bits)
        // byteL ( Buff[ n+1 ] ) -> x.x.N1.N1:N1.N1.N2.N2  (has the less significant bits)
  
        // Very important to discard first line which is TELEMETRY !!!
        // We will re-order bits, but not taking into account for AGC
        for (y=0; y< srcWidth; y++) {
            // short int reverse_16bits(unsigned char byteH, unsigned char byteL) {
            value = reverse_16bits( pSrcBuff[ y*2  ], pSrcBuff[ y*2 + 1 ] ) ;
      
            // Write back the reversed bytes and swap (LSB / MSB )
            // Store reversed order
            tmpL= value & 0xFF;
            tmpH= (value & 0xFF00) >> 8;
      
            pSrcBuff[ y*2 ]     = tmpH;
            pSrcBuff[ y*2 + 1 ] = tmpL;
        }
  
        // We continue with the rest of the IMAGE.
        // We need to order first, and we will perform a basic AGC after
        for (y=srcWidth; y< srcHeight*srcWidth; y++) {
            // short int reverse_16bits(unsigned char byteH, unsigned char byteL) {
            value = reverse_16bits( pSrcBuff[ y*2  ], pSrcBuff[ y*2 + 1 ] ) ;
          
            // Write back the reversed bytes and swap (LSB / MSB )
            // Store reversed order
            tmpL= value & 0xFF;
            tmpH= (value & 0xFF00) >> 8;
              
            pSrcBuff[ y*2 ]     = tmpH;
            pSrcBuff[ y*2 + 1 ] = tmpL;
              
            // find max, min of the pixels.
            // We will use this information to run a Linear AGC.
            // If a different AGC you can delete these lines.
            if ( value <= min ) {
                min=value;
            }
            if ( value >= max ) {
                max=value;
            }
        
        }
          
        // Do AGC in LUMA
        for (y=srcWidth; y<srcWidth * srcHeight; y++) {
            value =  ( (pSrcBuff[y*2]<<8) + pSrcBuff[y*2 + 1]) & 0xFFFF  ;
            value = ( ( ( 255 * ( value - min ) ) ) / (max-min) ) & 0xFF   ;
                    
            pDstBuff[ y * 4     ] = value;  //R
            pDstBuff[ y * 4 + 1 ] = value;  //G
            pDstBuff[ y * 4 + 2 ] = value;  //B
            pDstBuff[ y * 4 + 3 ] = alpha;
        }
    // @@@@ END----------------- FLIR BOSON ONLY ------------------

    } else {
        LOG_ERR("%s: Unsupported input raw format\n", __func__);
        status = NVMEDIA_STATUS_ERROR;
        goto done;
    }
    memset(&surfaceMap, 0, sizeof(surfaceMap));

    if (NvMediaImageLock(imgDst, NVMEDIA_IMAGE_ACCESS_WRITE, &surfaceMap) !=
       NVMEDIA_STATUS_OK) {
        LOG_ERR("%s: NvMediaImageLock failed\n", __func__);
        status = NVMEDIA_STATUS_ERROR;
        goto done;
    }

    status = NvMediaImagePutBits(imgDst, NULL, (void **)&pDstBuff, &dstPitch);
    NvMediaImageUnlock(imgDst);
    if (status != NVMEDIA_STATUS_OK) {
        LOG_ERR("%s: NvMediaImagePutBits() failed\n", __func__);
        goto done;
    }

    // write_raw_to_file(pDstBuff, dstWidth, dstHeight, "rawOut.raw");

    status = NVMEDIA_STATUS_OK;
done:
    if (pSrcBuff)
        free(pSrcBuff);
    if (pDstBuff)
        free(pDstBuff);

    return status;
}

static void
_CreateOutputFileName(char *saveFilePrefix,
                      char *calSettings,
                      uint32_t virtualGroupIndex,
                      uint32_t frame,
                      NvMediaBool useNvRawFormat,
                      char *outputFileName)
{
    char buf[MAX_STRING_SIZE] = {0};

    memset(outputFileName, 0, MAX_STRING_SIZE);
    strncpy(outputFileName, saveFilePrefix, MAX_STRING_SIZE);
    if(calSettings)
        strcat(outputFileName, calSettings);
    strcat(outputFileName, "_vc");
    sprintf(buf, "%d", virtualGroupIndex);
    strcat(outputFileName, buf);
    strcat(outputFileName, "_");
    sprintf(buf, "%02d", frame);
    strcat(outputFileName, buf);
    if (useNvRawFormat)
        strcat(outputFileName, ".nvraw");
    else
        strcat(outputFileName, ".raw");
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
_GetSettingNum(RuntimeSettings *rtSettings,
               uint32_t numRtSettings,
               uint32_t currentFrame)
{
    uint32_t i = 0, totalFrames = 0;
    RuntimeSettings *settings;

    for(i = 0; i < numRtSettings; i++) {
        settings = &rtSettings[i];
        totalFrames += settings->numFrames;
    }

    currentFrame = currentFrame % totalFrames;
    for(i = 0; i < numRtSettings; i++) {
        settings = &rtSettings[i];
        if(settings->numFrames > currentFrame) {
            return i;
        } else {
            currentFrame -= settings->numFrames;
        }
    }
    return -1;

}

static uint32_t
_SaveThreadFunc(void *data)
{
    SaveThreadCtx *threadCtx = (SaveThreadCtx *)data;
    NvMediaImage *image = NULL;
    NvMediaImage *convertedImage = NULL;
    NvMediaStatus status;
    uint32_t totalSavedFrames=0;
    char outputFileName[MAX_STRING_SIZE];
    char buf[MAX_STRING_SIZE] = {0};
    char *calSettings = NULL;

    NVM_SURF_FMT_DEFINE_ATTR(attr);

    while (!(*threadCtx->quit)) {
        image=NULL;
        /* Wait for captured frames */
        while (NvQueueGet(threadCtx->inputQueue, &image, SAVE_DEQUEUE_TIMEOUT) !=
           NVMEDIA_STATUS_OK) {
            LOG_DBG("%s: saveThread input queue %d is empty\n",
                     __func__, threadCtx->virtualGroupIndex);
            if (*threadCtx->quit)
                goto loop_done;
        }

        if (threadCtx->saveEnabled) {
            if (*threadCtx->numRtSettings) {
                calSettings = threadCtx->rtSettings[_GetSettingNum(threadCtx->rtSettings,
                                                                  *threadCtx->numRtSettings,
                                                                  totalSavedFrames)
                                                  ].outputFileName;
            } else if (threadCtx->sensorInfo) {
                memset(buf, 0 , MAX_STRING_SIZE);
                status = threadCtx->sensorInfo->AppendOutputFilename(buf,
                                                                     threadCtx->sensorProperties);
                if (status != NVMEDIA_STATUS_OK) {
                    LOG_ERR("%s: Failed to append output filename\n", __func__);
                    *threadCtx->quit = NVMEDIA_TRUE;
                    goto loop_done;
                }
                calSettings = buf;
            } else {
                calSettings = NULL;
            }

            /* Save image to file */
            _CreateOutputFileName(threadCtx->saveFilePrefix,
                                  calSettings,
                                  threadCtx->virtualGroupIndex,
                                  totalSavedFrames,
                                  threadCtx->useNvRawFormat,
                                  outputFileName);

            LOG_INFO("%s: Write image. res [%u:%u] (file: %s)\n",
                        __func__, image->width, image->height,
                        outputFileName);
            if (threadCtx->useNvRawFormat) {

                status = NvMediaSurfaceFormatGetAttrs(threadCtx->surfType,
                                                      attr,
                                                      NVM_SURF_FMT_ATTR_MAX);
                if (status != NVMEDIA_STATUS_OK) {
                    LOG_ERR("%s:NvMediaSurfaceFormatGetAttrs failed\n", __func__);
                   *threadCtx->quit = NVMEDIA_TRUE;
                    goto loop_done;
                }

                if (threadCtx->sensorInfo && (attr[NVM_SURF_ATTR_SURF_TYPE].value == NVM_SURF_ATTR_SURF_TYPE_RAW)) {
                    threadCtx->sensorInfo->WriteNvRawImage(&threadCtx->settingsCommands,
                                                           threadCtx->calParams,
                                                           image,
                                                           totalSavedFrames,
                                                           outputFileName);
                } else {
                    LOG_ERR("%s: NvRawFormat applicable only for RAW captured image \n", __func__);
                    *threadCtx->quit = NVMEDIA_TRUE;
                    goto loop_done;
                }
            } else {
                WriteImage(outputFileName,
                           image,
                           NVMEDIA_TRUE,
                           NVMEDIA_FALSE,
                           threadCtx->rawBytesPerPixel,
                           NULL);
           }
        }

        totalSavedFrames++;

        if (threadCtx->displayEnabled) {

            status = NvMediaSurfaceFormatGetAttrs(threadCtx->surfType,
                                                  attr,
                                                  NVM_SURF_FMT_ATTR_MAX);
            if (status != NVMEDIA_STATUS_OK) {
                LOG_ERR("%s:NvMediaSurfaceFormatGetAttrs failed\n", __func__);
               *threadCtx->quit = NVMEDIA_TRUE;
                goto loop_done;
            }

            if (attr[NVM_SURF_ATTR_SURF_TYPE].value == NVM_SURF_ATTR_SURF_TYPE_RAW) {
                /* Acquire image for storing converting images */
                while (NvQueueGet(threadCtx->conversionQueue,
                                  (void *)&convertedImage,
                                  SAVE_DEQUEUE_TIMEOUT) != NVMEDIA_STATUS_OK) {
                    LOG_ERR("%s: conversionQueue is empty\n", __func__);
                    if (*threadCtx->quit)
                        goto loop_done;
                }

                status = _ConvRawToRgba(image,
                                        convertedImage,
                                        threadCtx->rawBytesPerPixel,
                                        threadCtx->pixelOrder);
                if (status != NVMEDIA_STATUS_OK) {
                    LOG_ERR("%s: convRawToRgba failed for image %d in saveThread %d\n",
                            __func__, totalSavedFrames, threadCtx->virtualGroupIndex);
                    *threadCtx->quit = NVMEDIA_TRUE;
                    goto loop_done;
                }

                while (NvQueuePut(threadCtx->outputQueue,
                                  &convertedImage,
                                  SAVE_ENQUEUE_TIMEOUT) != NVMEDIA_STATUS_OK) {
                    LOG_DBG("%s: savethread output queue %d is full\n",
                             __func__, threadCtx->virtualGroupIndex);
                    if (*threadCtx->quit)
                        goto loop_done;
                }
                convertedImage = NULL;
            } else {
                while (NvQueuePut(threadCtx->outputQueue,
                                  &image,
                                  SAVE_ENQUEUE_TIMEOUT) != NVMEDIA_STATUS_OK) {
                    LOG_DBG("%s: savethread output queue %d is full\n",
                             __func__, threadCtx->virtualGroupIndex);
                    if (*threadCtx->quit)
                        goto loop_done;
                }
                image=NULL;
            }
        }

        if (threadCtx->numFramesToSave &&
           (totalSavedFrames == threadCtx->numFramesToSave)) {
            *threadCtx->quit = NVMEDIA_TRUE;
            goto loop_done;
        }
    loop_done:
        if (image) {
            if (NvQueuePut((NvQueue *)image->tag,
                           (void *)&image,
                           0) != NVMEDIA_STATUS_OK) {
                LOG_ERR("%s: Failed to put image back in queue\n", __func__);
                *threadCtx->quit = NVMEDIA_TRUE;
            };
            image = NULL;
        }
        if (convertedImage) {
            if (NvQueuePut((NvQueue *)convertedImage->tag,
                           (void *)&convertedImage,
                           0) != NVMEDIA_STATUS_OK) {
                LOG_ERR("%s: Failed to put image back in conversionQueue\n", __func__);
                *threadCtx->quit = NVMEDIA_TRUE;
            }
            convertedImage = NULL;
        }
    }
    LOG_INFO("%s: Save thread exited\n", __func__);
    threadCtx->exitedFlag = NVMEDIA_TRUE;
    return NVMEDIA_STATUS_OK;
}

NvMediaStatus
SaveInit(NvMainContext *mainCtx)
{
    NvSaveContext *saveCtx  = NULL;
    NvCaptureContext   *captureCtx = NULL;
    NvRuntimeSettingsContext *runtimeCtx = NULL;
    TestArgs           *testArgs = mainCtx->testArgs;
    uint32_t i = 0;
    NvMediaStatus status = NVMEDIA_STATUS_ERROR;
    NvMediaSurfAllocAttr surfAllocAttrs[8];
    uint32_t numSurfAllocAttrs;

    /* allocating save context */
    mainCtx->ctxs[SAVE_ELEMENT]= malloc(sizeof(NvSaveContext));
    if (!mainCtx->ctxs[SAVE_ELEMENT]){
        LOG_ERR("%s: Failed to allocate memory for save context\n", __func__);
        return NVMEDIA_STATUS_OUT_OF_MEMORY;
    }

    saveCtx = mainCtx->ctxs[SAVE_ELEMENT];
    memset(saveCtx,0,sizeof(NvSaveContext));
    captureCtx = mainCtx->ctxs[CAPTURE_ELEMENT];
    runtimeCtx = mainCtx->ctxs[RUNTIME_SETTINGS_ELEMENT];

    /* initialize context */
    saveCtx->quit      =  &mainCtx->quit;
    saveCtx->testArgs  = testArgs;
    saveCtx->numVirtualChannels = testArgs->numVirtualChannels;
    saveCtx->displayEnabled = testArgs->displayEnabled;
    saveCtx->inputQueueSize = testArgs->bufferPoolSize;
    /* Create NvMedia Device */
    saveCtx->device = NvMediaDeviceCreate();
    if (!saveCtx->device) {
        status = NVMEDIA_STATUS_ERROR;
        LOG_ERR("%s: Failed to create NvMedia device\n", __func__);
        goto failed;
    }

    /* Create save input Queues and set thread data */
    for (i = 0; i < saveCtx->numVirtualChannels; i++) {
        saveCtx->threadCtx[i].quit = saveCtx->quit;
        saveCtx->threadCtx[i].exitedFlag = NVMEDIA_TRUE;
        saveCtx->threadCtx[i].displayEnabled = testArgs->displayEnabled;
        saveCtx->threadCtx[i].saveEnabled = testArgs->useFilePrefix;
        saveCtx->threadCtx[i].saveFilePrefix = testArgs->filePrefix;
        saveCtx->threadCtx[i].useNvRawFormat = testArgs->useNvRawFormat;
        saveCtx->threadCtx[i].sensorInfo = testArgs->sensorInfo;
        saveCtx->threadCtx[i].calParams = &captureCtx->calParams;
        saveCtx->threadCtx[i].virtualGroupIndex = captureCtx->threadCtx[i].virtualGroupIndex;
        saveCtx->threadCtx[i].numFramesToSave = (testArgs->frames.isUsed)?
                                                 testArgs->frames.uIntValue : 0;
        saveCtx->threadCtx[i].surfType = captureCtx->threadCtx[i].surfType;
        saveCtx->threadCtx[i].pixelOrder = captureCtx->threadCtx[i].pixelOrder;
        saveCtx->threadCtx[i].rawBytesPerPixel = captureCtx->threadCtx[i].rawBytesPerPixel;
        NVM_SURF_FMT_DEFINE_ATTR(attr);
        status = NvMediaSurfaceFormatGetAttrs(captureCtx->threadCtx[i].surfType,
                                              attr,
                                              NVM_SURF_FMT_ATTR_MAX);
        if (status != NVMEDIA_STATUS_OK) {
            LOG_ERR("%s:NvMediaSurfaceFormatGetAttrs failed\n", __func__);
            goto failed;
        }
        saveCtx->threadCtx[i].width =  (attr[NVM_SURF_ATTR_SURF_TYPE].value == NVM_SURF_ATTR_SURF_TYPE_RAW )?
                                           captureCtx->threadCtx[i].width/2 : captureCtx->threadCtx[i].width;
        saveCtx->threadCtx[i].height = (attr[NVM_SURF_ATTR_SURF_TYPE].value == NVM_SURF_ATTR_SURF_TYPE_RAW )?
                                           captureCtx->threadCtx[i].height/2 : captureCtx->threadCtx[i].height;
        saveCtx->threadCtx[i].rtSettings = runtimeCtx->rtSettings;
        saveCtx->threadCtx[i].numRtSettings = &runtimeCtx->numRtSettings;
        saveCtx->threadCtx[i].sensorProperties = testArgs->sensorProperties;
        if (NvQueueCreate(&saveCtx->threadCtx[i].inputQueue,
                         saveCtx->inputQueueSize,
                         sizeof(NvMediaImage *)) != NVMEDIA_STATUS_OK) {
            LOG_ERR("%s: Failed to create save inputQueue %d\n",
                    __func__, i);
            status = NVMEDIA_STATUS_ERROR;
            goto failed;
        }
        if (testArgs->displayEnabled) {
            if (attr[NVM_SURF_ATTR_SURF_TYPE].value == NVM_SURF_ATTR_SURF_TYPE_RAW ) {
                /* For RAW images, create conversion queue for converting RAW to RGB images */

                surfAllocAttrs[0].type = NVM_SURF_ATTR_WIDTH;
                surfAllocAttrs[0].value = saveCtx->threadCtx[i].width;
                surfAllocAttrs[1].type = NVM_SURF_ATTR_HEIGHT;
                surfAllocAttrs[1].value = saveCtx->threadCtx[i].height;
                surfAllocAttrs[2].type = NVM_SURF_ATTR_CPU_ACCESS;
                surfAllocAttrs[2].value = NVM_SURF_ATTR_CPU_ACCESS_UNCACHED;
                numSurfAllocAttrs = 3;

                NVM_SURF_FMT_DEFINE_ATTR(surfFormatAttrs);
                NVM_SURF_FMT_SET_ATTR_RGBA(surfFormatAttrs,RGBA,UINT,8,PL);
                status = _CreateImageQueue(saveCtx->device,
                                           &saveCtx->threadCtx[i].conversionQueue,
                                           saveCtx->inputQueueSize,
                                           saveCtx->threadCtx[i].width,
                                           saveCtx->threadCtx[i].height,
                                           NvMediaSurfaceFormatGetType(surfFormatAttrs, NVM_SURF_FMT_ATTR_MAX),
                                           surfAllocAttrs,
                                           numSurfAllocAttrs);
                if (status != NVMEDIA_STATUS_OK) {
                    LOG_ERR("%s: conversionQueue creation failed\n", __func__);
                    goto failed;
                }

                LOG_DBG("%s: Save Conversion Queue %d: %ux%u, images: %u \n",
                        __func__, i, saveCtx->threadCtx[i].width,
                        saveCtx->threadCtx[i].height,
                        saveCtx->inputQueueSize);
            }
        }
    }
    return NVMEDIA_STATUS_OK;
failed:
    LOG_ERR("%s: Failed to initialize Save\n",__func__);
    return status;
}

NvMediaStatus
SaveFini(NvMainContext *mainCtx)
{
    NvSaveContext *saveCtx = NULL;
    NvMediaImage *image = NULL;
    uint32_t i;
    NvMediaStatus status = NVMEDIA_STATUS_OK;

    if (!mainCtx)
        return NVMEDIA_STATUS_OK;

    saveCtx = mainCtx->ctxs[SAVE_ELEMENT];
    if (!saveCtx)
        return NVMEDIA_STATUS_OK;

    /* Wait for threads to exit */
    for (i = 0; i < saveCtx->numVirtualChannels; i++) {
        if (saveCtx->saveThread[i]) {
            while (!saveCtx->threadCtx[i].exitedFlag) {
                LOG_DBG("%s: Waiting for save thread %d to quit\n",
                        __func__, i);
            }
        }
    }

    *saveCtx->quit = NVMEDIA_TRUE;

    /* Destroy threads */
    for (i = 0; i < saveCtx->numVirtualChannels; i++) {
        if (saveCtx->saveThread[i]) {
            status = NvThreadDestroy(saveCtx->saveThread[i]);
            if (status != NVMEDIA_STATUS_OK)
                LOG_ERR("%s: Failed to destroy save thread %d\n",
                        __func__, i);
        }
    }

    for (i = 0; i < saveCtx->numVirtualChannels; i++) {
        /*For RAW Images, destroy the conversion queue */
        if (saveCtx->threadCtx[i].conversionQueue) {
            while (IsSucceed(NvQueueGet(saveCtx->threadCtx[i].conversionQueue, &image, 0))) {
                if (image) {
                    NvMediaImageDestroy(image);
                    image = NULL;
                }
            }
            LOG_DBG("%s: Destroying conversion queue \n",__func__);
            NvQueueDestroy(saveCtx->threadCtx[i].conversionQueue);
        }

        /*Flush and destroy the input queues*/
        if (saveCtx->threadCtx[i].inputQueue) {
            LOG_DBG("%s: Flushing the save input queue %d\n", __func__, i);
            while (IsSucceed(NvQueueGet(saveCtx->threadCtx[i].inputQueue, &image, 0))) {
                if (image) {
                    if (NvQueuePut((NvQueue *)image->tag,
                                   (void *)&image,
                                   0) != NVMEDIA_STATUS_OK) {
                        LOG_ERR("%s: Failed to put image back in queue\n", __func__);
                        break;
                    }
                }
                image=NULL;
            }
            NvQueueDestroy(saveCtx->threadCtx[i].inputQueue);
        }
    }

    if (saveCtx->device)
        NvMediaDeviceDestroy(saveCtx->device);

    if (saveCtx)
        free(saveCtx);

    LOG_INFO("%s: SaveFini done\n", __func__);
    return NVMEDIA_STATUS_OK;
}


NvMediaStatus
SaveProc(NvMainContext *mainCtx)
{
    NvSaveContext        *saveCtx = NULL;
    NvCompositeContext   *compositeCtx = NULL;
    uint32_t i;
    NvMediaStatus status= NVMEDIA_STATUS_OK;

    if (!mainCtx) {
        LOG_ERR("%s: Bad parameter\n", __func__);
        return NVMEDIA_STATUS_ERROR;
    }
    saveCtx = mainCtx->ctxs[SAVE_ELEMENT];
    compositeCtx = mainCtx->ctxs[COMPOSITE_ELEMENT];

    /* Setting the queues */
    if (saveCtx->displayEnabled) {
        for (i = 0; i < saveCtx->numVirtualChannels; i++) {
            saveCtx->threadCtx[i].outputQueue = compositeCtx->inputQueue[i];
        }
    }

    /* Create thread to save images */
    for (i = 0; i < saveCtx->numVirtualChannels; i++) {
        saveCtx->threadCtx[i].exitedFlag = NVMEDIA_FALSE;
        status = NvThreadCreate(&saveCtx->saveThread[i],
                                &_SaveThreadFunc,
                                (void *)&saveCtx->threadCtx[i],
                                NV_THREAD_PRIORITY_NORMAL);
        if (status != NVMEDIA_STATUS_OK) {
            LOG_ERR("%s: Failed to create save Thread\n",
                    __func__);
            saveCtx->threadCtx[i].exitedFlag = NVMEDIA_TRUE;
        }
    }
    return status;
}
