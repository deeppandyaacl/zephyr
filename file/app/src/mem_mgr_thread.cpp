/**
* SIBEL INC ("SIBEL HEALTH") CONFIDENTIAL
*
* Copyright 2018-2024 [Sibel Inc.], All Rights Reserved.
*
* NOTICE: All information contained herein is, and remains the property of SIBEL
* INC. The intellectual and technical concepts contained herein are proprietary
* to SIBEL INC and may be covered by U.S. and Foreign Patents, patents in
* process, and are protected by trade secret or copyright law. Dissemination of
* this information or reproduction of this material is strictly forbidden unless
* prior written permission is obtained from SIBEL INC. Access to the source code
* contained herein is hereby forbidden to anyone except current SIBEL INC
* employees, managers or contractors who have executed Confidentiality and
* Non-disclosure agreements explicitly covering such access.
* The copyright notice above does not evidence any actual or intended
* publication or disclosure of this source code, which includes information that
* is confidential and/or proprietary, and is a trade secret of SIBEL INC.
*
* ANY REPRODUCTION, MODIFICATION, DISTRIBUTION, PUBLIC PERFORMANCE, OR PUBLIC
* DISPLAY OF OR THROUGH USE OF THIS SOURCE CODE WITHOUT THE EXPRESS WRITTEN
* CONSENT OF COMPANY IS STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE
* LAWS AND INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
* CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS TO
* REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE, USE, OR
* SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
*
*/

#include <zephyr/kernel.h>
#include <sh/sh_data_logger.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include "mem_mgr_thread.h"
#include "temp_sensor_thread.h"
#include "barometer_thread.h"
#include "imu_sensor_thread.h"
#include "dl_service/ble_dl.h"
#include "imu_sensor_thread.h"
#include "microphone_thread.h"
#include "session_thread.h"
#include "ptx30_thread.h"
#include "math.h"
#include <sh/sh_common.h>
#include "fault_handler.h"
#include "memfault/core/trace_event.h"
#ifdef CONFIG_SEGNO_LIBRARY
#include "segno_thread.h"
#endif
#include "haptic_motor_handler.h"
#ifdef CONFIG_WATCHDOG
#include "watchdog.h"
#endif

#if defined(CONFIG_SHRD_HEADER)
#include <sh/sh_shrd_data.h>
#endif

#include "factory_config.h"
#include "shrd_config.h"
#include "mic_config.h"

#include <sh/sh_littlefs.h>
#include <sh/sh_time.h>
//Page size of different sensor's data
#define FS_SIZE         (4 * 1024)

//Memory manager stack size
#define MEM_MGR_STACK   (8192)

// Define stack size for the work queue thread
#define WORKQUEUE_STACK_SIZE (10240)

// Define priority for the work queue thread
#define WORKQUEUE_PRIORITY (5)

//Retry count for subscribe/unsubscribe method
#define MAX_RETRY_CNT      (3)

/* Registering Log Module */
LOG_MODULE_REGISTER(mem_mgr_thread, LOG_LEVEL_DBG);

/* Subscribe zbus channel to store temperature data*/
ZBUS_MSG_SUBSCRIBER_DEFINE(mem_temp_sub);

/* Subscribe zbus channel to store barometer data*/
ZBUS_MSG_SUBSCRIBER_DEFINE(mem_barometer_sub);

/* Subscribe zbus channel to store imu data*/
ZBUS_MSG_SUBSCRIBER_DEFINE(mem_imu_data_sub);

/* Subscribe zbus channel to store imu step data*/
ZBUS_MSG_SUBSCRIBER_DEFINE(mem_imu_step_data_sub);

#if ((CONFIG_SEGNO_SCRATCH_PROGRAM) || (CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM) || \
            (CONFIG_SEGNO_WEAR_DETECT_PROGRAM) || (CONFIG_SEGNO_ANGLE_DETECT_PROGRAM))
/* Subscribe zbus channel to store segno data*/
ZBUS_MSG_SUBSCRIBER_DEFINE(mem_segno_sub);
#endif

/* Subscribe zbus channel to store event data*/
ZBUS_MSG_SUBSCRIBER_DEFINE(mem_event_sub);

#if defined(CONFIG_MICROPHONE)
/* Subscribe zbus channel to micro-phone data*/
ZBUS_MSG_SUBSCRIBER_DEFINE(mem_mic_data_sub);
#endif

/* BLE command subscribe channel */
ZBUS_MSG_SUBSCRIBER_DEFINE(mem_mngr_dl_sub);

/* Define ZBUS channel for sending session data to memory manager */
ZBUS_CHAN_DEFINE(mem_mngr_dl_chan, session_msg_t, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(0));

K_THREAD_STACK_DEFINE(mem_mngr_stack, MEM_MGR_STACK);

/* Kernel semaphore definition for memory manager data */
K_SEM_DEFINE(mem_mngr_sem, 0, 10);

// Timer handler function
void update_session_timer_handler(struct k_timer *arg);

// Define a session timer
K_TIMER_DEFINE(update_session_timer, update_session_timer_handler, NULL);

// Define work queue stack memory
K_THREAD_STACK_DEFINE(workqueue_stack_area, WORKQUEUE_STACK_SIZE);

/* Define ZBUS channel for sending event data */
ZBUS_CHAN_DEFINE(evt_chan, sh_mem_mngr::msg_t, NULL, NULL, ZBUS_OBSERVERS_EMPTY, \
    ZBUS_MSG_INIT(0));

static mem_mgr_parameters state = { 
            .mem_mgr_module_state = MEM_MGR_IDLE_STATE,
            .mem_mgr_is_all_channel_active = false,
            .mem_mgr_is_session_complete = false,
            .mem_mgr_resend = false,
            .mem_mgr_retry_cnt = 0,
            .mem_mgr_page_wise_local_crc32 = 0,
            .mem_mgr_req_cancel_download = 0,
            .mem_mgr_work_queue_state = 0
};

#ifdef CONFIG_WATCHDOG
/* Watchdog Channel ID for Memory manager thread */
int8_t wdt_mem_mgr_channel_id = WATCHDOG_CHANNEL_DEFAULT_ID;
#endif

#if defined(CONFIG_CODA_HEADER)
uint8_t temp_buffer[FS_SIZE];
uint8_t imu_buffer[FS_SIZE];
uint8_t barometer_buffer[FS_SIZE];

uint8_t rr_data_buffer[FS_SIZE];
uint8_t event_data_buffer[FS_SIZE];
uint8_t process_data_buffer[FS_SIZE];
uint8_t mic_data_buffer[FS_SIZE];
#endif

static shrd_config_t g_shrd_config_data;

#ifdef CONFIG_WATCHDOG
/** 
 * @brief Start Stop watchdog for Memory manager threads
 *
 * This function starts the watchdog for thread when sensor has been started to 
 * monitor its health. Once the session has been stopped, watchdog of the threads
 * will be stopped.
 * 
 * @param start_watchdog: Start/Stop watchdog flag
 */
static void start_stop_wdt(bool start_watchdog) 
{
    LOG_INF("memory manager Receiving start watchdog flag : %d and channel id is : %d", start_watchdog, wdt_mem_mgr_channel_id);

    if(start_watchdog) {
        // Starting watchdog for all sensor threads 
        if(wdt_mem_mgr_channel_id < 0) {
            wdt_mem_mgr_channel_id = task_wdt_add_thread(WATCHDOG_DEFAULT_TIMEOUT_MSEC);
            LOG_INF("Memory manager watchdog enabled with channel id :%d", wdt_mem_mgr_channel_id);
        } else {
            LOG_INF("Memory manager watchdog already enabled");
        }
    } else {
        // Stop watchdog to monitor all sensor threads 
        if(wdt_mem_mgr_channel_id >= 0) {
            LOG_INF("Memory manager watchdog disabled with channel id :%d", wdt_mem_mgr_channel_id);
            task_wdt_delete_thread(wdt_mem_mgr_channel_id);
            wdt_mem_mgr_channel_id = WATCHDOG_CHANNEL_DEFAULT_ID;
        } else {
            LOG_INF("Memory manager watchdog already disabled ");
        }
    }
}
#endif

#if defined(CONFIG_SHRD_HEADER)
static mem_mgr_sensor_page_index sensor_page_index;
static mem_mgr_sensor_page sensor_data;
static mem_mgr_event_queue evt_queue;

/**
 * @brief Create a SHRD header for given sensor
 *  
 * @param input_data : Sensor input data
 * @param data_type: Type of data
 * @param buf_index: Individual buffer storage index
 * @param data_size: Size of individual data
 * @param nand_page: Individual page instance for sensor
 * @param time_stamp: Timestamp of sensor
 * @param flag: State of Discovery RA state
 * 
 * @return SH_DL_SUCCESS : No errors
 * @return SH_DL_INVALID_STATE: Session state error
 * @return SH_DL_NAND_FLASH_FAILED: NAND flash oprtion fail
 */
static ret_code_t mem_mgr_storage_handler(uint8_t *input_data, uint8_t data_type, uint16_t *buf_index, 
                                uint16_t data_size, sh_shrd_data_t *nand_page, uint32_t time_stamp, uint8_t flag);

/**
 * @brief Initialize the SHRD header
 *  
 * @return None
 */
static void mem_mgr_shrd_init(void);

/**
 * @brief Send information data to BLE
 *  
 * @param session_info : Session information
 * 
 * @return SH_DL_NULL : Null pointer error
 */
static uint32_t mem_mgr_send_info_data_to_ble(sh_dl_session_info *session_info);

/**
 * @brief Store SHRD device information for start/stop session
 *  
 * @param NONE
 * 
 * @return NONE
 */
static void mem_mgr_set_device_info(void);

/**
 * @brief Subscribe all sensor channels
 *  
 * @param NONE
 * 
 * @return ZBus error if any
 */
static ret_code_t mem_mgr_subscribe_all_sensor_channel(void);

/**
 * @brief Unsubscribe all sensor channels
 *  
 * @param NONE
 * 
 * @return ZBus error if any
 */
static ret_code_t mem_mgr_unsubscribe_all_sensor_channel(void);

/**
 * @brief Save the remaing pages for stop session request
 *  
 * @param NONE
 * 
 * @return None
 */
static int save_remaing_pages(void);
#endif

/**
 * @brief Write sensor last page data
 *  
 * @param page_type : Type of page
 * @param page_data_type: Page data type
 * @param page_data_len: Page data length
 * @param page_flag: Page flag
 * 
 * @return Error code
 */
static int sesnor_last_page_data_write(sh_shrd_data_t *page_type, uint8_t page_data_type, 
    uint16_t page_data_len, uint8_t page_flag);

/**
 * @brief Memory manager thread function
 *
 * This function reads the messages received from various zbus
 * channels and store the data in the file system.
 */
