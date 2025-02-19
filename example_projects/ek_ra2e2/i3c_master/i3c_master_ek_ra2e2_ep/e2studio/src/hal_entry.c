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
#include "i3c_master_ep.h"

/*******************************************************************************************************************//**
 * @addtogroup i3c_master_ep
 * @{
 **********************************************************************************************************************/

FSP_CPP_HEADER
void R_BSP_WarmStart(bsp_warm_start_event_t event);
FSP_CPP_FOOTER

/* Global Variables */
static volatile uint32_t g_i3c_event_count[I3C_EVENT_INTERNAL_ERROR + ONE];
static volatile uint32_t g_i3c_event_status = RESET_VALUE;

/* Configure the information for the slave device. */
static i3c_device_cfg_t master_device_cfg =
{
    /* This is the Static I3C / I2C Legacy address defined by the device manufacturer. */
    .static_address  = I3C_MASTER_DEVICE_STATIC_ADDRESS,
    /* The dynamic address will be automatically updated when the master configures this device using CCC ENTDAA. */
    .dynamic_address = I3C_MASTER_DEVICE_DYNAMIC_ADDRESS
};

/* I3C bus/device management */
static i3c_device_table_cfg_t       g_device_table_cfg;
static i3c_device_information_t     g_device_slave_info;
static uint32_t                     g_num_device_on_bus = RESET_VALUE;
static uint8_t                      g_ibi_target_address = RESET_VALUE;
static volatile bool                b_bus_initialized = false;
static volatile bool                b_target_hj_received = false;
static volatile bool                b_target_ibi_transfer_received = false;

/* data buffers */
static uint8_t                      g_ibi_read_data[MAX_IBI_PAYLOAD_SIZE] BSP_ALIGN_VARIABLE(WORD_ALIGN);
static uint8_t                      g_write_data[MAX_WRITE_DATA_LEN] BSP_ALIGN_VARIABLE(WORD_ALIGN);
static uint8_t                      g_read_data[2][MAX_READ_DATA_LEN] BSP_ALIGN_VARIABLE(WORD_ALIGN);
static uint8_t                    * p_next = NULL;
static uint8_t                    * p_last = NULL;
static uint32_t                     g_data_transfer_size = RESET_VALUE;

/* Setup the command descriptor. */
static i3c_command_descriptor_t command_descriptor;

static uint32_t      g_write_read_routine_count = RESET_VALUE;
static volatile bool b_process_timeout = false;


/* Private function declarations.*/
static fsp_err_t i3c_broadcast_ccc_send(void);
static void set_next_read_buffer(void);
static fsp_err_t check_disp_i3c_slaveInfo(void);
static uint32_t i3c_app_event_notify(uint32_t set_event_flag_value, uint32_t timout);
static fsp_err_t master_write_read_verify(void);
static fsp_err_t start_timeout_timer_with_defined_ms(uint32_t timeout_ms);
static fsp_err_t hot_join_request_process(void);
static void i3c_deinit(void);
static void agt_deinit(void);

/*******************************************************************************************************************//**
 * main() is generated by the RA Configuration editor and is used to generate threads if an RTOS is used.  This function
 * is called by main() when no RTOS is used.
 **********************************************************************************************************************/
