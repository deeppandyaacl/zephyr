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

#include <sh/sh_utils.h>
#include "imu_sensor_thread.h"
#include "mem_mgr_thread.h"
#include <sh/sh_time.h>
#include "network_thread.h"
#include "fault_handler.h"
#include "memfault/core/trace_event.h"

#ifdef CONFIG_SEGNO_LIBRARY
#include "segno_thread.h"
#endif

#ifdef CONFIG_WATCHDOG
#include "watchdog.h"
#endif

#include "imu_config.h"

#if defined (CONFIG_SH_BMI323)
#include <sh/sh_bmi323.h>
LOG_MODULE_REGISTER(imu_sensor_thread, LOG_LEVEL_ERR);

/* ZBUS for sharing imu data */
ZBUS_SUBSCRIBER_DEFINE(imu_sensor_sub, 4);
K_PIPE_DEFINE(imu_pipe, sizeof(sh_imu_sensor::msg_t)*40, 4);
K_PIPE_DEFINE(imu_segno_pipe, sizeof(sh_imu_sensor::msg_t)*40, 4);
ZBUS_CHAN_DEFINE(imu_chan, sh_imu_sensor::msg_t, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                    ZBUS_MSG_INIT(0));

ZBUS_CHAN_DEFINE(imu_segno_chan, sh_imu_sensor::msg_t, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                    ZBUS_MSG_INIT(0));

ZBUS_CHAN_DEFINE(imu_step_chan, sh_imu_sensor::step_count_msg_t , NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(0));

#ifdef CONFIG_SH_BMI323_STEP_COUNT_ENABLE
// Timer handler function for step count
void imu_step_event_timer_handler(struct k_timer *arg);

// Define step count read timer handler
K_TIMER_DEFINE(imu_step_event_timer, imu_step_event_timer_handler, NULL);
#endif /* CONFIG_SH_BMI323_STEP_COUNT_ENABLE */

/* FIFO defines */
#define ACCEL_FIFO_TH_SAMPLES               ( 3 * 48 )
#define ACCEL_RTC_TICK_BTW_INT              ( 234 )
#define IMU_DATA_BUFFER_LEN                 ( 24 )
#define IMU_BUFFER_DIM                      (ACCEL_FIFO_TH_SAMPLES/3)
#define IMU_XY_DOWNSCALER                   ( 8 )

/* IMU state machine states */
typedef enum {
    IMU_STATE_IDLE,         // IMU idle state
    IMU_STATE_RECONFIG,     // IMU reconfiguration and reintialization state
    IMU_STATE_STREAMING,    // IMU streaming state
    IMU_STATE_START,
    IMU_STATE_STOP,
    IMU_STATE_FAULT
}imu_state_t;

typedef struct {
    int16_t data[CONFIG_SH_BMI323_WATERMARK];
}imu_buf_fifo_t;

typedef struct {
    int8_t data[120];
}imu_stream_data_t;

typedef struct {
    int16_t x[IMU_BUFFER_DIM/IMU_XY_DOWNSCALER];
    int16_t y[IMU_BUFFER_DIM/IMU_XY_DOWNSCALER];
    int16_t z[IMU_BUFFER_DIM];
} vs_imu_t;

/* Structure holding flags and variables related to IMU */
typedef struct {
    imu_buf_fifo_t      imu_buf_fifo_set[(IMU_DATA_BUFFER_LEN * 2)];
    imu_stream_data_t   imu_stream_data;
    k_sem               imu_trigger_sem;
    imu_state_t         imu_state;
    uint16_t            imu_fifo_buf_index;
    uint16_t            imu_buf_index;
    uint32_t            imu_step_count;
    uint8_t             imu_fail_cnt;
}imu_sensor_struct_t;

imu_sensor_struct_t imu_sensor_struct;
sh_imu_sensor::msg_t imu_data_s;
sh_imu_sensor::segno_msg_t imu_segno_data_s;
sh_imu_sensor::step_count_msg_t imu_step_data_s;
static int64_t imu_time_stamp_storage = 0;
static int64_t imu_time_stamp_streaming = 0;
static bool thread_suspend_flag = false;

#ifdef CONFIG_WATCHDOG
/* Watchdog Channel ID for IMU thread */
int8_t wdt_imu_channel_id = WATCHDOG_CHANNEL_DEFAULT_ID;
#endif


static k_tid_t imu_sensor_thread_id;
static struct k_thread imu_sensor_thread;
K_THREAD_STACK_DEFINE(imu_sensor_stack, 1024);

#ifdef CONFIG_WATCHDOG
/** 
 * @brief Start Stop watchdog for IMU threads
 *
 * This function starts the watchdog for thread when sensor has been started to 
 * monitor its health. Once the session has been stopped, watchdog of the threads
 * will be stopped.
 * 
 * @param start_watchdog: Start/Stop watchdog flag
 */
static void start_stop_wdt(bool start_watchdog) 
{
    LOG_INF("IMU Receiving start watchdog flag : %d and channel id is : %d", start_watchdog, wdt_imu_channel_id);

    if(start_watchdog) {
        // Starting watchdog for all sensor threads 
        if(wdt_imu_channel_id < 0) {
            wdt_imu_channel_id = task_wdt_add_thread(WATCHDOG_LOW_TIMEOUT_MSEC);
            LOG_INF("IMU watchdog enabled with channel id :%d", wdt_imu_channel_id);
        } else {
            LOG_INF("IMU watchdog already enabled");
        }
    } else {
        // Stop watchdog to monitor all sensor threads 
        if(wdt_imu_channel_id >= 0) {
            LOG_INF("IMU watchdog disabled with channel id :%d", wdt_imu_channel_id);
            task_wdt_delete_thread(wdt_imu_channel_id);
            wdt_imu_channel_id = WATCHDOG_CHANNEL_DEFAULT_ID;
        } else {
            LOG_INF("IMU watchdog already disabled ");
        }
    }
}
#endif