void mem_mgr_thread_func(void *p1, void *p2, void *p3)
{
    const struct zbus_channel *chan;
    int ret = 0;
#if defined(CONFIG_MICROPHONE)
    sh_microphone::msg_t mic_data = {0};
#endif

#if defined(CONFIG_BMA456)
    sh_imu_sensor::msg_t imu_data = {0};
#endif

#if defined(CONFIG_SH_BMI323)
    sh_imu_sensor::msg_t imu_data = {0};
#endif

#if defined(CONFIG_SH_BMP5)
    sh_barometer_thread::msg_t barometer_data = {0};
#endif

#if defined(CONFIG_SH_BMI323_STEP_COUNT_ENABLE)
    sh_imu_sensor::step_count_msg_t imu_step_data = {0};
#endif /* CONFIG_SH_BMI323_STEP_COUNT_ENABLE */

    sh_temp_sensor::msg_t temp_data = {0};
    sh_session_thread::msg_t cmd_msg = {0};
    #if (CONFIG_SEGNO_SCRATCH_PROGRAM || CONFIG_SEGNO_WEAR_DETECT_PROGRAM \
        || CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM || CONFIG_SEGNO_ANGLE_DETECT_PROGRAM)
    sh_segno_thread::msg_t segno_event_data = {0};
    sh_segno_thread::msg_t event_data = {0};
    #endif

    sh_dl_session_info session_info;
    memset(&session_info, 0, sizeof(sh_dl_session_info));
    uint16_t session_num = 0;
    uint8_t session_index = 0;

    while (1)
    {

#ifdef CONFIG_WATCHDOG
    if(wdt_mem_mgr_channel_id >= 0) {
        task_wdt_fed_from_thread(wdt_mem_mgr_channel_id);
    }
#endif 

        ret = k_sem_take(&mem_mngr_sem, K_FOREVER);

        if (!ret) 
        {
            /* Handle the coda and storage part in future*/
            if(!zbus_sub_wait_msg(&mem_mngr_dl_sub, &chan, &cmd_msg, K_NO_WAIT))
            {
                LOG_INF(" CMD %d and data size %d", cmd_msg.cmd, cmd_msg.len);
                switch (cmd_msg.cmd)
                {
                #if defined(CONFIG_DATA_LOGGER)
                case SESSION_START:
                {
                    LOG_INF("Start Session command");
                    //TODO: Implement the session timer if required
                    static uint32_t session_interval_s = 0;
                    static uint32_t session_duration_s = 0;
                    static uint16_t session_num   = 0;
                    static uint32_t session_delay_start_s = 0;
                    uint32_t session_start_time = 0;
                    uint8_t session_dev_type = 0;
                    uint32_t session_cmd_time = 0;
                    uint8_t start_code = 0;

                    #ifdef CONFIG_WATCHDOG
                        // Turn on watchdog channel 
                        start_stop_wdt(true);
                    #endif

                    LOG_INF("Start Session command");

                    // TODO: Add other parameters of headers for start BLE command
                    static char session_set_id[SH_SESSION_SET_ID_TOTAL_LENGTH] = {};
                    memset(session_set_id,SH_SESSION_INVALID_ID,SH_SESSION_SET_ID_TOTAL_LENGTH);

                    ret_code_t rc = sh_data_logger::get_session_mode((session_mode_type_t *)&(session_info.session_mode));
                    if(rc != SH_DL_SUCCESS)
                    {
                        LOG_ERR("Not able to get current session mode");
                    }

                    if(session_info.session_mode == SH_SESSION_MODE_BLE_MANUAL)
                    {
                        /* code */
                        session_start_time = uint32_decode(&(cmd_msg.data[1])); /* Get start epoch time(4 Byte) */
                        session_dev_type = cmd_msg.data[5];
                        memcpy(session_set_id, &cmd_msg.data[6], SH_SESSION_SET_ID_LENGTH_1);
                        session_cmd_time = uint32_decode(&(cmd_msg.data[11]));

                        //As of now not implemented below features
                        session_interval_s     = uint32_decode(&(cmd_msg.data[15]));
                        session_num            = uint16_decode(&(cmd_msg.data[19]));
                        session_delay_start_s  = uint32_decode(&(cmd_msg.data[21]));
                        session_duration_s     = uint32_decode(&(cmd_msg.data[25]));
                        start_code             = SH_DL_SESSION_SET_START;

                        memcpy(session_set_id + sizeof(uint8_t) * SH_SESSION_SET_ID_LENGTH_1, &(cmd_msg.data[29]), SH_SESSION_SET_ID_LENGTH_2);
                        
                        if(((session_num > 1) && (session_interval_s < 1) && (session_duration_s < 1)) || (session_num == 0) )
                        {
                            LOG_ERR("The information cannot be configured to start session");
                            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                            break;
                        }
                        session_start_time = session_cmd_time;
                    }
                    else
                    {
                        //Automatic mode
                        int64_t timestamp = 0;
                        #if defined(CONFIG_SH_TIME_MODULE)
                        app_time::get_current_epoch_time(&timestamp);
                        #endif
                        session_cmd_time = 0;
                        session_dev_type = 0;
                        memset(session_set_id,SH_SESSION_INVALID_ID,SH_SESSION_SET_ID_TOTAL_LENGTH);
                        start_code       = SH_DL_AUTO_START;
                        session_start_time = timestamp;
                    }

                    // Start the session
                    LOG_INF("START SESSION SET Start Time: %d cmd Time: %d Device Type: %d",
                            session_start_time, session_cmd_time, session_dev_type);
                    
                    LOG_INF("interval:%d number: %d delay start: %d duration: %d", session_interval_s, session_num,
                                session_delay_start_s, session_duration_s);

                    /* Stop previous session activity */
                    rc = sh_data_logger::session_stop();
                    if((rc != SH_DL_SUCCESS) && (rc != SH_DL_INVALID_STATE))
                    {
                        LOG_ERR("Not able to stop the session");
                        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                        break;
                    }
                    else
                    {
                        //Set mobile app status as stop to control active session status
                        uint8_t status = 0;
                        sh_data_logger::get_session_app_status(&status);
                        if((status == SH_SESSION_SET_STARTED_ACTIVE_SESSION) || 
                        (status == SH_SESSION_SET_STARTED_NO_SESSION))
                        {
                            sh_data_logger::update_session_status(SH_SESSION_SET_STOPPED);
                        }
                    }

                    #if defined(CONFIG_SH_TIME_MODULE)
                    app_time::init();
                    #endif

                    rc = sh_data_logger::session_start(session_start_time, session_cmd_time, session_dev_type,
                         (uint8_t *)session_set_id);
                    if (rc != SH_DL_SUCCESS)
                    {
                        LOG_ERR("Session start failure");
                        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                        if(rc == SH_DL_MAX_SESSION)
                        {
                            sh_data_logger::update_session_status(SH_SESSION_SET_MAX_SESSION);
                        }
                        else if(rc == SH_DL_NO_MEMORY)
                        {
                            sh_data_logger::update_session_status(SH_SESSION_SET_NO_MEMORY);
                        }
                        break;
                    }
                    else
                    {
                        // Now collecting the data into the session
                        #if defined(CONFIG_SHRD_HEADER)
                        //Now store information block to SHRD data format
                        mem_mgr_set_device_info();
                        rc = sh_shrd_header::save_info_block((sh_shrd_session_start_code_t)start_code ,SH_DL_STOP_DUMMY);
                        if(rc != SH_DL_SUCCESS)
                        {
                            LOG_INF("Couldn't save information block in shrd file");
                        }
                        #endif
                        state.mem_mgr_module_state = MEM_MGR_SESSION_STATE;
                    }

                    sh_data_logger::update_session_status(SH_SESSION_SET_STARTED_ACTIVE_SESSION);
                   
                    //Get the session information
                    memset(&session_info, 0, sizeof(session_info));
                    rc = sh_data_logger::get_session_info_status(&session_info);
                    if(rc != SH_DL_SUCCESS)
                    {
                        LOG_ERR("Couldn't get session information");
                        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                        break;
                    }

                    //Send the session information via BLE
                    rc = mem_mgr_send_info_data_to_ble(&session_info);
                    if(rc != SH_DL_SUCCESS)
                    {
                        LOG_ERR("Not able to send data to BLE");
                        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                        break;
                    }

                    if(!(state.mem_mgr_is_all_channel_active))
                    {
                        rc = mem_mgr_subscribe_all_sensor_channel();
                        if(rc != SH_DL_SUCCESS)
                        {
                            mem_mgr_unsubscribe_all_sensor_channel();
                            LOG_ERR("Not able to subscribe channels");
                            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                            break;
                        }
                        state.mem_mgr_is_all_channel_active = true;
                    }

                    //Start the active session timer
                    k_timer_start(&update_session_timer, K_NO_WAIT,  \
                        K_MSEC(UPDATE_SESSION_INFO_TIMER_IN_MS));

                    session_index = 0;
                }
                break;

                case SESSION_STOP:
                {
                    ret_code_t rc = 0;
                    uint8_t stop_code = 0;

                    #ifdef CONFIG_WATCHDOG
                        // Turn off watchdog channel 
                        start_stop_wdt(false);
                    #endif

                    // Now all data before closing the session
                    LOG_INF("Stop session command");

                    
                    #if defined(CONFIG_CODA_HEADER)
                    // Now store all vital data from raw buffer to flash
                    do
                    {
                        rc = sh_mem_mngr::store_data(SH_CODA_HEADER_TEMP, temp_buffer);
                        if (rc != SH_DL_SUCCESS)
                        {
                            break;
                        }

                        rc = sh_mem_mngr::store_data(SH_CODA_HEADER_IMU, imu_buffer);
                        if (rc != SH_DL_SUCCESS)
                        {
                            break;
                        }

                        rc = sh_mem_mngr::store_data(SH_CODA_HEADER_BEROMETER, barometer_buffer);
                        if (rc != SH_DL_SUCCESS)
                        {
                            break;
                        }

                        rc = sh_mem_mngr::store_data(SH_CODA_HEADER_EVT, event_data_buffer);
                        if (rc != SH_DL_SUCCESS)
                        {
                            break;
                        }

                        rc = sh_mem_mngr::store_data(SH_CODA_HEADER_PROCESS_DATA, process_data_buffer);
                        if (rc != SH_DL_SUCCESS)
                        {
                            break;
                        }

                        rc = sh_mem_mngr::store_data(SH_CODA_HEADER_RR_DATA, rr_data_buffer);
                        if (rc != SH_DL_SUCCESS)
                        {
                            break;
                        }

                    } while (0);

                    if (rc != SH_DL_SUCCESS)
                    {
                        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                        break;
                    }
                    #endif

                    #if defined(CONFIG_SHRD_HEADER)
                        sh_dl_state_t status = SH_DL_STATE_UNINITIALIZED;
                        sh_data_logger::get_session_status(&status);
                        memset(&session_info, 0, sizeof(session_info));
                        rc = sh_data_logger::get_session_mode((session_mode_type_t *)&(session_info.session_mode));
                        if(rc != SH_DL_SUCCESS)
                        {
                            LOG_ERR("Not able to get current session mode");
                            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                            break;
                        }

                        if(session_info.session_mode == SH_SESSION_MODE_BLE_MANUAL)
                        {
                            stop_code = SH_DL_BLE_SESSION_STOP;
                        }
                        else
                        {
                            stop_code = SH_DL_AUTO_EXIT_ACTIVE_SESSION;
                        }

                        if( status == SH_DL_STATE_ACTIVE_SESSION)
                        {
                            //Save remaing pages in external flash and clear the buffer
                            rc = save_remaing_pages();
                            if(rc != SH_DL_SUCCESS)
                            {
                                LOG_INF("Couldn't save last information block");
                                state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                                break;
                            }

                            //If session is not active then do not store information block
                            // mem_mgr_set_device_info();
                            rc = sh_shrd_header::save_info_block(SH_DL_START_DUMMY ,(sh_shrd_session_stop_code_t)stop_code);
                            if(rc != SH_DL_SUCCESS)
                            {
                                LOG_INF("Couldn't save information block in shrd file");
                                state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                                break;
                            }

                            // Stop the session
                            rc = sh_data_logger::session_stop();
                            if (rc != SH_DL_SUCCESS)
                            {
                                state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                                fault_handler::send_diagnostics(NAND_FLASH_FAULT);
                                break;
                            }
                            else
                            {
                                //Set mobile app status as stop to control active session status
                                uint8_t status = 0;
                                session_index++;
                                sh_data_logger::get_session_app_status(&status);
                                if(session_num <= session_index)
                                {
                                    if(status == SH_SESSION_SET_STARTED_ACTIVE_SESSION)
                                    {
                                        status = SH_SESSION_SET_STOPPED;
                                    }
                                    LOG_INF("stop the session set!");
                                }
                                else
                                {
                                    status = SH_SESSION_SET_STARTED_NO_SESSION;
                                }
                                // if((status == SH_SESSION_SET_STARTED_ACTIVE_SESSION) || 
                                // (status == SH_SESSION_SET_STARTED_NO_SESSION))
                                // {
                                    sh_data_logger::update_session_status(status);
                                // }
                            }
                        }
                    #endif

                    state.mem_mgr_module_state = MEM_MGR_IDLE_STATE;

                    //Get the session information
                    memset(&session_info, 0, sizeof(session_info));
                    rc = sh_data_logger::get_session_info_status(&session_info);
                    if(rc != SH_DL_SUCCESS)
                    {
                        LOG_INF("Couldn't get session information");
                        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                        break;
                    }

                    //Send the session information via BLE
                    rc = mem_mgr_send_info_data_to_ble(&session_info);
                    if(rc != SH_DL_SUCCESS)
                    {
                        LOG_ERR("Not able to send data to BLE");
                        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                        break;
                    }

                    if(state.mem_mgr_is_all_channel_active)
                    {
                        rc = mem_mgr_unsubscribe_all_sensor_channel();
                        if(rc != SH_DL_SUCCESS)
                        {
                            LOG_ERR("Not able to unsubscribe channels");
                            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                            break;
                        }
                        state.mem_mgr_is_all_channel_active = false;
                    }

                    // Stop the timer active session timer
                    k_timer_stop(&update_session_timer);

                }
                break;

                case SESSION_DOWNLOAD:
                {
                    ret_code_t rc = 0;


                    #if defined(CONFIG_SHRD_HEADER)
                    sh_dl_state_t status = SH_DL_STATE_UNINITIALIZED;
                    sh_data_logger::get_session_status(&status);
                    if(status == SH_DL_STATE_ACTIVE_SESSION)
                    {
                        //Now store information block to SHRD data format
                        // mem_mgr_set_device_info();
                        rc = sh_shrd_header::save_info_block(SH_DL_START_DUMMY ,SH_DL_START_DOWNLOAD);
                        if(rc != SH_DL_SUCCESS)
                        {
                            LOG_INF("Couldn't save information block in shrd file");
                            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                        }
                   
                        // Stop the session
                        rc = sh_data_logger::session_stop();
                        if(rc != SH_DL_SUCCESS)
                        {
                            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                            fault_handler::send_diagnostics(NAND_FLASH_FAULT);
                            break;
                        }
                        else
                        {
                            //Set mobile app status as stop to control active session status
                            uint8_t status = 0;
                            sh_data_logger::get_session_app_status(&status);
                            if((status == SH_SESSION_SET_STARTED_ACTIVE_SESSION) || 
                            (status == SH_SESSION_SET_STARTED_NO_SESSION))
                            {
                                sh_data_logger::update_session_status(SH_SESSION_SET_STOPPED);
                            }
                            
                        }
                     }
                    #endif

                    state.mem_mgr_module_state = MEM_MGR_IDLE_STATE;

                    //Get the session information
                    memset(&session_info, 0, sizeof(session_info));
                    rc = sh_data_logger::get_session_info_status(&session_info);
                    if(rc != SH_DL_SUCCESS)
                    {
                        LOG_INF("Couldn't get session information");
                        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                        break;
                    }

                    //Send the session information via BLE
                    rc = mem_mgr_send_info_data_to_ble(&session_info);
                    
                    if(rc != SH_DL_SUCCESS)
                    {
                        LOG_ERR("Not able to send data to BLE");
                        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                        break;
                    }

                    if(state.mem_mgr_is_all_channel_active)
                    {
                        rc = mem_mgr_unsubscribe_all_sensor_channel();
                        if(rc != SH_DL_SUCCESS)
                        {
                            LOG_ERR("Not able to unsubscribe channels");
                            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                            break;
                        }
                        state.mem_mgr_is_all_channel_active = false;
                    }
                    state.mem_mgr_module_state = MEM_MGR_DOWNLOAD_STATE;

                    k_work_submit_to_queue(&(evt_queue.my_workqueue), &(evt_queue.download_work));
                }
                break;

                case SESSION_REQ_INFO:
                {
                    ret_code_t rc = 0;
                    LOG_INF("Get current session information request");
                    //Get the session information
                    memset(&session_info, 0, sizeof(session_info));
                    rc = sh_data_logger::get_session_info_status(&session_info);
                    if(rc != SH_DL_SUCCESS)
                    {
                        LOG_INF("Couldn't get session information");
                        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                        break;
                    }

                    //Send the session information via BLE
                    rc = mem_mgr_send_info_data_to_ble(&session_info);
                }
                break;

                case SESSION_DOWNLOAD_ACK:
                {
                    //Download ACK request
                    uint32_t crc32_from_host = uint32_decode(&(cmd_msg.data[1]));
                    LOG_INF("Recieved Ack from the base station local: %x host: %x", 
                        state.mem_mgr_page_wise_local_crc32, crc32_from_host);
                    if(state.mem_mgr_page_wise_local_crc32 == crc32_from_host)
                    {
                        state.mem_mgr_is_session_complete = true;
                        state.mem_mgr_resend = false;
                    }
                    else
                    {
                        LOG_INF("mem_mgr_resend the data!");
                        state.mem_mgr_resend = true;
                    }
                }
                break;

                case SESSION_DOWNLOAD_CANCEL:
                {
                    uint8_t work_queue_retry_cnt = 30;
                    if(state.mem_mgr_module_state == MEM_MGR_DOWNLOAD_STATE)
                    {
                        sh_session_thread::msg_t ble_notify_data;
                        LOG_INF("Cancel the download request from mobile");
                        state.mem_mgr_req_cancel_download = true;
                        state.mem_mgr_module_state = MEM_MGR_IDLE_STATE;
                        state.mem_mgr_retry_cnt = 0;
                        state.mem_mgr_is_session_complete = false;
                        state.mem_mgr_resend = false;
                        state.mem_mgr_page_wise_local_crc32 = sh_utils::crc32(NULL, 0, NULL);

                        while(state.mem_mgr_work_queue_state && (work_queue_retry_cnt != 0))
                        {
                            k_msleep(100);
                            work_queue_retry_cnt--;
                        }

                        if(work_queue_retry_cnt == 0)
                        {
                            LOG_ERR("Work queue execution is not completed");
                        }

                        memset(&ble_notify_data, 0, sizeof(ble_notify_data)); 
                        ble_notify_data.data[0] = SESSION_DOWNLOAD_CANCEL;
                        ble_notify_data.len = 1;
                        ble_notify_data.cmd = SESSION_DOWNLOAD;
                        ret = zbus_chan_pub(&mem_mngr_chan, &ble_notify_data, K_FOREVER);
                        if(ret != 0){
                            LOG_ERR("Failed to publish Flash data to BLE [%d]",ret);
                        }
                        k_sem_give(&dl_session_sem);
                        

                    }
                    else
                    {
                        LOG_INF("Download is not started.");
                    }
                }
                break;

                case SESSION_RESET_MEM:
                {
                    ret_code_t rc = 0;
                    rc = sh_data_logger::reset();
                    if (rc != SH_DL_SUCCESS)
                    {
                        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                        LOG_ERR("Not able to reset the session");
                    }
                }
                break;
                #endif

                case SESSION_EPOCH_TIME:
                    break;

                default:
                    LOG_ERR("Got unknown message from mobile App");
                    state.mem_mgr_module_state = MEM_MGR_IDLE_STATE;
                    break;
                }
            }
        }

        if(MEM_MGR_SESSION_STATE == state.mem_mgr_module_state)
        {
            //#if defined(CONFIG_AS6221)       
            // Check and process the incoming messages from the mem_temp_sub
            if (!zbus_sub_wait_msg(&mem_temp_sub, &chan, &temp_data, K_NO_WAIT))
            {
                // Read temperature data from the channel
            #if defined(CONFIG_CODA_HEADER)
                coda_temp_t data;
                memset((void *)&data, 0, sizeof(data));
                // Now convert the raw data in to one byte
                data.temperature = temp_data.data; // combine two bytes
                data.timestamp = temp_data.timestamp;

                // Create a seprate thread and zbus if required
                sh_coda_ret_code_t ret = sh_coda_temperature_header(&data, temp_buffer + 1, FS_SIZE);
                if (ret == SH_CODA_PAGE_BUFFER_FULL)
                {
                    // Store the data into data logger module
                    LOG_INF("Now store temperature data");
                    sh_mem_mngr::store_data(SH_CODA_HEADER_TEMP, temp_buffer);
                }
            #endif

            #if defined(CONFIG_SHRD_HEADER)
                if(g_shrd_config_data.temp) {
                    uint8_t temperature_data[2];
                    int16_t tmp = (uint16_t)((((int16_t)(temp_data.data) * (int32_t)78125) / 10) / 1000000);
                    // int16_t tmp = (uint16_t)(temp_data.data);

                    //Encode 2 bytes and store temperature into SHRD format
                    uint16_encode(tmp, temperature_data);

                    ret = mem_mgr_storage_handler(temperature_data, SH_DATA_TYPE_TEMP1_MODE_2, 
                        &(sensor_page_index.temp_buf_index), TEMP_DATA_LEN, &sensor_data.temp_nand_page, temp_data.timestamp_storage, 0);
                    if(ret == SH_DL_NO_MEMORY)
                    {
                        LOG_ERR("Memory full stop the session");
                        state.mem_mgr_module_state  = MEM_MGR_MEMORY_FULL_STATE;
                    }
                    if(ret != SH_DL_SUCCESS)
                    {
                        LOG_ERR("Not able to store temperature data");
                        fault_handler::send_diagnostics(NAND_FLASH_FAULT);
                        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                    }
                }
            #endif
            }

        #if defined(CONFIG_SH_BMP5)
            #if CONFIG_PIPES
            size_t barometer_bytes_read = 0;
            if(!k_pipe_get(&barometer_pipe, (void *)&barometer_data, sizeof(barometer_data), &barometer_bytes_read,sizeof(barometer_data), K_NO_WAIT) \
                && barometer_bytes_read >= sizeof(barometer_data))
            #else
            if (!zbus_sub_wait_msg(&mem_barometer_sub, &chan, &barometer_data, K_NO_WAIT))
            #endif
            {
                // Read barometer data from the channel
                #if defined(CONFIG_CODA_HEADER)
                    coda_berometer_t data;
                    memset((void *)&data, 0, sizeof(data));
                    // Now store berometer data
                    memcpy(data.pressure, barometer_data.data, sizeof(barometer_data.data));
                    data.timestamp = barometer_data.timestamp;
                    // Create a seprate thread and zbus if required
                    sh_coda_ret_code_t ret = sh_coda_barometer_header(&data, barometer_buffer + 1, FS_SIZE);
                    if (ret == SH_CODA_PAGE_BUFFER_FULL)
                    {
                        // Store the data into data logger module
                        LOG_INF("Now store barometer data");
                        sh_mem_mngr::store_data(SH_CODA_HEADER_BEROMETER, barometer_buffer);
                    }
                #endif

                #if defined(CONFIG_SHRD_HEADER)
                    if(g_shrd_config_data.baro) {
                        uint8_t baro_data[80];
                        memcpy(baro_data, barometer_data.data, sizeof(baro_data));
                        ret = mem_mgr_storage_handler(baro_data, SH_DATA_TYPE_BAROMTER, 
                            &(sensor_page_index.baro_buf_index), 80, &sensor_data.baro_nand_page, barometer_data.timestamp_storage, 0x01);
                        if(ret == SH_DL_NO_MEMORY)
                        {
                            LOG_ERR("Memory full stop the session");
                            state.mem_mgr_module_state  = MEM_MGR_MEMORY_FULL_STATE;
                        }
                        if(ret != SH_DL_SUCCESS)
                        {
                            LOG_ERR("Not able to store Barometer data");
                            fault_handler::send_diagnostics(NAND_FLASH_FAULT);
                            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                        }
                    }
                #endif
            }
        #endif // (CONFIG_SH_BMP5)
            
        #if defined(CONFIG_SH_BMI323)
            #if CONFIG_PIPES
            size_t imu_bytes_read = 0;
            if(!k_pipe_get(&imu_pipe, (void *)&imu_data, sizeof(imu_data), &imu_bytes_read,sizeof(imu_data), K_NO_WAIT) \
                && imu_bytes_read >= sizeof(imu_data))
            #else
            if (!zbus_sub_wait_msg(&mem_imu_data_sub, &chan, &imu_data, K_NO_WAIT))
            #endif
            {
                if(imu_data.type == 0xFF){    // IMU Fault stop the session
                    state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;     // Dummy failure to stop the session due to sensor fault
                }
                else{    
                    #if defined(CONFIG_CODA_HEADER)
                        coda_imu_t data;
                        memset((void *)&data, 0, sizeof(data));
                        // Now store berometer data
                        memcpy(data.x, imu_data.x, sizeof(imu_data.x));
                        memcpy(data.y, imu_data.y, sizeof(imu_data.y));
                        //memcpy(data.z, imu_data.z, sizeof(imu_data.z));
                        data.timestamp = imu_data.timestamp;

                        // Create a seprate thread and zbus if required
                        sh_coda_ret_code_t ret = sh_coda_imu_header(&data, imu_buffer + 1, FS_SIZE);
                        if (ret == SH_CODA_PAGE_BUFFER_FULL)
                        {
                            // Store the data into data logger module
                            LOG_INF("Now store imu data");
                            // k_msleep(1);
                            // sh_mem_mngr::store_data(SH_CODA_HEADER_IMU, barometer_buffer);
                        }
                    #endif

                    #if defined(CONFIG_SHRD_HEADER)
                        if(g_shrd_config_data.imu) {
                            uint8_t accel_info = 0xA0;
                            #if defined (CONFIG_SH_BMI323)
                                if(CONFIG_SH_BMI323_ACCEL_RANGE == 1){
                                    accel_info |= 0x01;
                                }
                                else if(CONFIG_SH_BMI323_ACCEL_RANGE == 2){
                                    accel_info |= 0x02;
                                }
                                else if(CONFIG_SH_BMI323_ACCEL_RANGE == 3){
                                    accel_info |= 0x04;
                                }
                                else if(CONFIG_SH_BMI323_ACCEL_RANGE == 4){
                                    accel_info |= 0x08;
                                }
                            #endif

                            ret = mem_mgr_storage_handler(imu_data.data, SH_DATA_TYPE_IMU1_LSM6DSL_MODE_8, 
                                &(sensor_page_index.imu_buf_index), IMU_DATA_LEN, &sensor_data.imu_nand_pages, imu_data.timestamp_storage, accel_info);
                            if(ret == SH_DL_NO_MEMORY)
                            {
                                LOG_ERR("Memory full stop the session");
                                state.mem_mgr_module_state  = MEM_MGR_MEMORY_FULL_STATE;
                            }
                            if(ret != SH_DL_SUCCESS)
                            {
                                LOG_ERR("Not able to store IMU data");
                                fault_handler::send_diagnostics(NAND_FLASH_FAULT);
                                state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                            }
                        }
                    #endif
                }
            }
        #endif // (CONFIG_SH_BMI323)

        #if defined(CONFIG_SH_BMI323_STEP_COUNT_ENABLE)
            if(!zbus_sub_wait_msg(&mem_imu_step_data_sub, &chan, &imu_step_data, K_NO_WAIT))
            {
            #if defined(CONFIG_SHRD_HEADER)
                if(g_shrd_config_data.steps) {
                    uint8_t step_count[2];

                    // Encode 2 bytes of step count data and store in shrd
                    uint16_encode(imu_step_data.step_count, step_count);

                    ret = mem_mgr_storage_handler(step_count, SH_DATA_TYPE_STEP_COUNT,
                    &(sensor_page_index.step_count_buf_index), IMU_STEP_DATA_LEN, &sensor_data.step_count_nand_pages,
                    imu_step_data.timestamp_storage, 0);
                    if(ret == SH_DL_NO_MEMORY) {
                        LOG_ERR("Memory full stop the session");
                        state.mem_mgr_module_state = MEM_MGR_MEMORY_FULL_STATE;
                    }
                    if(ret != SH_DL_SUCCESS) {
                        LOG_ERR("Not able to store temperature data");
                        fault_handler::send_diagnostics(NAND_FLASH_FAULT);
                        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                    }
                }
            #endif /* CONFIG_SHRD_HEADER */
            }
        #endif /* CONFIG_SH_BMI323_STEP_COUNT_ENABLE */

        #if defined(CONFIG_BMA456)
            #if CONFIG_PIPES
            size_t imu_bytes_read = 0;
            if(!k_pipe_get(&imu_pipe, (void *)&imu_data, sizeof(imu_data), &imu_bytes_read,sizeof(imu_data), K_NO_WAIT) \
                && imu_bytes_read >= sizeof(imu_data))
            #else
            if (!zbus_sub_wait_msg(&mem_imu_data_sub, &chan, &imu_data, K_NO_WAIT))
            #endif
            {
                if(imu_data.type == 0xFF){    // IMU Fault stop the session
                    state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;     // Dummy failure to stop the session due to sensor fault
                }
                else{   
                    #if defined(CONFIG_CODA_HEADER)
                        coda_imu_t data;
                        memset((void *)&data, 0, sizeof(data));
                        // Now store berometer data
                        memcpy(data.x, imu_data.x, sizeof(imu_data.x));
                        memcpy(data.y, imu_data.y, sizeof(imu_data.y));
                        //memcpy(data.z, imu_data.z, sizeof(imu_data.z));
                        data.timestamp = imu_data.timestamp;

                        // Create a seprate thread and zbus if required
                        sh_coda_ret_code_t ret = sh_coda_imu_header(&data, imu_buffer + 1, FS_SIZE);
                        if (ret == SH_CODA_PAGE_BUFFER_FULL)
                        {
                            // Store the data into data logger module
                            LOG_INF("Now store imu data");
                            // k_msleep(1);
                            // sh_mem_mngr::store_data(SH_CODA_HEADER_IMU, barometer_buffer);
                        }
                    #endif

                    #if defined(CONFIG_SHRD_HEADER)
                        if(g_shrd_config_data.imu) {
                            uint8_t accel_info = 0xA0;

                            #if defined(CONFIG_BMA456)
                                if(CONFIG_BMA456_ACCEL_RANGE == 2){
                                    accel_info |= 0x01;
                                }
                                else if(CONFIG_BMA456_ACCEL_RANGE == 4){
                                    accel_info |= 0x02;
                                }
                                else if(CONFIG_BMA456_ACCEL_RANGE == 8){
                                    accel_info |= 0x04;
                                }
                                else if(CONFIG_BMA456_ACCEL_RANGE == 16){
                                    accel_info |= 0x08;
                                }
                            #endif
                            ret = mem_mgr_storage_handler(imu_data.data, SH_DATA_TYPE_IMU1_LSM6DSL_MODE_8, 
                                &(sensor_page_index.imu_buf_index), IMU_DATA_LEN, &sensor_data.imu_nand_pages, imu_data.timestamp_storage, accel_info);
                            if(ret == SH_DL_NO_MEMORY)
                            {
                                LOG_ERR("Memory full stop the session");
                                state.mem_mgr_module_state  = MEM_MGR_MEMORY_FULL_STATE;
                            }
                            if(ret != SH_DL_SUCCESS)
                            {
                                LOG_ERR("Not able to store IMU data");
                                fault_handler::send_diagnostics(NAND_FLASH_FAULT);
                                state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                            }
                        }
                    #endif
                }
            }
        #endif // (CONFIG_BMA456)
            
        #if defined(CONFIG_MICROPHONE)
            #if CONFIG_PIPES
            size_t mic_bytes_read = 0;
            if(!k_pipe_get(&microphone_pipe, (void *)&mic_data, sizeof(mic_data), &mic_bytes_read, sizeof(mic_data), K_NO_WAIT) \
                && mic_bytes_read >= sizeof(mic_data))
            #else
            if (!zbus_sub_wait_msg(&mem_mic_data_sub, &chan, &mic_data, K_NO_WAIT))
            #endif
            {
                if(mic_data.type == 0xFF){    // Mic Fault stop the session
                    state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;     // Dummy failure to stop the session due to sensor fault
                }
                else{      
                    #if defined(CONFIG_CODA_HEADER)
                        static uint8_t mic_data_index = 0;
                        if(mic_data_index < 20)
                        {
                            mic_data_index = 0;
                            LOG_INF("Now store mic data");
                            // sh_mem_mngr::store_data(SH_CODA_HEADER_MIC_DATA, mic_data_buffer);
                            memset(mic_data_buffer, 0, sizeof(mic_data_buffer));
                        }
                        else
                        {
                            // memcpy((mic_data_buffer + 1 + (mic_data_index * MICRO_PHONE_DATA_LEN) ) , 
                            //     mic_data.micro_phone, sizeof(mic_data.micro_phone));
                            mic_data_index++;
                        }
                    #endif

                    #if defined(CONFIG_SHRD_HEADER)
                        if(g_shrd_config_data.mic) {
                            microphone_config_t config;
                            memset(&config, 0, sizeof(config));
                            uint8_t data_type = SH_DATA_TYPE_INVALID;
                            uint16_t mic_data_length = 0;
                            sh_microphone::get_current_config(&config);
                            if(config.num_of_chan == STEREO)
                            {
                                data_type = SH_DATA_TYPE_MIC_LC3_STEREO;
                                mic_data_length = MAX_MIC_DATA_LEN;
                            }
                            else
                            {
                                data_type = SH_DATA_TYPE_MIC_LC3_MONO;
                                mic_data_length = (MAX_MIC_DATA_LEN / 2);
                            }
                            ret = mem_mgr_storage_handler(mic_data.data, data_type, 
                                &(sensor_page_index.mic_buf_index), mic_data_length, &sensor_data.mic_nand_pages, mic_data.timestamp_storage, 
                                    ((config.mic_lchannel_gain+config.mic_rchannel_gain)/2));
                            if(ret == SH_DL_NO_MEMORY)
                            {
                                LOG_ERR("Memory full stop the session");
                                state.mem_mgr_module_state = MEM_MGR_MEMORY_FULL_STATE;
                            }
                            if(ret != SH_DL_SUCCESS)
                            {
                                LOG_ERR("Not able to store Mic data");
                                fault_handler::send_diagnostics(NAND_FLASH_FAULT);
                                state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                            }
                        }
                    #endif
                }
            }
        #endif //(CONFIG_MICROPHONE)

        if (!zbus_sub_wait_msg(&mem_event_sub, &chan, &event_data, K_NO_WAIT))
        {
    #if defined(CONFIG_SHRD_HEADER)
            if(g_shrd_config_data.events) {
                uint8_t event_data_store[5];
                memcpy(event_data_store, &event_data, sizeof(event_data_store));

                ret = mem_mgr_storage_handler(event_data_store, SH_DATA_TYPE_EVENT_TIMESTAMP, 
                    &(sensor_page_index.event_buf_index), sizeof(event_data_store), &sensor_data.event_nand_pages,
                    event_data.timestamp_storage, 0x00);
                if(ret == SH_DL_NO_MEMORY)
                {
                    LOG_ERR("Memory full stop the session");
                    state.mem_mgr_module_state = MEM_MGR_MEMORY_FULL_STATE;
                }
                if(ret != SH_DL_SUCCESS)
                {
                    LOG_ERR("Not able to store Event data");
                    fault_handler::send_diagnostics(NAND_FLASH_FAULT);
                    state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                }
            }
    #endif
        }

#if ((CONFIG_SEGNO_SCRATCH_PROGRAM) || (CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM) || \
            (CONFIG_SEGNO_WEAR_DETECT_PROGRAM) || (CONFIG_SEGNO_ANGLE_DETECT_PROGRAM))
            if((g_shrd_config_data.activity) || (g_shrd_config_data.arm_angle) || (g_shrd_config_data.imu_aggregated) || (g_shrd_config_data.scratch))
            {   
                if (!zbus_sub_wait_msg(&mem_segno_sub, &chan, &segno_event_data, K_NO_WAIT))
                {   
                    switch(segno_event_data.type)
                    {
                    #if ((CONFIG_SHRD_HEADER) && (CONFIG_SEGNO_SCRATCH_PROGRAM))
                        case sh_segno_thread::segno_eve_type::SEGNO_SCRATCH_DETECT:
                        {
                            if(g_shrd_config_data.scratch) {
                                ret = mem_mgr_storage_handler((uint8_t*)&(segno_event_data.segno_data.scratch_data), SH_DATA_TYPE_SCRATCH_DETECTION, 
                                &(sensor_page_index.scratch_buf_index), sizeof(float), &sensor_data.scratch_nand_pages, 
                                segno_event_data.timestamp_storage, 0x00);
                            }
                        }
                        break;
                    #endif

                    #if ((CONFIG_SHRD_HEADER) && (CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM))
                        case sh_segno_thread::segno_eve_type::SEGNO_ACTIVITY_INDEX_DETECT:
                        {
                            if(g_shrd_config_data.activity) {
                                uint8_t activity_detection_data[4];
                                memcpy(activity_detection_data, &(segno_event_data.segno_data.activity_index), sizeof(activity_detection_data));
                                ret = mem_mgr_storage_handler(activity_detection_data, SH_DATA_TYPE_ACTIVITY_INDEX, 
                                &(sensor_page_index.activity_detct_buf_index), sizeof(activity_detection_data), &sensor_data.activity_detection_nand_pages, 
                                segno_event_data.timestamp_storage, 0x00);
                            }
                        }
                        break;
                    #endif

                    #if ((CONFIG_SHRD_HEADER) && (CONFIG_SEGNO_WEAR_DETECT_PROGRAM))
                        case sh_segno_thread::segno_eve_type::SEGNO_WEAR_DETECT:
                        {
                            if(g_shrd_config_data.imu_aggregated) {
                                uint8_t wear_detection_data[36];
                                memcpy(wear_detection_data, &(segno_event_data.segno_data.wear_detection), sizeof(wear_detection_data));
                                ret = mem_mgr_storage_handler(wear_detection_data, SH_DATA_TYPE_WEAR_DETECTION, 
                                &(sensor_page_index.wear_detection_buf_index), sizeof(wear_detection_data),
                                &sensor_data.wear_detection_nand_pages, segno_event_data.timestamp_storage, 0x00);   
                            }
                        }
                        break;
                    #endif

                    #if ((CONFIG_SHRD_HEADER) && (CONFIG_SEGNO_ANGLE_DETECT_PROGRAM))
                        case sh_segno_thread::segno_eve_type::SEGNO_ANGLE_DETECT:
                        {
                            if(g_shrd_config_data.arm_angle) {
                                uint8_t angle_data[4];
                                memcpy(angle_data, &(segno_event_data.segno_data.angle_data), sizeof(angle_data));
                                ret = mem_mgr_storage_handler(angle_data, SH_DATA_TYPE_ARM_ANGLE_DETECTION, 
                                &(sensor_page_index.angle_detection_buf_index), sizeof(angle_data), 
                                &sensor_data.angle_detection_nand_pages, segno_event_data.timestamp_storage, 0x00);  
                            }
                        }
                        break;
                    #endif

                        default:
                        LOG_ERR("Invalid segno event");
                        break;
                    }
                    
                    if(ret == SH_DL_NO_MEMORY)
                    {
                        LOG_ERR("Memory full stop the session");
                        state.mem_mgr_module_state  = MEM_MGR_MEMORY_FULL_STATE;
                    }
                    if(ret != SH_DL_SUCCESS)
                    {
                        LOG_ERR("Not able to store stratch data");
                        fault_handler::send_diagnostics(NAND_FLASH_FAULT);
                        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
                    }
                }
#endif
            }
        }
        
        if((MEM_MGR_MEMORY_FULL_STATE == state.mem_mgr_module_state) || (MEM_MGR_FAILURE_STATE == state.mem_mgr_module_state))
        {
    #if defined(CONFIG_DATA_LOGGER)
            LOG_INF("Stop the session");
            if(state.mem_mgr_is_all_channel_active)
            {
                ret_code_t rc = 0;
                rc = mem_mgr_unsubscribe_all_sensor_channel();
                if(rc != SH_DL_SUCCESS)
                {
                    LOG_ERR("Not able to unsubscribe channels");
                }
                state.mem_mgr_is_all_channel_active = false;
            }
            
            // mem_mgr_set_device_info();
            ret = sh_shrd_header::save_info_block(SH_DL_START_DUMMY ,SH_DL_SESSION_END_CALL_BACK);
            if(ret != SH_DL_SUCCESS)
            {
                LOG_INF("Couldn't save information block in shrd file");
            }

            ret = sh_data_logger::session_stop();
            if (ret != SH_DL_SUCCESS)
            {
                LOG_ERR("Not able to stop the session in fault state");
            }
            else
            {
                //Set mobile app status as stop to control active session status
                uint8_t status = 0;
                sh_data_logger::get_session_app_status(&status);
                if((status == SH_SESSION_SET_STARTED_ACTIVE_SESSION) || 
                (status == SH_SESSION_SET_STARTED_NO_SESSION))
                {
                    sh_data_logger::update_session_status(SH_SESSION_SET_STOPPED);
                }
            }

            sh_data_logger::update_session_status(SH_SESSION_SET_STOPPED);

            //Get the session information
            memset(&session_info, 0, sizeof(session_info));
            ret = sh_data_logger::get_session_info_status(&session_info);
            if(ret != SH_DL_SUCCESS)
            {
                LOG_ERR("Couldn't get session information");
            }

            //Send the session information via BLE
            ret = mem_mgr_send_info_data_to_ble(&session_info);
            if(ret != SH_DL_SUCCESS)
            {
                LOG_ERR("Not able to send data to BLE");
            }

            if(MEM_MGR_FAILURE_STATE == state.mem_mgr_module_state)
            {
                fault_handler::send_diagnostics(NAND_FLASH_FAULT);
            }
            state.mem_mgr_module_state = MEM_MGR_IDLE_STATE;
    #endif
        }
    }
}
#if defined(CONFIG_DATA_LOGGER)
ret_code_t sh_mem_mngr::update_mode(session_mode_type_t mode)
{
    return sh_data_logger::update_session_mode(mode);
}
#endif

