/* Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef _NVRAWFILE_INTERFACE_H_
#define _NVRAWFILE_INTERFACE_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_EXPOSURE_MODES 3

// Latest version
#define NVRAW_INTERFACE_VERSION 2
//
// Version history:
//
// Version 2.0
// Change 936449 (nvrawfile_interface: add new HDRInfo version)
//
// Version 1.0
// Add basic interface definitions and APIs

/**
 * \brief The set of all NvRaw Output Compression Formats.
 */

typedef enum
{
     NvRawCompressionFormat_10BitLinear = 0,
     NvRawCompressionFormat_2x11_1,
     NvRawCompressionFormat_3x12,
     NvRawCompressionFormat_12BitLinear,
     NvRawCompressionFormat_12BitCombinedCompressed,
     NvRawCompressionFormat_12BitCombinedCompressedExtended,
     NvRawCompressionFormat_16bitLinear,
     NvRawCompressionFormat_16BitLogDomain,
     NvRawCompressionFormat_16BitLogDomainExtended,
     NvRawCompressionFormat_20BitLinear,
     NvRawCompressionFormat_20BitLinearExtended,
     NvRawCompressionFormat_16BitCombinedCompressed
} NvRawOutputCompressionFormat;

/**
 * \brief The set of all possible error codes.
 */
typedef enum {
    /** \The operation completed successfully; no error. */
    NvRawFileError_Success = 0,
    /** Bad parameter was passed. */
    NvRawFileError_BadParameter,
    /** File Read operation Failed. */
    NvRawFileError_FileReadFailed,
    /** File Write operation Failed. */
    NvRawFileError_FileWriteFailed,
    /** Operation timed out. */
    NvRawFileError_Timeout,
    /** Out of memory. */
    NvRawFileError_InsufficientMemory,
    /** A catch-all error, used when no other error code applies. */
    NvRawFileError_Failure
} NvRawFileErrorStatus;

/**
 * NvRawSensorHDRInfo version1
 * This is old structure and to be
 * replaced by NvRawSensorHDRInfo_v2
 */
typedef struct {
    struct {
       float_t exposureTime;
       float_t analogGain;
       float_t digitalGain;
       uint32_t conversionGain;
    } exposure[MAX_EXPOSURE_MODES];

    struct {
        float_t value[4];
    } wbGain[MAX_EXPOSURE_MODES];
} NvRawSensorHDRInfo;

/**
 * NvRawSensorHDRInfo version2
 * Latest structure definition version
 * This version should be used in place
 * of outdated NvRawSensorHDRInfo
 */
typedef struct {
    struct {
       float_t exposureTime;
       float_t analogGain;
       float_t digitalGain;
       uint32_t conversionGain;
    } exposure;

    struct {
        float_t value[4];
    } wbGain;
} NvRawSensorHDRInfo_v2;


/**
 * NvRawFileHeaderChunk - first chunk present in NvRaw file.
 */
typedef struct _NvRawFileHeaderChunkHandle NvRawFileHeaderChunkHandle;
NvRawFileHeaderChunkHandle* NvRawFileHeaderChunkCreate(void);
NvRawFileErrorStatus NvRawFileHeaderChunkSize(NvRawFileHeaderChunkHandle* hdr, uint32_t *size);
NvRawFileErrorStatus NvRawFileHeaderChunkFileWrite(NvRawFileHeaderChunkHandle* hdr, FILE* f);
NvRawFileErrorStatus NvRawFileHeaderChunkMemWrite(NvRawFileHeaderChunkHandle* hdr, uint8_t* dest, uint8_t** memPtr);
NvRawFileErrorStatus NvRawFileHeaderChunkGetBitsPerSample(NvRawFileHeaderChunkHandle *hdr,
                                     uint32_t *bitsPerSample);
NvRawFileErrorStatus NvRawFileHeaderChunkSetBitsPerSample(NvRawFileHeaderChunkHandle *hdr,
                                     uint32_t bitsPerSample);
