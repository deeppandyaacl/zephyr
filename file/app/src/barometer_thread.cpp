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

#if defined(CONFIG_SH_BMP5)

// Zephyr modules 
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

// Driver files
#include <sh/sh_bmp5.h>
#include "bmp5/drv_bmp5.h"
#include "barometer_thread.h"
#include "mem_mgr_thread.h"
#include "network_thread.h"
#include <sh/sh_time.h>
#include "memfault/core/trace_event.h"
#ifdef CONFIG_WATCHDOG
#include "watchdog.h"
#endif

#include <fault_handler.h>
#include "barometer_config.h"


LOG_MODULE_REGISTER(barometer_thread, LOG_LEVEL_INF);

K_PIPE_DEFINE(barometer_pipe, sizeof(sh_barometer_thread::msg_t)*2, 4);
/* Define ZBUS channel for sending Pressure data */
ZBUS_CHAN_DEFINE(bmp5_chan, sh_barometer_thread::msg_t, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(0));

typedef enum {
    BMP5_STATE_IDLE     = 0x00,     // Idle state
    BMP5_STATE_MEASURE  = 0x01,     // Temperature Measurement state
    BMP5_STATE_STREAM   = 0x02,     // Streaming temperature data
    BMP5_STATE_STORE    = 0x03,     // Store temperature data
    BMP5_STATE_FAULT    = 0X04      // Fault state      
}bmp5_states_t;

/** 
 * @brief Structure contains the barometer states and opeations
 * 
 * fifo_cnt : FIFO sample count present in buffer 
 * bmp5_state : Barometer thread state machine
*/
typedef struct {
    uint8_t fifo_cnt;
    uint8_t fail_cnt;
    bmp5_states_t bmp5_state;
}bmp5_t;

/* Default states */
bmp5_t bmp5_st = {
    .fifo_cnt = 0,
    .fail_cnt = 0,
    .bmp5_state = BMP5_STATE_MEASURE,
};

/* This time is for reference */
bmp5_user_config_t bmp5_user_config;
sh_barometer_thread::msg_t bmp5_msg;            // Structure to hold ZBUS data
static int64_t baro_time_stamp_storage;
static int64_t baro_time_stamp_streaming;

#ifdef CONFIG_WATCHDOG
/* Watchdog Channel ID for Barometer thread */
int8_t wdt_bmp5_channel_id = WATCHDOG_CHANNEL_DEFAULT_ID;
#endif


static k_tid_t barometer_thread_id;
static struct k_thread barometer_thread;
K_THREAD_STACK_DEFINE(barometer_stack, 512);

#ifdef CONFIG_WATCHDOG
/** 
 * @brief Start Stop watchdog for Barometer threads
 *
 * This function starts the watchdog for thread when sensor has been started to 
 * monitor its health. Once the session has been stopped, watchdog of the threads
 * will be stopped.
 * 
 * @param start_watchdog: Start/Stop watchdog flag
 */
static void start_stop_wdt(bool start_watchdog) 
{
    LOG_INF("BMP5 Receiving start watchdog flag : %d and channel id is : %d", start_watchdog, wdt_bmp5_channel_id);

    if(start_watchdog) {
        // Starting watchdog for all sensor threads 
        if(wdt_bmp5_channel_id < 0) {
            wdt_bmp5_channel_id = task_wdt_add_thread(WATCHDOG_LOW_TIMEOUT_MSEC);
            LOG_INF("BMP5 watchdog enabled with channel id :%d", wdt_bmp5_channel_id);
        } else {
            LOG_INF("BMP5 watchdog already enabled");
        }
    } else {
        // Stop watchdog to monitor all sensor threads 
        if(wdt_bmp5_channel_id >= 0) {
            LOG_INF("BMP5 watchdog disabled with channel id :%d", wdt_bmp5_channel_id);
            task_wdt_delete_thread(wdt_bmp5_channel_id);
            wdt_bmp5_channel_id = WATCHDOG_CHANNEL_DEFAULT_ID;
        } else {
            LOG_INF("BMP5 watchdog already disabled ");
        }
    }
}
#endif

/**
 * @brief BMP5 trigger handler to get pressure data from FIFO buffer
 *  
 * @param dev: BMP5 driver instance 
 * @param trigger: BMP5 trigger type (Not used)
 * 
 * @return none
 */