void sh_mem_mngr::get_memory_manager_state(state_t *status)
{
    *status = state.mem_mgr_module_state;
}

#if defined(CONFIG_DATA_LOGGER)
// Workqueue handler function for download process
void download_work_handler(struct k_work *work)
{
    state.mem_mgr_work_queue_state = 1;
    LOG_INF("Now Download work queue started");
    uint32_t rc = sh_mem_mngr::download_data();
    if (rc != SH_DL_SUCCESS)
    {
        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
    }
    else
    {
        LOG_INF("Download is completed");
        state.mem_mgr_module_state = MEM_MGR_IDLE_STATE;
    }
    state.mem_mgr_work_queue_state = 0;
}

// Workqueue handler function for update session
void update_session_handler(struct k_work *work)
{
    printf("Update the session information\r\n");
    sh_data_logger::update_session_info_memory();
}

void update_session_timer_handler(struct k_timer *arg)
{
    //10 minute timer callback
    k_work_submit(&(evt_queue.update_session_timer_work));
}
#endif

#if defined(CONFIG_CODA_HEADER)
uint32_t sh_mem_mngr::store_data(coda_data_header_t type, uint8_t *data)
{
    if (data == NULL)
    {
        return SH_DL_NULL;
    }
    // Store the data into data logger module
    data[0] = type;
    return sh_data_logger::session_write_page(data);
}
#endif