NvRawFileErrorStatus NvRawFileHeaderChunkGetSamplesPerPixel(NvRawFileHeaderChunkHandle *hdr,
                                       uint32_t *samplesPerPixel);
NvRawFileErrorStatus NvRawFileHeaderChunkSetSamplesPerPixel(NvRawFileHeaderChunkHandle *hdr,
                                       uint32_t samplesPerPixel);
NvRawFileErrorStatus NvRawFileHeaderChunkGetNumImages(NvRawFileHeaderChunkHandle *hdr,
                                 uint32_t *numImages);
NvRawFileErrorStatus NvRawFileHeaderChunkSetNumImages(NvRawFileHeaderChunkHandle *hdr,
                                 uint32_t numImages);
NvRawFileErrorStatus NvRawFileHeaderChunkGetProcessingFlags(NvRawFileHeaderChunkHandle *hdr,
                                       uint32_t *processingFlags);
NvRawFileErrorStatus NvRawFileHeaderChunkSetProcessingFlags(NvRawFileHeaderChunkHandle *hdr,
                                       uint32_t processingFlags);
NvRawFileErrorStatus NvRawFileHeaderChunkGetDataFormat(NvRawFileHeaderChunkHandle *hdr,
                                  uint32_t *dataFormat);
NvRawFileErrorStatus NvRawFileHeaderChunkSetDataFormat(NvRawFileHeaderChunkHandle *hdr,
                                  uint32_t dataFormat);

NvRawFileErrorStatus NvRawFileHeaderChunkGetImageWidth(NvRawFileHeaderChunkHandle *hdr,
                                  uint32_t *width);
NvRawFileErrorStatus NvRawFileHeaderChunkSetImageWidth(NvRawFileHeaderChunkHandle *hdr,
                                  uint32_t width);
NvRawFileErrorStatus NvRawFileHeaderChunkGetImageHeight(NvRawFileHeaderChunkHandle *hdr,
                                   uint32_t *height);
NvRawFileErrorStatus NvRawFileHeaderChunkSetImageHeight(NvRawFileHeaderChunkHandle *hdr,
                                   uint32_t height);
void NvRawFileHeaderChunkDelete(NvRawFileHeaderChunkHandle* hdr);


/**
 * NvRawFileDataChunk- second chunk present in NvRaw file.
 *
 */
typedef struct _NvRawFileDataChunkHandle NvRawFileDataChunkHandle;
NvRawFileDataChunkHandle* NvRawFileDataChunkCreate(uint32_t dataLength, bool shouldAlloc);
NvRawFileErrorStatus NvRawFileDataChunkGetSize(NvRawFileDataChunkHandle* dc, uint32_t *size);
NvRawFileErrorStatus NvRawFileDataChunkFileWrite(NvRawFileDataChunkHandle* dc, FILE* f, bool writePixels);
NvRawFileErrorStatus NvRawFileDataChunkMemWrite(NvRawFileDataChunkHandle* dc, uint8_t* dest, bool writePixels, uint8_t** memPtr);
NvRawFileErrorStatus NvRawFileDataChunkGet(NvRawFileDataChunkHandle* nrfd, uint16_t** dataChunk);
void NvRawFileDataChunkDelete(NvRawFileDataChunkHandle* dc);


/*
 * NvRawFileCaptureChunk - contains the image's exposure
 * information.
 */
typedef enum
{
    NvRawFilePixelFormat_Int16 = 1,
    NvRawFilePixelFormat_S114,
    NvRawFilePixelFormat_IEEE_FP16,
    NvRawFilePixelFormat_ISP_FP16,
    NvRawFilePixelFormat_U16
} NvRawFilePixelFormat;