void barometer_trigger_handler(const struct device *dev,
					 const struct sensor_trigger *trigger)
{
    int ret;

#ifdef CONFIG_WATCHDOG
    if(wdt_bmp5_channel_id >= 0) {
        task_wdt_fed_from_thread(wdt_bmp5_channel_id);
    }
#endif 

    ret = bmp581::get_fifo_length(&bmp5_st.fifo_cnt);
    if(ret) {
        LOG_ERR("Failed to get the fifo count %d.\n",ret);
        bmp5_st.fail_cnt++;
        if(bmp5_st.fail_cnt >= BAROMTER_MAX_FAIL_CNT)
        {
            bmp5_st.bmp5_state = BMP5_STATE_FAULT;
        }
    }
    else
    {
        bmp5_st.fail_cnt = 0;
    }
    

    if(bmp5_st.fifo_cnt >= BAROMETER_THRESHOLD_LEVEL) {
        ret = bmp581::get_fifo_data(bmp5_msg.data, bmp5_st.fifo_cnt);
        if(ret) {
            LOG_ERR("Failed to get the pressure value %d.\n",ret);
            bmp5_st.bmp5_state = BMP5_STATE_FAULT;
        }
        else{
            #if defined(CONFIG_SH_TIME_MODULE)
            if (app_time::get_time_ticks(TimeResolution::MILLISECOND_TICKS_4096, &baro_time_stamp_streaming) < 0) 
            {
                LOG_ERR("Failed to get time for streaming");
            }
            if (app_time::get_time_ticks(TimeResolution::MILLISECOND_TICKS_8192, &baro_time_stamp_storage) < 0) 
            {
                LOG_ERR("Failed to get time");
            }
            #endif
            bmp5_st.bmp5_state = BMP5_STATE_STORE;
            bmp5_st.fail_cnt = 0;
        }
    }
}

/** 
 * @brief Setting BMP5 User parameters
 *
 * This function handles temperature sensor operations 
 * and data publishing over zbus.
 */
void barometer_set_usr_param()
{
    // Get the stored configuration from the NVS
    bmp5_user_config.bmp5_odr = sh_barometer_config::get_odr();
    bmp5_user_config.bmp5_fifo_threshold = sh_barometer_config::get_fifo_threshold();
    bmp5_user_config.bmp5_fifo_frame_sel = sh_barometer_config::get_frame_sel();
    bmp5_user_config.handler = barometer_trigger_handler;
}

/** 
 * @brief Temperature sensor thread module.
 *
 * This function handles temperature sensor operations 
 * and data publishing over zbus.
 */
void barometer_thread_func(void *arg1, void *arg2, void *arg3)
{
    int32_t ret;
    uint8_t falure_cnt = 0;
    
    size_t bytes_written = 0;
    while(1)
    {

#ifdef CONFIG_WATCHDOG
    if(wdt_bmp5_channel_id >= 0) {
        task_wdt_fed_from_thread(wdt_bmp5_channel_id);
    }
#endif 
        switch (bmp5_st.bmp5_state)
        {
        case BMP5_STATE_IDLE:
            // if not interrupt data received for 4 seconds set the failure mode
            falure_cnt++;
            if(falure_cnt > 40){
                bmp5_st.bmp5_state = BMP5_STATE_FAULT;
            }
            break;

        case BMP5_STATE_FAULT:
            fault_handler::send_diagnostics(BAROMETER_FAULT);
            sh_barometer_thread::stop_sensing();
            break;

        case BMP5_STATE_MEASURE:
        case BMP5_STATE_STREAM:
        case BMP5_STATE_STORE:

            falure_cnt = 0;
            
            bmp5_msg.length = sizeof(bmp5_msg.data);
            bmp5_msg.type = SENSOR_BAROMETER;
            bmp5_msg.timestamp_storage = baro_time_stamp_storage;
            bmp5_msg.timestamp_streaming = baro_time_stamp_streaming;
            //Updated below line for Storage as of now data is comming in pascal and 
            //we are storing it in Kpa
            for(uint8_t id = 0 ; id < BAROMETER_THRESHOLD_LEVEL ; id++)
            {
                bmp5_msg.data[id] *= 1000;
            }
            LOG_DBG("===============Pressure Data===============");
            LOG_DBG("bmp5_msg.length :: %d", bmp5_msg.length);
            LOG_DBG("bmp5_msg.timestamp :: %d", bmp5_msg.timestamp_storage);
            LOG_DBG("===========================================");
#if CONFIG_PIPES
            bytes_written = 0;
            /* send data to the consumers */
            ret = k_pipe_put(&barometer_pipe, (void*) &bmp5_msg, sizeof(bmp5_msg), &bytes_written,
                            sizeof(bmp5_msg), K_NO_WAIT);

            if (ret < 0) {
                /* Incomplete message header sent */
                // LOG_ERR("Error when data was sent through mic pipe: %d", ret);
            } else if (bytes_written < sizeof(bmp5_msg)) {
                /* Some of the data was sent */
                LOG_ERR("Not all data was sent through barometer pipe");
            }
#else
            // Publish temperature data over bmp5_chan zbus channel
            ret = zbus_chan_pub(&bmp5_chan, &bmp5_msg, K_NO_WAIT);
            if(ret != 0){
                LOG_ERR("Failed to publish berometer data [%d]",ret);
            }
#endif
            
            bmp5_st.bmp5_state = BMP5_STATE_IDLE;
            k_sem_give(&mem_mngr_sem);
#if CONFIG_BAROMETER_STREAMING
            k_sem_give(&stream_sem);
#endif
            break;

        default:
            break;
        }
        k_msleep(100);
    }
}