#if defined(CONFIG_DATA_LOGGER)
static uint32_t mem_mgr_send_info_data_to_ble(sh_dl_session_info *session_info)
{
    uint8_t data_buf[12];
    uint32_t ret = 0;
    sh_session_thread::msg_t ble_notify_data;
    sh_ble::adv_data_t adv_batt_data;

    if(session_info == NULL)
    {
        LOG_ERR("NULL pointer");
        return SH_DL_NULL;
    }

    LOG_INF("Start time stamp is %d", session_info->start_time);

    data_buf[0] = session_info->num_pages & 0xFF;
    data_buf[1] = ((session_info->num_pages) >> 8 ) & 0xFF;
    data_buf[2] = ((session_info->num_pages)  >> 16) & 0xFF;
    data_buf[3] = ((session_info->num_pages) >> 24) & 0xFF;
    data_buf[4] = session_info->session_flag;  
    data_buf[5] = session_info->nand_status;  
    data_buf[6] = session_info->memory_percentage;  
    data_buf[7] = session_info->session_mode;

    adv_batt_data.memory_percentage = session_info->memory_percentage;
    sh_ble::update_advertising_data(sh_ble::BLE_ADV_MEMORY_PERCENTAGE, adv_batt_data);

    adv_batt_data.session_status = session_info->session_flag;
    sh_ble::update_advertising_data(sh_ble::BLE_ADV_SESSION_STATUS, adv_batt_data);

    adv_batt_data.session_mode = session_info->session_mode;
    sh_ble::update_advertising_data(sh_ble::BLE_ADV_SESSION_MODE, adv_batt_data);

    memcpy(&data_buf[8], &(session_info->start_time), sizeof(uint32_t));
    
    memset(&ble_notify_data, 0, sizeof(ble_notify_data));
    memcpy(ble_notify_data.data, data_buf, 12);
    ble_notify_data.len = 12;
    ble_notify_data.cmd = SESSION_REQ_INFO;
    ret = zbus_chan_pub(&mem_mngr_chan, &ble_notify_data, K_FOREVER);
    if(ret != 0){
        LOG_ERR("Failed to publish Flash data to BLE [%d]",ret);
    }
    k_sem_give(&dl_session_sem);
    return ret;
}