void hal_entry(void)
{
    fsp_err_t err = FSP_SUCCESS;
    fsp_pack_version_t version = {RESET_VALUE};
    unsigned char input_data[BUFFER_SIZE_DOWN] = {RESET_VALUE};
    uint8_t converted_rtt_input = RESET_VALUE;

    /* version get API for FLEX pack information */
    R_FSP_VersionGet(&version);
    APP_PRINT(BANNER_INFO,EP_VERSION,version.major, version.minor, version.patch );
    APP_PRINT(EP_INFO);

    /* Initialize AGT driver */
    err = R_AGT_Open(&g_timeout_timer_ctrl, &g_timeout_timer_cfg);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT ("\r\nERROR : R_AGT_Open API FAILED \r\n");
        APP_ERR_TRAP(err);
    }

    /* Initializes the I3C module. */
    err = R_I3C_Open(&g_i3c0_ctrl, &g_i3c0_cfg);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT ("\r\nERROR : R_I3C_Open API FAILED \r\n");
        /* de-initialize the opened AGT timer module.*/
        agt_deinit();
        APP_ERR_TRAP(err);
    }
    APP_PRINT("\r\nINFO : I3C Initialized successfully in master mode.\r\n");

    /* Set the device configuration for this device. */
    err = R_I3C_DeviceCfgSet(&g_i3c0_ctrl, &master_device_cfg);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT ("\r\nERROR : R_I3C_DeviceCfgSet API FAILED \r\n");
        /* de-initialize the opened I3C and AGT timer module.*/
        i3c_deinit();
        agt_deinit();
        APP_ERR_TRAP(err);
    }

    /* Set the I3C devices information through device table entries */
    memset(&g_device_table_cfg, RESET_VALUE, sizeof(i3c_device_table_cfg_t));

    g_device_table_cfg.dynamic_address = (uint8_t)(I3C_SLAVE_DEVICE_DYNAMIC_ADDRESS_START);
    g_device_table_cfg.device_protocol = I3C_DEVICE_PROTOCOL_I3C;
    g_device_table_cfg.ibi_accept = true;
    g_device_table_cfg.ibi_payload = true;
    g_device_table_cfg.master_request_accept = false;

    err = R_I3C_MasterDeviceTableSet(&g_i3c0_ctrl, RESET_VALUE, &g_device_table_cfg);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT ("\r\nERROR : R_I3C_MasterDeviceTableSet API FAILED \r\n");
        /* de-initialize the opened I3C and AGT timer module.*/
        i3c_deinit();
        agt_deinit();
        APP_ERR_TRAP(err);
    }

    /* Enable I3C Mode. */
    err = R_I3C_Enable(&g_i3c0_ctrl);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT ("\r\nERROR : R_I3C_Enable API FAILED \r\n");
        /* de-initialize the opened I3C and AGT timer module.*/
        i3c_deinit();
        agt_deinit();
        APP_ERR_TRAP(err);
    }

    /* Set the buffer for storing IBI data that is read from the slave. */
    err = R_I3C_IbiRead(&g_i3c0_ctrl, g_ibi_read_data, MAX_IBI_PAYLOAD_SIZE);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT ("\r\nERROR : R_I3C_IbiRead API FAILED \r\n");
        /* de-initialize the opened I3C and AGT timer module.*/
        i3c_deinit();
        agt_deinit();
        APP_ERR_TRAP(err);
    }

    /* Reset the buffer for storing data received during a read transfer. */
    p_next = g_read_data[RESET_VALUE];

    /* Start assigning dynamic addresses to devices on the bus using the CCC ENTDAA command. */
    /* we have one slave device hence last argument is ONE */
    err = R_I3C_DynamicAddressAssignmentStart(&g_i3c0_ctrl, I3C_ADDRESS_ASSIGNMENT_MODE_ENTDAA, RESET_VALUE, ONE);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT ("\r\nERROR : R_I3C_DynamicAddressAssignmentStart API FAILED \r\n");
        /* de-initialize the opened I3C and AGT timer module.*/
        i3c_deinit();
        agt_deinit();
        APP_ERR_TRAP(err);
    }

    /* waiting for the bus initialization */
    /* hold up the application until the DAA is completed */
    i3c_app_event_notify(I3C_EVENT_FLAG_ADDRESS_ASSIGNMENT_COMPLETE, WAIT_TIME);
    if((b_bus_initialized)&&(g_num_device_on_bus))
    {
        APP_PRINT ("\r\nINFO : Bus configuration is completed successfully.\r\n");
        APP_PRINT ("INFO : Number of I3C device on bus: %d **\r\n", g_num_device_on_bus);
    }
    else
    {
        APP_PRINT("INFO : Sending CCC broadcast signal for Dynamic address assignment\r\n");
        /* sending broadcast signal.*/
        err = i3c_broadcast_ccc_send();
        if(FSP_SUCCESS != err)
        {
            APP_ERR_PRINT("\r\nERROR : i3c_broadcast_ccc_send function failed.\r\n");
            /* de-initialize the opened I3C and AGT timer module.*/
            i3c_deinit();
            agt_deinit();
            APP_ERR_TRAP(err);
        }
    }
    /* Print menu option.*/
    APP_PRINT(EP_FUNCTION_MENU)

    while (true)
    {
        /*Check for RTT input from user*/
        if (APP_CHECK_DATA)
        {
            /* Cleaning buffer */
            memset(&input_data[RESET_VALUE], NULL_CHAR, BUFFER_SIZE_DOWN);
            converted_rtt_input = RESET_VALUE;

            /*Read RTT input from user*/
            APP_READ (input_data);
            converted_rtt_input = (uint8_t)atoi((char *)input_data);

            switch (converted_rtt_input)
            {
                case DISPLAY_I3C_SLAVE_INFO:
                {
                    /* Display slave info.*/
                    check_disp_i3c_slaveInfo();
                    break;
                }

                case MASTER_WRITE_READ:
                {
                    /* Perform master write read operation.*/
                    err = master_write_read_verify();
                    if (FSP_SUCCESS != err)
                    {
                        APP_ERR_PRINT("\r\nERROR : master_write_read_verify function failed.\r\n");
                        /* de-initialize the opened I3C and AGT timer module.*/
                        i3c_deinit();
                        agt_deinit();
                        APP_ERR_TRAP(err);
                    }
                    break;
                }

                default:
                {
                    APP_PRINT("Invalid Input\r\n");
                    break;
                }
            } /* switch end */
            APP_PRINT(EP_FUNCTION_MENU)
        }/* if APP check end */

        /* wait for I3C events.*/
        uint32_t event_flag = i3c_app_event_notify((I3C_EVENT_FLAG_ADDRESS_ASSIGNMENT_COMPLETE |
                I3C_EVENT_FLAG_COMMAND_COMPLETE |
                I3C_EVENT_FLAG_WRITE_COMPLETE |
                I3C_EVENT_FLAG_READ_COMPLETE |
                I3C_EVENT_FLAG_IBI_READ_COMPLETE |
                I3C_EVENT_FLAG_IBI_READ_BUFFER_FULL |
                I3C_EVENT_FLAG_INTERNAL_ERROR), WAIT_TIME);

        /* check if event is IBI read complete and hot-join request received.*/
        if((event_flag & I3C_EVENT_FLAG_IBI_READ_COMPLETE) && (b_target_hj_received))
        {
            /* perform hot join request process.*/
            err = hot_join_request_process();
            if(FSP_SUCCESS != err)
            {
                APP_ERR_PRINT("\r\nERROR : hot_join_request_process function failed.\r\n");
                /* de-initialize the opened I3C and AGT timer module.*/
                i3c_deinit();
                agt_deinit();
                APP_ERR_TRAP(err);
            }
        }

        /* check if event is IBI read complete and IBI transfer is received.*/
        if((event_flag & I3C_EVENT_FLAG_IBI_READ_COMPLETE) && (b_target_ibi_transfer_received))
        {
            APP_PRINT ("\r\nINFO : a slave IBI transfer is received.\r\n");
            APP_PRINT ("\r\nINFO : Target address:0x%02x, IBI Payload size:%d\r\n", g_ibi_target_address, g_data_transfer_size);
            b_target_ibi_transfer_received = false;
        }

    }

