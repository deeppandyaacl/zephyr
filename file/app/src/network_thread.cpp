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
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "network_thread.h"
#include "mem_mgr_thread.h"
#include "temp_sensor_thread.h"
#include "imu_sensor_thread.h"
#include "barometer_thread.h"
#include "microphone_thread.h"
#include "sh/sh_utils.h"
#include "temp_service/ble_temp.h"
#include "imu_service/ble_imu.h"
#include "ble_base/ble.h"
#include "baro_service/ble_baro.h"
#include "mic_service/ble_mic.h"

LOG_MODULE_REGISTER(network_thread, LOG_LEVEL_ERR);


/* Define the application thread details */
static k_tid_t network_thread_id;
static struct k_thread network_thread;
K_THREAD_STACK_DEFINE(network_thread_stack, 1024);

ZBUS_MSG_SUBSCRIBER_DEFINE(ble_streaming_sub);

/* Define the application thread observer */
/* Define subscriber to Service Commands channel */
ZBUS_MSG_SUBSCRIBER_DEFINE(temp_chan_sub);

#define ADVERTISE_RESEND_INTERVAL   10*60   //10 mins

static void advertise_timer_handler(struct k_timer *timer_id);

K_TIMER_DEFINE(advertise_timer, advertise_timer_handler, NULL);

/**
 * @brief Handler to start advertisement again
 * 
 * @param timer_id 
 */
static void advertise_timer_handler(struct k_timer *timer_id)
{
    int32_t ret = 0;
    if( sh_ble::get_state() != sh_ble::CONNECTED)
    {
        LOG_INF("\n\nperiod advertisement start");
        ret = sh_ble::start_advertising();
        if (ret < 0) {
            LOG_ERR("\n\nAdvertising start failed (err %d)\n", ret);
        }
    }
}

void stop_data_collection_after_disconnection() {
    int ret = 0;
#if defined(CONFIG_BAROMETER_STREAMING)
    ret = sh_barometer_thread::stop_sensing();
    LOG_INF("result code for barometer stop :: %d", ret);
    if (ret < 0)
    {
        LOG_ERR("Failed to stop the Barometer session : %d", ret);
    }
#endif

#if defined(CONFIG_TEMP_STREAMING)
    ret = sh_temp_sensor::stop_sensing(1);
    if (ret < 0)
    {
        LOG_ERR("Failed to stop the Temperature session : %d", ret);
    }
#endif

#if defined(CONFIG_IMU_STREAMING)
    ret = sh_imu_sensor::stop_sensing();
    if (ret < 0)
    {
        LOG_ERR("Failed to stop the IMU session : %d", ret);
    }
#endif

#if defined(CONFIG_MIC_STREAMING)
    ret = sh_microphone::stop_sensing();
    if (ret < 0)
    {
        LOG_ERR("Failed to stop the Microphone session : %d", ret);
    }
#endif
}



void network_thread_handle_event(sh_ble::streaming_msg_t streaming_msg)
{
    uint16_t streaming_bit_field = streaming_msg.stream_bitfield;
    LOG_INF(" Ble streaming message %d to %s", streaming_bit_field, streaming_msg.enable_flag ? "enable" : "disable");

    // Handle disconnection events by making sure all sensors have stopped data collection
    if (streaming_msg.evt == sh_ble::DISCONNECTION_EVT)
    {
        stop_data_collection_after_disconnection();
    }
    // Handle characteristics notification changing to enable or disable sensors individually
    else if (streaming_msg.evt == sh_ble::CHAR_NOTIFICATION_EVT)
    {
        state_t memory_state;
        sh_mem_mngr::get_memory_manager_state(&memory_state);
        if (memory_state != MEM_MGR_IDLE_STATE && streaming_bit_field != TEMP_STREAMING) {
            LOG_ERR("Can't start/stop sensors while data collection is ongoing");
            return;
        }
        //Check what measurement notificaition changed
        switch (streaming_bit_field)
        {
#if CONFIG_TEMP_STREAMING
        case TEMP_STREAMING:
        {
            streaming_msg.enable_flag ? sh_temp_sensor::start_sensing() : sh_temp_sensor::stop_sensing(1);
            break;
        }
#endif
#if CONFIG_IMU_STREAMING
        case IMU_STREAMING:
        {
            streaming_msg.enable_flag ? sh_imu_sensor::start_sensing(CONFIG_IMU_ACCL_ODR_STREAMING) : sh_imu_sensor::stop_sensing();
            break;
        }
#endif
#if CONFIG_BAROMETER_STREAMING
        case BARO_STREAMING:
        {
            streaming_msg.enable_flag ? sh_barometer_thread::start_sensing() : sh_barometer_thread::stop_sensing();
            break;
        }
#endif
#if CONFIG_MIC_STREAMING
        case MIC_STREAMING:
        {
            microphone_config_t mic_cfg;
            sh_microphone::get_current_config(&mic_cfg);
            if (mic_cfg.num_of_chan != STEREO)
            {
                streaming_msg.enable_flag ? sh_microphone::start_sensing() : sh_microphone::stop_sensing();
            }
            else if (mic_cfg.num_of_chan == STEREO && !streaming_msg.enable_flag)
            {
                // Allow disabling of mics even when in stereo mode
                sh_microphone::stop_sensing();
            }
            else
            {
                LOG_ERR("Streaming stereo audio is not supported");
            }

            break;
        }
#endif
        default:
            LOG_ERR("Sensor is not available for streaming");
            break;
        }
    }
}

/** 
 * @brief Application thread module.
 *
 * This function collects the structure from the different ZBUS channels
 * and handle the operations accordingly. 
 */
