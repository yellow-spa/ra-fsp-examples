/***********************************************************************************************************************
 * File Name    : i2c_slave.c
 * Description  : Contains data structures and functions used in i2c_slave.c.
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
 * Copyright (C) 2019 Renesas Electronics Corporation. All rights reserved.
 ***********************************************************************************************************************/

#include "common_utils.h"
#include "i2c_slave.h"


/*******************************************************************************************************************//**
 * @addtogroup r_iic_slave_ep
 * @{
 **********************************************************************************************************************/

/*
 * Private Global Variables
 */

/* enumerators to identify Slave event to be processed when on board push button is pressed */
typedef enum slave
{
    SLAVE_READ  = 1U,
    SLAVE_WRITE = 2U,
    SLAVE_NO_RW = 3U
}slave_transfer_mode_t;

/* Slave transmit buffer */
static uint8_t g_slave_tx_buf[BUF_LEN];
/* Slave receive buffer */
static uint8_t g_slave_rx_buf[BUF_LEN];
/* Master buffer for both read and write */
static uint8_t g_master_buf[BUF_LEN];

/* capture callback event for Slave and master module */
static volatile i2c_master_event_t g_master_event = (i2c_master_event_t)RESET_VALUE;
static volatile i2c_slave_event_t  g_slave_event  = (i2c_slave_event_t)RESET_VALUE;

/* Capture return value from slave read and write API */
static volatile fsp_err_t g_slave_api_ret_err = FSP_SUCCESS;

/* IRQ callback */
static volatile uint8_t g_switch_count = false;
/* Event flag to identify the slave event when on board push button is pressed  */
static volatile slave_transfer_mode_t g_slave_RW  = SLAVE_NO_RW;

/* for on board LEDs */
extern bsp_leds_t g_bsp_leds;

/*
 * private functions
 */
static fsp_err_t iic_slave_write(void);
static fsp_err_t iic_slave_read(void);
static void toggle_led(void);

/*******************************************************************************************************************//**
 * @brief     Initialize IIC master and slave driver.
 * @param[IN] None
 * @retval    FSP_SUCCESS       R_IIC_Master and Slave driver opened successfully.
 * @retval    err               Any Other Error code apart from FSP_SUCCES like Unsuccessful Open.
 **********************************************************************************************************************/
fsp_err_t init_i2C_driver(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Open mater I2C channel */
    err = R_IIC_MASTER_Open(&g_i2c_master_ctrl,&g_i2c_master_cfg);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT ("R_IIC_MASTER_Open API failed \r\n");
        return err;
    }

    /* Open slave I2C channel */
    err = R_IIC_SLAVE_Open(&g_i2c_slave_ctrl, g_i2c_slave.p_cfg);
    if (FSP_SUCCESS != err)
    {
        /* Display failure message in RTT */
        APP_ERR_PRINT ("R_IIC_SLAVE_Open API failed \r\n");

        /* Slave Open unsuccessful closing master module */

    	if (FSP_SUCCESS !=  R_IIC_MASTER_Close(&g_i2c_master_ctrl))
        {
            /* Display failure message in RTT */
            APP_ERR_PRINT ("R_IIC_SLAVE_Close API failed \r\n");
        }
    }

    return err;
}

/*******************************************************************************************************************//**
 *  @brief     Initialize and enable external IRQ on pin connected to on board push button
 *  @param[IN] None
 *  @retval    FSP_SUCCESS       External irq driver opened and enabled successfully.
 *  @retval    err               Any Other Error code apart from FSP_SUCCES  Unsuccessful initialization.
 **********************************************************************************************************************/
fsp_err_t init_ext_irq(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Open external IRQ module for on board Switch detection */
    err = R_ICU_ExternalIrqOpen(&g_external_irq_ctrl, &g_external_irq_cfg);
    /* handle error */
    if (FSP_SUCCESS != err)
    {
        /* Display failure message in RTT */
        APP_ERR_PRINT ("R_ICU_ExternalIrqOpen API failed \r\n");
        return err;
    }

    /* Enable external interrupt for specified pin */
    err = R_ICU_ExternalIrqEnable(&g_external_irq_ctrl);
    /* handle error */
    if (FSP_SUCCESS != err)
    {
        /* close opened external irq module since module is in open state now  */
        deinit_external_irq();

        /* Display failure message in RTT */
        APP_ERR_PRINT ("External Irq Enable failed \r\n");
    }

    return err;
}