/**
 * @brief Read and process IMU data
 * 
 * @return int8_t  
 */

static int8_t imu_read(void)    
{
    int ret;
    uint16_t sample_to_read = 0;
    vs_imu_t imu_data = {0};

 
    ret = sh_bmi323_read_fifo_frames(imu_sensor_struct.imu_buf_fifo_set[imu_sensor_struct.imu_fifo_buf_index].data, &sample_to_read);
    if(ret < 0) {
        return sh_imu_sensor::IMU_RECOVERABLE_ERROR;
    }

    if(sample_to_read < CONFIG_SH_BMI323_WATERMARK) {
        LOG_ERR("IMU Sample Low expected: %d actual: %d", CONFIG_SH_BMI323_WATERMARK, sample_to_read);
        return sh_imu_sensor::IMU_RECOVERABLE_ERROR;
    }
    else if(sample_to_read > (CONFIG_SH_BMI323_WATERMARK * 2)) {
        LOG_ERR("IMU FIFO Reset: %d",sample_to_read);
        sh_bmi323_fifo_reset();
        return sh_imu_sensor::IMU_RECOVERABLE_ERROR;
    }

    ret = sh_bmi323_read_step_counter((int32_t *)&imu_step_data_s.step_count);
    if(ret < 0) {
        LOG_ERR("Failed to read the step counter from BMI323");
    }

    int16_t *current_imu_buf = imu_sensor_struct.imu_buf_fifo_set[imu_sensor_struct.imu_fifo_buf_index].data;
    int8_t *current_page = imu_sensor_struct.imu_stream_data.data;

    int32_t x_temp_sum = 0;
    int32_t y_temp_sum = 0;
    imu_sensor_struct.imu_buf_index = 0;

    for (int i=0; i < (CONFIG_SH_BMI323_WATERMARK/3); i++)
    {
        if ((i == 7) || (i == 15) || (i == 23))
        {
            x_temp_sum   += *(current_imu_buf + i * 3);
            y_temp_sum   += *(current_imu_buf + i * 3 + 1);
            //save into buffer here - average of 8 x and y and z complete
            imu_data.x[i/IMU_XY_DOWNSCALER]   = (int16_t) ((x_temp_sum >> 3) & 0x0000FFFF);
            imu_data.y[i/IMU_XY_DOWNSCALER]   = (int16_t) ((y_temp_sum >> 3) & 0x0000FFFF);
            imu_data.z[i] = *(current_imu_buf + i * 3 + 2);
            memcpy(current_page + imu_sensor_struct.imu_buf_index, &imu_data.x[i/IMU_XY_DOWNSCALER], 2);
            memcpy(current_page + imu_sensor_struct.imu_buf_index + 2, &imu_data.y[i/IMU_XY_DOWNSCALER], 2);
            memcpy(current_page + imu_sensor_struct.imu_buf_index + 4, current_imu_buf + i * 3 + 2, 2);
            LOG_DBG("x = %d, y = %d, z = %d", imu_data.x[i/IMU_XY_DOWNSCALER], imu_data.y[i/IMU_XY_DOWNSCALER], imu_data.z[i]);
            
            imu_sensor_struct.imu_buf_index += 6;
            
            x_temp_sum = 0;
            y_temp_sum = 0;
        } 
        else
        {
            x_temp_sum   += *(current_imu_buf + i * 3);
            y_temp_sum   += *(current_imu_buf + i * 3 + 1);
            imu_data.z[i] = *(current_imu_buf + i * 3 + 2);
            memcpy(current_page + imu_sensor_struct.imu_buf_index, current_imu_buf + i * 3 + 2, 2);
            LOG_DBG("z = %d", imu_data.z[i]);
            imu_sensor_struct.imu_buf_index += 2;
        }
    }

    if(imu_sensor_struct.imu_buf_index >= IMU_DATA_LEN) {
        imu_sensor_struct.imu_buf_index = 0;
    }

    //imu_epoch_timestamp += ACCEL_RTC_TICK_BTW_INT;

    imu_data_s.type = 1;
    imu_data_s.length = IMU_DATA_LEN;
    imu_data_s.timestamp_storage = imu_time_stamp_storage;
    imu_data_s.timestamp_streaming = imu_time_stamp_streaming;

    // imu_data_s.timestamp = imu_epoch_timestamp;
    memcpy(imu_data_s.data, imu_sensor_struct.imu_stream_data.data, IMU_DATA_LEN);

#if CONFIG_PIPES
    /* send data to the consumers */
    size_t bytes_written = 0;
    ret = k_pipe_put(&imu_pipe, (void*) &imu_data_s, sizeof(imu_data_s), &bytes_written,
                    sizeof(imu_data_s), K_NO_WAIT);

    if (ret < 0) {
        /* Incomplete message header sent */
        // LOG_ERR("Error when data was sent through mic pipe: %d", ret);
    } else if (bytes_written < sizeof(imu_data_s)) {
        /* Some of the data was sent */
        LOG_ERR("Not all data was sent through mic pipe");
    }
#else
    // Publish IMU data ove zbus channel
    ret = zbus_chan_pub(&imu_chan, &imu_data_s, K_FOREVER);
    if(ret != 0) {
        LOG_ERR("Failed to publish IMU data = %d", ret);
    }
#endif
    k_sem_give(&mem_mngr_sem);
#if CONFIG_IMU_STREAMING
    k_sem_give(&stream_sem);
#endif

#ifdef CONFIG_SEGNO_LIBRARY
    // Publish IMU data over zbus channel for segno library
#if CONFIG_PIPES
    size_t s_bytes_written = 0;
    ret = k_pipe_put(&imu_segno_pipe, (void*) &imu_data_s, sizeof(imu_data_s), &s_bytes_written,
                    sizeof(imu_data_s), K_NO_WAIT);

    if (ret < 0) {
        /* Incomplete message header sent */
        // LOG_ERR("Error when data was sent through mic pipe: %d", ret);
    } else if (s_bytes_written < sizeof(imu_data_s)) {
        /* Some of the data was sent */
        LOG_ERR("Not all data was sent through mic pipe");
    }
#else
    ret = zbus_chan_pub(&imu_segno_chan, &imu_data_s, K_FOREVER);
    if(ret != 0) {
        LOG_ERR("Failed to publish IMU data = %d", ret);
    }
#endif
    k_sem_give(&segno_mngr_sem);
#endif

    imu_sensor_struct.imu_fifo_buf_index++;

    if(imu_sensor_struct.imu_fifo_buf_index < (IMU_DATA_BUFFER_LEN - 1)) {
        imu_sensor_struct.imu_fifo_buf_index += 1;
    }
    else {
        imu_sensor_struct.imu_fifo_buf_index = 0;
    }

    return 0;
}