void network_thread_func(void *arg1, void *arg2, void *arg3)
{
    int32_t ret;
    const struct zbus_channel *chan;
    sh_ble::streaming_msg_t streaming_msg;
    uint8_t stream_data[CONFIG_BT_IMU_STREAM_LEN];
    size_t bytes_read = 0;
    
    while(1)
    {
        ret = k_sem_take(&stream_sem, K_FOREVER);
        if (!ret)
        {
            //Handle streaming events first
            if (!zbus_sub_wait_msg(&ble_streaming_sub, &chan, &streaming_msg, K_NO_WAIT))
            {
                network_thread_handle_event(streaming_msg);
            }
            
            //Handle data streaming 
#if CONFIG_TEMP_STREAMING
            sh_temp_sensor::msg_t temp_msg;
            uint8_t temp_data[6];
            ret = zbus_sub_wait_msg(&temp_chan_sub, &chan, &temp_msg, K_NO_WAIT);
            if (!ret)
            {
                if (sh_ble::get_streaming_state() & TEMP_STREAMING)
                {
                    uint32_encode(temp_msg.timestamp_streaming, temp_data);
                    uint16_big_encode(temp_msg.data, &temp_data[4]);
                    sh_ble_temp_meas_send(temp_data, 6);
                }
            }
#endif

#if CONFIG_BAROMETER_STREAMING
            sh_barometer_thread::msg_t barometer_msg;
            uint8_t barometer_data[BAROMETER_STREAM_LEN];
            if (sh_ble::get_streaming_state() & BARO_STREAMING)
            {
                bytes_read = 0;
                ret = k_pipe_get(&barometer_pipe, &barometer_msg, sizeof(barometer_msg), &bytes_read,
                                sizeof(barometer_msg), K_NO_WAIT);
                ret |= (bytes_read < sizeof(barometer_msg));
                if (!ret)
                {
                    uint32_encode(barometer_msg.timestamp_streaming, barometer_data);
                    memcpy(&barometer_data[4], barometer_msg.data, BAROMETER_STREAM_LEN - 4);
                    sh_ble_baro_meas_send(barometer_data, BAROMETER_STREAM_LEN);
                }
            }
#endif

#if CONFIG_IMU_STREAMING
            sh_imu_sensor::msg_t imu_msg;
            if (sh_ble::get_streaming_state() & IMU_STREAMING)
            {
                bytes_read = 0;
                ret = k_pipe_get(&imu_pipe, &imu_msg, sizeof(imu_msg), &bytes_read,
                                sizeof(imu_msg), K_NO_WAIT);
                ret |= (bytes_read < sizeof(imu_msg));
                if (!ret)
                {
                    /* All data was received */
                    memset(&stream_data[0], 0x00, CONFIG_BT_IMU_STREAM_LEN);
                    uint32_encode(imu_msg.timestamp_streaming, stream_data);
                    memcpy(&stream_data[4], imu_msg.data, CONFIG_BT_IMU_STREAM_LEN - 4);
                    sh_ble_imu_meas_send(stream_data, CONFIG_BT_IMU_STREAM_LEN);
                }
            }
#endif // CONFIG_IMU_STREAMING

#if CONFIG_MIC_STREAMING
            sh_microphone::msg_t mic_msg;
            uint8_t mic_data[MIC_MAX_STREAM_LEN];
            if (sh_ble::get_streaming_state() & MIC_STREAMING)
            {
                size_t bytes_read = 0;
                ret = k_pipe_get(&microphone_pipe, &mic_msg, sizeof(mic_msg), &bytes_read,
                        sizeof(mic_msg) , K_NO_WAIT);
                ret |= (bytes_read <  sizeof(mic_msg));
                if (!ret) {
                    /* All data was received */
                    uint32_encode(mic_msg.timestamp_streaming, mic_data);
                    memcpy(&mic_data[4], mic_msg.data, MIC_MAX_STREAM_LEN - 4);
                    sh_ble_mic_meas_send(mic_data, MIC_MAX_STREAM_LEN);
                }
            }
#endif
        } // zbus
    } // while loop
} // network_thread_func

/**
 * @brief Initializes the Sensor Application thread
 *
 * @return uint32_t Success indicates 0 and failure is non-zero.
 */
int32_t sh_network_thread::init(void)
{
    int32_t ret = 0;

    ret = sh_ble::init();
    if (ret < 0) {
        LOG_ERR("Bluetooth init failed (err %d)\n", ret);
    }
    
    ret = sh_ble::start_advertising();
    if (ret < 0) {
        LOG_ERR("Advertising start failed (err %d)\n", ret);
    }

    k_timer_start(&advertise_timer, K_SECONDS(ADVERTISE_RESEND_INTERVAL), K_SECONDS(ADVERTISE_RESEND_INTERVAL));

    // Add thread as a zbus observer to temperature chan
    ret = zbus_chan_add_obs(&ble_streaming_chan, &ble_streaming_sub, K_MSEC(200));
    if(ret){
        LOG_ERR("Failed to add thread as zbus observer to ble streaming chan (err: %d)", ret);
    } 
    // Add thread as a zbus observer to temperature chan
    ret = zbus_chan_add_obs(&temp_chan, &temp_chan_sub, K_MSEC(200));
    if(ret){
        LOG_ERR("Failed to add thread as zbus observer to temp chan (err: %d)", ret);
    } 

    // Create the temperature sensor streaming thread
    network_thread_id = k_thread_create(&network_thread, network_thread_stack,
			K_THREAD_STACK_SIZEOF(network_thread_stack),
			network_thread_func, NULL, NULL, NULL,
			3, K_USER, K_NO_WAIT);

    k_thread_name_set(network_thread_id, "network");

    return ret;
}
