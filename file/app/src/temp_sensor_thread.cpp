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
#include "mem_mgr_thread.h"
// Driver files
#if defined(CONFIG_AS6221)
#include <sh/as6221.h>
#endif
#if defined(CONFIG_TMP119SH)
#include <sh/tmp119sh.h>
#endif
#include "temp_sensor_thread.h"
#include "network_thread.h"
#include "session_thread.h"
#include <sh/sh_time.h>
#include <fault_handler.h>
#include "memfault/core/trace_event.h"
#ifdef CONFIG_WATCHDOG
#include "watchdog.h"
#endif

LOG_MODULE_REGISTER(temp_sensor_thread, LOG_LEVEL_INF);

/* ZBUS for sharing temperature data */
ZBUS_SUBSCRIBER_DEFINE(temp_sensor_sub, 4);
ZBUS_CHAN_DEFINE(temp_chan, sh_temp_sensor::msg_t, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(0));

typedef enum {
    TEMP_STATE_IDLE     = 0x00,     // Idle state
    TEMP_STATE_MEASURE  = 0x01,     // Temperature Measurement state
    TEMP_STATE_STREAM   = 0x02,     // Streaming temperature data
    TEMP_STATE_STORE    = 0x03,     // Store temperature data
    TEMP_STATE_FAULT    = 0x04      // Fault state in temperature sensor
}temp_states_t;

/** 
 * @brief Structure contains the temperature states and opeations
 * 
 * temp_enable_streaming : Flag to control start stop enable data streaming
 * temp_enable_storage : Flag to control the enable disable the data storage 
 * temp_state : Temperature thread state machine
*/
typedef struct {
    bool temp_enable_streaming;
    bool temp_enable_storage;
    bool temp_init_fail;
    uint8_t temp_fail_cnt;
    temp_states_t temp_state;
}temp_t;

/* Default states */
temp_t temp_st = {
    .temp_enable_streaming = false,
    .temp_enable_storage = false,
    .temp_init_fail = false,
    .temp_fail_cnt = 0,
    .temp_state = TEMP_STATE_MEASURE
};

#ifdef CONFIG_WATCHDOG
/* Watchdog Channel ID for Temperature thread */
int8_t wdt_temperature_channel_id = WATCHDOG_CHANNEL_DEFAULT_ID;
#endif

static k_tid_t temp_sensor_thread_id;
static struct k_thread temp_sensor_thread;
K_THREAD_STACK_DEFINE(temp_sensor_stack, 512);


#ifdef CONFIG_WATCHDOG
/** 
 * @brief Start Stop watchdog for Temperature threads
 *
 * This function starts the watchdog for thread when sensor has been started to 
 * monitor its health. Once the session has been stopped, watchdog of the threads
 * will be stopped.
 * 
 * @param start_watchdog: Start/Stop watchdog flag
 */
static void start_stop_wdt(bool start_watchdog) 
{
    LOG_INF("Temperature Receiving start watchdog flag : %d and channel id is : %d", start_watchdog, wdt_temperature_channel_id);

    if(start_watchdog) {
        // Starting watchdog for all sensor threads 
        if(wdt_temperature_channel_id < 0) {
            wdt_temperature_channel_id = task_wdt_add_thread(WATCHDOG_TEMP_TIMEOUT_MSEC);
            LOG_INF("Temperature watchdog enabled with channel id :%d", wdt_temperature_channel_id);
        } else {
            LOG_INF("Temperature watchdog already enabled");
        }
    } else {
        // Stop watchdog to monitor all sensor threads 
        if(wdt_temperature_channel_id >= 0) {
            LOG_INF("Temperature watchdog disabled with channel id :%d", wdt_temperature_channel_id);
            task_wdt_delete_thread(wdt_temperature_channel_id);
            wdt_temperature_channel_id = WATCHDOG_CHANNEL_DEFAULT_ID;
        } else {
            LOG_INF("Temperature watchdog already disabled ");
        }
    }
}
#endif