uint32_t sh_mem_mngr::download_data(void)
{
    // Start Reading the coda data from session
    uint8_t cur_page[FS_SIZE];
    ret_code_t rc = 0;
    uint16_t deleted_id;
    mic_data_config_t mic_config_data;

    #if defined(CONFIG_SHRD_HEADER)
    uint8_t m_download_session_buf[244];
    sh_session_thread::msg_t ble_notify_data;
    int ret = 0;
    #endif

    memset(cur_page, 0, sizeof(cur_page));

    sh_dl_session_info_v2_t cur_session;
    memset(&cur_session, 0, sizeof(cur_session));
    LOG_INF("Download data started");

    //TODO: Fix the issue from app side as of now we have set the connection interval as 50 (62.5 ms) and 70 (85.5ms)
    //TODO: Fix this issue in future set the connection interval as 12(15 ms)
    sh_mic_config::get_mic_conf(&mic_config_data);

    if(mic_config_data.enabled) {
        ret = sh_ble::update_connection_parameters(sh_ble::CONN_CONFIG_MIC_STREAM);
    }
    else {
        ret = sh_ble::update_connection_parameters(sh_ble::CONN_CONFIG_STREAM);
    }
    if(ret != 0){
        LOG_ERR("Failed to update connection parameters with error %d", ret);
    }

    //As of now added 1 second of delay to update the connection interval
    k_msleep(1000);

    state.mem_mgr_retry_cnt = 0;
    state.mem_mgr_is_session_complete = false;
    state.mem_mgr_resend = false;
    state.mem_mgr_page_wise_local_crc32 = sh_utils::crc32(NULL, 0, NULL);

    while ((ret = sh_data_logger::get_session_info(sh_data_logger::get_record_id(), &cur_session) ) != SH_DL_SESSION_NOT_FOUND)
    {
#ifdef CONFIG_WATCHDOG
    if(wdt_mem_mgr_channel_id >= 0) {
        task_wdt_fed_from_thread(wdt_mem_mgr_channel_id);
    }
#endif 
        if(ret != SH_DL_SUCCESS)
        {
            fault_handler::send_diagnostics(NAND_FLASH_FAULT);
            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
            return ret;
        }

        if(state.mem_mgr_req_cancel_download)
        {
            LOG_ERR("Cancel download request");
            state.mem_mgr_req_cancel_download = false;
            return 0;
        }

        LOG_INF("Record ID %d", cur_session.record_id);
        LOG_INF("Start time %d", cur_session.start_time_ms);
        LOG_INF("CMD time %d", cur_session.cmd_time_ms);
        
        if(cur_session.pages_per_session == 0)
        {
            LOG_INF("Delete the session");
            sh_data_logger::session_delete_oldest(&deleted_id);
            if (deleted_id != cur_session.record_id) {
                LOG_ERR("Deleted ID doesn't match current session id");
            }
            continue;
        }

        cur_session.pages_per_session -= 1;
        LOG_INF("Page length %d", cur_session.pages_per_session);

#if defined(CONFIG_CODA_HEADER)
        for (uint8_t data = SH_CODA_HEADER_IMU; data < SH_CODA_HEADER_MAX; data++)
        {
            sh_dl_session_read_token_t read_token;
            read_token.session = cur_session;
            read_token.ptr = 0;

            // Read each page in the fragment and check that the timestamp matches
            while (true)
            {

#ifdef CONFIG_WATCHDOG
    if(wdt_mem_mgr_channel_id >= 0) {
        task_wdt_fed_from_thread(wdt_mem_mgr_channel_id);
    }
#endif 

                rc = sh_data_logger::session_read_page(cur_page, &read_token);
                if (rc == SH_DL_SESSION_READ_DONE)
                {
                    LOG_INF("All data read done for coda format");
                    break;
                }
                // LOG_INF("Read data is %d", cur_page[0]);

                // Now seprate data according to header type
                if (cur_page[0] == data)
                {
                    // Now send data via BLE notification from here
                    // As of Now print the type of data and just LOG first 10
                    // bytes of data
                    if (cur_page[0] == SH_CODA_HEADER_IMU)
                    {
                        LOG_INF("***IMU data***");
                        // Send data to BLE notifcation thread
                    }
                    else if(cur_page[0] == SH_CODA_HEADER_TEMP)
                    {
                        LOG_INF("***Temperture data****");
                        // Send data to BLE notifcation thread
                    }
                    else if(cur_page[0] == SH_CODA_HEADER_BEROMETER)
                    {
                        LOG_INF("***Berometer data****");
                        // Send data to BLE notifcation thread
                    }
                    else if(cur_page[0] == SH_CODA_HEADER_EVT)
                    {
                        LOG_INF("***Event data****");
                        // Send data to BLE notifcation thread
                    }
                    else if(cur_page[0] == SH_CODA_HEADER_PROCESS_DATA)
                    {
                        LOG_INF("***Process data****");
                        // Send data to BLE notifcation thread
                    }
                    else if(cur_page[0] == SH_CODA_HEADER_RR_DATA)
                    {
                        LOG_INF("***RR data****");
                    }
                    else
                    {
                        LOG_INF("***Mic data****");
                    }
                }
                k_msleep(1);
            }
        }
#endif

    #if defined(CONFIG_SHRD_HEADER)
        uint32_t start_time_ms = cur_session.start_time_ms;
        uint32_t cmd_time_ms = cur_session.cmd_time_ms;
        uint32_t page_len = sh_data_logger::get_app_num_pages(cur_session.pages_per_session);
        uint16_t session_idx = cur_session.record_id;
        uint8_t session_data_len = 0;
        uint8_t retry_cnt = 0;
        memset(&ble_notify_data, 0, sizeof(ble_notify_data));

        m_download_session_buf[session_data_len++] = SH_BLE_DOWNLOAD_INFO;
        m_download_session_buf[session_data_len++] = start_time_ms & 0xFF;
        m_download_session_buf[session_data_len++] = (start_time_ms >> 8) & 0xFF;
        m_download_session_buf[session_data_len++] = (start_time_ms >> 16) & 0xFF;
        m_download_session_buf[session_data_len++] = (start_time_ms >> 24) & 0xFF;
        m_download_session_buf[session_data_len++] = page_len & 0xFF;
        m_download_session_buf[session_data_len++] = (page_len >> 8) & 0xFF;
        m_download_session_buf[session_data_len++] = (page_len >> 16) & 0xFF;
        m_download_session_buf[session_data_len++] = (page_len >> 24) & 0xFF;
        m_download_session_buf[session_data_len++] = cur_session.dev_type;
        m_download_session_buf[session_data_len++] = state.mem_mgr_resend;
        memcpy(m_download_session_buf + session_data_len * sizeof(uint8_t),cur_session.session_id,SH_SESSION_SET_ID_LENGTH_1);
        session_data_len += SH_SESSION_SET_ID_LENGTH_1;
        m_download_session_buf[session_data_len++] = cmd_time_ms & 0xFF;
        m_download_session_buf[session_data_len++] = (cmd_time_ms >> 8) & 0xFF;
        m_download_session_buf[session_data_len++] = (cmd_time_ms >> 16) & 0xFF;
        m_download_session_buf[session_data_len++] = (cmd_time_ms >> 24) & 0xFF;
        m_download_session_buf[session_data_len++] = session_idx & 0xFF;
        m_download_session_buf[session_data_len++] = (session_idx >> 8) & 0xFF;
        memcpy(m_download_session_buf + session_data_len * sizeof(uint8_t),&cur_session.session_id[SH_SESSION_SET_ID_LENGTH_1],SH_SESSION_SET_ID_LENGTH_2);
        session_data_len += SH_SESSION_SET_ID_LENGTH_2;

        LOG_INF("Session length is %d", session_data_len);

        //Send this header via BLE notification
        memcpy(ble_notify_data.data, m_download_session_buf, session_data_len);
        ble_notify_data.len = session_data_len;
        ble_notify_data.cmd = SESSION_DOWNLOAD;

        ret = sh_ble_dl_dwnld_send(ble_notify_data.data, ble_notify_data.len);
        if(ret < 0) {
            LOG_ERR("Failed to notify the data [%d]",ret);
        }

        memset(&ble_notify_data, 0, sizeof(ble_notify_data));

        /** Get Session Read Token */
        sh_dl_session_read_token_t read_token;
        read_token.session = cur_session;
        read_token.ptr = 0;

        /** Stream Data */
        uint16_t bytes_to_read = 0;
        uint16_t page_ptr = 0;
        uint32_t num_page_read_failed = 0;
        uint16_t cnt = 0;

        memset(m_download_session_buf, 0, sizeof(m_download_session_buf));
        m_download_session_buf[0] = SH_BLE_DOWNLOAD_DATA;
        state.mem_mgr_page_wise_local_crc32 = sh_utils::crc32(NULL, 0, NULL);
        uint32_t total_packet = 0; 
        while(true)
        {

#ifdef CONFIG_WATCHDOG
    if(wdt_mem_mgr_channel_id >= 0) {
        task_wdt_fed_from_thread(wdt_mem_mgr_channel_id);
    }
#endif 
            if(state.mem_mgr_req_cancel_download)
            {
                LOG_ERR("Cancel download request");
                state.mem_mgr_req_cancel_download = false;

                //TODO: Fix the issue from app side as of now we have set the connection interval as 50 (62.5 ms) and 70 (85.5ms) for idle
                //TODO: Remove below commented line
                // ret = sh_ble::update_connection_parameters(sh_ble::CONN_CONFIG_IDLE);
                // if(ret != 0){
                //     LOG_ERR("Failed to update connection parameters with error %d", ret);
                // }
                return 0;
            }

            rc = sh_data_logger::session_read_page(cur_page, &read_token);
            if (rc == SH_DL_NAND_FLASH_FAILED)
            {
                num_page_read_failed++;
            } 
            else if (rc == SH_DL_SESSION_READ_DONE)
            {
                break;
            }

            //Now validate the crc for first block
            rc = sh_shrd_header::validate_crc_page_wise(cur_page);
            if(rc != SH_DL_SUCCESS)
            {
                LOG_ERR("Varification fail. CRC is not matching");
            }

            //Now validate the crc for second block
            rc = sh_shrd_header::validate_crc_page_wise(cur_page + 2048);
            if(rc != SH_DL_SUCCESS)
            {
                LOG_ERR("Varification fail. CRC is not matching");
            }

            bytes_to_read = MT29F_EXP_PAGE_SIZE;
            page_ptr = 0;
            while (bytes_to_read != 0)
            {

#ifdef CONFIG_WATCHDOG
    if(wdt_mem_mgr_channel_id >= 0) {
        task_wdt_fed_from_thread(wdt_mem_mgr_channel_id);
    }
#endif 
                //Send 181 
                uint16_t send_bytes = MIN(bytes_to_read, 181);
                bytes_to_read -= send_bytes;
                total_packet += send_bytes;
               
                memcpy(m_download_session_buf + 1, cur_page + page_ptr, send_bytes);

                state.mem_mgr_page_wise_local_crc32 = sh_utils::crc32(m_download_session_buf + 1, send_bytes,&(state.mem_mgr_page_wise_local_crc32));
                page_ptr += send_bytes;
                LOG_DBG("Now sending data to BLE %d, CRC %u, length %d", cnt, state.mem_mgr_page_wise_local_crc32, total_packet);
                cnt = cnt+1;

                memset(&ble_notify_data, 0, sizeof(ble_notify_data));
                //Send this data to BLE Application
                memcpy(ble_notify_data.data, m_download_session_buf, send_bytes+1);
                ble_notify_data.len = send_bytes + 1;
                ble_notify_data.cmd = SESSION_DOWNLOAD;

                if(state.mem_mgr_req_cancel_download)
                {
                    LOG_ERR("Cancel download request");
                    state.mem_mgr_req_cancel_download = false;
                    return 0;
                }

                ret = sh_ble_dl_dwnld_send(ble_notify_data.data, ble_notify_data.len);
                if(ret < 0) {
                    LOG_ERR("Failed to notify the data [%d]",ret);
                }
            }
        }

        /** Acknowledge the downloaded data and do CRC32 Check */
        memset(m_download_session_buf, 0, sizeof(m_download_session_buf));
        m_download_session_buf[0] = SH_BLE_DOWNLOAD_ACK_REQUEST;
        uint32_encode(state.mem_mgr_page_wise_local_crc32, &m_download_session_buf[1]);
        LOG_INF("SENDING ACK Request");
        LOG_INF("CRC : data[0]: %x, data[1]: %x, data[2]: %x, data[3]: %x\r\n", m_download_session_buf[1], m_download_session_buf[2], \
            m_download_session_buf[3], m_download_session_buf[4]);

        memcpy(ble_notify_data.data, m_download_session_buf, 5);
        ble_notify_data.len = 5;
        ble_notify_data.cmd = SESSION_DOWNLOAD;
        ret = sh_ble_dl_dwnld_send(ble_notify_data.data, ble_notify_data.len);
        if(ret < 0) {
            LOG_ERR("Failed to notify the data [%d]",ret);
        }
        k_msleep(50);

        LOG_INF("AFTER ACK Request");

        memset(&ble_notify_data, 0, sizeof(ble_notify_data));

        while(!(state.mem_mgr_is_session_complete) && !(state.mem_mgr_resend))
        {

#ifdef CONFIG_WATCHDOG
    if(wdt_mem_mgr_channel_id >= 0) {
        task_wdt_fed_from_thread(wdt_mem_mgr_channel_id);
    }
#endif 
            //Wait for the session complete event
            k_msleep(1000); //1 seconds delay
            retry_cnt++;
            LOG_INF("Waiting for the ACK from mobile");
            if(retry_cnt == SH_RETRY_LIMIT)
            {
                state.mem_mgr_resend = true;
            }
        }

        if(state.mem_mgr_resend){
            state.mem_mgr_resend = false;
            state.mem_mgr_is_session_complete = false;
            retry_cnt = 0;
            if(state.mem_mgr_retry_cnt >= SH_RESEND_LIMIT){
                LOG_INF("Number of attempts to send data has reached limit");
                state.mem_mgr_resend = false;

                ret = sh_ble::disable_ble();
                if(ret < 0) {
                    LOG_ERR("Failed to disabled the BLE in session thread");
                }
                k_msleep(100);

                NVIC_SystemReset();
            }
            state.mem_mgr_retry_cnt++;
            continue;
        }

        if(state.mem_mgr_is_session_complete)
        {
            retry_cnt = 0;
            sh_data_logger::session_delete_oldest(&deleted_id);
            if (deleted_id != cur_session.record_id) {
                LOG_ERR("Deleted ID doesn't match current session id");
                rc = sh_data_logger::reset();
                if( rc != SH_DL_SUCCESS )
                {
                    LOG_ERR("Not able to delete all sessions");
                }
                memset(m_download_session_buf, 0, sizeof(m_download_session_buf));
                m_download_session_buf[0] = SH_BLE_DOWNLOAD_DONE;
                LOG_INF("SH_BLE_DOWNLOAD_DONE");
                memcpy(ble_notify_data.data, m_download_session_buf, 1);
                ble_notify_data.len = 1;
                ble_notify_data.cmd = SESSION_DOWNLOAD;
                ret = zbus_chan_pub(&mem_mngr_chan, &ble_notify_data, K_FOREVER);
                if(ret != 0){
                    LOG_ERR("Failed to publish Flash data to BLE [%d]",ret);
                }
                k_sem_give(&dl_session_sem);
                memset(&ble_notify_data, 0, sizeof(ble_notify_data)); 

                //TODO: Fix the issue from app side as of now we have set the connection interval as 50 (62.5 ms) and 70 (85.5ms) for idle
                //TODO: Remove below commented line
                // ret = sh_ble::update_connection_parameters(sh_ble::CONN_CONFIG_IDLE);
                // if(ret != 0){
                //     LOG_ERR("Failed to update connection parameters with error %d", ret);
                // }
                return SH_DL_INVALID_PARAM;
            }
            state.mem_mgr_is_session_complete = false;
            state.mem_mgr_resend = false;
        }
        else
        {
            LOG_INF("Timout has happened for recieving ACK from base station");
            state.mem_mgr_resend = false;
            state.mem_mgr_retry_cnt = 0;
            state.mem_mgr_is_session_complete = false;
            state.mem_mgr_page_wise_local_crc32 = sh_utils::crc32(NULL, 0, NULL);
            continue;
        }
    #endif
    } // Download while loop

    //TODO: Fix the issue from app side as of now we have set the connection interval as 50 (62.5 ms) and 70 (85.5ms) for idle
    //TODO: Remove below commented line
    //Go back to idle conn interval
    // ret = sh_ble::update_connection_parameters(sh_ble::CONN_CONFIG_IDLE);
    // if(ret != 0){
    //     LOG_ERR("Failed to update connection parameters with error %d", ret);
    // }

    //Clear information about all session
    rc = sh_data_logger::reset();
    if( rc != SH_DL_SUCCESS )
    {
        LOG_ERR("Not able to delete all sessions");
    }

    m_download_session_buf[0] = SH_BLE_DOWNLOAD_DONE;
    LOG_INF("SH_BLE_DOWNLOAD_END");
    memcpy(ble_notify_data.data, m_download_session_buf, 1);
    ble_notify_data.len = 1;
    ble_notify_data.cmd = SESSION_DOWNLOAD;
    ret = zbus_chan_pub(&mem_mngr_chan, &ble_notify_data, K_FOREVER);
    if(ret != 0){
        LOG_ERR("Failed to publish Flash data to BLE [%d]",ret);
    }
    k_sem_give(&dl_session_sem);
    memset(&ble_notify_data, 0, sizeof(ble_notify_data));
    LOG_INF("Download data testing completed");
    
    // ret = sh_ble::update_connection_parameters(sh_ble::CONN_CONFIG_IDLE);
    // if(ret != 0){
    //     LOG_ERR("Failed to update connection parameters with error %d", ret);
    // }

    return 0;
}
#endif