typedef struct _NvRawFileCaptureChunkHandle NvRawFileCaptureChunkHandle;
NvRawFileCaptureChunkHandle* NvRawFileCaptureChunkCreate(void);
NvRawFileErrorStatus NvRawFileCaptureChunkVersion(NvRawFileCaptureChunkHandle* cc, uint32_t *version);
NvRawFileErrorStatus NvRawFileCaptureChunkGetSize(NvRawFileCaptureChunkHandle* cc, uint32_t *size);
NvRawFileErrorStatus NvRawFileCaptureChunkFileWrite(NvRawFileCaptureChunkHandle* cc, FILE* f);
NvRawFileErrorStatus NvRawFileCaptureChunkMemWrite(NvRawFileCaptureChunkHandle* cc, uint8_t* dest, uint8_t** memPtr);
void NvRawFileCaptureChunkDelete(NvRawFileCaptureChunkHandle* cc);
NvRawFileErrorStatus NvRawFileCaptureChunkInvalidateExposureForHDR(NvRawFileCaptureChunkHandle *cc);
NvRawFileErrorStatus NvRawFileCaptureChunkSetIspDigitalGain(NvRawFileCaptureChunkHandle *cc, float_t ispDigitalGain);
NvRawFileErrorStatus NvRawFileCaptureChunkGetIspDigitalGain(NvRawFileCaptureChunkHandle *cc,  float_t *ispDigitalGain);
NvRawFileErrorStatus NvRawFileCaptureChunkSetPixelFormat(NvRawFileCaptureChunkHandle *cc,
                                                         NvRawFilePixelFormat pf);
NvRawFileErrorStatus NvRawFileCaptureChunkGetPixelFormat(NvRawFileCaptureChunkHandle *cc,
                                                         NvRawFilePixelFormat *pf);
NvRawFileErrorStatus NvRawFileCaptureChunkSetOutputDataFormat(NvRawFileCaptureChunkHandle *cc,
                                         NvRawOutputCompressionFormat outputCompressionFormat);
NvRawFileErrorStatus NvRawFileCaptureChunkGetOutputDataFormat(NvRawFileCaptureChunkHandle *cc,
                                         NvRawOutputCompressionFormat *outputCompressionFormat);
NvRawFileErrorStatus NvRawFileCaptureChunkSetPixelEndianness(NvRawFileCaptureChunkHandle *cc,
                                          bool bPixelLittleEndian);
NvRawFileErrorStatus NvRawFileCaptureChunkGetPixelEndianness(NvRawFileCaptureChunkHandle *cc,
                                          bool *bPixelLittleEndian);
NvRawFileErrorStatus NvRawFileCaptureChunkSetEmbeddedLineCountTop(NvRawFileCaptureChunkHandle *cc,
                                             uint32_t embeddedLineCount);
NvRawFileErrorStatus NvRawFileCaptureChunkGetEmbeddedLineCountTop(NvRawFileCaptureChunkHandle *cc,
                                             uint32_t *embeddedLineCount);
NvRawFileErrorStatus NvRawFileCaptureChunkSetEmbeddedLineCountBottom(NvRawFileCaptureChunkHandle *cc,
                                                uint32_t embeddedLineCount);
NvRawFileErrorStatus NvRawFileCaptureChunkGetEmbeddedLineCountBottom(NvRawFileCaptureChunkHandle *cc,
                                                uint32_t *embeddedLineCount);
NvRawFileErrorStatus NvRawFileCaptureChunkSetLut(NvRawFileCaptureChunkHandle *cc, uint8_t *pBuffer, uint32_t size);
NvRawFileErrorStatus NvRawFileCaptureChunkGetLut(NvRawFileCaptureChunkHandle *cc, uint8_t **ppBuffer, uint32_t *pSize);
NvRawFileErrorStatus NvRawFileCaptureChunkSetExposureTime(NvRawFileCaptureChunkHandle *cc,
                                     float_t exposureTime);
