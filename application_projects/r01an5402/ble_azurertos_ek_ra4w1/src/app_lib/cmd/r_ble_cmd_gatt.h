/***********************************************************************************************************************
* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
* other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
* applicable laws, including copyright laws.
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
* THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
* EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
* SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
* SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
* this software. By using this software, you agree to the additional terms and conditions found by accessing the
* following link:
* http://www.renesas.com/disclaimer
*
* Copyright (C) 2019-2020 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @file
 * @defgroup cmd_gatt GATT Command
 * @{
 * @ingroup app_lib
 * @brief GATT Commands
 * @details Provides CLI for GATT. And exposes APIs used in the commands.
***********************************************************************************************************************/
/***********************************************************************************************************************
* History : DD.MM.YYYY Version Description           
*         : 15.12.2020 1.00    First Release
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include "cli/r_ble_cli.h"

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/
#ifndef R_BLE_CMD_GATT_H
#define R_BLE_CMD_GATT_H

/*******************************************************************************************************************//**
 * @brief Maximum attr value length used in this library.
***********************************************************************************************************************/
#define BLE_CMD_ATTR_VAL_MAX_LEN (23)

/***********************************************************************************************************************
Exported global functions (to be accessed by other files)
***********************************************************************************************************************/

/** @brief GATT Server command definition. */
extern const st_ble_cli_cmd_t g_gatts_cmd;

/** @brief GATT Client command definition. */
extern const st_ble_cli_cmd_t g_gattc_cmd;

/*******************************************************************************************************************//**
 * @brief Register GATT command set.
***********************************************************************************************************************/
void R_BLE_CMD_RegisterGattCmd(void);

/*******************************************************************************************************************//**
 * @brief GATTC event handler.
 * @details This function shall be called from GATTS callback registred by @ref R_BLE_GATTC_RegisterCb function.
***********************************************************************************************************************/
void R_BLE_CMD_GattcCb(uint16_t type, ble_status_t result, st_ble_gattc_evt_data_t *data);

/*******************************************************************************************************************//**
 * @brief GATTS event handler.
 * @details This function shall be called from GATTS callback registred by @ref R_BLE_GATTS_RegisterCb function.
***********************************************************************************************************************/
void R_BLE_CMD_GattsCb(uint16_t type, ble_status_t result, st_ble_gatts_evt_data_t *data);

#endif /* R_BLE_CMD_GATT_H */

/** @} */
