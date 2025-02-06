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

// Zephyr modules 
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

// Driver files
#include <dl_service/ble_dl.h>
#include <cmd_service/ble_cmd.h>
#include <ble_base/ble.h>
#include <sh/sh_time.h>
#include "session_thread.h"
#include "mem_mgr_thread.h"
#include "haptic_motor_handler.h"
#include <sh/sh_ptx30.h>
#include "ptx30_thread.h"
#include "barometer_thread.h"
#include "imu_sensor_thread.h"
#include "microphone_thread.h"
#include "temp_sensor_thread.h"

#ifdef CONFIG_SEGNO_LIBRARY
#include "segno_thread.h"
#endif
#include "shrd_config.h"

#define DEAD_BAT_SAMPLE_CNT     5

LOG_MODULE_REGISTER(session_thread, LOG_LEVEL_DBG);

/* Define ZBUS channel for BLE Session message data handling */
ZBUS_CHAN_DEFINE(ble_session_cmd_chan, session_msg_t, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(0));

/* Define ZBUS channel for sending session data to memory manager */
ZBUS_CHAN_DEFINE(mem_mngr_chan, session_msg_t, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(0));

/* Define subscriber to BLE session channel */ 
ZBUS_MSG_SUBSCRIBER_DEFINE(session_sub);

/* Define subscriber to BLE session channel */ 
ZBUS_SUBSCRIBER_DEFINE(session_evt_sub, 4);

/* Define subscriber to Memory manager channel */ 
ZBUS_SUBSCRIBER_DEFINE(mem_mngr_sub, 10);

/* Define subscriber to the BLE command channel */
ZBUS_MSG_SUBSCRIBER_DEFINE(command_sub);

#if defined(CONFIG_PTX30)
/* Define subscriber to PTX ZBUS channel */ 
ZBUS_MSG_SUBSCRIBER_DEFINE(charger_evt_subscriber);
#endif  //CONFIG_PTX30

K_SEM_DEFINE(dl_session_sem, 0, 10);


typedef enum {
    SESSION_STATE_IDLE      = 0x00,     // Idle state
    SESSION_STATE_STARTING  = 0x01,     // Prepare system to start the session
    SESSION_STATE_START     = 0x02,     // Start session in memory manager thread 
    SESSION_STATE_STOPING   = 0x03,     // Prepare system to stop the session
    SESSION_STATE_STOP      = 0x04,     // Stop session in memory manager thread
    SESSION_STATE_DOWNLOAD  = 0x05,     // Start session data download
    SESSION_STATE_LOW_PWR   = 0x06,     // Handle low power states, turn off threads and BLE 
    SESSION_STATE_IDLE_PWR  = 0x07,     // Handle low power states, turn on BLE and advertise
    SESSION_STATE_DEAD_PWR  = 0x08,     // Turn off all things and enter into a ship mode
}session_ctrl_states_t;

/** 
 * @brief Structure contains the session thread states and opeations
 * 
 * bmp5_state : Barometer thread state machine
*/
typedef struct {
    session_ctrl_states_t session_state;
    session_ctrl_states_t pmic_state;
    uint8_t ded_bat_hys;
}session_t;

/* Default states */
session_t session_st = {
    .session_state = SESSION_STATE_IDLE,
    .pmic_state = SESSION_STATE_IDLE_PWR,
    .ded_bat_hys = 0,
};

//variable is update when other threads are starting or stoping
static bool session_cur_state = false;  

/** 
 * @brief get current session state
 *
 * This function return the current session state
 */
bool sh_session_thread::get_current_session_state()
{
    return session_cur_state;
}

/** 
 * @brief Start Stop all sensor threads 
 *
 * This function resumes all suspended sensor threads that re-initialize the sensor 
 * and start the sensor measurement and then will send to memory manager for storage
 * 
 * @param start_session: Start/Stop session identification flag
 */