NvRawFileErrorStatus NvRawFileCaptureChunkGetExposureTime(NvRawFileCaptureChunkHandle *cc,
                                     float_t *exposureTime);
NvRawFileErrorStatus NvRawFileCaptureChunkSetISO(NvRawFileCaptureChunkHandle *cc, uint32_t iso);
NvRawFileErrorStatus NvRawFileCaptureChunkGetISO(NvRawFileCaptureChunkHandle *cc, uint32_t *iso);
NvRawFileErrorStatus NvRawFileCaptureChunkSetSensorGain(NvRawFileCaptureChunkHandle *cc, float_t *sensorGains);
NvRawFileErrorStatus NvRawFileCaptureChunkGetSensorGain(NvRawFileCaptureChunkHandle *cc, float_t *sensorGains);
NvRawFileErrorStatus NvRawFileCaptureChunkSetFlashPower(NvRawFileCaptureChunkHandle *cc,
                                   float_t flashPower);
NvRawFileErrorStatus NvRawFileCaptureChunkGetFlashPower(NvRawFileCaptureChunkHandle *cc,
                                   float_t *flashPower);
NvRawFileErrorStatus NvRawFileCaptureChunkSetFocusPosition(NvRawFileCaptureChunkHandle *cc,
                                      int32_t focusPosition);
NvRawFileErrorStatus NvRawFileCaptureChunkGetFocusPosition(NvRawFileCaptureChunkHandle *cc,
                                      int32_t *focusPosition);
NvRawFileErrorStatus NvRawFileCaptureChunkSetLux(NvRawFileCaptureChunkHandle *cc,
                                      float_t lux);
NvRawFileErrorStatus NvRawFileCaptureChunkGetLux(NvRawFileCaptureChunkHandle *cc,
                                      float_t *lux);
NvRawFileErrorStatus NvRawFileCaptureChunkSetDynamicPixelBitDepth(NvRawFileCaptureChunkHandle *cc,
                                      uint32_t dynamicPixelBitDepth);
NvRawFileErrorStatus NvRawFileCaptureChunkGetDynamicPixelBitDepth(NvRawFileCaptureChunkHandle *cc,
                                      uint32_t *dynamicPixelBitDepth);
NvRawFileErrorStatus NvRawFileCaptureChunkSetCSIpixelBitDepth(NvRawFileCaptureChunkHandle *cc,
                                      uint32_t csipPixelBitDepth);
NvRawFileErrorStatus NvRawFileCaptureChunkGetCSIpixelBitDepth(NvRawFileCaptureChunkHandle *cc,
                                      uint32_t *csipPixelBitDepth);

/**
 * NvRawFileHDRChunk- information about
 * the exposure, pixel readout, etc.
 */

typedef struct _NvRawFileHDRChunkHandle NvRawFileHDRChunkHandle;
NvRawFileHDRChunkHandle* NvRawFileHDRChunkCreate(void);
NvRawFileErrorStatus NvRawFileHDRChunkGetVersion(NvRawFileHDRChunkHandle* hc, uint32_t* version);
NvRawFileErrorStatus NvRawFileHDRChunkGetSize(NvRawFileHDRChunkHandle* hc, uint32_t* size);
NvRawFileErrorStatus NvRawFileHDRChunkFileWrite(NvRawFileHDRChunkHandle* hc, FILE* f);
NvRawFileErrorStatus NvRawFileHDRChunkMemWrite(NvRawFileHDRChunkHandle* hc, uint8_t* dest, uint8_t** memPtr);
void NvRawFileHDRChunkDelete(NvRawFileHDRChunkHandle* hc);
NvRawFileErrorStatus NvRawFileHDRChunkSetNumberOfExposures(NvRawFileHDRChunkHandle* hc, uint32_t count);
NvRawFileErrorStatus NvRawFileHDRChunkSetExposureInfo(NvRawFileHDRChunkHandle* hc, NvRawSensorHDRInfo *sensorInfo);
NvRawFileErrorStatus NvRawFileHDRChunkSetExposureInfo_v2(NvRawFileHDRChunkHandle* hc, NvRawSensorHDRInfo_v2 *sensorInfo);
NvRawFileErrorStatus NvRawFileHDRChunkSetReadoutScheme(NvRawFileHDRChunkHandle* hc, const char* scheme);
NvRawFileErrorStatus NvRawFileHDRChunkGetReadoutScheme(NvRawFileHDRChunkHandle* hc, const char** scheme);
NvRawFileErrorStatus NvRawFileHDRChunkGetExposureInfoSize(NvRawFileHDRChunkHandle* hc, uint32_t* exposureInfoSize);