#if BSP_TZ_SECURE_BUILD
    /* Enter non-secure code */
    R_BSP_NonSecureEnter();
#endif
}

/*******************************************************************************************************************//**
 * This function is called at various points during the startup process.  This implementation uses the event that is
 * called right before main() to set up the pins.
 *
 * @param[in]  event    Where at in the start up process the code is currently at
 **********************************************************************************************************************/
void R_BSP_WarmStart(bsp_warm_start_event_t event)
{
    if (BSP_WARM_START_RESET == event)
    {
#if BSP_FEATURE_FLASH_LP_VERSION != 0

        /* Enable reading from data flash. */
        R_FACI_LP->DFLCTL = 1U;

        /* Would normally have to wait tDSTOP(6us) for data flash recovery. Placing the enable here, before clock and
         * C runtime initialization, should negate the need for a delay since the initialization will typically take more than 6us. */
#endif
    }

    if (BSP_WARM_START_POST_C == event)
    {
        /* C runtime environment and system clocks are setup. */

        /* Configure pins. */
        R_IOPORT_Open (&g_ioport_ctrl, g_ioport.p_cfg);
    }
}

#if BSP_TZ_SECURE_BUILD

BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ();

/* Trustzone Secure Projects require at least one nonsecure callable function in order to build (Remove this if it is not required to build). */
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ()
{

}
#endif