#if defined(CONFIG_SHRD_HEADER)
static void mem_mgr_shrd_init(void) 
{
    memset((void *)&sensor_data.imu_nand_pages, 0 , sizeof(sensor_data.imu_nand_pages));
    memset((void *)&sensor_data.baro_nand_page, 0 , sizeof(sensor_data.baro_nand_page));
    memset((void *)&sensor_data.mic_nand_pages, 0 , sizeof(sensor_data.mic_nand_pages));
    memset((void *)&sensor_data.temp_nand_page, 0 , sizeof(sensor_data.temp_nand_page));
    memset((void *)&sensor_data.event_nand_pages, 0 , sizeof(sensor_data.event_nand_pages));
#ifdef CONFIG_SEGNO_WEAR_DETECT_PROGRAM
    memset((void *)&sensor_data.wear_detection_nand_pages, 0 , sizeof(sensor_data.wear_detection_nand_pages));
#endif
#ifdef CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM
    memset((void *)&sensor_data.activity_detection_nand_pages, 0 , sizeof(sensor_data.activity_detection_nand_pages));
#endif
#ifdef CONFIG_SEGNO_SCRATCH_PROGRAM
    memset((void *)&sensor_data.scratch_nand_pages, 0 , sizeof(sensor_data.scratch_nand_pages));
#endif
#ifdef CONFIG_SEGNO_ANGLE_DETECT_PROGRAM
    memset((void *)&sensor_data.angle_detection_nand_pages, 0 , sizeof(sensor_data.angle_detection_nand_pages));
#endif
#ifdef CONFIG_SH_BMI323_STEP_COUNT_ENABLE
    memset(&sensor_data.step_count_nand_pages, 0, sizeof(sensor_data.step_count_nand_pages));
#endif /* CONFIG_SH_BMI323_STEP_COUNT_ENABLE */
}