int start_stop_all_sessions(bool start_session) 
{
    int8_t ret = 0;

    session_cur_state = start_session;

    if(start_session) {

#if defined (CONFIG_SEGNO_LIBRARY)
        sh_segno_thread::start_sensing();
        LOG_INF("started the segno ");
#endif // CONFIG_SEGNO_LIBRARY

#if defined(CONFIG_SH_BMP5)
        ret = sh_barometer_thread::start_sensing();
        if(ret < 0) {
            LOG_ERR("Failed to start the Barometer session : %d", ret);
        }
        else{
            LOG_INF("started the barometer ");
        }
#endif
      
#if defined (CONFIG_AS6221)
        ret = sh_temp_sensor::start_sensing();
        if(ret < 0) {
            LOG_ERR("Failed to start the Temperature session : %d", ret);
        }
        else{
            LOG_INF("started the temperature ");
        }
#endif 

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
        ret = sh_imu_sensor::start_sensing();
        if(ret < 0) {
            LOG_ERR("Failed to start the IMU session : %d", ret);
        }
        else{
            LOG_INF("started the IMU ");
        }
#endif

#if defined(CONFIG_MICROPHONE)
        ret = sh_microphone::start_sensing();
        if(ret < 0) {
            LOG_ERR("Failed to start the Microphone session : %d", ret);
        }
        else{
            LOG_INF("Started MIC Thread");
        }
#endif

    } 
    else {
        unsigned int key;
        key = irq_lock();
        k_sched_lock();

        ret = ptx30w::handle_pmic_interrupt(false);
        if(ret < 0) {
            LOG_ERR("Failed to disable PTX interrupt : %d", ret);
        } else{
            LOG_INF("PTX interrupt disabled");
        }

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
        ret = sh_imu_sensor::stop_sensing();
        if(ret < 0) {
            LOG_ERR("Failed to stop the IMU session : %d", ret);
        } else{
            LOG_INF("stopped the IMU session success");
        }
#endif 

#if defined (CONFIG_SEGNO_LIBRARY)
        sh_segno_thread::stop_sensing();
        LOG_INF("stopped the segno thread");
#endif // CONFIG_SEGNO_LIBRARY

#if defined(CONFIG_MICROPHONE)
        ret = sh_microphone::stop_sensing();
        if(ret < 0) {
            LOG_ERR("Failed to stop the Microphone session : %d", ret);
        } else {
            LOG_INF("stopped the microphone session success");
        }
#endif

#if defined(CONFIG_SH_BMP5)
        ret = sh_barometer_thread::stop_sensing();
        LOG_INF("result code for barometer stop :: %d", ret);
        if(ret < 0) {
            LOG_ERR("Failed to stop the Barometer session : %d", ret);
        } else {
            LOG_INF("stopped the barometer session success");
        }
#endif

#if defined (CONFIG_AS6221)
        ret = sh_temp_sensor::stop_sensing();
        if(ret < 0) {
            LOG_ERR("Failed to stop the Temperature session : %d", ret);
        }else {
            LOG_INF("stopped the temperature session success");
        }
#endif 

    ret = ptx30w::handle_pmic_interrupt(true);
    if(ret < 0) {
        LOG_ERR("Failed to enable PTX interrupt : %d", ret);
    } else{
        LOG_INF("PTX interrupt enabled");
    }
    
    k_sched_unlock();
    irq_unlock(key);

    }

    return ret;
}


#if defined(CONFIG_DATA_LOGGER)
static uint8_t get_current_session_mode()
{
    session_mode_type_t session_mode = SH_SESSION_MODE_BLE_MANUAL;
    
    // Check the last session mode
    ret_code_t rc = sh_data_logger::get_session_mode((session_mode_type_t *)&session_mode);
    if(rc != SH_DL_SUCCESS){
        LOG_ERR("Not able to get current session mode");
    } 

    return session_mode;
}
#endif

static int store_haptic_event_to_shrd(void)
{
    uint8_t haptic_enable = 0;
    uint16_t duration = 0;
    uint16_t interval = 0;
    sh_shrd_evt_type event = HAPTIC_FEATURE_ENABLE_EVT;
    
    sh_data_logger::get_haptic_enable_status(&haptic_enable, &duration, &interval);
    if(!haptic_enable)
    {
        event = HAPTIC_FEATURE_DISABLE_EVT;
    }

    int ret = sh_mem_mngr::store_events_to_shrd(&event);
    if(ret) {
        LOG_ERR("Failed to store haptic feature events : %d", ret);
    }
    
    return ret;
}

/** 
 * @brief Temperature sensor thread module.
 *
 * This function handles temperature sensor operations 
 * and data publishing over zbus.
 */