/*******************************************************************************************************************//**
 * @brief       This function processes dynamic address assignment procedure, If a Hot-Join event is received.
 * @param[IN]   None
 * @retval      FSP_SUCCESS or Any Other Error code apart from FSP_SUCCESS upon unsuccessful hot_join_request_process.
 **********************************************************************************************************************/
static fsp_err_t hot_join_request_process(void)
{
    fsp_err_t err = FSP_SUCCESS;
    uint32_t status = RESET_VALUE;

    APP_PRINT ("\r\nINFO : A hot Join event is received, Initiate DAA using CCC transmission.\r\n");

    /* If a Hot-Join event is received, then the master can initiate the dynamic address assignment procedure. */
    err = R_I3C_DynamicAddressAssignmentStart(&g_i3c0_ctrl, I3C_ADDRESS_ASSIGNMENT_MODE_ENTDAA, RESET_VALUE, ONE);
    if(FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\nERROR : R_I3C_DynamicAddressAssignmentStart API FAILED.\r\n");
        return err;
    }

    /* wait for address assignment complete event. */
    status = i3c_app_event_notify(I3C_EVENT_FLAG_ADDRESS_ASSIGNMENT_COMPLETE, WAIT_TIME);
    if(RESET_VALUE == status)
    {
        APP_ERR_PRINT ("\r\nERROR : Requested event not received with in specified timeout.\r\n");
        err = FSP_ERR_TIMEOUT;
    }
    /* Reset hot joint event flag.*/
    b_target_hj_received = false;
    /* Update number of device as ONE */
    g_num_device_on_bus = ONE;
    APP_PRINT("\r\nINFO : DAA using CCC transmission completed, \r\nplease check by pressing user input 1 (available at menu option) for slave information\r\n");
    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief       This function send a broadcast or direct command to slave devices on the bus.
 * @param[IN]   None
 * @retval      FSP_SUCCESS or Any Other Error code apart from FSP_SUCCESS upon unsuccessful i3c_broadcast_ccc_send.
 **********************************************************************************************************************/
static fsp_err_t i3c_broadcast_ccc_send(void)
{
    fsp_err_t       err = FSP_SUCCESS;
    uint32_t        status = RESET_VALUE;

    /* Send the command RSTDAA */
    command_descriptor.command_code = I3C_CCC_BROADCAST_RSTDAA;
    /* Set a buffer for storing the data read by the command. */
    command_descriptor.p_buffer = NULL;
    /* The length for a GETMWL command is 2 bytes. */
    command_descriptor.length = RESET_VALUE;
    /* Terminate the transfer with a Repeated Start condition. */
    command_descriptor.restart = false;
    /* The GETMWL command is a Direct Get Command. */
    command_descriptor.rnw = false;

    /* Send a broadcast or direct command to slave devices on the bus.*/
    err = R_I3C_CommandSend(&g_i3c0_ctrl, &command_descriptor);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\nERROR : R_I3C_CommandSend API FAILED.\r\n");
        return err;
    }

    /* wait for command complete.*/
    status = i3c_app_event_notify(I3C_EVENT_FLAG_COMMAND_COMPLETE, WAIT_TIME);
    if(RESET_VALUE == status)
    {
        APP_ERR_PRINT ("\r\nERROR : Requested event not received with in specified timeout.\r\n");
        err = FSP_ERR_TIMEOUT;
    }

    APP_PRINT ("\r\nINFO : CCC Dynamic Address Assignment transfer completed successfully.\r\n");
    g_num_device_on_bus = RESET_VALUE;

    return err;
}

/*******************************************************************************************************************//**
 * @brief This function is callback for i3c.
 *
 * @param[IN] p_args
 **********************************************************************************************************************/