static int save_remaing_pages(void)
{
    int rc = 0;
    if((sensor_page_index.event_buf_index) != 0)
    {
        LOG_INF("Event Data write and size is %d", sensor_page_index.event_buf_index);
        rc = sesnor_last_page_data_write(&sensor_data.event_nand_pages, SH_DATA_TYPE_EVENT_TIMESTAMP, (sensor_page_index.event_buf_index / 5), 0);
        sensor_page_index.event_buf_index = 0;
        if(rc)
        {
            return rc;
        }
    }

    if((sensor_page_index.imu_buf_index) != 0)
    {
        LOG_INF("IMU Data write");
        uint8_t accel_info = 0xA0;
        #if defined (CONFIG_SH_BMI323)
        if(CONFIG_SH_BMI323_ACCEL_RANGE == 1){
            accel_info |= 0x01;
        }
        else if(CONFIG_SH_BMI323_ACCEL_RANGE == 2){
            accel_info |= 0x02;
        }
        else if(CONFIG_SH_BMI323_ACCEL_RANGE == 3){
            accel_info |= 0x04;
        }
        else if(CONFIG_SH_BMI323_ACCEL_RANGE == 4){
            accel_info |= 0x08;
        }
        #endif


        #if defined(CONFIG_BMA456)
        if(CONFIG_BMA456_ACCEL_RANGE == 2){
            accel_info |= 0x01;
        }
        else if(CONFIG_BMA456_ACCEL_RANGE == 4){
            accel_info |= 0x02;
        }
        else if(CONFIG_BMA456_ACCEL_RANGE == 8){
            accel_info |= 0x04;
        }
        else if(CONFIG_BMA456_ACCEL_RANGE == 16){
            accel_info |= 0x08;
        }
        #endif
        rc = sesnor_last_page_data_write(&sensor_data.imu_nand_pages, SH_DATA_TYPE_IMU1_LSM6DSL_MODE_8, ((sensor_page_index.imu_buf_index * 792) / 1980), accel_info);
        sensor_page_index.imu_buf_index = 0;
        if(rc)
        {
            LOG_ERR("Not able to write IMU page data");
            mem_mgr_unsubscribe_all_sensor_channel();
            fault_handler::send_diagnostics(NAND_FLASH_FAULT);
            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
        }
    }

    if((sensor_page_index.baro_buf_index) != 0)
    {
        LOG_INF("Baro Data write");
        rc = sesnor_last_page_data_write(&sensor_data.baro_nand_page, SH_DATA_TYPE_BAROMTER, (sensor_page_index.baro_buf_index / 4), 0x01);
        sensor_page_index.baro_buf_index = 0;
        if(rc)
        {
            LOG_ERR("Not able to write Baro page data");
            mem_mgr_unsubscribe_all_sensor_channel();
            fault_handler::send_diagnostics(NAND_FLASH_FAULT);
            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
        }
    }

    #if defined(CONFIG_MICROPHONE)
    if((sensor_page_index.mic_buf_index) != 0)
    {
        LOG_INF("Mic Data write");
        microphone_config_t config;
        memset(&config, 0, sizeof(config));
        uint8_t data_type = SH_DATA_TYPE_INVALID;
        sh_microphone::get_current_config(&config);
        if(config.num_of_chan == STEREO)
        {
            data_type = SH_DATA_TYPE_MIC_LC3_STEREO;
        }
        else
        {
            data_type = SH_DATA_TYPE_MIC_LC3_MONO;
        }
        rc = sesnor_last_page_data_write(&sensor_data.mic_nand_pages, data_type, (sensor_page_index.mic_buf_index), ((config.mic_lchannel_gain + config.mic_rchannel_gain) / 2));
        sensor_page_index.mic_buf_index = 0;
        if(rc)
        {
            LOG_ERR("Not able to write Mic page data");
            mem_mgr_unsubscribe_all_sensor_channel();
            fault_handler::send_diagnostics(NAND_FLASH_FAULT);
            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
        }
    }
    #endif

    if((sensor_page_index.temp_buf_index) != 0)
    {
        LOG_INF("Temperature Data write");
        rc = sesnor_last_page_data_write(&sensor_data.temp_nand_page, SH_DATA_TYPE_TEMP1_MODE_2, (sensor_page_index.temp_buf_index / 2), 0);
        sensor_page_index.temp_buf_index = 0;
        if(rc)
        {
            LOG_ERR("Not able to write temperature page data");
            mem_mgr_unsubscribe_all_sensor_channel();
            fault_handler::send_diagnostics(NAND_FLASH_FAULT);
            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
        }
    }

#ifdef CONFIG_SEGNO_SCRATCH_PROGRAM
    if((sensor_page_index.scratch_buf_index) != 0)
    {
        LOG_INF("scratch Data write data lenth: %d", sensor_page_index.scratch_buf_index);
        rc = sesnor_last_page_data_write(&sensor_data.scratch_nand_pages, SH_DATA_TYPE_SCRATCH_DETECTION, (sensor_page_index.scratch_buf_index / 4), 0);
        sensor_page_index.scratch_buf_index = 0;
        if(rc)
        {
            LOG_ERR("Not able to write scratch page data");
            mem_mgr_unsubscribe_all_sensor_channel();
            fault_handler::send_diagnostics(NAND_FLASH_FAULT);
            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
        }
    }
#endif

#ifdef CONFIG_SEGNO_WEAR_DETECT_PROGRAM
    if((sensor_page_index.wear_detection_buf_index) != 0)
    {
        LOG_INF("Wear detect Data write and size is %d", sensor_page_index.wear_detection_buf_index);
        rc = sesnor_last_page_data_write(&sensor_data.wear_detection_nand_pages, SH_DATA_TYPE_WEAR_DETECTION, (sensor_page_index.wear_detection_buf_index/36), 0);
        sensor_page_index.wear_detection_buf_index = 0;
        if(rc)
        {
            LOG_ERR("Not able to write wear detect page data");
            mem_mgr_unsubscribe_all_sensor_channel();
            fault_handler::send_diagnostics(NAND_FLASH_FAULT);
            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
        }
    }
#endif

#ifdef CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM
    if((sensor_page_index.activity_detct_buf_index) != 0)
    {
        LOG_INF("Activity detect Data write and size is %d", sensor_page_index.activity_detct_buf_index);
        rc = sesnor_last_page_data_write(&sensor_data.activity_detection_nand_pages, SH_DATA_TYPE_ACTIVITY_INDEX, (sensor_page_index.activity_detct_buf_index / 4), 0);
        sensor_page_index.activity_detct_buf_index = 0;
        if(rc)
        {
            LOG_ERR("Not able to write activity detect page data");
            mem_mgr_unsubscribe_all_sensor_channel();
            fault_handler::send_diagnostics(NAND_FLASH_FAULT);
            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
        }
    }
#endif

#ifdef CONFIG_SEGNO_ANGLE_DETECT_PROGRAM
    if((sensor_page_index.angle_detection_buf_index) != 0)
    {   
        LOG_INF("Angle detect Data write and size is %d", sensor_page_index.angle_detection_buf_index);
        rc = sesnor_last_page_data_write(&sensor_data.angle_detection_nand_pages, SH_DATA_TYPE_ARM_ANGLE_DETECTION, (sensor_page_index.angle_detection_buf_index / 4), 0);
        sensor_page_index.angle_detection_buf_index = 0;
        if(rc)
        {
            LOG_ERR("Not able to write angle detect page data");
            mem_mgr_unsubscribe_all_sensor_channel();
            fault_handler::send_diagnostics(NAND_FLASH_FAULT);
            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
        }
    }
#endif

#ifdef CONFIG_SH_BMI323_STEP_COUNT_ENABLE
    if(sensor_page_index.step_count_buf_index != 0)
    {
        LOG_INF("Step count data write");
        rc = sesnor_last_page_data_write(&sensor_data.step_count_nand_pages, SH_DATA_TYPE_STEP_COUNT, (sensor_page_index.step_count_buf_index/2), 0);
        sensor_page_index.step_count_buf_index = 0;
        if(rc) 
        {
            LOG_ERR("Not able to write step count data");
            mem_mgr_unsubscribe_all_sensor_channel();
            fault_handler::send_diagnostics(NAND_FLASH_FAULT);
            state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
        }
    }
#endif /* CONFIG_SH_BMI323_STEP_COUNT_ENABLE */

    //Now clear all pages
    memset(&sensor_data.imu_nand_pages, 0, sizeof(sensor_data.imu_nand_pages));
    memset(&sensor_data.baro_nand_page, 0, sizeof(sensor_data.baro_nand_page));
    memset(&sensor_data.mic_nand_pages, 0, sizeof(sensor_data.mic_nand_pages));
    memset(&sensor_data.temp_nand_page, 0, sizeof(sensor_data.temp_nand_page));
    memset(&sensor_data.event_nand_pages, 0, sizeof(sensor_data.event_nand_pages));
#ifdef CONFIG_SEGNO_SCRATCH_PROGRAM
    memset(&sensor_data.scratch_nand_pages, 0, sizeof(sensor_data.scratch_nand_pages));
#endif

#ifdef CONFIG_SEGNO_WEAR_DETECT_PROGRAM
    memset(&sensor_data.wear_detection_nand_pages, 0, sizeof(sensor_data.wear_detection_nand_pages));
#endif

#ifdef CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM
    memset(&sensor_data.activity_detection_nand_pages, 0, sizeof(sensor_data.activity_detection_nand_pages));
#endif

#ifdef CONFIG_SEGNO_ANGLE_DETECT_PROGRAM
    memset(&sensor_data.angle_detection_nand_pages, 0, sizeof(sensor_data.angle_detection_nand_pages));
#endif
#ifdef CONFIG_SH_BMI323_STEP_COUNT_ENABLE
    memset(&sensor_data.step_count_nand_pages, 0, sizeof(sensor_data.step_count_nand_pages));
#endif /* CONFIG_SH_BMI323_STEP_COUNT_ENABLE */
    return rc;

}

static int sesnor_last_page_data_write(sh_shrd_data_t *page_type, uint8_t page_data_type, 
    uint16_t page_data_len, uint8_t page_flag)
{
    uint32_t page_crc32 = 0;
    page_type->section.header.data_type = page_data_type;
    page_type->section.header.data_len = page_data_len;
    page_type->section.header.flag = page_flag;

    page_crc32 = sh_utils::crc32(page_type->section.body, SH_DL_BODY_BYTE_SIZE, &page_crc32);
    page_type->section.pagecrc = page_crc32;

    int rc = sh_shrd_header::write_data(page_type, false);
    if(rc != SH_DL_SUCCESS)
    {
        LOG_ERR("Not able to write page data");
        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
    }
    return rc;
}

static ret_code_t mem_mgr_storage_handler(uint8_t *input_data, uint8_t data_type, uint16_t *buf_index, 
                                uint16_t data_size, sh_shrd_data_t *nand_page, uint32_t time_stamp, uint8_t flag)
{
    ret_code_t rc = 0;
    uint32_t page_crc32 = 0;
    sh_dl_session_info ses_info;
    static sh_ble::adv_data_t adv_data;

    memcpy(nand_page->section.body + *buf_index, input_data, data_size);
    nand_page->section.header.timestamp_ms = time_stamp;

    *buf_index += data_size;
    memset(input_data, 0, data_size);

    if( (*buf_index + data_size) > SH_DL_BODY_BYTE_SIZE)        // checking whether next chunk of data will overflow the buffer
    {
        nand_page->section.header.data_type = data_type; 
        if(data_type == SH_DATA_TYPE_EVENT_TIMESTAMP)
        {
            //event type detection
            nand_page->section.header.data_len = *buf_index / 5;
        }

#if defined(CONFIG_MICROPHONE)
           
        if((data_type == SH_DATA_TYPE_MIC_LC3_MONO) || (data_type == SH_DATA_TYPE_MIC_LC3_STEREO))
        {
            //Mic data
            nand_page->section.header.data_len = *buf_index;
        }
#endif

#ifdef CONFIG_SEGNO_SCRATCH_PROGRAM
        else if(data_type == SH_DATA_TYPE_SCRATCH_DETECTION)
        {
            //scratch detection data
            LOG_INF("scratch data index data size is : %d", *buf_index);
            nand_page->section.header.data_len = (*buf_index / 4);
        }
#endif

#ifdef CONFIG_SEGNO_ANGLE_DETECT_PROGRAM
        else if((data_type == SH_DATA_TYPE_ARM_ANGLE_DETECTION))
        {
            //angle detection data
            nand_page->section.header.data_len = (*buf_index / 4);

        }
#endif
        else if(data_type == SH_DATA_TYPE_IMU1_LSM6DSL_MODE_8)
        {
            //IMU data
            nand_page->section.header.data_len = 792;
        }
        else if(data_type == SH_DATA_TYPE_BAROMTER)
        {
            //Barometer data
            nand_page->section.header.data_len = (*buf_index / 4);
        }

#ifdef CONFIG_SEGNO_WEAR_DETECT_PROGRAM
        else if(data_type == SH_DATA_TYPE_WEAR_DETECTION)
        {
            //Wear detection
            LOG_INF("Wear data index data size is : %d", *buf_index);
            nand_page->section.header.data_len = (*buf_index / 36);
        }
#endif

#ifdef CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM
        else if(data_type == SH_DATA_TYPE_ACTIVITY_INDEX)
        {
            //Activity index detection
            LOG_INF("Activity index data size is : %d", *buf_index);
            nand_page->section.header.data_len = (*buf_index / 4);
        }
#endif
        else
        {
            //Temperature data
            nand_page->section.header.data_len = *buf_index / 2;
        }
        
        nand_page->section.header.flag = flag;
        page_crc32 = sh_utils::crc32(nand_page->section.body, SH_DL_BODY_BYTE_SIZE, &page_crc32);
        LOG_DBG("CRC: %u", page_crc32); 
        nand_page->section.pagecrc = page_crc32;
        LOG_DBG("Writing data type: %d into to flash, Time %d",data_type, time_stamp/8192);
        LOG_DBG("Header data type:%d", nand_page->section.header.data_type);
        LOG_DBG("Header timestamp:%d", nand_page->section.header.timestamp_ms);
        LOG_DBG("Header Data length:%d", nand_page->section.header.data_len);
        LOG_DBG("Header flag:%d", nand_page->section.header.flag);

        //Now store Data into SHRD format
        rc = sh_shrd_header::write_data(nand_page, false);

        //Now clear the NAND page buffer
        memset(nand_page, 0, sizeof(sh_shrd_data_t));
        *buf_index = 0;

        memset(&ses_info, 0, sizeof(ses_info));
        rc = sh_data_logger::get_session_info_status(&ses_info);
        if(rc != SH_DL_SUCCESS) {
            LOG_ERR("Couldn't get session information");
            adv_data.memory_percentage = 49;    //test
        }
        else{
            adv_data.memory_percentage = ses_info.memory_percentage;
        }
        sh_ble::update_advertising_data(sh_ble::BLE_ADV_MEMORY_PERCENTAGE, adv_data);
    }
    return rc;
}

static void mem_mgr_set_device_info(void)
{
    struct ptxSystemStatus ptx_status;
    uint8_t battery_level = 0;
    uint8_t devicename[CONFIG_BT_DEVICE_NAME_MAX];
    uint8_t system_status = 0;

    memset((void*)&ptx_status, 0, sizeof(ptx_status));
    memset(devicename, 0, sizeof(devicename));

    ptx_status.VddBat = powermngr::get_battery_voltage();
    battery_level = powermngr::determine_power_level(ptx_status.VddBat);

    //Get Device name
    sh_ble::get_device_name(devicename);
    
    //Now update the device information in SHRD
    fault_handler::get_diagnostics(&system_status);

    sh_shrd_header::set_device_info(system_status, battery_level, (const char *)devicename, 
        (const char*)sh_factory_config::get_hw_version(), CONFIG_BT_DIS_FW_REV_STR);
}