/*******************************************************************************************************************//**
 *  @brief      performs Slave write read operations and toggle LED on successful operation
 *              else Turn LED ON on failure
 *              If slave i2c transaction failure occurs then it halts the application turning LED ON
 *              as sign of failure. Also displays failure messages in RTT.
 *  @retval     FSP_SUCCESS    On successfully R_IIC_Mater and Slave Write/Read operation.
 *  @retval     err            Any Other Error code apart from FSP_SUCCES  Unsuccessful Write/Read operation.
 **********************************************************************************************************************/
fsp_err_t process_slave_WriteRead(void)
{
    fsp_err_t      error            = FSP_SUCCESS;

    switch(g_slave_RW)
    {
        case SLAVE_READ:
        {
            /* Reset variable value */
            g_slave_RW = SLAVE_NO_RW;

            /* Before beginning the operation turn off LED */
            set_led(LED_OFF);

            /*slave starts reading the data*/
            error = iic_slave_read();
            if (FSP_SUCCESS != error)
            {
                /* Turn on LED */
                set_led(LED_ON);

                /* print RTT message */
                APP_ERR_PRINT ("** Slave read operation failed !  **\r\n");
            }
            else
            {
                /* toggle LED as sign of success*/
                toggle_led();

                /* print RTT message */
                APP_PRINT ("** Slave read operation is successful **\r\n");
            }

            break;
        }
        case SLAVE_WRITE:
        {
            /* Reset variable value */
            g_slave_RW = SLAVE_NO_RW;

            /* Before beginning the operation turn off LED */
            set_led(LED_OFF);

            /* slave starts transmission of data */
            error = iic_slave_write();
            if (FSP_SUCCESS != error)
            {
                /* Turn on LED */
                set_led(LED_ON);

                /* print RTT message */
                APP_ERR_PRINT ("** Slave write operation Failed ! **\r\n");
            }
            else
            {
                /* toggle LED as sign of success*/
                toggle_led();

                /* print RTT message */
                APP_PRINT ("** Slave Write operation is successful **\r\n");
            }

            break;
        }
        default:
        	break;
    }

    return error;
}

/*******************************************************************************************************************//**
 *  @brief       Perform Slave write operation
 *  @param[in]   None
 *  @retval      FSP_SUCCESS               Slave successfully write all data to master.
 *  @retval      FSP_ERR_TRANSFER_ABORTED  callback event failure
 *  @retval      FSP_ERR_ABORTED           data mismatched occurred.
 *  @retval      FSP_ERR_TIMEOUT           in case of no callback event occurrence
 *  @retval      write_err                 API returned error if any
**********************************************************************************************************************/
static fsp_err_t iic_slave_write(void)
{
    fsp_err_t write_err           = FSP_SUCCESS;
    uint16_t write_time_out       = UINT16_MAX;
    uint8_t write_buffer[BUF_LEN] = {0x01, 0x02, 0x03, 0x04, 0x05};

    /* update slave transmit buffer and master is recipient so clear master buffer*/
    memset(g_master_buf,'\0',BUF_LEN);
    memcpy(g_slave_tx_buf, write_buffer, BUF_LEN);

    /* resetting callback event */
    g_master_event = (i2c_master_event_t)RESET_VALUE;
    g_slave_event  = (i2c_slave_event_t)RESET_VALUE;

    /* Start master read.  Master has to initiate the transfer. */
    write_err = R_IIC_MASTER_Read(&g_i2c_master_ctrl, g_master_buf, BUF_LEN, false);
    if (FSP_SUCCESS != write_err)
    {
        APP_ERR_PRINT("\r\n ** R_IIC_MASTER_Read API failed ** \r\n");
        return write_err;
    }

    /* Wait until slave write and master read process gets completed */
    while( (I2C_SLAVE_EVENT_TX_COMPLETE != g_slave_event) || (I2C_MASTER_EVENT_RX_COMPLETE != g_master_event) )
    {
        /* check for aborted event */
        if ( (I2C_MASTER_EVENT_ABORTED == g_master_event) || (I2C_SLAVE_EVENT_ABORTED == g_slave_event) )
        {
            	APP_ERR_PRINT ("** EVENT_ABORTED received during Slave write operation **\r\n");

                /* I2C transaction failure */
                return FSP_ERR_TRANSFER_ABORTED;
        }
        /*
         * handle error for slave write API return value g_slave_api_ret_err gets
         * updated only in case of error return from SLAVE write API
         */
        else if (FSP_SUCCESS != g_slave_api_ret_err)
        {
        	write_err = g_slave_api_ret_err;

        	/* reset this with success again for further usage to capture in case of API failure */
        	g_slave_api_ret_err = FSP_SUCCESS;

        	return write_err;
        }
        else
        {
            /* start checking for time out to avoid infinite loop */
            --write_time_out;

            /* check for time elapse */
            if (RESET_VALUE == write_time_out)
            {
                /* we have reached to a scenario where i2c event not occurred */
            	APP_ERR_PRINT (" ** No event received during slave write operation **\r\r");

            	/* no event received */
            	return FSP_ERR_TIMEOUT;
            }
        }
    }

    /* compare Slave write buffer with data received by master device */
    if ( BUFF_EQUAL == memcmp(g_slave_tx_buf,g_master_buf, BUF_LEN) )
    {
        write_err = FSP_SUCCESS;
    }
    else
    {
        write_err = FSP_ERR_ABORTED;
    }

    return write_err;
}