void g_i3c0_callback(i3c_callback_args_t const *const p_args)
{
    /* update the event in global array and this will be used in i3c_app_event_notify function.*/
    g_i3c_event_status = p_args->event_status;
    g_i3c_event_count[p_args->event]++;

    switch(p_args->event)
    {
        case I3C_EVENT_ENTDAA_ADDRESS_PHASE:
        {
            /* The device PID, DCR, and BCR registers will be available in i3c_callback_args_t::p_slave_info. */
            g_device_slave_info.dynamic_address = (uint8_t)(I3C_SLAVE_DEVICE_DYNAMIC_ADDRESS_START);
            memcpy(g_device_slave_info.slave_info.pid, p_args->p_slave_info->pid, sizeof(p_args->p_slave_info->pid));
            g_device_slave_info.slave_info.bcr = p_args->p_slave_info->bcr;
            g_device_slave_info.slave_info.dcr = p_args->p_slave_info->dcr;
            break;
        }

        case I3C_EVENT_ADDRESS_ASSIGNMENT_COMPLETE:
        {
            /* set the bus initialized flag.*/
            b_bus_initialized = true;
            break;
        }

        case I3C_EVENT_READ_COMPLETE:
        {
           /* The number of bytes returns from the slave will be available in i3c_callback_args_t::transfer_size. */
            g_data_transfer_size = p_args->transfer_size;
            set_next_read_buffer();
            break;
        }

        case I3C_EVENT_WRITE_COMPLETE:
        {
            /* The number of bytes writes to the slave will be available in i3c_callback_args_t::transfer_size. */
            g_data_transfer_size = p_args->transfer_size;
            break;
        }

        case I3C_EVENT_COMMAND_COMPLETE:
        {
            /* The command code and transfer size will be available in p_args.
             * If the command code is a Broadcast or Direct Set, then data will
             * be stored in the read buffer provided by i3c_api_t::read.
             * If the command code is a Direct Get, then the data will be automatically
             * sent from device SFR. */
            g_data_transfer_size = p_args->transfer_size;
            break;
        }

        case I3C_EVENT_IBI_READ_COMPLETE:
        {
            /* When an IBI is completed, the transfer_size, ibi_type, and ibi_address will be available in p_args. */
            switch (p_args->ibi_type)
            {
                case I3C_IBI_TYPE_INTERRUPT:
                {
                    /* Notify the application that an IBI was read. */
                    b_target_ibi_transfer_received = true;
                    g_data_transfer_size = p_args->transfer_size;
                    g_ibi_target_address = p_args->ibi_address;
                    break;
                }
                case I3C_IBI_TYPE_HOT_JOIN:
                {
                    b_target_hj_received = true;
                    break;
                }
                default:
                {
                    break;
                }
            }
            break;
        }

        default:
        {
           break;
        }
    }
}

/*******************************************************************************************************************//**
 * @brief This function starts the timer and wait for the event set in the i3c callback till specified timeout.
 * @param[IN]   set_event_flag_value  requested event flag
 * @param[IN]   timeout               specified timeout
 * @retval      on successful operation, returns i3c event flag value.
 **********************************************************************************************************************/
static uint32_t i3c_app_event_notify(uint32_t set_event_flag_value, uint32_t timeout)
{
    fsp_err_t       err = FSP_SUCCESS;
    uint32_t        get_event_flag_value = RESET_VALUE;
    /* Reset the timeout flag. */
    b_process_timeout = false;

    /* start the timer.*/
    err = start_timeout_timer_with_defined_ms(timeout);
    if(FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\nERROR : start_timeout_timer_with_defined_ms function failed.\r\n");
        /* de-initialize the opened I3C and AGT timer module.*/
        i3c_deinit();
        agt_deinit();
        APP_ERR_TRAP(err);
    }

    /* wait for the event set in the i3c callback till specified timeout.*/
    while (!b_process_timeout)
    {
        /* process for all i3c events.*/
        for(uint8_t cnt = RESET_VALUE; cnt < (I3C_EVENT_INTERNAL_ERROR+ONE); cnt++)
        {
            /* check for callback event.*/
            if(g_i3c_event_count[cnt] > RESET_VALUE)
            {
                /* store the event in local variable.*/
                get_event_flag_value |= (uint32_t)(0x1 << cnt);
                g_i3c_event_count[cnt] -= ONE;
            }
        }

        /* check for event received from i3c callback function is similar to event which user wants.*/
        get_event_flag_value = (set_event_flag_value & get_event_flag_value);
        if(get_event_flag_value)
        {
            g_i3c_event_status = RESET_VALUE;
            return get_event_flag_value;
        }
    }
    return 0;
}