/**
 * NvRawFileSensorInfoChunk - contains several ASCII strings.
 * This chunk and other chunks that contain strings should
 * not be used with sizeof() since their size is not dictated
 * by structure layout, but by dynamic contents. Use
 * NvRawFileSensorInfoChunk_size() and NvRawFileSensorInfoChunk_set()
 * to ensure correct memory management and size determination.
 */
typedef struct _NvRawFileSensorInfoChunkHandle NvRawFileSensorInfoChunkHandle;
NvRawFileSensorInfoChunkHandle* NvRawFileSensorInfoChunkCreate(void);
NvRawFileErrorStatus NvRawFileSensorInfoChunkGetSize(NvRawFileSensorInfoChunkHandle* sc, uint32_t* size);
NvRawFileErrorStatus NvRawFileSensorInfoChunkSetSensor(NvRawFileSensorInfoChunkHandle* sc, const char* sensorString);
NvRawFileErrorStatus NvRawFileSensorInfoChunkGetSensor(NvRawFileSensorInfoChunkHandle* sc, const char** sensorString);
NvRawFileErrorStatus NvRawFileSensorInfoChunkSetFuse(NvRawFileSensorInfoChunkHandle* sc, const char* fuseString);
NvRawFileErrorStatus NvRawFileSensorInfoChunkGetFuse(NvRawFileSensorInfoChunkHandle* sc, const char** fuseString);
NvRawFileErrorStatus NvRawFileSensorInfoChunkSetModule(NvRawFileSensorInfoChunkHandle* sc, const char* moduleString);
NvRawFileErrorStatus NvRawFileSensorInfoChunkGetModule(NvRawFileSensorInfoChunkHandle* sc, const char** moduleString);
NvRawFileErrorStatus NvRawFileSensorInfoChunkFileWrite(NvRawFileSensorInfoChunkHandle* sc, FILE* f);
NvRawFileErrorStatus NvRawFileSensorInfoChunkMemWrite(NvRawFileSensorInfoChunkHandle* sc, uint8_t* dest, uint8_t** memPtr);
void NvRawFileSensorInfoChunkDelete(NvRawFileSensorInfoChunkHandle* sc);


/**
 * NvRawFileCameraStateChunk- contains information about
 * the auto-algorithms' convergence states at the time
 * of exposure.
 */
typedef struct _NvRawFileCameraStateChunkHandle NvRawFileCameraStateChunkHandle;
NvRawFileCameraStateChunkHandle* NvRawFileCameraStateChunkCreate(void);
NvRawFileErrorStatus NvRawFileCameraStateChunkGetSize(NvRawFileCameraStateChunkHandle* ec, uint32_t *size);
NvRawFileErrorStatus NvRawFileCameraStateChunkFileWrite(NvRawFileCameraStateChunkHandle* ec, FILE* f);
NvRawFileErrorStatus NvRawFileCameraStateChunkMemWrite(NvRawFileCameraStateChunkHandle* ec, uint8_t* dest, uint8_t** memPtr);
void NvRawFileCameraStateChunkDelete(NvRawFileCameraStateChunkHandle* ec);

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* _NVMEDIA_RAWFILE_H */