/** 
 * @brief IMU interrupt trigger handler
 * 
 * @param dev 
 * @param trigger Sesnor trigger specification
 */
void imu_trigger_handler(const struct device *dev,
					 const struct sensor_trigger *trigger)
{
    #if defined(CONFIG_SH_TIME_MODULE)
    // Read the epoch timestamp value for IMU
    if (app_time::get_time_ticks(TimeResolution::MILLISECOND_TICKS_4096, &imu_time_stamp_streaming) < 0) 
    {
        LOG_ERR("Failed to get time for streaming");
    }
    if (app_time::get_time_ticks(TimeResolution::MILLISECOND_TICKS_8192, &imu_time_stamp_storage) < 0) 
    {
        LOG_ERR("Failed to get time for storage");
    }
    #endif
  
#ifdef CONFIG_WATCHDOG
    if(wdt_imu_channel_id >= 0) {
        task_wdt_fed_from_thread(wdt_imu_channel_id);
    }
#endif 
    // Give semaphore to read and publish the IMU FIFO data
    k_sem_give(&imu_sensor_struct.imu_trigger_sem);
}

#ifdef CONFIG_SH_BMI323_STEP_COUNT_ENABLE
/**
 * @brief Event timer handler which read the step counts and publish the 
 *        data to store into shrd or steaming the data
 * 
 * @param arg Timer parameters
 */
void imu_step_event_timer_handler(struct k_timer *arg)
{
    int ret = 0;
    int64_t step_ts_storage = 0;
    int64_t step_ts_streaming = 0;

#ifdef CONFIG_SH_TIME_MODULE
    // Read the epoch timestamp value for IMU
    if (app_time::get_time_ticks(TimeResolution::MILLISECOND_TICKS_4096,
                                &step_ts_streaming) < 0) 
    {
        LOG_ERR("Failed to get time for streaming");
    }
    if (app_time::get_time_ticks(TimeResolution::MILLISECOND_TICKS_8192,
                                &step_ts_storage) < 0) 
    {
        LOG_ERR("Failed to get time for storage");
    }
#endif /* CONFIG_SH_TIME_MODULE */
    imu_step_data_s.type = 1;
    imu_step_data_s.length = IMU_STEP_DATA_LEN;
    imu_step_data_s.timestamp_storage = (uint32_t)step_ts_storage;
    imu_step_data_s.timestamp_streaming = (uint32_t)step_ts_streaming;

    LOG_INF("IMU step count %d", (uint16_t)imu_step_data_s.step_count);

    // Publish IMU step data to the channel
    ret = zbus_chan_pub(&imu_step_chan, &imu_step_data_s, K_FOREVER);
    if(ret != 0) {
        LOG_ERR("Failed to publish IMU step data = %d", ret);
    }
    k_sem_give(&mem_mngr_sem);
}
#endif /* CONFIG_SH_BMI323_STEP_COUNT_ENABLE */

/**
 * @brief IMU sensor thread module. This handles IMU sensor operations and
 *        data publishing over zbus.
 */
void imu_sensor_thread_func(void *arg1, void *arg2, void *arg3)
{
    int32_t ret;

    while(1)
    {
        if(thread_suspend_flag)
        {
            thread_suspend_flag = false;
            LOG_ERR("imu thread suspend");
            k_thread_suspend(&imu_sensor_thread);
        }
        switch(imu_sensor_struct.imu_state)
        {
        case IMU_STATE_IDLE:
            k_sleep(K_SECONDS(5));
            imu_sensor_struct.imu_state = IMU_STATE_RECONFIG;
            break;
        
        case IMU_STATE_STREAMING:
            k_sem_take(&imu_sensor_struct.imu_trigger_sem, K_MSEC(500));
            ret = imu_read();
            if((ret < 0) && (ret != sh_imu_sensor::IMU_RECOVERABLE_ERROR)) {
                LOG_ERR("Failed to read and publish IMU data");
                imu_sensor_struct.imu_fail_cnt++;
                if(imu_sensor_struct.imu_fail_cnt >= IMU_MAX_FAIL_CNT)
                {
                    imu_sensor_struct.imu_state = IMU_STATE_FAULT;
                }
            }
            break;

        case IMU_STATE_RECONFIG:
            imu_sensor_struct.imu_state = IMU_STATE_STOP;
            LOG_DBG("sh_imu_sensor :: Reconfiguring IMU");
            break;

        case IMU_STATE_START:
            LOG_DBG("sh_imu_sensor :: Start IMU");
            if(sh_bmi323_fifo_reset() != 0){
                imu_sensor_struct.imu_state = IMU_STATE_FAULT;
            }
            else{
                imu_sensor_struct.imu_state = IMU_STATE_STREAMING;
                imu_sensor_struct.imu_fail_cnt = 0;
            }
            break;

        case IMU_STATE_STOP:
            LOG_DBG("sh_imu_sensor :: Stop IMU");
            if(sh_bmi323_fifo_reset() != 0){
                imu_sensor_struct.imu_state = IMU_STATE_FAULT;
            }
            else{
                imu_sensor_struct.imu_state = IMU_STATE_START;
                imu_sensor_struct.imu_fail_cnt = 0;
            }
            break;

        case IMU_STATE_FAULT:
            fault_handler::send_diagnostics(IMU_FAULT);
            sh_imu_sensor::stop_sensing();
            break;
        
        default:
            break;
        }
    }
}