/*******************************************************************************************************************//**
 *  @brief      Perform Slave read operation
 *  @param[in]  None
 *  @retval     FSP_SUCCESS               Slave successfully received all data issued by master.
 *  @retval     FSP_ERR_TRANSFER_ABORTED  callback event failure
 *  @retval     FSP_ERR_ABORTED           data mismatched occurred.
 *  @retval     FSP_ERR_TIMEOUT           in case of no callback event occurrence
 *  @retval     read_err                  API returned error if any
 **********************************************************************************************************************/
static fsp_err_t iic_slave_read(void)
{
    fsp_err_t read_err           = FSP_SUCCESS;
    uint16_t read_time_out       = UINT16_MAX;
    uint8_t read_buffer[BUF_LEN] = {0x10, 0x20, 0x30, 0x40, 0x50};

    /* update master buffer and slave is recipient so clear slave RX buffer*/
    memset(g_slave_rx_buf,'\0',BUF_LEN);
    memcpy(g_master_buf, read_buffer, BUF_LEN);

    /* resetting callback event */
    g_master_event = (i2c_master_event_t)RESET_VALUE;
    g_slave_event  = (i2c_slave_event_t)RESET_VALUE;

    /* Master write to slave  */
    read_err = R_IIC_MASTER_Write(&g_i2c_master_ctrl, g_master_buf, BUF_LEN, false);
    /* Handle error */
    if (FSP_SUCCESS != read_err)
    {
        APP_ERR_PRINT("\r\n ** R_IIC_MASTER_Write API failed ** \r\n");
        return read_err;
    }

    /* Wait until slave read and master write process gets completed */
    while( (I2C_SLAVE_EVENT_RX_COMPLETE != g_slave_event) || (I2C_MASTER_EVENT_TX_COMPLETE != g_master_event) )
    {
    	 /* check for aborted event */
        if ( (I2C_MASTER_EVENT_ABORTED == g_master_event) || (I2C_SLAVE_EVENT_ABORTED == g_slave_event) )
        {
           	APP_ERR_PRINT ("** error EVENT_ABORTED received during Slave read operation **\r\n");

            /* I2C transaction failure */
            return FSP_ERR_TRANSFER_ABORTED;

        }
        /*
         * handle error for slave read API return value g_slave_api_ret_err gets
         * updated only in case of error return from SLAVE read API
         */
        else if (FSP_SUCCESS != g_slave_api_ret_err)
        {
        	read_err = g_slave_api_ret_err;

        	/* reset this with success again for further usage to capture in case of API failure */
        	g_slave_api_ret_err = FSP_SUCCESS;

        	return read_err;
        }
        else
        {
            /* start checking for time out to avoid infinite loop */
            --read_time_out;

            /* check for time elapse */
            if (RESET_VALUE == read_time_out)
            {
            	/* we have reached to a scenario where i2c event not occurred */
            	APP_ERR_PRINT (" ** No event received during slave read operation **\r\r");

            	/* no event received */
            	return FSP_ERR_TIMEOUT;
            }
        }
    }

    /* compare Slave read buffer with data written by master device */
    if ( BUFF_EQUAL == memcmp(g_slave_rx_buf, g_master_buf, BUF_LEN) )
    {
        read_err = FSP_SUCCESS;
    }
    else
    {
        read_err = FSP_ERR_ABORTED;
    }

    return read_err;
}

/*******************************************************************************************************************//**
 *  @brief        User defined master callback function
 *  @param[in]    p_args
 *  @retval       None
 **********************************************************************************************************************/
void i2c_master_callback(i2c_master_callback_args_t * p_args)
{
    if (NULL != p_args)
    {
        g_master_event = p_args->event;
    }
}

/*******************************************************************************************************************//**
 *  @brief        User defined slave callback function.
 *  @param[in]    p_args
 *  @retval       None
 **********************************************************************************************************************/