/*******************************************************************************************************************//**
 * @brief This function sets the next read buffer.
 * @param[IN]   None
 * @retval      None
 **********************************************************************************************************************/
static void set_next_read_buffer(void)
{
    p_last = p_next;
    p_next = ((p_next == g_read_data[RESET_VALUE]) ? g_read_data[ONE] : g_read_data[RESET_VALUE]);
}

/*******************************************************************************************************************//**
 * @brief       This function checks the slave device is present on bus and display slave device information.
 * @param[IN]   None
 * @retval      FSP_SUCCESS or Any Other Error code apart from FSP_SUCCESS upon unsuccessful check_disp_i3c_slaveInfo.
 **********************************************************************************************************************/
static fsp_err_t  check_disp_i3c_slaveInfo(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* check for slave device existence on the bus. */
    if ((RESET_VALUE == g_num_device_on_bus) && (RESET_VALUE == g_device_slave_info.dynamic_address))
    {
        APP_ERR_PRINT("\r\nERROR : No Slave device exists on I3C bus,\r\n"
                "Sending broadcast common command code to check for slave Hot Join requests\r\n"
                "Please re-check again\r\n");

        /* sending broadcast ccc.*/
        err = i3c_broadcast_ccc_send();
        if (FSP_SUCCESS != err)
        {
            APP_ERR_PRINT("\r\nERROR : No Slave device exists on I3C bus, please confirm on physical connection on bus.\r\n");
            return err;
        }
        APP_PRINT("\r\nINFO : Please re-check with menu option 1 to view dynamic address assigned to slave before using menu option 2\r\n");
    }
    else
    {
        APP_PRINT ("\r\nINFO : number of I3C device on bus: %d **\r\n\n", g_num_device_on_bus);

        APP_PRINT("***********************************************\r\n")
        APP_PRINT("*                 I3C Slave Info              *\r\n")
        APP_PRINT("***********************************************\r\n")
        APP_PRINT("- Dynamic Address:   0x%02x\r\n", g_device_slave_info.dynamic_address)
        APP_PRINT("- BCR:               0x%02x\r\n", g_device_slave_info.slave_info.bcr)
        APP_PRINT("- DCR:               0x%02x\r\n", g_device_slave_info.slave_info.dcr)
        APP_PRINT("- PID:               0x%02x%02x%02x%02x%02x%02x\r\n",
                  g_device_slave_info.slave_info.pid[0],
                  g_device_slave_info.slave_info.pid[1],
                  g_device_slave_info.slave_info.pid[2],
                  g_device_slave_info.slave_info.pid[3],
                  g_device_slave_info.slave_info.pid[4],
                  g_device_slave_info.slave_info.pid[5])
    }
    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief       This function performs the I3C master write and read operation.
 * @param[IN]   None
 * @retval      FSP_SUCCESS or Any Other Error code apart from FSP_SUCCESS upon unsuccessful master_write_read_verify.
 **********************************************************************************************************************/
static fsp_err_t master_write_read_verify(void)
{
    fsp_err_t err = FSP_SUCCESS;
    uint32_t status = RESET_VALUE;

    /* update buffer */
   for (uint32_t cnt = RESET_VALUE; cnt < sizeof(g_write_data); cnt++)
   {
       g_write_data[cnt] = (uint8_t) (cnt + g_write_read_routine_count) & UINT8_MAX;
   }
   g_write_read_routine_count++;

   /* perform write operation.*/
   err = R_I3C_Write(&g_i3c0_ctrl, g_write_data, sizeof(g_write_data), false);
   if (FSP_SUCCESS != err)
   {
       APP_ERR_PRINT("\r\nERROR : R_I3C_Write API FAILED.\r\n");
       return err;
   }

   /* wait for write complete event.*/
   status = i3c_app_event_notify(I3C_EVENT_FLAG_WRITE_COMPLETE, WAIT_TIME);
   if(RESET_VALUE == status)
   {
       APP_ERR_PRINT ("\r\nERROR : Requested event not received with in specified timeout.\r\n");
       return FSP_ERR_TIMEOUT;
   }
   R_BSP_SoftwareDelay(500, BSP_DELAY_UNITS_MICROSECONDS);

   /* Start a read operation. */
   err = R_I3C_Read(&g_i3c0_ctrl, p_next, MAX_READ_DATA_LEN, false);
   if (FSP_SUCCESS != err)
   {
       APP_ERR_PRINT("\r\nERROR : R_I3C_Read API FAILED.\r\n");
       return err;
   }

   /* wait for read complete event.*/
   status = i3c_app_event_notify(I3C_EVENT_FLAG_READ_COMPLETE, WAIT_TIME);
   if(RESET_VALUE == status)
   {
       APP_ERR_PRINT ("\r\nERROR : Requested event not received with in specified timeout.\r\n");
       return FSP_ERR_TIMEOUT;
   }

   /* compare read data with written data on slave.*/
   if (RESET_VALUE == memcmp(g_write_data, p_last, sizeof(g_write_data)))
   {
       APP_PRINT("\r\nINFO : Data written to I3C slave is read back and matching - SUCCESS\r\n");
       APP_PRINT("INFO: Data Transfer size 0x%x\r\n",g_data_transfer_size);
   }
   else
   {
       APP_ERR_PRINT("\r\nERROR : Data mismatch - FAILURE\r\n");
       return FSP_ERR_INVALID_DATA;
   }
   return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief This function closes opened I3C module before the project ends up in an Error Trap.
 * @param[IN]   None
 * @retval      None
 **********************************************************************************************************************/
void i3c_deinit(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Close I3C module */
    err = R_I3C_Close(&g_i3c0_ctrl);
    /* handle error */
    if (FSP_SUCCESS != err)
    {
        /* I3C Close failure message */
        APP_ERR_PRINT("\r\nERROR : R_I3C_Close API FAILED.\r\n");
    }
}

/* timer related functions */
static uint32_t timeout_value_in_ms = RESET_VALUE;

/*******************************************************************************************************************//**
 * @brief This function is callback for periodic mode timer and stops AGT0 timer in Periodic mode.
 *
 * @param[in] (timer_callback_args_t *) p_args
 **********************************************************************************************************************/
void g_timeout_timer_callback(timer_callback_args_t *p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);

    /* check if specified timeout is zero.*/
    if(RESET_VALUE == --timeout_value_in_ms)
    {
        /* set the timeout flag.*/
        b_process_timeout = true;
        /* stop AGT timer.*/
        R_AGT_Stop(&g_timeout_timer_ctrl);
    }
}

/*******************************************************************************************************************//**************
 * @brief       This function Resets the counter value and start the AGT timer.
 * @param[IN]   timeout_ms
 * @retval      FSP_SUCCESS or Any Other Error code apart from FSP_SUCCESS upon unsuccessful start_timeout_timer_with_defined_ms.
 ***********************************************************************************************************************************/
static fsp_err_t start_timeout_timer_with_defined_ms(uint32_t timeout_ms)
{
    fsp_err_t err = FSP_SUCCESS;

    /* update the specified timeout into a global variable and this will be checked in timer callback.*/
    timeout_value_in_ms = timeout_ms;
    /* Resets the counter value.*/
    err = R_AGT_Reset(&g_timeout_timer_ctrl);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT ("\r\nERROR : R_AGT_Reset API FAILED \r\n");
        return err;
    }

    /* start the AGT timer.*/
    err = R_AGT_Start(&g_timeout_timer_ctrl);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT ("\r\nERROR : R_AGT_Start API FAILED \r\n");
    }
    return err;
}

/*******************************************************************************************************************//**
 * @brief This function closes opened AGT module before the project ends up in an Error Trap.
 * @param[IN]   None
 * @retval      None
 **********************************************************************************************************************/
void agt_deinit(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Close AGT0 module */
    err = R_AGT_Close(&g_timeout_timer_ctrl);
    /* handle error */
    if (FSP_SUCCESS != err)
    {
        /* AGT0 Close failure message */
        APP_ERR_PRINT("\r\nERROR : R_AGT_Close API FAILED.\r\n");
    }
}

/*******************************************************************************************************************//**
 * @} (end addtogroup i3c_master_ep)
 **********************************************************************************************************************/