int32_t get_temperature(int16_t *temp_val)
{
    int32_t ret = 0;
    // Read temperature value from the sensor
#if (CONFIG_AS6221)
    ret = as6221::get_temperature(temp_val);
#elif (CONFIG_TMP119SH)
    ret = tmp119::get_temperature(temp_val);
#endif
    if(ret < 0) {
        LOG_ERR("Failed to get the temperature value %d.\n",ret);
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to get the temperature value %d.\n",ret);
    }
    else{
        LOG_DBG("Temperature value received : %d C\n", (int16_t) (*temp_val) );
    }  
    return ret;
}


/** 
 * @brief Temperature sensor thread module.
 *
 * This function handles temperature sensor operations 
 * and data publishing over zbus.
 */
void temp_sensor_thread_func(void *arg1, void *arg2, void *arg3)
{
    int32_t ret;
    sh_temp_sensor::msg_t temp_data;            // Structure to hold temperature data
    int64_t time_stamp_storage = 0;
    int64_t time_stamp_streaming = 0;
    
    while(1)
    {

#ifdef CONFIG_WATCHDOG
    if(wdt_temperature_channel_id >= 0) {
        task_wdt_fed_from_thread(wdt_temperature_channel_id);
    }
#endif 
        //TODO: Uncomment following condition after temperature module fix
        // if(temp_st.temp_init_fail)
        // {
        //     //if initialization fail then set to fail state
        //     temp_st.temp_state = TEMP_STATE_FAULT;
        // }

        switch (temp_st.temp_state)
        {
        case TEMP_STATE_IDLE:
            /* code */
            temp_st.temp_state = TEMP_STATE_MEASURE;
            break;

        case TEMP_STATE_FAULT:
            // TODO: Enable the fault handler once the hardware issue is fixed.
            // fault_handler::send_diagnostics(TEMP_FAULT);
            // sh_temp_sensor::stop_sensing();
            temp_st.temp_state = TEMP_STATE_IDLE;
            break;

        case TEMP_STATE_MEASURE:
        {
            int16_t temp_val = 0;
            // TODO: Remove the condition once the fix is there
            if(!temp_st.temp_init_fail) {
                ret = get_temperature(&temp_val);
            }
            else {
                ret = 0;
            }
            if(ret >= 0) 
            {
                #if defined(CONFIG_SH_TIME_MODULE)
                if (app_time::get_time_ticks(TimeResolution::MILLISECOND_TICKS_4096, &time_stamp_streaming) < 0) 
                {
                    LOG_ERR("Failed to get time");
                }
                if (app_time::get_time_ticks(TimeResolution::MILLISECOND_TICKS_8192, &time_stamp_storage) < 0) 
                {
                    LOG_ERR("Failed to get time");
                }
                #endif
                temp_data.type = SENSOR_TEMP;
                temp_data.timestamp_storage = time_stamp_storage;
                temp_data.timestamp_streaming = time_stamp_streaming;
                temp_data.length = TEMP_DATA_LEN;
                temp_data.data = temp_val;
                temp_st.temp_fail_cnt = 0;

                // Publish temperature data over temp_chan zbus channel
                ret = zbus_chan_pub(&temp_chan, &temp_data, K_NO_WAIT);
                if(ret != 0){
                    LOG_ERR("Failed to publish temp data [%d]",ret);
                }
                k_sem_give(&mem_mngr_sem);
                k_sem_give(&stream_sem);
            }
            else
            {
                temp_st.temp_fail_cnt++;
                if(temp_st.temp_fail_cnt >= MAX_FAIL_CNT_TEMP_SENSOR)
                {
                    temp_st.temp_state = TEMP_STATE_FAULT;
                    temp_st.temp_fail_cnt = 0;
                }
            }
        }
        break;
        
        case TEMP_STATE_STORE:
            /* Storage data storage logic */
            temp_st.temp_state = TEMP_STATE_IDLE;
            break;
        
        case TEMP_STATE_STREAM:
            /* Streaming of temperature data  */
            temp_st.temp_state = TEMP_STATE_IDLE;
            break;

        default:
            break;
        }

        k_msleep(4000);
    }
}