void session_thread_func(void *arg1, void *arg2, void *arg3)
{
    int32_t ret;
    const struct zbus_channel *chan;

#if defined(CONFIG_PTX30)
    ptxSystemStatus_t pmic_data; 
    memset(&pmic_data, 0, sizeof(ptxSystemStatus_t));
#endif

    int8_t last_charger_status = -1;
    uint8_t time_sync_pending_state = 0;
    state_t memory_state = MEM_MGR_IDLE_STATE;
    cmd_msg_t network_msg;
    memset(&network_msg, 0, sizeof(cmd_msg_t));
    session_msg_t session_msg;
    memset(&session_msg, 0, sizeof(session_msg_t));
    /* This time is for reference */
    int64_t session_epoch_timestamp = 0;
    
    while(1)
    {
        ret = k_sem_take(&dl_session_sem, K_FOREVER);
        if (!ret) 
        {
            if (!zbus_sub_wait_msg(&command_sub, &chan, &network_msg, K_NO_WAIT))
            {
                LOG_INF("BLE network command received = %d", network_msg.cmd_type);
                LOG_INF("BLE network command length = %d", network_msg.data_len);

                if(network_msg.cmd_type == CMD_SESSION_MODE_COMMANDS)
                {
                    //If existing status is start then stop the session
                    LOG_INF("Now change to %d mode", network_msg.data[0]);
                
                #if defined(CONFIG_DATA_LOGGER)
                    ret = sh_mem_mngr::update_mode((session_mode_type_t)network_msg.data[0]);
                    if(ret != SH_DL_SUCCESS) {
                        LOG_ERR("Not able to switch the mode");
                    } 
                #endif
                
                }
            }
        #if defined(CONFIG_PTX30)
            if (!zbus_sub_wait_msg(&charger_evt_subscriber, &chan, &pmic_data, K_NO_WAIT))
            {
                LOG_INF("charger_evt_zbus receive");
                if(pmic_data.VddBat <= DEAD_BATTERY_THRESHOLD) {
                    /* In case of dead battery turn off all things and enter into a ship mode */
                    session_st.session_state = SESSION_STATE_DEAD_PWR;
                } else {
                    sh_mem_mngr::get_memory_manager_state(&memory_state);

                    LOG_INF("LCS %d ,new CS %d, mem %d", last_charger_status, pmic_data.RfFieldDetected, memory_state);

                    if((last_charger_status != pmic_data.RfFieldDetected) && (memory_state != MEM_MGR_DOWNLOAD_STATE) && time_sync_pending_state)
                    {
                        if(pmic_data.RfFieldDetected)
                        {
                        
                        #if defined(CONFIG_DATA_LOGGER)
                            LOG_INF("***Charger connect event now stop the session***");
                            if(get_current_session_mode() == SH_SESSION_MODE_AUTOMATIC) {
                                session_msg.cmd_type = SESSION_STOP;
                                session_st.session_state = SESSION_STATE_STOPING;
                            }
                        #endif
                        
                        }
                        else
                        {
                        
                        #if defined(CONFIG_DATA_LOGGER)
                            LOG_INF("***Charger disconnect event now start the session***");
                            if(get_current_session_mode() == SH_SESSION_MODE_AUTOMATIC) {
                                session_msg.cmd_type = SESSION_START;
                                session_st.session_state = SESSION_STATE_STARTING;
                            }
                        #endif
                        
                        }
                        last_charger_status = pmic_data.RfFieldDetected;
                    }
                }
            }
        #endif

            else if (!zbus_sub_wait_msg(&session_sub, &chan, &session_msg, K_NO_WAIT))
            {
                LOG_INF("BLE session command received : %d", session_msg.cmd_type);
                LOG_INF("BLE session command len : %d", session_msg.data_len);

                for(int i=0; i<session_msg.data_len; i++) {
                    LOG_DBG("0x%02x\n", session_msg.data[i]);
                }

                /* If Dead battery is detected or currently streaming then do not start the session */
                if(session_st.session_state != SESSION_STATE_DEAD_PWR ) {
                    switch (session_msg.cmd_type)
                    {
                    case SESSION_START:
                    #if (CONFIG_MIC_STREAMING || CONFIG_BAROMETER_STREAMING || CONFIG_IMU_STREAMING)
                        //Don't allow start session when streaming for now
                        if (!sh_ble::is_streaming_waveform_data()) {
                            LOG_DBG("Received Start session request from BLE");
                            session_st.session_state = SESSION_STATE_STARTING;
                        }
                        else {
                            LOG_INF("Streaming is ongoing, can't start session");
                        }
                        break;
                    #else
                        LOG_DBG("Received Start session request from BLE");
                        session_st.session_state = SESSION_STATE_STARTING;
                        break;
                    #endif

                    case SESSION_STOP:
                        LOG_DBG("Received Stop session request from BLE");
                        session_st.session_state = SESSION_STATE_STOPING;
                        break;
                    
                    case SESSION_EPOCH_TIME:
                    case SESSION_DOWNLOAD:
                    case SESSION_DOWNLOAD_ACK:
                    case SESSION_DOWNLOAD_CANCEL:
                    case SESSION_REQ_INFO:
                    case SESSION_RESET_MEM:
                        if(session_msg.cmd_type == SESSION_EPOCH_TIME) {
                            session_epoch_timestamp = session_msg.data[1] | (session_msg.data[2] << 8) | (session_msg.data[3] << 16) | (session_msg.data[4] << 24);
                            #if defined(CONFIG_SH_TIME_MODULE)
                            app_time::set_epoch_time((int64_t *)&session_epoch_timestamp);
                            sh_ble::state next_state = sh_ble::CONNECTED;
                            //Update BLE status as connected state
                            sh_ble::set_state(&next_state);
                            #endif

                            ret = zbus_chan_pub(&mem_mngr_dl_chan, &session_msg, K_NO_WAIT);
                            if(ret != 0) {
                                LOG_ERR("Failed to publish Memory manager data = %d", ret);
                            }
                            k_sem_give(&mem_mngr_sem);
                            session_st.session_state = SESSION_STATE_IDLE;


                            //Start the first session if we are getting time sync request from mobile
                            if(!time_sync_pending_state)
                            {
                                sh_mem_mngr::get_memory_manager_state(&memory_state);

                                LOG_INF("LCS %d ,new CS %d, mem %d", last_charger_status, pmic_data.RfFieldDetected, memory_state);

                                if((last_charger_status != pmic_data.RfFieldDetected) && (memory_state != MEM_MGR_DOWNLOAD_STATE))
                                {
                                    if(pmic_data.RfFieldDetected)
                                    {
                                    #if defined(CONFIG_DATA_LOGGER)
                                        LOG_INF("***Charger connect event now stop the session***");
                                        if(get_current_session_mode() == SH_SESSION_MODE_AUTOMATIC) {
                                            session_msg.cmd_type = SESSION_STOP;
                                            session_st.session_state = SESSION_STATE_STOPING;
                                        }
                                    #endif
                                    }
                                    else
                                    {
                                    #if defined(CONFIG_DATA_LOGGER)
                                        LOG_INF("***Charger disconnect event now start the session***");
                                        if(get_current_session_mode() == SH_SESSION_MODE_AUTOMATIC) {
                                            session_msg.cmd_type = SESSION_START;
                                            session_st.session_state = SESSION_STATE_STARTING;
                                        }
                                    #endif
                                    }
                                    last_charger_status = pmic_data.RfFieldDetected;
                                }
                                time_sync_pending_state = 1;
                            }
                        }
                        else
                        {
                            ret = zbus_chan_pub(&mem_mngr_dl_chan, &session_msg, K_NO_WAIT);
                            if(ret != 0) {
                                LOG_ERR("Failed to publish Memory manager data = %d", ret);
                            }
                            k_sem_give(&mem_mngr_sem);
                            session_st.session_state = SESSION_STATE_IDLE;
                        } 
                        break;
                    
                    default:
                        break;
                    }
                }
            }

            else if(!zbus_sub_wait(&mem_mngr_sub, &chan, K_NO_WAIT)) {

                memset(&session_msg, 0, sizeof(session_msg_t));
                ret = zbus_chan_read(chan, &session_msg, K_MSEC(200));
                if(ret){
                    LOG_ERR("Failed to read channel [%d]",ret);
                }
                else{
                    LOG_DBG("Notification command received : %d", session_msg.cmd_type);
                    LOG_DBG("Notification command len : %d", session_msg.data_len);

                    for(int i=0; i<session_msg.data_len; i++) {
                        LOG_DBG("0x%02x\n", session_msg.data[i]);
                    }
                    
                     /* If Dead battery is detected then do not start the session */
                    if(session_st.session_state != SESSION_STATE_DEAD_PWR) {
                        /* Notify the App with session data response */
                        switch (session_msg.cmd_type)
                        {
                        case SESSION_IDLE:    
                        case SESSION_START:
                        case SESSION_STOP:
                            /* code */
                            break;

                        case SESSION_REQ_INFO:
                            if(session_msg.data_len > 1) {
                                LOG_INF("Sending Session Info");
                                ret = sh_ble_dl_info_send(session_msg.data, session_msg.data_len);
                                if(ret < 0) {
                                    LOG_ERR("Failed to notify the data [%d]",ret);
                                }
                            }
                            
                            break;

                        case SESSION_DOWNLOAD:
                        case SESSION_DOWNLOAD_ACK:
                            session_st.session_state = SESSION_STATE_DOWNLOAD;

                        default:
                            break;
                        }  
                    }          
                }
            }


            switch (session_st.session_state)
            {
            case SESSION_STATE_IDLE:
                /* code */
                break;

            case SESSION_STATE_STARTING:
            case SESSION_STATE_START:
                {
                    shrd_config_t config;
                    memset(&config, 0, sizeof(config));
                    if(session_st.session_state == SESSION_STATE_STARTING) {
                        LOG_INF("state starting");
                        sh_shrd_config::get_shrd_conf(&config);

                        // Publish data to Memory manager thread ove zbus channel
                        ret = zbus_chan_pub(&mem_mngr_dl_chan, &session_msg, K_NO_WAIT);
                        if(ret != 0) {
                            LOG_ERR("Failed to publish Memory manager data = %d", ret);
                        }
                        k_sem_give(&mem_mngr_sem);
                        LOG_INF("state starting sending");
                        
                        start_stop_all_sessions(true);
                        LOG_INF("session start all");
                        session_st.session_state = SESSION_STATE_START;
                        session_st.pmic_state = SESSION_STATE_START;
                        
                    #ifdef CONFIG_HAPTIC_SWALLOW_DETECT_PROGRAM
                        if((config.scratch == 0))
                        {
                            //Swallow profile is selected hence not init the swallow profile
                            LOG_ERR("***Swallow profile is selected***\r\n");
                            ret = sh_haptic_motor_handler::operation(sh_haptic_motor_handler::type_t::SWALLOW, NULL);
                            if(ret != 0) 
                            {
                                LOG_ERR("Failed to start haptic motor = %d", ret);

                                //stop the session if haptic fails
                                session_msg.cmd_type = SESSION_STOP;

                                ret = zbus_chan_pub(&mem_mngr_dl_chan, &session_msg, K_NO_WAIT);
                                if(ret != 0) {
                                    LOG_ERR("Failed to publish Memory manager data = %d", ret);
                                }
                                k_sem_give(&mem_mngr_sem);

                                start_stop_all_sessions(false);
                                session_st.session_state = SESSION_STATE_IDLE;
                                session_st.pmic_state = SESSION_STATE_STOP;
                            }
                        }
                    #endif /* CONFIG_HAPTIC_SWALLOW_DETECT_PROGRAM */ 
                        /* Added start up delay for haptic */
                        k_msleep(400);
                        
                        ret = store_haptic_event_to_shrd();
                        if(ret != 0)
                        {
                            LOG_ERR("Not able to store haptic event to SHRD");
                        }
                    }
                }
                break;

            case SESSION_STATE_STOPING:
            case SESSION_STATE_STOP:
                {
                    shrd_config_t config;
                    memset(&config, 0, sizeof(config));
                    if(session_st.session_state == SESSION_STATE_STOPING) {
                        LOG_INF("state stopping");
                        //Now stop the haptic motor
                #ifdef CONFIG_HAPTIC_SWALLOW_DETECT_PROGRAM
                        if((config.scratch == 0))
                        {
                            ret = sh_haptic_motor_handler::operation(sh_haptic_motor_handler::type_t::OFF, NULL);
                            if(ret != 0) {
                                LOG_ERR("Failed to stop haptic motor error = %d", ret);
                            }
                        }
                #endif /* CONFIG_HAPTIC_SWALLOW_DETECT_PROGRAM */ 
                        ret = store_haptic_event_to_shrd();
                        if(ret != 0)
                        {
                            LOG_ERR("Not able to store haptic event to SHRD");
                        }                        
                        LOG_INF("state stopping haptic");
                        start_stop_all_sessions(false);
                        LOG_INF("state stopping all");
                        // Publish data to Memory manager thread ove zbus channel
                        ret = zbus_chan_pub(&mem_mngr_dl_chan, &session_msg, K_NO_WAIT);
                        if(ret != 0) {
                            LOG_ERR("Failed to publish Memory manager data = %d", ret);
                        }
                        k_sem_give(&mem_mngr_sem);

                        session_st.session_state = SESSION_STATE_IDLE;
                        session_st.pmic_state = SESSION_STATE_STOP;
                        LOG_INF("state stoping sending");    
                    }
                    
                }
                break;

            case SESSION_STATE_DOWNLOAD:
                {
                
                #if defined(CONFIG_DATA_LOGGER)
                    if( (session_msg.data[0] != SESSION_DOWNLOAD) && (session_msg.data[0] != SESSION_DOWNLOAD_ACK)){
                        ret = sh_ble_dl_dwnld_send(session_msg.data, session_msg.data_len);
                        if(ret < 0) {
                            LOG_ERR("Failed to notify the data [%d]",ret);
                        }

                        if(session_msg.data[0] == SESSION_END_DOWNLOAD || (session_msg.data[0] == SESSION_DOWNLOAD_CANCEL)) {
                            session_st.session_state = SESSION_STATE_IDLE;
                            session_st.pmic_state = SESSION_STATE_IDLE_PWR;

                            if(pmic_data.RfFieldDetected)
                            {
                                if(get_current_session_mode() == SH_SESSION_MODE_AUTOMATIC) {
                                    LOG_INF("***Charger connect event now stop the session***");
                                    session_msg.cmd_type = SESSION_STOP;
                                    session_st.session_state = SESSION_STATE_STOPING;
                                }
                            }
                            else
                            {
                                if(get_current_session_mode() == SH_SESSION_MODE_AUTOMATIC) {
                                    LOG_INF("***Charger disconnect event now start the session***");
                                    session_msg.cmd_type = SESSION_START;
                                    session_st.session_state = SESSION_STATE_STARTING;
                                }
                            }
                            k_sem_give(&dl_session_sem);
                            last_charger_status = pmic_data.RfFieldDetected;

                        } else {
                            session_st.pmic_state = SESSION_STATE_DOWNLOAD;
                        }
                    }
                #endif
                
                }            
                break;
            
            case SESSION_STATE_LOW_PWR:
                {
                    if(session_st.pmic_state == SESSION_STATE_START) {
                        session_msg.cmd_type = SESSION_STOP;
                        session_msg.data_len = 0;

                        ret = store_haptic_event_to_shrd();
                        if(ret != 0)
                        {
                            LOG_ERR("Not able to store haptic event to SHRD");
                        }
                        
                        ret = zbus_chan_pub(&mem_mngr_dl_chan, &session_msg, K_NO_WAIT);
                        if(ret != 0) {
                            LOG_ERR("Failed to publish Memory manager data = %d", ret);
                        }
                        k_sem_give(&mem_mngr_sem);

                        start_stop_all_sessions(false);
                    } else if(session_st.pmic_state == SESSION_STATE_DOWNLOAD) {
                        // Handle download stop 
                        session_msg.cmd_type = SESSION_DOWNLOAD_CANCEL;
                        session_msg.data_len = 0;
                        
                        ret = zbus_chan_pub(&mem_mngr_dl_chan, &session_msg, K_NO_WAIT);
                        if(ret != 0) {
                            LOG_ERR("Failed to publish Memory manager data = %d", ret);
                        }
                        k_sem_give(&mem_mngr_sem);

                        start_stop_all_sessions(false);
                    } 
                    session_st.session_state = SESSION_STATE_IDLE;
                    session_st.pmic_state = SESSION_STATE_IDLE_PWR;
                }
                break;

            case SESSION_STATE_IDLE_PWR :
                break;

            case SESSION_STATE_DEAD_PWR :
                {
                    int ret = 0;
                    if(session_st.pmic_state == SESSION_STATE_START) {
                        session_msg.cmd_type = SESSION_STOP;
                        session_msg.data_len = 0;

                        ret = store_haptic_event_to_shrd();
                        if(ret != 0)
                        {
                            LOG_ERR("Not able to store haptic event to SHRD");
                        }
                        
                        ret = zbus_chan_pub(&mem_mngr_dl_chan, &session_msg, K_NO_WAIT);
                        if(ret != 0) {
                            LOG_ERR("Failed to publish Memory manager data = %d", ret);
                        }
                        k_sem_give(&mem_mngr_sem);

                        start_stop_all_sessions(false);
                    } else if(session_st.pmic_state == SESSION_STATE_DOWNLOAD) {
                        // Handle download stop 
                        session_msg.cmd_type = SESSION_DOWNLOAD_CANCEL;
                        session_msg.data_len = 0;
                        
                        ret = zbus_chan_pub(&mem_mngr_dl_chan, &session_msg, K_NO_WAIT);
                        if(ret != 0) {
                            LOG_ERR("Failed to publish Memory manager data = %d", ret);
                        }
                        k_sem_give(&mem_mngr_sem);

                        start_stop_all_sessions(false);
                    } 

                    session_st.session_state = SESSION_STATE_IDLE;
                    session_st.pmic_state = SESSION_STATE_IDLE_PWR;
                }
                break;

            default:
                break;
            }
        }
    }
}