/**
 * @brief This function will resume the sensor thread and 
 * start the measurement of sensor data
 *
 * @return uint32_t Success indicates 0 and failure is non-zero.
 */
int sh_imu_sensor::start_sensing(uint16_t accl_odr)
{
    int ret = 0;
    bool low_odr_set = false;
    LOG_INF("sh_imu_sensor :: start_sensing");

#ifdef CONFIG_SEGNO_LEAD_ON_OFF
    if(sh_segno_thread::check_body_detect_status()) {
        sh_segno_thread::config_imu_segno_w_low_odr();
        low_odr_set = true;
    }
#endif

    //If low odr was set already do not change the sampling rate
    if (!low_odr_set) {
        LOG_INF("sh_imu_sensor :: low_odr_set");
        //If zero, accel should be set to default value
        if (accl_odr == 0 ) {
            accl_odr = SH_BMI323_DEFAULT_ACCEL_ODR;
        }

        ret = sh_bmi323_change_accl_odr(accl_odr);
        if (ret < 0)
        {
            LOG_ERR("Failed to change accl ODR of BMI323 sensor %d", ret);
        }
        //Delay in order for ODR change to go through
        k_msleep(5);
    }

    // ret = sh_bmi323_resume_power();
    // if(ret < 0) {
    //     LOG_ERR("Failed to Suspend BMA456 sensor power %d", ret);
    // }

    thread_suspend_flag = false;

    imu_sensor_struct.imu_state = IMU_STATE_START;
    k_thread_resume(&imu_sensor_thread);
    LOG_INF("sh_imu_sensor :: resume");
#ifdef CONFIG_SH_BMI323_STEP_COUNT_ENABLE
    //Start 1 seconds timer for step count event
    k_timer_start(&imu_step_event_timer, K_MSEC(200),  \
    K_MSEC(IMU_STEP_EVENT_TIMER_IN_MS));
    LOG_INF("Start step count timer event");
#endif /* CONFIG_SH_BMI323_STEP_COUNT_ENABLE */
#ifdef CONFIG_WATCHDOG
    // Turn on watchdog channel 
    start_stop_wdt(true);
#endif

    return ret;
}

/**
 * @brief This function will suspend the running sensor thread and 
 * stop the the sensor measurement
 *
 * @return uint32_t Success indicates 0 and failure is non-zero.
 */
int sh_imu_sensor::stop_sensing(void)
{
    int ret = 0;

    LOG_INF("sh_imu_sensor :: stop_sensing"); 
    // k_thread_suspend(&imu_sensor_thread);
    thread_suspend_flag = true;

    LOG_INF("sh_imu_sensor :: suspend");
#ifdef  CONFIG_SH_BMI323_STEP_COUNT_ENABLE
    //Stop 1 second timer for step count event
    k_timer_stop(&imu_step_event_timer);
    //Reset the step counter
    sh_bmi323_reset_step_counter();
#endif /* CONFIG_SH_BMI323_STEP_COUNT_ENABLE */
#ifdef CONFIG_WATCHDOG
    // Turn off watchdog channel 
    start_stop_wdt(false);
#endif  
  
    // ret = sh_bmi323_suspend_power();

    if(ret < 0) {
        LOG_ERR("Failed to Suspend BMA456 sensor power %d", ret);
    }
    
    return ret;
}

/**
 * @brief Load the IMU parameters from the NVS 
 * 
 * @return none
 */
static void load_bmi_parameters(sh_bmi323_user_config_t *bmi323_config)
{
    bmi323_config->bmi_acc_odr = sh_imu_config::get_acc_odr();
    bmi323_config->bmi_acc_mode = sh_imu_config::get_acc_mode();
    bmi323_config->bmi_acc_range = sh_imu_config::get_acc_range();
    bmi323_config->bmi_gyro_odr = sh_imu_config::get_gyro_odr();
    bmi323_config->bmi_gyro_mode = sh_imu_config::get_gyro_mode();
    bmi323_config->bmi_gyro_range = sh_imu_config::get_gyro_range();
    bmi323_config->handler = (sensor_trigger_handler_t)imu_trigger_handler;
}

/**
 * @brief Initialization of IMU sensor streaming thread and adds zbus
 *        observers for IMU channel
 * 
 * @return uint32_t 0 = Success, Non-zero = Failure
 */