/**
 * @brief This function will resume the sensor thread and 
 * start the measurement of sensor data
 *
 * @return uint32_t Success indicates 0 and failure is non-zero.
 */
int sh_barometer_thread::start_sensing(void)
{
    int8_t ret = 0;
    if(sh_barometer_config::get_enabled() == BAROMETER_DISABLE) {
        LOG_ERR("Barometer config disabled in the NVS. Cannot initialize barometer");
        return -1;
    }

    LOG_DBG("sh_barometer_thread :: start_sensing");

    // Resume the sensor thread 
    k_thread_resume(&barometer_thread);
    LOG_DBG("sh_barometer_thread :: resume");

#ifdef CONFIG_WATCHDOG
    // Turn on watchdog channel 
    start_stop_wdt(true);
#endif

    // Start the sensor 
    ret = bmp581::barometer_resume_power();
    if(ret < 0) {
        LOG_ERR("Failed to resume sensor power %d.\n",ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to resume sensor power %d.\n",ret);
#endif 
    }
    LOG_INF("sh_barometer_thread :: power resume");

    // Start pressure measurement
    ret = bmp581::barometer_start();
    if(ret < 0) {
        LOG_ERR("Failed to Start sensor %d.\n",ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to Start sensor %d.\n",ret);
#endif 
            sh_barometer_thread::stop_sensing();
            fault_handler::send_diagnostics(BAROMETER_FAULT);
    }
    else{
        bmp5_st.fail_cnt = 0;
    }
    LOG_INF("sh_barometer_thread :: started all");

    return ret;
}
/**

 * @brief This function will suspend the running sensor thread and 
 * stop the the sensor measurement
 *
 * @return uint32_t Success indicates 0 and failure is non-zero.
 */
int sh_barometer_thread::stop_sensing(void)
{
    int8_t ret = 0;
    if(sh_barometer_config::get_enabled() == BAROMETER_DISABLE) {
        LOG_ERR("Barometer config disabled in the NVS. Cannot initialize barometer");
        return 0;
    }

    LOG_DBG("sh_barometer_thread :: stop_sensing");

    // Suspend the sensor thread
    k_thread_suspend(&barometer_thread);
    LOG_INF("sh_barometer_thread :: suspend");

#ifdef CONFIG_WATCHDOG
    // Turn off watchdog channel 
    start_stop_wdt(false);
#endif

    // Stop pressure measurement
    ret = bmp581::barometer_stop();
    if(ret < 0) {
        LOG_ERR("Failed to Stop sensor %d.\n",ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to Stop sensor %d.\n",ret);
#endif 
    }
    LOG_INF("sh_barometer_thread :: stop");
    // Stop the sensor 
    ret = bmp581::barometer_suspend_power();
    if(ret < 0) {
        LOG_ERR("Failed to suspend sensor power %d.\n",ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to suspend sensor power %d.\n",ret);
#endif 
    }

    LOG_DBG("ret code for barometer stop :: %d", ret);
#if defined(CONFIG_MEMFAULT) 
    // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"ret code for barometer stop :: %d", ret);
#endif 
    return ret;
}


/**
 * @brief Initializes the temperature streaming thread 
 * and adds zbus observers for temperature channel.
 *
 * @return uint32_t Success indicates 0 and failure is non-zero.
 */
int32_t sh_barometer_thread::init(void)
{
    int32_t ret = 0;

#if defined(CONFIG_SH_BMP5)

    // Check if Barometer is enabled from the NVS saved configurations
    if(sh_barometer_config::get_enabled() == BAROMETER_DISABLE) {
        LOG_ERR("Barometer config disabled in the NVS. Cannot initialize barometer");
        return 0;
    }
    barometer_set_usr_param();

    ret = bmp581::init(&bmp5_user_config);
    if(ret < 0){
        LOG_ERR("Failed to Initialize BMP581 barometer Sensor %d.",ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to Initialize BMP581 barometer Sensor %d.",ret);
#endif 
        fault_handler::send_diagnostics(BAROMETER_FAULT);
    }

    // Create the temperature sensor streaming thread
    barometer_thread_id = k_thread_create(&barometer_thread, barometer_stack,
			K_THREAD_STACK_SIZEOF(barometer_stack),
			barometer_thread_func, NULL, NULL, NULL,
			3, K_USER, K_NO_WAIT);

    k_thread_name_set(barometer_thread_id, "barometer");


    // Stop sensor thread until session starts
    ret = sh_barometer_thread::stop_sensing();
    if(ret < 0) {
        LOG_ERR("Failed to Start sensor %d.\n",ret);
    }
#endif //CONFIG_SH_BMP5

    return ret;
}

#endif //CONFIG_SH_BMP5