static ret_code_t mem_mgr_subscribe_all_sensor_channel(void)
{
    ret_code_t ret = 0;
    uint8_t retry = MAX_RETRY_CNT;
    do
    {
        #if defined(CONFIG_AS6221)
            if (g_shrd_config_data.temp)
            {
                // Configure zbus observers for temperature channel
                ret = zbus_chan_add_obs(&temp_chan, &mem_temp_sub, K_MSEC(200));
                if (ret)
                {
                    LOG_ERR("Failed to add thread as zbus observer to temp chan (err: %d), (retry :%d)", ret, retry);
                }
                else
                {
                    break;
                }
            }
        #endif /*CONFIG_AS6221*/
    }while(retry--);

    retry = MAX_RETRY_CNT;

    do
    {
        #if defined(CONFIG_SH_BMP5)
            if (g_shrd_config_data.baro)
            {
                ret = zbus_chan_add_obs(&bmp5_chan, &mem_barometer_sub, K_MSEC(200));
                if (ret)
                {
                    LOG_ERR("Failed to add thread as zbus observer to barometer chan (err: %d), (retry :%d)", ret, retry);
                }
                else
                {
                    break;
                }
            }
        #endif // (CONFIG_SH_BMP5)
    }while(retry--);

    retry = MAX_RETRY_CNT;

    do
    {
        #if defined(CONFIG_BMA456)
            if (g_shrd_config_data.imu)
            {
                ret = zbus_chan_add_obs(&imu_chan, &mem_imu_data_sub, K_MSEC(200));
                if (ret)
                {
                    LOG_ERR("Failed to add thread as zbus observer to imu chan (err: %d), (retry :%d)", ret, retry);
                }
                else
                {
                    break;
                }
            }
        #endif // (CONFIG_BMA456)
    }while(retry--);
    
    retry = MAX_RETRY_CNT;

    do
    {
        #if defined(CONFIG_SH_BMI323)
            if (g_shrd_config_data.imu)
            {
                ret = zbus_chan_add_obs(&imu_chan, &mem_imu_data_sub, K_MSEC(200));
                if (ret)
                {
                    LOG_ERR("Failed to add thread as zbus observer to imu chan (err: %d), (retry :%d)", ret, retry);
                }
                else
                {
                    break;
                }
            }
        #endif // (CONFIG_SH_BMI323)
    }while(retry--);

    retry = MAX_RETRY_CNT;

    do
    {
        #if defined(CONFIG_SH_BMI323_STEP_COUNT_ENABLE)
            if (g_shrd_config_data.steps)
            {
                ret = zbus_chan_add_obs(&imu_step_chan, &mem_imu_step_data_sub, K_MSEC(200));
                if (ret)
                {
                    LOG_ERR("Failed to add thread as zbus observer to imu step chan (err: %d), (retry :%d)", ret, retry);
                }
                else
                {
                    break;
                }
            }
        #endif /* CONFIG_SH_BMI323_STEP_COUNT_ENABLE */
    }while(retry--);

    retry = MAX_RETRY_CNT;

    do
    {
        #if defined(CONFIG_MICROPHONE)
            if (g_shrd_config_data.mic)
            {
                ret = zbus_chan_add_obs(&microphone_chan, &mem_mic_data_sub, K_MSEC(200));
                if (ret)
                {
                    LOG_ERR("Failed to add thread as zbus observer to mic chan (err: %d), (retry :%d)", ret, retry);
                }
                else
                {
                    break;
                }
            }
        #endif
    }while(retry--);

    retry = MAX_RETRY_CNT;

    do
    {
        ret = zbus_chan_add_obs(&evt_chan, &mem_event_sub, K_MSEC(200));
        if (ret)
        {
            LOG_ERR("Failed to add thread as zbus observer to event chan (err: %d), (retry :%d)", ret, retry);
        }
        else
        {
            break;
        }
    }while(retry--);

    retry = MAX_RETRY_CNT;

    do
    {
        #if ((CONFIG_SEGNO_SCRATCH_PROGRAM) || (CONFIG_SEGNO_WEAR_DETECT_PROGRAM) || \
        (CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM) || (CONFIG_SEGNO_ANGLE_DETECT_PROGRAM))
        if((g_shrd_config_data.activity) || (g_shrd_config_data.arm_angle) || (g_shrd_config_data.imu_aggregated) || (g_shrd_config_data.scratch))
        { 
            ret = zbus_chan_add_obs(&segno_chan, &mem_segno_sub, K_MSEC(200));
            if (ret)
            {
                LOG_ERR("Failed to add thread as zbus observer to scratch chan (err: %d), (retry :%d)", ret, retry);
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
        #endif
    }while(retry--);

    return ret;
}

static ret_code_t mem_mgr_unsubscribe_all_sensor_channel(void)
{
    ret_code_t ret = 0;
    uint8_t retry = MAX_RETRY_CNT;
   
    do
    {
        #if defined(CONFIG_AS6221)
            if (g_shrd_config_data.temp)
            {
                //Configure zbus observers for temperature channel
                ret = zbus_chan_rm_obs(&temp_chan, &mem_temp_sub, K_MSEC(200));
                if (ret)
                {
                    LOG_ERR("Failed to remove thread as zbus observer to temp chan (err: %d)", ret);
                }
                else
                {
                    break;
                }
            }
        #endif /*CONFIG_AS6221*/
    }while(retry--);

    retry = MAX_RETRY_CNT;

    do
    {
        #if defined(CONFIG_SH_BMP5)
            if (g_shrd_config_data.baro)
            {
                ret = zbus_chan_rm_obs(&bmp5_chan, &mem_barometer_sub, K_MSEC(200));
                if (ret)
                {
                    LOG_ERR("Failed to remove thread as zbus observer to barometer chan (err: %d)", ret);
                    return ret;
                }
                else
                {
                    break;
                }
            }
        #endif // (CONFIG_SH_BMP5)
    }while(retry--);

    retry = MAX_RETRY_CNT;

    do
    {
        #if defined(CONFIG_BMA456)
            if (g_shrd_config_data.imu)
            {
                ret = zbus_chan_rm_obs(&imu_chan, &mem_imu_data_sub, K_MSEC(200));
                if (ret)
                {
                    LOG_ERR("Failed to remove thread as zbus observer to imu chan (err: %d)", ret);
                }
                else
                {
                    break;
                }
            }
        #endif // (CONFIG_BMA456)
    }while(retry--);

    retry = MAX_RETRY_CNT;

    do
    {
        #if defined(CONFIG_SH_BMI323)
            if (g_shrd_config_data.imu)
            {
                ret = zbus_chan_rm_obs(&imu_chan, &mem_imu_data_sub, K_MSEC(200));
                if (ret)
                {
                    LOG_ERR("Failed to add thread as zbus observer to imu chan (err: %d)", ret);
                }
                else
                {
                    break;
                }
            }
        #endif // (CONFIG_SH_BMI323)
    }while(retry--);

    retry = MAX_RETRY_CNT;

    do
    {
        #if defined(CONFIG_MICROPHONE)
            if (g_shrd_config_data.mic)
            {
                ret = zbus_chan_rm_obs(&microphone_chan, &mem_mic_data_sub, K_MSEC(200));
                if (ret)
                {
                    LOG_ERR("Failed to remove thread as zbus observer to mic chan (err: %d)", ret);
                    return ret;
                }
                else
                {
                    break;
                }
            }
        #endif
    }while(retry--);

    retry = MAX_RETRY_CNT;

    do
    {
        ret = zbus_chan_rm_obs(&evt_chan, &mem_event_sub, K_MSEC(200));
        if (ret)
        {
            LOG_ERR("Failed to remove thread as zbus observer to event chan (err: %d)", ret);
            return ret;
        }
        else
        {
            break;
        }
    }while(retry--);

    retry = MAX_RETRY_CNT;

    do
    {
        #if defined(CONFIG_SH_BMI323_STEP_COUNT_ENABLE)
            if (g_shrd_config_data.steps)
            {
                ret = zbus_chan_rm_obs(&imu_step_chan, &mem_imu_step_data_sub, K_MSEC(200));
                if (ret)
                {
                    LOG_ERR("Failed to remove thread as zbus observer to imu step chan (err: %d)", ret);
                }
                else
                {
                    break;
                }
            }
        #endif /* CONFIG_SH_BMI323_STEP_COUNT_ENABLE */
    }while(retry--);

    retry = MAX_RETRY_CNT;

    do
    {
        #if ((CONFIG_SEGNO_SCRATCH_PROGRAM) || (CONFIG_SEGNO_WEAR_DETECT_PROGRAM) || \
        (CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM) || (CONFIG_SEGNO_ANGLE_DETECT_PROGRAM))
        if((g_shrd_config_data.activity) || (g_shrd_config_data.arm_angle) || (g_shrd_config_data.imu_aggregated) || (g_shrd_config_data.scratch))
        { 
            ret = zbus_chan_rm_obs(&segno_chan, &mem_segno_sub, K_MSEC(200));
            if (ret)
            {
                LOG_ERR("Failed to remove thread as zbus observer to event chan (err: %d)", ret);
                break;
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
        #endif
    }while(retry--);
    return ret;
}

/** 
 * @brief Initializes the memory manager thread and related functionalities.
 *
 * This function initializes the Littlefs file system and performs
 * various tests based on the defined configurations. It creates the
 * memory manager thread and sets up Zbus observers for different channels
 * such as command, temperature, IMU, and ECG channels, enabling the thread
 * to receive data from these channels.
 *
 * @return uint32_t Success indicates 0, while failure is non-zero.
 */
uint32_t sh_mem_mngr::thread_init(void)
{
    uint32_t ret = 0;
    static k_tid_t mem_mgr_tid;
    static struct k_thread mem_mgr_thread;
    sh_ble::adv_data_t adv_session;

#if defined(CONFIG_CODA_HEADER)
    coda_init();
#endif

#if defined(CONFIG_SHRD_HEADER)
    mem_mgr_shrd_init();
    sh_shrd_config::get_shrd_conf(&g_shrd_config_data);
    sh_shrd_config::print_config_data();
#endif

#if defined(CONFIG_TEST_LITTLEFS)
    ret = sh_littlefs::init();
    if (ret < 0)
    {
        LOG_ERR("Littlefs file system init failed (err %d)\n", ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Littlefs file system init failed (err %d)\n", ret);
#endif 
    }
#endif

#if defined(CONFIG_TEST_FLASH)
    ret = sh_littlefs::flash_functional_test((char *)Flash_Testbuffer,
                                             strlen((char *)Flash_Testbuffer), 2045, 4096);
    if (ret < 0)
    {
        LOG_ERR("Flash read write failed (err %d)\n", ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Flash read write failed (err %d)\n", ret);
#endif 
    }
#endif

#if defined(CONFIG_DATA_LOGGER)
    ret = sh_data_logger::init();
    if (ret != 0) {
    	LOG_ERR("Data logger module init failed(err %d)\n", ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Data logger module init failed(err %d)\n", ret);
#endif 
        fault_handler::send_diagnostics(NAND_FLASH_FAULT);
        state.mem_mgr_module_state = MEM_MGR_FAILURE_STATE;
    }

    ret_code_t rc = sh_data_logger::get_session_mode((session_mode_type_t *)&(adv_session.session_mode));
    if(rc != SH_DL_SUCCESS) {
        LOG_ERR("Not able to get current session mode");
    }
    sh_ble::update_advertising_data(sh_ble::BLE_ADV_SESSION_MODE, adv_session);

    // Initialize the work queue
    k_work_queue_start(&(evt_queue.my_workqueue), workqueue_stack_area, K_THREAD_STACK_SIZEOF(workqueue_stack_area), WORKQUEUE_PRIORITY, NULL);

    // Initialize the workqueue for update the session
    k_work_init(&(evt_queue.update_session_timer_work), update_session_handler);

    // Initialize the workqueue for download
    k_work_init(&(evt_queue.download_work), download_work_handler);
#endif
    // Create the memory manager thread
    mem_mgr_tid = k_thread_create(&mem_mgr_thread, mem_mngr_stack,
                                  K_THREAD_STACK_SIZEOF(mem_mngr_stack),
                                  mem_mgr_thread_func, NULL, NULL, NULL,
                                  7, K_USER, K_NO_WAIT);

    k_thread_name_set(mem_mgr_tid, "memory manager");
                                  

    // Add thread as a zbus observer to cmd channel
    ret = zbus_chan_add_obs(&mem_mngr_dl_chan, &mem_mngr_dl_sub, K_MSEC(200));
    if(ret){
        LOG_ERR("Failed to add thread as zbus observer to cmd chan (err: %d)", ret);
    }

    return ret;
}

int sh_mem_mngr::store_events_to_shrd(sh_shrd_evt_type *event)
{
    int64_t time_stamp_evt = 0;
    sh_mem_mngr::msg_t shrd_message;
    memset(&shrd_message, 0, sizeof(shrd_message));
    if(event == NULL)
    {
        return -EINVAL;
    }

    //Get timestamp of event operation
    if (app_time::get_time_ticks(TimeResolution::MILLISECOND_TICKS_8192, &(time_stamp_evt)) < 0) 
    {
        LOG_ERR("Failed to get time for haptic start event");
        return -EINVAL;
    }
    shrd_message.type = *event;
    shrd_message.timestamp_storage = time_stamp_evt;

    // Publish enent timestamp to memory manager thread and time out is 5 seconds
    int ret = zbus_chan_pub(&evt_chan, &shrd_message, K_MSEC(5000));
    if(ret != 0){
        LOG_ERR("Failed to publish event data [%d]",ret);
    }
    k_sem_give(&mem_mngr_sem);
    return ret;
}

#endif
