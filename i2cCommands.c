/* Copyright (c) 2014-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "i2cCommands.h"
#include "os_common.h"

NvMediaStatus
I2cSetupGroups(I2cCommands *allCommands,
               I2cGroups *allGroups)
{
    uint32_t i = 0;
    NvMediaBool groupRegisterActive = NVMEDIA_FALSE;
    Command *command = NULL;

    for (i = 0; i < allCommands->numCommands; i++) {
        command = &allCommands->commands[i];
        switch (command->processType) {
            case(GROUP_REG):
                if (!groupRegisterActive && (command->commandType == SECTION_START)) {
                    if (allGroups->numGroups == MAX_NUM_GROUPS) {
                        LOG_ERR("%s: Invalid number of groups used\n",
                                __func__);
                        return NVMEDIA_STATUS_ERROR;
                    }
                    allGroups->groups[allGroups->numGroups].firstCommand = i;
                    groupRegisterActive = NVMEDIA_TRUE;
                } else if (command->commandType == SECTION_STOP) {
                    allGroups->groups[allGroups->numGroups].numCommands++;
                    groupRegisterActive = NVMEDIA_FALSE;
                    allGroups->numGroups++;
                } else {
                    allGroups->groups[allGroups->numGroups].numCommands++;
                }
                break;
            case(DEFAULT):
            case(PRESET_REG):
                // Do nothing
                break;
            default:
                LOG_ERR("%s: Invalid process type encountered\n");
                return NVMEDIA_STATUS_ERROR;
        }
    }
    return NVMEDIA_STATUS_OK;
}

static NvMediaStatus
ProcessCommands(I2cHandle handle, unsigned int startCmd, unsigned int stopCmd,
                I2cCommands *allCommands, I2cOperation operation, ProcessType type)
{
    Command *cmd = NULL;
    uint32_t  i = 0;
    NvMediaBool checkI2cErr = NVMEDIA_TRUE;
    uint8_t readWriteData = 0;

    for (i = startCmd; i < stopCmd; i++) {
        cmd = &allCommands->commands[i];

        if(cmd->processType != type && operation == I2C_WRITE) {
            continue;
        }

        switch (cmd->commandType) {
            case(I2C_DEVICE):
                if (operation == I2C_WRITE) {
                    if (handle)
                        testutil_i2c_close(handle);
                    testutil_i2c_open(cmd->i2cDevice, &handle);
                    if (!handle) {
                        LOG_ERR("%s: Failed to open handle with id %u\n",
                                __func__, cmd->i2cDevice);
                        return NVMEDIA_STATUS_ERROR;
                    }
                    LOG_DBG("%s: Opening handle %u\n", __func__,
                            cmd->i2cDevice);
                }
                break;
            case(I2C_ERR):
                if (operation == I2C_WRITE)
                    checkI2cErr = cmd->i2cErr;
                break;
            case(DELAY):
                if (operation == I2C_WRITE) {
                    nvsleep(cmd->delay);
                }
                break;
            case(WRITE_REG_1):
                if (operation == I2C_WRITE) {
                    nvsleep(5);
                    if (testutil_i2c_write_subaddr(handle,
                       cmd->deviceAddress,
                       cmd->buffer,
                       cmd->dataLength + 1) && checkI2cErr)
                    {
                        LOG_ERR("%s: Failed to write to I2C %02x %02x %02x",
                                __func__, cmd->deviceAddress,
                                cmd->buffer[0],
                                cmd->buffer[1]);
                        return NVMEDIA_STATUS_ERROR;
                    }
#ifndef DEBUG
                    break;
#endif
                }
                // Fall through to read if reading registers for dump
                // or to read after a write (only in DEBUG mode)
            case(READ_REG_1):
                // Reads one byte data ONLY
                if (testutil_i2c_read_subaddr(handle,
                            cmd->deviceAddress,
                            cmd->buffer,
                            sizeof(char),
                            &cmd->buffer[1],
                            sizeof(char)) && checkI2cErr)
                {
                    LOG_ERR("%s: Failed to read I2C %02x %02x",
                            __func__, cmd->deviceAddress,
                            cmd->buffer[0]);
                    return NVMEDIA_STATUS_ERROR;
                }

                LOG_INFO("%s: I2C Read: %02x %02x %02x", __func__,
                        cmd->deviceAddress,
                        cmd->buffer[0],
                        cmd->buffer[1]);

                break;
            case(WRITE_REG_2):
                if (operation == I2C_WRITE) {
                    nvsleep(5);
                    if (testutil_i2c_write_subaddr(handle,
                                cmd->deviceAddress,
                                cmd->buffer,
                                cmd->dataLength + 2) && checkI2cErr)
                    {
                        LOG_ERR("%s: Failed to write to I2C %02x %02x%02x %02x",
                                __func__, cmd->deviceAddress,
                                cmd->buffer[0],
                                cmd->buffer[1],
                                cmd->buffer[2]);
                        return NVMEDIA_STATUS_ERROR;
                    }
#ifndef DEBUG
                    break;
#endif
                }
                // Fall through to read if reading registers for dump
                // or to read after a write (only in DEBUG mode)
            case(READ_REG_2):
                // Reads one byte data ONLY
                if (testutil_i2c_read_subaddr(handle,
                            cmd->deviceAddress,
                            cmd->buffer,
                            sizeof(char)*2,
                            &cmd->buffer[2],
                            sizeof(char)) && checkI2cErr)
                {
                    LOG_ERR("%s: Failed to read I2C %02x %02x%02x",
                            __func__, cmd->deviceAddress,
                            cmd->buffer[0],
                            cmd->buffer[1]);
                        return NVMEDIA_STATUS_ERROR;
                }

                LOG_INFO("%s: I2C Read: %02x %02x%02x %02x", __func__,
                        cmd->deviceAddress,
                        cmd->buffer[0],
                        cmd->buffer[1],
                        cmd->buffer[2]);
                break;
            case(READ_WRITE_REG_1):
                // Read-writes one byte data ONLY
                if (testutil_i2c_read_subaddr(handle,
                            cmd->deviceAddress,
                            cmd->buffer,
                            sizeof(char),
                            &readWriteData,
                            sizeof(char)) && checkI2cErr)
                {
                    LOG_ERR("%s: Failed to read I2C %02x %02x",
                            __func__, cmd->deviceAddress,
                            cmd->buffer[0]);
                    return NVMEDIA_STATUS_ERROR;
                }
#ifdef DEBUG
                LOG_DBG("%s: %02x %02x %02x", __func__,
                        cmd->deviceAddress,
                        cmd->buffer[0],
                        readWriteData);
#endif
                cmd->buffer[2] = readWriteData;
                nvsleep(5);
                if (testutil_i2c_write_subaddr(handle,
                   cmd->deviceAddress,
                   &cmd->buffer[1],
                   cmd->dataLength + 1) && checkI2cErr)
                {
                    LOG_ERR("%s: Failed to write to I2C %02x %02x %02x",
                            __func__, cmd->deviceAddress,
                            cmd->buffer[1],
                            cmd->buffer[2]);
                    return NVMEDIA_STATUS_ERROR;
                }
                break;
            case(READ_WRITE_REG_2):
                // Read-writes one byte data ONLY
                if (testutil_i2c_read_subaddr(handle,
                            cmd->deviceAddress,
                            cmd->buffer,
                            sizeof(char)*2,
                            &readWriteData,
                            sizeof(char)) && checkI2cErr)
                {
                    LOG_ERR("%s: Failed to read I2C %02x %02x%02x",
                            __func__, cmd->deviceAddress,
                            cmd->buffer[0],
                            cmd->buffer[1]);
                        return NVMEDIA_STATUS_ERROR;
                }
#ifdef DEBUG
                LOG_DBG("%s: %02x %02x%02x %02x", __func__,
                        cmd->deviceAddress,
                        cmd->buffer[0],
                        cmd->buffer[1],
                        readWriteData);
#endif
                cmd->buffer[4] = readWriteData;
                nvsleep(5);
                if (testutil_i2c_write_subaddr(handle,
                            cmd->deviceAddress,
                            &cmd->buffer[2],
                            cmd->dataLength + 2) && checkI2cErr)
                {
                    LOG_ERR("%s: Failed to write to I2C %02x %02x%02x %02x",
                            __func__, cmd->deviceAddress,
                            cmd->buffer[2],
                            cmd->buffer[3],
                            cmd->buffer[4]);
                    return NVMEDIA_STATUS_ERROR;
                }
                break;
            case(SECTION_START):
            case(SECTION_STOP):
                // Do nothing
                break;
            default:
                LOG_ERR("%s: Invalid command type encountered\n", __func__);
                return NVMEDIA_STATUS_ERROR;
        }
    }

    return NVMEDIA_STATUS_OK;
}

NvMediaStatus
I2cProcessInitialRegisters(I2cCommands *allCommands, int i2cDevice)
{
    I2cHandle handle = NULL;
    NvMediaStatus status;

    testutil_i2c_open(i2cDevice, &handle);
    if (!handle) {
        LOG_ERR("%s: Failed to open handle with id %u\n", __func__,
                i2cDevice);
        return NVMEDIA_STATUS_ERROR;
    }

    status = ProcessCommands(handle, 0, allCommands->numCommands, allCommands,
                             I2C_WRITE, PRESET_REG);
    if (status != NVMEDIA_STATUS_OK) {
        goto done;
    }

    status = NVMEDIA_STATUS_OK;
done:
    if (handle)
        testutil_i2c_close(handle);
    return status;
}

NvMediaStatus
I2cProcessGroup(I2cHandle handle, I2cCommands *allCommands, GroupData *grpData)
{
    NvMediaStatus status;

    status = ProcessCommands(handle, grpData->firstCommand,
                             (grpData->firstCommand + grpData->numCommands),
                             allCommands, I2C_WRITE, GROUP_REG);
    if(status != NVMEDIA_STATUS_OK) {
        LOG_ERR("%s: Failed to write group registers\n", __func__);
    }

    return status;
}

NvMediaStatus
I2cProcessCommands(I2cCommands *allCommands, I2cOperation operation,
                   int i2cDevice)
{
    I2cHandle handle = NULL;
    NvMediaStatus status;

    testutil_i2c_open(i2cDevice, &handle);
    if(!handle) {
        LOG_ERR("%s: Failed to open handle with id %u\n", __func__,
                i2cDevice);
        return NVMEDIA_STATUS_ERROR;
    }

    status = ProcessCommands(handle, 0, allCommands->numCommands,
                             allCommands, operation, DEFAULT);

    testutil_i2c_close(handle);

    return status;
}

uint8_t *
I2cSetupRegister(I2cCommands *allCommands,
                 CommandType type,
                 uint32_t deviceAddress,
                 uint8_t *regAddress,
                 uint8_t *valueToWrite,
                 uint32_t dataRegLen)
{
    Command *cmd = NULL;
    uint8_t *data = NULL;

    if (allCommands->numCommands >= MAX_NUM_COMMANDS) {
        LOG_ERR("%s: Maximum number of commands reached\n");
        return NULL;
    }
    cmd = &(allCommands->commands[allCommands->numCommands]);

    switch (type) {
        case(WRITE_REG_1):
            cmd->buffer[1] = *valueToWrite;
            if (dataRegLen == 2) {
               cmd->buffer[2] = *(valueToWrite + 1);
            } else {
                memcpy(&cmd->buffer[2], valueToWrite + 1, dataRegLen - 1);
            }
        case(READ_REG_1):
            cmd->buffer[0] = regAddress[0];
            data = &cmd->buffer[1];
            break;
        case(WRITE_REG_2):
            cmd->buffer[2] = *valueToWrite;
            if (dataRegLen == 2) {
               cmd->buffer[3] = *(valueToWrite + 1);
            } else {
                memcpy(&cmd->buffer[3], valueToWrite + 1, dataRegLen - 1);
            }
        case(READ_REG_2):
            cmd->buffer[0] = regAddress[0];
            cmd->buffer[1] = regAddress[1];
            data = &cmd->buffer[2];
            break;
        case(DELAY):
            cmd->delay = dataRegLen;
            data = &cmd->buffer[0]; //The caller will check for NULL return to check failure
            break;
        case(I2C_DEVICE):
        case(I2C_ERR):
        case(SECTION_START):
        case(SECTION_STOP):
            LOG_ERR("%s: Unsupported command type used. \n", __func__);
            return NULL;
        default:
            LOG_ERR("%s: Unknown command type found\n", __func__);
            return NULL;
    }

    if (type != DELAY) {
        cmd->deviceAddress = deviceAddress;
    }
    cmd->commandType = type;
    cmd->processType = DEFAULT;
    cmd->dataLength = dataRegLen;
    allCommands->numCommands++;
    return data;
}

uint32_t
I2cGetNumCommands(I2cCommands *allCommands)
{
    return allCommands->numCommands;
}

void
I2cSetNumCommands(I2cCommands *allCommands,
    uint32_t setNumCommands)
{
    if (setNumCommands >= MAX_NUM_COMMANDS) {
        LOG_ERR("%s: Maximum number of commands reached\n");
        return;
    }

    allCommands->numCommands = setNumCommands;
}