static k_tid_t session_thread_id;
static struct k_thread session_thread;
K_THREAD_STACK_DEFINE(session_stack, (2048));

/**
 * @brief Initializes the temperature streaming thread 
 * and adds zbus observers for temperature channel.
 *
 * @return uint32_t Success indicates 0 and failure is non-zero.
 */
uint32_t sh_session_thread::init(void)
{
    uint32_t ret = 0;

#if defined(CONFIG_DATA_LOGGER)
    session_mode_type_t session_mode = SH_SESSION_MODE_BLE_MANUAL;
#endif

#if defined(CONFIG_PTX30)
    ptxSystemStatus_t pmic_data; 
    memset(&pmic_data, 0, sizeof(ptxSystemStatus_t));
#endif

    // Add thread as a zbus observer to data logger cmd channel
    ret = zbus_chan_add_obs(&ble_session_cmd_chan, &session_sub, K_MSEC(200));
    if(ret){
        LOG_ERR("Failed to add thread as zbus observer to cmd chan (err: %d)", ret);
    }

    // Add thread as a zbus observer to data logger cmd channel
    ret = zbus_chan_add_obs(&mem_mngr_chan, &mem_mngr_sub, K_MSEC(200));
    if(ret){
        LOG_ERR("Failed to add thread as zbus observer to cmd chan (err: %d)", ret);
    }

#if defined(CONFIG_PTX30)
    // Add thread as a zbus observer to cmd channel
    ret = zbus_chan_add_obs(&ptx30_chan, &charger_evt_subscriber, K_MSEC(200));
    if(ret){
        LOG_ERR("Failed to add charger event thread as zbus observer to cmd chan (err: %d)", ret);
    } 
#endif

    // Add thread as a ZBUS observer to Commands channel
    ret = zbus_chan_add_obs(&cmd_chan, &command_sub, K_MSEC(200));
    if(ret) {
        LOG_ERR("Failed to add thread as ZBUS observer to command chan (err: %d)", ret);
    }

    // Create the session thread
    session_thread_id = k_thread_create(&session_thread, session_stack,
			K_THREAD_STACK_SIZEOF(session_stack),
			session_thread_func, NULL, NULL, NULL,
			4, K_USER, K_NO_WAIT);

    k_thread_name_set(session_thread_id, "session");
    
    #if defined(CONFIG_DATA_LOGGER)
    // Check the last session mode
    ret = sh_data_logger::get_session_mode((session_mode_type_t *)&session_mode);
    if(ret != SH_DL_SUCCESS)
    {
        LOG_ERR("Not able to get current session mode");
    }
    #endif 

    #if defined(CONFIG_PTX30)
    //Get the pmic data
    ret = ptx30w::get_system_status(&pmic_data);
    if(ret != 0)
    {
        LOG_ERR("Not able to get the charging status");
    }
    #endif

    
  

#if defined(CONFIG_DATA_LOGGER)
    // if((session_mode == SH_SESSION_MODE_AUTOMATIC) && (!pmic_data.RfFieldDetected))
    // {
    //     session_msg_t session_msg;
    //     //Start the session in automatic mode
    //     memset(&session_msg, 0, sizeof(session_msg));
    //     session_msg.cmd_type = SESSION_START;
    //     session_msg.data_len = 44;
    //     LOG_INF("Start the session in automatic mode");
        
    //     // Publish data to Memory manager thread ove zbus channel
    //     ret = zbus_chan_pub(&ble_session_cmd_chan, &session_msg, K_NO_WAIT);
    //     if(ret != 0) {
    //         LOG_ERR("Failed to publish Memory manager data = %d", ret);
    //     }
    //     k_sem_give(&dl_session_sem);
    // }
#endif

    return ret;
}
