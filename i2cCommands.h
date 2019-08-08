/* Copyright (c) 2014-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef _I2C_INTERFACE_
#define _I2C_INTERFACE_

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nvmedia_core.h"
#include "nvmedia_surface.h"
#include "testutil_i2c.h"
#include "log_utils.h"

#define MAX_BUF_LENGTH          34   // to handle 32 byte data + 2 bytes sub address
#define MAX_NUM_COMMANDS        10000
#define MAX_NUM_GROUPS          10

typedef enum {
    WRITE_REG_1 = 0,            // 1 byte register address to write
    WRITE_REG_2,                // 2 byte register address to write
    READ_REG_1,                 // 1 byte register address to read
    READ_REG_2,                 // 2 byte register address to read
    DELAY,                      // Delay between commands
    I2C_DEVICE,                 // I2c device number
    I2C_ERR,                    // I2c Error Check
    SECTION_START,              // Indicate the start of a group/preset registers section
    SECTION_STOP,               // Indicate the end of a group/preset registers section
    READ_WRITE_REG_1,           // 1 byte registers to read and write
    READ_WRITE_REG_2,           // 2 byte registers to read and write
} CommandType;

typedef enum {
    DEFAULT = 0,
    GROUP_REG,
    PRESET_REG
} ProcessType;

typedef enum {
    I2C_READ = 0,
    I2C_WRITE
} I2cOperation;

typedef struct {
    CommandType                 commandType;
    ProcessType                 processType;
    uint8_t                     buffer[MAX_BUF_LENGTH];
    union {
        uint32_t                deviceAddress;
        uint32_t                delay;
        int                     i2cDevice;
        int                     triggerFrame;
        NvMediaBool             i2cErr;
    };
    uint8_t                     dataLength;   // support multiple bytes data for i2c write
} Command;

typedef struct {
    uint32_t                    firstCommand;
    uint32_t                    numCommands;
} GroupData;

typedef struct {
    Command                     commands[MAX_NUM_COMMANDS];
    uint32_t                    numCommands;
} I2cCommands;

typedef struct {
    GroupData                   groups[MAX_NUM_GROUPS];
    uint32_t                    numGroups;
} I2cGroups;

NvMediaStatus
I2cSetupGroups(I2cCommands *allCommands,
               I2cGroups   *allGroups);

NvMediaStatus
I2cProcessCommands(I2cCommands *allCommands,
                   I2cOperation operation,
                   int i2cDevice);

uint8_t *
I2cSetupRegister(I2cCommands *allCommands,
                 CommandType type,
                 uint32_t deviceAddress,
                 uint8_t *regAddress,
                 uint8_t *valueToWrite,
                 uint32_t dataRegLen);

NvMediaStatus
I2cProcessGroup(I2cHandle handle,
                I2cCommands *allCommands,
                GroupData *grpData);

NvMediaStatus
I2cProcessInitialRegisters(I2cCommands *allCommands,
                           int i2cDevice);

uint32_t
I2cGetNumCommands(I2cCommands *allCommands);

void
I2cSetNumCommands(I2cCommands *allCommands,
                  uint32_t setNumCommands);

#endif