void i2c_slave_callback(i2c_slave_callback_args_t * p_args)
{
    fsp_err_t err = FSP_SUCCESS;

    if (NULL != p_args)
    {
        switch(p_args->event)
        {
            case I2C_SLAVE_EVENT_RX_REQUEST:
            {
            	/*Perform Slave Read operation*/
                err = R_IIC_SLAVE_Read(&g_i2c_slave_ctrl, g_slave_rx_buf, BUF_LEN);
                if(FSP_SUCCESS != err)
                {
                	/* Update return error here */
                	g_slave_api_ret_err = err;
                }

                break;
            }
            case I2C_SLAVE_EVENT_TX_REQUEST:
            {
                /*Perform Slave Write operation*/
                err = R_IIC_SLAVE_Write(&g_i2c_slave_ctrl, g_slave_tx_buf, BUF_LEN);
                if(FSP_SUCCESS != err)
                {
                	/* Update return error here */
                	g_slave_api_ret_err = err;
                }

                break;
            }
            default:
                /* log the event to global variable for any other slave event */
                g_slave_event = p_args->event;
                break;
        }
    }
 }

/*******************************************************************************************************************//**
 *  @brief           User defined external IRQ driver callback function
 *  @param[in]       p_args
 *  @retval          None
 **********************************************************************************************************************/
void ext_irq_cb(external_irq_callback_args_t *p_args)
{
    /* on a push button press output is displayed to RTT and can be viewed as LED status  */
    if ( (NULL != p_args) && (IRQ_CHANNEL == p_args->channel) )
    {
    	/* toggle slave read write operation */
    	if(false == g_switch_count)
         {
             g_slave_RW = SLAVE_READ;
             g_switch_count = true;
         }
         else
         {
             g_slave_RW = SLAVE_WRITE;
             g_switch_count = false;
         }
    }
}

/*******************************************************************************************************************//**
 * @brief     Toggle on board LED, which are connected and supported by BSP
 * @param[in] None
 * @retval    None
 **********************************************************************************************************************/
static void toggle_led(void)
{
    /* Get LED information (pins) for this board */
    bsp_leds_t leds = g_bsp_leds;

    for(uint8_t cnt = RESET_VALUE; leds.led_count > cnt; cnt++)
    {
        R_IOPORT_PinWrite(g_ioport.p_ctrl,(bsp_io_port_pin_t)leds.p_leds[cnt], LED_ON);
        R_BSP_SoftwareDelay(TOGGLE_DELAY, BSP_DELAY_UNITS_MILLISECONDS);

        R_IOPORT_PinWrite(g_ioport.p_ctrl,(bsp_io_port_pin_t)leds.p_leds[cnt], LED_OFF);
    }
}

/*******************************************************************************************************************//**
 *  @brief       Turn on_board LED ON or OFF.
 *  @param[in]   led_state     LED_ON or LED_OFF
 *  @retval      None
 **********************************************************************************************************************/
void set_led(bsp_io_level_t led_state)
{
    /* Get LED information (pins) for this board */
    bsp_leds_t leds = g_bsp_leds;

    for(uint8_t cnt = RESET_VALUE ; leds.led_count > cnt; cnt++)
    {
       R_IOPORT_PinWrite(g_ioport.p_ctrl,(bsp_io_port_pin_t)leds.p_leds[cnt], led_state);
    }
}

/*******************************************************************************************************************//**
 * This function is called to do closing of external i2c master and slave module using its HAL level API.
 * @brief     Close the IIC Master and Slave module. Handle the Error internally with Proper RTT Message.
 *            Application handles the rest.
 * @param[in] None
 * @retval    None
 **********************************************************************************************************************/
void deinit_i2c_driver(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* close opened IIC master module */
    err = R_IIC_MASTER_Close(&g_i2c_master_ctrl);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT(" ** R_IIC_MASTER_Close API failed ** \r\n");
    }

    /* close opened IIC slave module */
    err = R_IIC_SLAVE_Close(&g_i2c_slave_ctrl);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT(" ** R_IIC_Slave_Close API failed ** \r\n");
    }
}

/*******************************************************************************************************************//**
 * This function is called to do closing of external irq module using its HAL level API.
 * @brief     Close the external irq module. Handle the Error internally with Proper Message.
 *            Application handles the rest.
 * @param[in] None
 * @retval    None
 **********************************************************************************************************************/
void deinit_external_irq(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* close opened external interrupt module */
    err = R_ICU_ExternalIrqClose(&g_external_irq_ctrl);
    /* Handle error */
    if(FSP_SUCCESS != err)
    {
        APP_ERR_PRINT ("** R_ICU_ExternalIrqClose API failed **\r\n");
    }
}

/*******************************************************************************************************************//**
 * @} (end addtogroup r_iic_slave_ep)
 **********************************************************************************************************************/