uint32_t sh_imu_sensor::init(void)
{
    int ret = 0;
    imu_sensor_struct.imu_state = IMU_STATE_IDLE;
    imu_sensor_struct.imu_fifo_buf_index = 0;
    imu_sensor_struct.imu_buf_index = 0;
    sh_bmi323_user_config_t bmi323_config;

    // Check if IMU is enabled from the NVS saved configurations
    if(sh_imu_config::get_enabled() == IMU_DISABLE) {
        LOG_ERR("IMU config disabled in the NVS. Cannot initialize IMU driver");
        return -1;
    }

    load_bmi_parameters(&bmi323_config);
    ret = sh_bmi323_init(&bmi323_config);

    if(ret != 0) {
        LOG_ERR("Failed to initialize BMI323 IMU sensor %d", ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to initialize BMI323 IMU sensor %d", ret);
#endif 
        fault_handler::send_diagnostics(IMU_FAULT);
    }

    ret = sh_bmi323_config_step_counter();

    if(ret != 0) {
        LOG_ERR("Failed to configure BMI323 IMU step counter %d", ret);
        fault_handler::send_diagnostics(IMU_FAULT);
    }

    // Create IMU semaphore to handle the interrupt handler
    k_sem_init(&imu_sensor_struct.imu_trigger_sem, 0, K_SEM_MAX_LIMIT);

    // Create IMU sensor streaming thread
    imu_sensor_thread_id = k_thread_create(&imu_sensor_thread, imu_sensor_stack,
                                            K_THREAD_STACK_SIZEOF(imu_sensor_stack),
                                            imu_sensor_thread_func, NULL, NULL, NULL,
                                            2, K_USER, K_NO_WAIT);

    k_thread_name_set(imu_sensor_thread_id, "imu");
    
    // Stop sensor thread until session starts
    imu_sensor_struct.imu_state = IMU_STATE_STOP;
    ret = sh_imu_sensor::stop_sensing();
    if(ret != 0) {
        LOG_ERR("Failed to Start sensor %d.\n",ret);
    }
    return ret;
}
#endif // CONFIG_SH_BMI323

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined (CONFIG_BMA456)
#include <sh/bma456.h>
LOG_MODULE_REGISTER(imu_sensor_thread, LOG_LEVEL_INF);

/* ZBUS for sharing imu data */
ZBUS_SUBSCRIBER_DEFINE(imu_sensor_sub, 4);
K_PIPE_DEFINE(imu_pipe, sizeof(sh_imu_sensor::msg_t)*2, 4);
K_PIPE_DEFINE(imu_segno_pipe, sizeof(sh_imu_sensor::msg_t)*40, 4);
ZBUS_CHAN_DEFINE(imu_chan, sh_imu_sensor::msg_t, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                    ZBUS_MSG_INIT(0));

ZBUS_CHAN_DEFINE(imu_segno_chan, sh_imu_sensor::segno_msg_t, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                    ZBUS_MSG_INIT(0));

ZBUS_CHAN_DEFINE(imu_step_chan, uint16_t , NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(0));

/* FIFO defines */
#define ACCEL_FIFO_TH_SAMPLES               ( 3 * 48 )
#define ACCEL_RTC_TICK_BTW_INT              ( 234 )
#define IMU_DATA_BUFFER_LEN                 ( 24 )
#define IMU_BUFFER_DIM                      (ACCEL_FIFO_TH_SAMPLES/3)
#define IMU_XY_DOWNSCALER                   ( 8 )

/* IMU state machine states */
typedef enum {
    IMU_STATE_IDLE,         // IMU idle state
    IMU_STATE_RECONFIG,     // IMU reconfiguration and reintialization state
    IMU_STATE_STREAMING,    // IMU streaming state
    IMU_STATE_START,
    IMU_STATE_STOP,
    IMU_STATE_FAULT
}imu_state_t;

typedef struct {
    uint8_t data[CONFIG_BMA456_WATERMARK * 2];
}imu_buf_fifo_t;

typedef struct {
    uint8_t data[120];
}imu_stream_data_t;

typedef struct {
    int16_t x[IMU_BUFFER_DIM/IMU_XY_DOWNSCALER];
    int16_t y[IMU_BUFFER_DIM/IMU_XY_DOWNSCALER];
    int16_t z[IMU_BUFFER_DIM];
} vs_imu_t;

/* Structure holding flags and variables related to IMU */
typedef struct {
    imu_buf_fifo_t      imu_buf_fifo_set[IMU_DATA_BUFFER_LEN * 2];
    imu_stream_data_t   imu_stream_data;
    k_sem               imu_trigger_sem;
    imu_state_t         imu_state;
    uint16_t            imu_fifo_buf_index;
    uint16_t            imu_buf_index;
    uint32_t            imu_step_count;
    uint8_t             imu_fail_cnt;
}imu_sensor_struct_t;

imu_sensor_struct_t imu_sensor_struct;
sh_imu_sensor::msg_t imu_data_s;
sh_imu_sensor::segno_msg_t imu_segno_data_s;
static int64_t imu_time_stamp_storage = 0;
static int64_t imu_time_stamp_streaming = 0;
static bool thread_suspend_flag = false;

#ifdef CONFIG_WATCHDOG
/* Watchdog Channel ID for IMU thread */
int8_t wdt_imu_channel_id = WATCHDOG_CHANNEL_DEFAULT_ID;
#endif


static k_tid_t imu_sensor_thread_id;
static struct k_thread imu_sensor_thread;
K_THREAD_STACK_DEFINE(imu_sensor_stack, 1024);

#ifdef CONFIG_WATCHDOG
/** 
 * @brief Start Stop watchdog for IMU threads
 *
 * This function starts the watchdog for thread when sensor has been started to 
 * monitor its health. Once the session has been stopped, watchdog of the threads
 * will be stopped.
 * 
 * @param start_watchdog: Start/Stop watchdog flag
 */
static void start_stop_wdt(bool start_watchdog) 
{
    LOG_INF("IMU Receiving start watchdog flag : %d and channel id is : %d", start_watchdog, wdt_imu_channel_id);

    if(start_watchdog) {
        // Starting watchdog for all sensor threads 
        if(wdt_imu_channel_id < 0) {
            wdt_imu_channel_id = task_wdt_add_thread(WATCHDOG_LOW_TIMEOUT_MSEC);
            LOG_INF("IMU watchdog enabled with channel id :%d", wdt_imu_channel_id);
        } else {
            LOG_INF("IMU watchdog already enabled");
        }
    } else {
        // Stop watchdog to monitor all sensor threads 
        if(wdt_imu_channel_id >= 0) {
            LOG_INF("IMU watchdog disabled with channel id :%d", wdt_imu_channel_id);
            task_wdt_delete_thread(wdt_imu_channel_id);
            wdt_imu_channel_id = WATCHDOG_CHANNEL_DEFAULT_ID;
        } else {
            LOG_INF("IMU watchdog already disabled ");
        }
    }
}
#endif


/**
 * @brief IMU interrupt trigger handler
 * 
 * @param dev 
 * @param trigger Sesnor trigger specification
 */
void imu_trigger_handler(const struct device *dev,
					 const struct sensor_trigger *trigger)
{
    int ret;
    uint16_t int_status;

#ifdef CONFIG_WATCHDOG
    if(wdt_imu_channel_id >= 0) {
        task_wdt_fed_from_thread(wdt_imu_channel_id);
    }
#endif 

    ret = bma456::read_interrupt_status(&int_status);
    if(ret < 0) {
        LOG_ERR("Failed to get the interrupt status");
    }

    if(int_status == BMA4_FIFO_WM_INT)
    {
        #if defined(CONFIG_SH_TIME_MODULE)
        // Give semaphore to IMU thread function
        if (app_time::get_time_ticks(TimeResolution::MILLISECOND_TICKS_4096, &imu_time_stamp_streaming) < 0) 
        {
            LOG_ERR("Failed to get time for streaming");
        }
        if (app_time::get_time_ticks(TimeResolution::MILLISECOND_TICKS_8192, &imu_time_stamp_storage) < 0) 
        {
            LOG_ERR("Failed to get time for storage");
        }
        #endif
        k_sem_give(&imu_sensor_struct.imu_trigger_sem);
    }
}

int8_t imu_read(void)
{
    int ret;
    uint16_t sample_to_read = 0;
    vs_imu_t imu_data = {0};


    ret = bma456::read_fifo_length(&sample_to_read);
    if(ret < 0) {
        return sh_imu_sensor::IMU_NON_RECOVERABLE_ERROR;
    }

    /* Get the step count value from IMU */
    bma456::read_step_counter(&imu_sensor_struct.imu_step_count);

    if(sample_to_read < CONFIG_BMA456_WATERMARK) {
        LOG_INF("IMU Sample Low expected: %d actual: %d", CONFIG_BMA456_WATERMARK, sample_to_read);
        return sh_imu_sensor::IMU_RECOVERABLE_ERROR;
    }
    else if(sample_to_read > (CONFIG_BMA456_WATERMARK * 2)) {
        LOG_INF("IMU FIFO Reset: %d",sample_to_read);
        bma456::reset_fifo();
        return sh_imu_sensor::IMU_RECOVERABLE_ERROR;
    }

    ret = bma456::read_fifo_data(imu_sensor_struct.imu_buf_fifo_set[imu_sensor_struct.imu_fifo_buf_index].data, CONFIG_BMA456_WATERMARK);
    if(ret < 0)
    {
        imu_sensor_struct.imu_state = IMU_STATE_FAULT;
    }

    uint8_t *current_imu_buf = imu_sensor_struct.imu_buf_fifo_set[imu_sensor_struct.imu_fifo_buf_index].data;
    uint8_t *current_page = imu_sensor_struct.imu_stream_data.data;

    int32_t x_temp_sum = 0;
    int32_t y_temp_sum = 0;
    imu_sensor_struct.imu_buf_index = 0;

    for (int i=0; i < (CONFIG_BMA456_WATERMARK/2/3); i++)
    {
        if ((i == 7) || (i == 15) || (i == 23))
        {
            x_temp_sum   += (int32_t) ((int16_t) uint16_decode(current_imu_buf + i * 6));
            y_temp_sum   += (int32_t) ((int16_t) uint16_decode(current_imu_buf + i * 6 + 2));
            //save into buffer here - average of 8 x and y and z complete
            imu_data.x[i/IMU_XY_DOWNSCALER]   = (uint16_t) ((x_temp_sum >> 3) & 0x0000FFFF);
            imu_data.y[i/IMU_XY_DOWNSCALER]   = (uint16_t) ((y_temp_sum >> 3) & 0x0000FFFF);
            imu_data.z[i] = uint16_decode(current_imu_buf + i * 6 + 4);
            memcpy(current_page + imu_sensor_struct.imu_buf_index, &imu_data.x[i/IMU_XY_DOWNSCALER], 2);
            memcpy(current_page + imu_sensor_struct.imu_buf_index + 2, &imu_data.y[i/IMU_XY_DOWNSCALER], 2);
            memcpy(current_page + imu_sensor_struct.imu_buf_index + 4, current_imu_buf + i * 6 + 4, 2);
            LOG_DBG("x = %d, y = %d, z = %d", imu_data.x[i/IMU_XY_DOWNSCALER], imu_data.y[i/IMU_XY_DOWNSCALER], imu_data.z[i]);
            
            imu_sensor_struct.imu_buf_index += 6;
            
            x_temp_sum = 0;
            y_temp_sum = 0;
        }
        else
        {
            x_temp_sum   += (int32_t) ((int16_t) uint16_decode(current_imu_buf + i * 6));
            y_temp_sum   += (int32_t) ((int16_t) uint16_decode(current_imu_buf + i * 6 + 2));
            imu_data.z[i] = uint16_decode(current_imu_buf + i * 6 + 4);
            memcpy(current_page + imu_sensor_struct.imu_buf_index, current_imu_buf + i * 6 + 4, 2);
            LOG_DBG("z = %d", imu_data.z[i]);
            imu_sensor_struct.imu_buf_index += 2;
        }
    }

    if(imu_sensor_struct.imu_buf_index >= IMU_DATA_LEN) {
        imu_sensor_struct.imu_buf_index = 0;
    }

    //imu_epoch_timestamp += ACCEL_RTC_TICK_BTW_INT;

    imu_data_s.type = 1;
    imu_data_s.length = IMU_DATA_LEN;
    imu_data_s.timestamp_storage = imu_time_stamp_storage;
    imu_data_s.timestamp_streaming = imu_time_stamp_streaming;

    // imu_data_s.timestamp = imu_epoch_timestamp;
    memcpy(imu_data_s.data, imu_sensor_struct.imu_stream_data.data, IMU_DATA_LEN);

#if CONFIG_PIPES
    /* send data to the consumers */
    size_t bytes_written = 0;
    ret = k_pipe_put(&imu_pipe, (void*) &imu_data_s, sizeof(imu_data_s), &bytes_written,
                    sizeof(imu_data_s), K_NO_WAIT);

    if (ret < 0) {
        /* Incomplete message header sent */
        // LOG_ERR("Error when data was sent through mic pipe: %d", ret);
    } else if (bytes_written < sizeof(imu_data_s)) {
        /* Some of the data was sent */
        LOG_ERR("Not all data was sent through mic pipe");
    }
#else
    // Publish IMU data ove zbus channel
    ret = zbus_chan_pub(&imu_chan, &imu_data_s, K_NO_WAIT);
    if(ret != 0) {
        LOG_ERR("Failed to publish IMU data = %d", ret);
    }
#endif
    k_sem_give(&mem_mngr_sem);
#if CONFIG_IMU_STREAMING
    k_sem_give(&stream_sem);
#endif

#ifdef CONFIG_SEGNO_LIBRARY
    // Publish IMU data over zbus channel for segno library
#if CONFIG_PIPES

    size_t s_bytes_written = 0;
    ret = k_pipe_put(&imu_segno_pipe, (void*) &imu_data_s, sizeof(imu_data_s), &s_bytes_written,
                    sizeof(imu_data_s), K_NO_WAIT);

    if (ret < 0) {
        /* Incomplete message header sent */
        // LOG_ERR("Error when data was sent through mic pipe: %d", ret);
    } else if (s_bytes_written < sizeof(imu_data_s)) {
        /* Some of the data was sent */
        LOG_ERR("Not all data was sent through mic pipe");
    }
#else
    ret = zbus_chan_pub(&imu_segno_chan, &imu_data_s, K_FOREVER);
    if(ret != 0) {
        LOG_ERR("Failed to publish IMU data = %d", ret);
    }
#endif
    k_sem_give(&segno_mngr_sem);
#endif
    ret = zbus_chan_pub(&imu_step_chan, (const void *)imu_sensor_struct.imu_step_count, K_NO_WAIT);
    if(ret != 0) {
        LOG_ERR("Failed to publish IMU step count = %d", ret);
    }

    imu_sensor_struct.imu_fifo_buf_index++;

    if(imu_sensor_struct.imu_fifo_buf_index < (IMU_DATA_BUFFER_LEN - 1)) {
        imu_sensor_struct.imu_fifo_buf_index += 1;
    }
    else {
        imu_sensor_struct.imu_fifo_buf_index = 0;
    }

    return 0;
}

/**
 * @brief IMU sensor thread module. This handles IMU sensor operations and
 *        data publishing over zbus.
 */
void imu_sensor_thread_func(void *arg1, void *arg2, void *arg3)
{
    int32_t ret;

    while(1)
    {
        if(thread_suspend_flag)
        {
            thread_suspend_flag = false;
            LOG_ERR("imu thread suspend");
            k_thread_suspend(&imu_sensor_thread);
        }
        switch(imu_sensor_struct.imu_state)
        {
        case IMU_STATE_IDLE:
            k_sleep(K_SECONDS(5));
            imu_sensor_struct.imu_state = IMU_STATE_RECONFIG;
            break;
        
        case IMU_STATE_STREAMING:
            k_sem_take(&imu_sensor_struct.imu_trigger_sem, K_MSEC(500));   
            ret = imu_read();
            if(ret < 0) {
                LOG_ERR("Failed to read the IMU trigger data.");
                imu_sensor_struct.imu_fail_cnt++;
                if(imu_sensor_struct.imu_fail_cnt >= IMU_MAX_FAIL_CNT)
                {
                    imu_sensor_struct.imu_state = IMU_STATE_FAULT;
                }
            }
            break;

        case IMU_STATE_RECONFIG:
            imu_sensor_struct.imu_state = IMU_STATE_STOP;
            LOG_DBG("sh_imu_sensor :: Reconfiguring IMU");
            break;

        case IMU_STATE_START:
            ret = bma456::enable_imu();
            if(ret < 0)
            {
                imu_sensor_struct.imu_state = IMU_STATE_FAULT;
            }
            else{
                LOG_DBG("sh_imu_sensor :: Start IMU");
                imu_sensor_struct.imu_fail_cnt = 0;
                imu_sensor_struct.imu_state = IMU_STATE_STREAMING;
            }
            break;

        case IMU_STATE_STOP:
            bma456::disable_imu();
            bma456::reset_fifo();
            LOG_DBG("sh_imu_sensor :: Stop IMU");
            imu_sensor_struct.imu_state = IMU_STATE_START;
            break;

        case IMU_STATE_FAULT:
            
            imu_data_s.type = 0xFF; // send signal to stop the session

            // Publish IMU data ove zbus channel
            ret = zbus_chan_pub(&imu_chan, &imu_data_s, K_FOREVER);
            if(ret != 0) {
                LOG_ERR("Failed to publish IMU data = %d", ret);
                
            }
            k_sem_give(&mem_mngr_sem);

            fault_handler::send_diagnostics(IMU_FAULT);
            sh_imu_sensor::stop_sensing();
            break;
        
        default:
            break;
        }
    }
}

/**
 * @brief This function will resume the sensor thread and 
 * start the measurement of sensor data
 *
 * @return uint32_t Success indicates 0 and failure is non-zero.
 */
int sh_imu_sensor::start_sensing(uint16_t accl_odr)
{
    int ret = 0;
    bool low_odr_set = false;
    LOG_DBG("sh_imu_sensor :: start_sensing");

    #ifdef CONFIG_SEGNO_LIBRARY
    if(sh_segno_thread::check_body_detect_status()) {
        sh_segno_thread::config_imu_segno_w_low_odr();
        low_odr_set = true;
    }
    #endif
    //If low odr was set already do not change the sampling rate
    if (!low_odr_set) {
        //If zero, accel should be set to default value
        if (accl_odr == 0 ) {
            accl_odr = SH_BMI456_DEFAULT_ACCEL_ODR;
        }

        ret = bma456::change_accl_odr(accl_odr);
        if (ret < 0)
        {
            LOG_ERR("Failed to change accl ODR of BMA456 sensor %d", ret);
        }
        //Delay in order for ODR change to go through
        k_msleep(5);
    }
    
    ret = bma456::imu_resume_power();
    if(ret < 0) {
        LOG_ERR("Failed to Suspend BMA456 sensor power %d", ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to Suspend BMA456 sensor power %d", ret);
#endif 
    }

    thread_suspend_flag = false;
    imu_sensor_struct.imu_state = IMU_STATE_START;
    k_thread_resume(&imu_sensor_thread);

#ifdef CONFIG_WATCHDOG
    // Turn on watchdog channel 
    start_stop_wdt(true);
#endif

    return ret;
}

/**
 * @brief This function will suspend the running sensor thread and 
 * stop the the sensor measurement
 *
 * @return uint32_t Success indicates 0 and failure is non-zero.
 */
int sh_imu_sensor::stop_sensing(void)
{
    int ret = 0;

    LOG_DBG("sh_imu_sensor :: stop_sensing");
    // k_thread_suspend(&imu_sensor_thread);
    thread_suspend_flag = true;


#ifdef CONFIG_WATCHDOG
    // Turn off watchdog channel 
    start_stop_wdt(false);
#endif

    ret = bma456::imu_suspend_power();
    if(ret < 0) {
        LOG_ERR("Failed to Suspend BMA456 sensor power %d", ret);
    }

    return ret;
}

/**
 * @brief Load the IMU parameters from the NVS 
 * 
 * @return none
 */
static void load_bma_parameters(sh_bma456_user_config_t *bma456_config)
{
    bma456_config->bma_acc_odr = sh_imu_config::get_acc_odr();
    bma456_config->bma_acc_mode = sh_imu_config::get_acc_mode();
    bma456_config->bma_acc_range = sh_imu_config::get_acc_range();
    bma456_config->bma_gyro_odr = sh_imu_config::get_gyro_odr();
    bma456_config->bma_gyro_mode = sh_imu_config::get_gyro_mode();
    bma456_config->bma_gyro_range = sh_imu_config::get_gyro_range();
    bma456_config->handler = (sensor_trigger_handler_t)imu_trigger_handler;
}

/**
 * @brief Initialization of IMU sensor streaming thread and adds zbus
 *        observers for IMU channel
 * 
 * @return uint32_t 0 = Success, Non-zero = Failure
 */
uint32_t sh_imu_sensor::init(void)
{
    int ret = 0;
    imu_sensor_struct.imu_state = IMU_STATE_IDLE;
    imu_sensor_struct.imu_fifo_buf_index = 0;
    imu_sensor_struct.imu_buf_index = 0;
    sh_bma456_user_config_t bma456_config;

    // Check if IMU is enabled from the NVS saved configurations
    if(sh_imu_config::get_enabled() == IMU_DISABLE) {
        LOG_ERR("IMU config disabled in the NVS. Cannot initialize IMU driver");
        return -1;
    }

    load_bma_parameters(&bma456_config);

    ret = bma456::init(&bma456_config);
    if(ret != 0) {
        LOG_ERR("Failed to initialize BMA456 IMU sensor %d", ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to initialize BMA456 IMU sensor %d", ret);
#endif 
		fault_handler::send_diagnostics(IMU_FAULT);
        return ret;
    }

    // Create IMU sensor interrupt semaphore
     k_sem_init(&imu_sensor_struct.imu_trigger_sem, 0, K_SEM_MAX_LIMIT);

    // Create IMU sensor streaming thread
    imu_sensor_thread_id = k_thread_create(&imu_sensor_thread, imu_sensor_stack,
                                            K_THREAD_STACK_SIZEOF(imu_sensor_stack),
                                            imu_sensor_thread_func, NULL, NULL, NULL,
                                            2, K_USER, K_NO_WAIT);

    k_thread_name_set(imu_sensor_thread_id, "imu");
    
    // Stop sensor thread until session starts
    imu_sensor_struct.imu_state = IMU_STATE_STOP;
    ret = sh_imu_sensor::stop_sensing();
    if(ret != 0) {
        LOG_ERR("Failed to Start sensor %d.\n",ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to Start sensor %d.\n",ret);
#endif 
    }
    return ret;
}
#endif // CONFIG_BMA456