/**
 * @brief This function will resume the sensor thread and 
 * start the measurement of sensor data
 *
 * @return uint32_t Success indicates 0 and failure is non-zero.
 */
int sh_temp_sensor::start_sensing(void)
{
    int ret = 0;

    LOG_DBG("sh_temp_sensor :: start_sensing");

    k_thread_resume(&temp_sensor_thread);
    LOG_INF("sh_temp_sensor :: resume");

#ifdef CONFIG_WATCHDOG
    // Turn on watchdog channel 
    start_stop_wdt(true);
#endif

#if 0
    ret = as6221::temp_resume_power();
    if(ret < 0) {
        LOG_ERR("Failed to suspend sensor power %d.\n",ret);
    }
#endif // Enable this section to suspend the Temperature sensor power

    temp_st.temp_state = TEMP_STATE_IDLE;

    return ret;
}

/**
 * @brief This function will suspend the running sensor thread and 
 * stop the the sensor measurement
 * 
 * @param flag default value is 0 it is for session, 1 is for streaming
 *
 * @return uint32_t Success indicates 0 and failure is non-zero.
 */
int sh_temp_sensor::stop_sensing(uint8_t flag)
{
    int ret = 0;

    if(flag != 0) {
       //received from non session thread
        if(sh_session_thread::get_current_session_state() == true){
            //session is already on going then do not stop temperature thread
            LOG_DBG("sh_temp_sensor :: flag %d",flag);
            return ret;
        }
    }

    LOG_DBG("sh_temp_sensor :: stop_sensing");

    k_thread_suspend(&temp_sensor_thread);
    LOG_INF("sh_temp_sensor :: suspend");

#ifdef CONFIG_WATCHDOG
    // Turn off watchdog channel 
    start_stop_wdt(false);
#endif

#if 0
    ret = as6221::temp_suspend_power();
    if(ret < 0) {
        LOG_ERR("Failed to suspend sensor power %d.\n",ret);
    }
#endif // Enable this section to suspend the Temperature sensor power
    return ret;
}

/**
 * @brief Initializes the temperature streaming thread 
 * and adds zbus observers for temperature channel.
 *
 * @return uint32_t Success indicates 0 and failure is non-zero.
 */
uint32_t sh_temp_sensor::init(void)
{
    int32_t ret = 0;

#if defined(CONFIG_AS6221)
    ret = as6221::init();
    if(ret < 0){
        LOG_ERR("Failed to Initialize AS6221 Temp Sensor %d.", ret);
        // TODO: Enable the fault handler once the hardware issue is fixed. Disable the memfault event log.
        MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to Initialize AS6221 Temp Sensor %d.", ret);
        // fault_handler::send_diagnostics(TEMP_FAULT);
        temp_st.temp_init_fail = true;
    }  

#elif defined(CONFIG_TMP119SH)
    ret = tmp119::init();
    if(ret < 0){
        LOG_ERR("Failed to Initialize TMP119 Temp Sensor %d.", ret);
        // TODO: Enable the fault handler once the hardware issue is fixed. Disable the memfault event log.
        MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to Initialize TMP119 Temp Sensor %d.", ret);
        // fault_handler::send_diagnostics(TEMP_FAULT);
        temp_st.temp_init_fail = true;
    }
#endif /* CONFIG_AS6221 or CONFIG_TMP119SH */

    // Create the temperature sensor streaming thread
    temp_sensor_thread_id = k_thread_create(&temp_sensor_thread, temp_sensor_stack,
			K_THREAD_STACK_SIZEOF(temp_sensor_stack),
			temp_sensor_thread_func, NULL, NULL, NULL,
			3, K_USER, K_NO_WAIT);

    k_thread_name_set(temp_sensor_thread_id, "temp");

    // Stop sensor thread until session starts
    ret = sh_temp_sensor::stop_sensing();
    if(ret < 0) {
        LOG_ERR("Failed to Stop sensor %d.\n",ret);
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to Start sensor %d.\n",ret);
    }
    
    return ret;
}
