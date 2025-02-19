/***********************************************************************************************************************
 * File Name    : hal_entry.c
 * Description  : Contains data structures and functions used in hal_entry.c.
 **********************************************************************************************************************/
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
 * Copyright (C) 2020 Renesas Electronics Corporation. All rights reserved.
 ***********************************************************************************************************************/

#include "common_utils.h"
#include "icu_ep.h"


/*******************************************************************************************************************//**
 * @addtogroup icu_ep
 * @{
 **********************************************************************************************************************/


FSP_CPP_HEADER
void R_BSP_WarmStart(bsp_warm_start_event_t event);
FSP_CPP_FOOTER



/* Board's user LED */
extern bsp_leds_t g_bsp_leds;
/* Boolean flag to determine switch is pressed or not.*/
extern volatile bool g_sw_press;

/*******************************************************************************************************************//**
 * main() is generated by the RA Configuration editor and is used to generate threads if an RTOS is used.  This function
 * is called by main() when no RTOS is used.
 **********************************************************************************************************************/
void hal_entry(void)
{
    fsp_err_t err                           = FSP_SUCCESS;
    bsp_io_level_t led_current_state        = (bsp_io_level_t) RESET_VALUE;
    fsp_pack_version_t version              = {RESET_VALUE};

    /* LED type structure */
    bsp_leds_t leds = g_bsp_leds;

    /* version get API for FLEX pack information */
    R_FSP_VersionGet(&version);

    /* Example Project information printed on the RTT */
    APP_PRINT(BANNER_INFO, EP_VERSION, version.major, version.minor, version.patch);
    APP_PRINT(EP_INFO);

    /* Initialize External IRQ driver*/
    err = icu_init();
    /* Handle error */
    if(FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("** ICU INIT FAILED ** \r\n");
        APP_ERR_TRAP(err);
    }

    /* Enable External IRQ driver*/
    err = icu_enable();
    /* Handle error */
    if(FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("** ICU ENABLE FAILED ** \r\n");
        /* Close External IRQ module.*/
        icu_deinit();
        APP_ERR_TRAP(err);
    }

    /* If this board has no LEDs then trap here */
    if (RESET_VALUE == leds.led_count)
    {
        APP_PRINT("\r\nThere are no LEDs on this board\r\n");
        /* Close External IRQ module.*/
        icu_deinit();
        APP_ERR_TRAP(err);
    }

    /* Main loop */
    while (true)
    {
        /* Toggle user LED  when user pushbutton is pressed*/
        if(true == g_sw_press)
        {
            /* Clear user pushbutton flag */
            g_sw_press = false;

            /* Notify that user pushbutton is pressed */
            APP_PRINT("\r\nUser Pushbutton Pressed\r\n");

            /* Read user LED  pin */
            err = R_IOPORT_PinRead(&g_ioport_ctrl, (bsp_io_port_pin_t)leds.p_leds[RESET_VALUE], &led_current_state);
            /* Handle error */
            if (FSP_SUCCESS != err)
            {
                APP_ERR_PRINT("** R_IOPORT_PinRead FAILED ** \r\n");
                /* Close External IRQ module.*/
                icu_deinit();
                APP_ERR_TRAP(err);
            }

            /* Reverse LED pin state*/
            led_current_state ^= BSP_IO_LEVEL_HIGH;

            /* Toggle user LED */
            err = R_IOPORT_PinWrite(&g_ioport_ctrl, (bsp_io_port_pin_t)leds.p_leds[RESET_VALUE], led_current_state);

            /* Handle error */
            if (FSP_SUCCESS != err)
            {
                APP_ERR_PRINT("** R_IOPORT_PinWrite FAILED ** \r\n");
                /* Close External IRQ module.*/
                icu_deinit();
                APP_ERR_TRAP(err);
            }

#if defined (BOARD_RA6T1_RSSK) || defined (BOARD_RA4W1_EK) || defined (BOARD_RA6T2_MCK)
            if(BSP_IO_LEVEL_LOW == led_current_state)
#else
                if(BSP_IO_LEVEL_HIGH == led_current_state)
#endif
                {
                    /* Print LED Pin state */
                    APP_PRINT("LED State: High{ON}\r\n");
                }
                else
                {
                    /* Print LED Pin state */
                    APP_PRINT("LED State: Low{OFF}\r\n");
                }
        }
    }
}

/*******************************************************************************************************************//**
 * This function is called at various points during the startup process.  This implementation uses the event that is
 * called right before main() to set up the pins.
 *
 * @param[in]  event    Where at in the start up process the code is currently at
 **********************************************************************************************************************/
void R_BSP_WarmStart(bsp_warm_start_event_t event) {
    if (BSP_WARM_START_RESET == event) {
#if BSP_FEATURE_FLASH_LP_VERSION != 0

        /* Enable reading from data flash. */
        R_FACI_LP->DFLCTL = 1U;

        /* Would normally have to wait tDSTOP(6us) for data flash recovery. Placing the enable here, before clock and
         * C runtime initialization, should negate the need for a delay since the initialization will typically take more than 6us. */
#endif
    }

    if (BSP_WARM_START_POST_C == event) {
        /* C runtime environment and system clocks are setup. */

        /* Configure pins. */
        R_IOPORT_Open(&g_ioport_ctrl, &g_bsp_pin_cfg);
    }
}

/*******************************************************************************************************************//**
 * @} (end addtogroup icu_ep)
 **********************************************************************************************************************/
