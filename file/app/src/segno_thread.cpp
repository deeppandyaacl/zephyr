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

#if defined(CONFIG_SEGNO_LIBRARY)

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#if defined CONFIG_SH_BMI323
#include <sh/sh_bmi323.h>
#else
#include <sh/bma456.h>
#endif

#include <sh/sh_time.h>

#include "segno_runtime.h"
#include "segno_thread.h"
#include "imu_sensor_thread.h"
#include "mem_mgr_thread.h"
#include "haptic_motor_handler.h"
#include "data_storage.h"
#include "shrd_config.h"

LOG_MODULE_REGISTER(segno_thread, LOG_LEVEL_INF);

/* Segno subscriber channel */
ZBUS_MSG_SUBSCRIBER_DEFINE(segno_imu_sub);

#if ((CONFIG_SEGNO_WEAR_DETECT_PROGRAM) || (CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM) || (CONFIG_SEGNO_ANGLE_DETECT_PROGRAM)||(CONFIG_SEGNO_SCRATCH_PROGRAM))
ZBUS_CHAN_DEFINE(segno_chan, sh_segno_thread::msg_t, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                    ZBUS_MSG_INIT(0));
#endif

#if (CONFIG_SEGNO_SCRATCH_PROGRAM)
#define SEGNO_SCRATCH_DEBOUNCE      ( 250 )
#endif

/* segno state machine states */
typedef enum {
    SEGNO_STATE_IDLE,         // segno idle state
    SEGNO_STATE_IMU_COMPUTEING,     // segno imu data computing
    SEGNO_STATE_FAULT         // this id for future
}segno_state_t;

typedef struct{
    float prev_scratch_val;
    uint8_t imu_feed_stop;
    uint8_t haptic_debounce;
    uint8_t activate_feedback;
    int64_t segno_time_stamp_storage;
}segno_haptic_status;

typedef struct{
    #ifdef CONFIG_SEGNO_WEAR_DETECT_PROGRAM
    uint8_t wear_detct_cnt;
    #endif
    #ifdef CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM
    uint8_t activity_detct_cnt;
    #endif
    #ifdef CONFIG_SEGNO_ANGLE_DETECT_PROGRAM
    uint8_t angle_detect_cnt;
    #endif
}segno_event_timer_cnt;

#if ((CONFIG_SEGNO_WEAR_DETECT_PROGRAM) || (CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM) || (CONFIG_SEGNO_ANGLE_DETECT_PROGRAM))
static segno_event_timer_cnt segno_timer_cnt = {0};
#endif

// Timer handler function
void segno_event_timer_handler(struct k_timer *arg);

// Define segno vital monitor timer
K_TIMER_DEFINE(segno_event_timer, segno_event_timer_handler, NULL);

/* Define the application thread details */
static k_tid_t segno_thread_id;
static struct k_thread segno_thread;
K_THREAD_STACK_DEFINE(segno_thread_stack, 2048);

/* Kernel semaphore definition for segno manager */
K_SEM_DEFINE(segno_mngr_sem, 0, 10);

/* Define work to store all the segno related activity */
static void segno_data_store_work_handler(struct k_work *work);
K_WORK_DEFINE(k_work_segno_data_store, segno_data_store_work_handler);

/* Signal */
segno_runtime * segno_processor;

/* Global */
sh_segno_thread::msg_t scratch_detect;
segno_haptic_status haptic_stats;

#if defined (CONFIG_SH_BMI323)

#ifndef SH_BMI323_DEFAULT_ACCEL_RANGE_RUNTIME
ftr_precision imu_scaler = (ftr_precision) (SH_BMI323_DEFAULT_ACCEL_RANGE / 32768.0f);
#else
ftr_precision imu_scaler = (ftr_precision) (4.0f / 32768.0f);
#endif

#endif // (CONFIG_SH_BMI323)

#if defined (CONFIG_BMA456)
#ifndef BMA456_DEFAULT_ACCEL_ODR_RUNTIME
ftr_precision imu_scaler = (ftr_precision) ((ftr_precision)4.0 / 32768.0f);
#else
ftr_precision imu_scaler = (ftr_precision) (4.0f / 32768.0f);
#endif

#endif

/**
 * @brief Function passes x, y and z data to segno library
 * 
 * @param raw_imu_buf Pointer to the data buffer
 */
#if defined (CONFIG_BMA456)
float previous_timestamp = -1/1600;
#else
#ifndef SH_BMI323_DEFAULT_ACCEL_ODR_RUNTIME
float previous_timestamp = -1/SH_BMI323_DEFAULT_ACCEL_ODR;
#else
float previous_timestamp = -1/1600;
#endif
#endif

static shrd_config_t g_shrd_config_data;

static bool thread_suspend_flag = false;
static segno_state_t segno_state = SEGNO_STATE_IDLE;

static void feed_accel_to_segno(sh_imu_sensor::msg_t *msg)
{
    ftr_precision x;
    ftr_precision y;
    ftr_precision z;

    for(int i = 0; i < (IMU_DATA_LEN / 20); i++) {
        x = (ftr_precision)((int16_t)uint16_decode(&msg->data[(i * 20) + 14])) * imu_scaler;
        y = (ftr_precision)((int16_t)uint16_decode(&msg->data[(i * 20) + 16])) * imu_scaler;

        for(int j = 0; j < (IMU_DATA_LEN / 3); j+=2) {
            if(j != 14 && j!= 16) {
                z = (ftr_precision)((int16_t)uint16_decode(&msg->data[( i * 20 ) + j])) * imu_scaler;
                segno_runtime_feed_accl_signal(segno_processor, 0, x, y, z);
            }
        }
    }
}


/**
 * @brief Function to initialize signal processor and activate necessary
 *        agent
 * 
 */
static void init_signal_processor(void)
{
    /* Initialize signal processor */
    segno_processor = segno_runtime_make();

#if defined(CONFIG_SEGNO_SCRATCH_PROGRAM)
    if(g_shrd_config_data.scratch)
    {
    /* Activate the scratch model agent */
#ifdef CONFIG_SH_BMI323   
    segno_runtime_activate_agent(segno_processor, SEGNO_AGENT_TYPE_ARIA_SCRATCH_MODEL, SH_BMI323_DEFAULT_ACCEL_ODR);
#else
    segno_runtime_activate_agent(segno_processor, SEGNO_AGENT_TYPE_ARIA_SCRATCH_MODEL, SH_BMI456_DEFAULT_ACCEL_ODR);
#endif
    }
#endif /* CONFIG_SEGNO_SCRATCH_PROGRAM */

#if ((CONFIG_SEGNO_WEAR_DETECT_PROGRAM) || (CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM) || (CONFIG_SEGNO_ANGLE_DETECT_PROGRAM))
    if((g_shrd_config_data.activity) || (g_shrd_config_data.arm_angle) || (g_shrd_config_data.imu_aggregated))
    {
    /* Activate the limb sleep model agent */
    segno_runtime_activate_agent(segno_processor, SEGNO_AGENT_TYPE_LIMB_SLEEP, 0);
    }
#endif

    if((g_shrd_config_data.scratch) || (g_shrd_config_data.activity) || 
            (g_shrd_config_data.arm_angle) || (g_shrd_config_data.imu_aggregated))
    {
#ifdef CONFIG_SH_BMI323
    segno_runtime_set_sampling_rate(segno_processor, SEGNO_CHANNEL_TYPE_ACCL, SH_BMI323_DEFAULT_ACCEL_ODR);
#else
    segno_runtime_set_sampling_rate(segno_processor, SEGNO_CHANNEL_TYPE_ACCL, SH_BMI456_DEFAULT_ACCEL_ODR);
#endif
    }
}

/**
 * @brief Activate haptic feedbakc based on the flag set by the scratch detection
 * 
 */
static void activate_haptic_feedback(void)
{
    if(haptic_stats.activate_feedback) {
#if defined(CONFIG_DRV2624)
        /* Activate haptic feedback */
        LOG_INF("Activating haptic scratch feedback");
        if(sh_haptic_motor_handler::operation(sh_haptic_motor_handler::type_t::SCRATCH, NULL) != 0) 
        {
            LOG_ERR("Not able to start haptic motor");
        }
#endif
        haptic_stats.activate_feedback = false;
    }
}

/** 
 * @brief Application thread module.
 *
 * This function collects the structure from the different ZBUS channels
 * and handle the operations accordingly. 
 */
void segno_thread_func(void *arg1, void *arg2, void *arg3)
{
    const struct zbus_channel *chan;
    int ret = 0;

    memset(&haptic_stats, 0, sizeof(segno_haptic_status));

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
    sh_imu_sensor::msg_t imu_segno_data;
    memset(&imu_segno_data, 0, sizeof(sh_imu_sensor::msg_t));
#endif 

    // Initialize the segno processer
    init_signal_processor();

    while(1)
    {
        switch(segno_state)
        {
            case SEGNO_STATE_IDLE:
            {
                if(thread_suspend_flag)
                {
                    //thread suspend when receive request
                    thread_suspend_flag = false;
                    LOG_ERR("segno thread suspend");
                    k_thread_suspend(&segno_thread);
                }
                else
                {
                    ret = k_sem_take(&segno_mngr_sem, K_FOREVER);
                    if(!ret)
                    {
                        segno_state = SEGNO_STATE_IMU_COMPUTEING;
                    }
                }
            }
            break;

            case SEGNO_STATE_IMU_COMPUTEING:
            {

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
                /* Process and feed the data to segno library */
#if CONFIG_PIPES
                size_t imu_bytes_read = 0;
                if(!k_pipe_get(&imu_segno_pipe, (void *)&imu_segno_data, sizeof(imu_segno_data), &imu_bytes_read, 
                    sizeof(imu_segno_data), K_NO_WAIT) && imu_bytes_read >= sizeof(imu_segno_data))
#else
                if(!zbus_sub_wait_msg(&segno_imu_sub, &chan, &imu_segno_data, K_NO_WAIT))
#endif
                {
                    if(!haptic_stats.imu_feed_stop) {
                        /* Feed recieved data to segno */
                        feed_accel_to_segno(&imu_segno_data);
                    }
                }
                /* Activate haptic feedback */
                activate_haptic_feedback();
#endif 
                segno_state = SEGNO_STATE_IDLE;
            }
            break;

            // default: 
            //     segno_state = SEGNO_STATE_IDLE;
            // break;
        }
    }
}

#ifdef CONFIG_SEGNO_SCRATCH_PROGRAM
static int check_scratch_status(int64_t *segno_time_stamp_storage)
{
    if(haptic_stats.imu_feed_stop) {
        haptic_stats.imu_feed_stop = false;
    }

    if(segno_time_stamp_storage == NULL)
    {
        LOG_ERR("Failed to get time for scratch detect");
        return -EINVAL;
    }

    scratch_detect.segno_data.scratch_data = 
    segno_runtime_get_vital(segno_processor, SEGNO_VITAL_TYPE_ARIA_SCRATCH);

    LOG_INF("Segno scratch data: %f", scratch_detect.segno_data.scratch_data);

    if(scratch_detect.segno_data.scratch_data > 0.5f) {
        LOG_INF("Scratch event detected");

        if(haptic_stats.prev_scratch_val != scratch_detect.segno_data.scratch_data) {
            // Give feedback upon detection and wait for 3 seconds if
            // scratch continues
            if(haptic_stats.haptic_debounce % 4 == 0) {
                haptic_stats.activate_feedback = true;
                haptic_stats.imu_feed_stop = true;
                haptic_stats.haptic_debounce = 0;
            }
            haptic_stats.haptic_debounce++;
        }
    }
    else {
        haptic_stats.haptic_debounce = 0;
    }

    scratch_detect.length = sizeof(scratch_detect.segno_data.scratch_data);
    scratch_detect.type = sh_segno_thread::segno_eve_type::SEGNO_SCRATCH_DETECT;
    scratch_detect.timestamp_storage = *segno_time_stamp_storage;
    LOG_DBG("Segno scratch data: %f", scratch_detect.segno_data.scratch_data);

    // Publish scratch data over zbus channel
    int ret = zbus_chan_pub(&segno_chan, &scratch_detect, K_FOREVER);
    if(ret != 0) {
        LOG_ERR("Failed to publish scratch data = %d", ret);
    }
    k_sem_give(&mem_mngr_sem);

    haptic_stats.prev_scratch_val = scratch_detect.segno_data.scratch_data;

    memset(&scratch_detect, 0, sizeof(scratch_detect));

    return 0;
}
#endif

#ifdef CONFIG_SEGNO_WEAR_DETECT_PROGRAM
static int check_wear_detect_status(int64_t *segno_time_stamp_storage)
{
    sh_segno_thread::msg_t wear_detect;
    memset(&wear_detect, 0, sizeof(wear_detect));

    if(segno_time_stamp_storage == NULL)
    {
        LOG_ERR("Failed to get time for wear detect");
        return -EINVAL;
    }

    wear_detect.length = sizeof(wear_detect.segno_data.wear_detection);
    wear_detect.type = sh_segno_thread::segno_eve_type::SEGNO_WEAR_DETECT;
    wear_detect.timestamp_storage = *segno_time_stamp_storage;

    wear_detect.segno_data.wear_detection.mean_accl[0] = \
        segno_runtime_get_vital(segno_processor, SEGNO_VITAL_TYPE_LIMB_SLEEP_ACCL_MEAN_X);
    wear_detect.segno_data.wear_detection.mean_accl[1] = \
        segno_runtime_get_vital(segno_processor, SEGNO_VITAL_TYPE_LIMB_SLEEP_ACCL_MEAN_Y);
    wear_detect.segno_data.wear_detection.mean_accl[2] = \
        segno_runtime_get_vital(segno_processor, SEGNO_VITAL_TYPE_LIMB_SLEEP_ACCL_MEAN_Z);

    wear_detect.segno_data.wear_detection.min_accl[0] = \
        segno_runtime_get_vital(segno_processor, SEGNO_VITAL_TYPE_LIMB_SLEEP_ACCL_MIN_X);
    wear_detect.segno_data.wear_detection.min_accl[1] = \
        segno_runtime_get_vital(segno_processor, SEGNO_VITAL_TYPE_LIMB_SLEEP_ACCL_MIN_Y);
    wear_detect.segno_data.wear_detection.min_accl[2] = \
        segno_runtime_get_vital(segno_processor, SEGNO_VITAL_TYPE_LIMB_SLEEP_ACCL_MIN_Z);

    wear_detect.segno_data.wear_detection.max_accl[0] = 
        segno_runtime_get_vital(segno_processor, SEGNO_VITAL_TYPE_LIMB_SLEEP_ACCL_MAX_X);
    wear_detect.segno_data.wear_detection.max_accl[1] = 
        segno_runtime_get_vital(segno_processor, SEGNO_VITAL_TYPE_LIMB_SLEEP_ACCL_MAX_Y);
    wear_detect.segno_data.wear_detection.max_accl[2] = 
        segno_runtime_get_vital(segno_processor, SEGNO_VITAL_TYPE_LIMB_SLEEP_ACCL_MAX_Z);
    
    LOG_DBG("Segno Mean X : %f,Y : %f,Z : %f", wear_detect.segno_data.wear_detection.mean_accl[0],
        wear_detect.segno_data.wear_detection.mean_accl[1], wear_detect.segno_data.wear_detection.mean_accl[2]);
    
    LOG_DBG("Segno Min X : %f,Y : %f,Z : %f", wear_detect.segno_data.wear_detection.min_accl[0],
        wear_detect.segno_data.wear_detection.min_accl[1], wear_detect.segno_data.wear_detection.min_accl[2]);

    LOG_DBG("Segno Max X : %f,Y : %f,Z : %f", wear_detect.segno_data.wear_detection.max_accl[0],
        wear_detect.segno_data.wear_detection.max_accl[1], wear_detect.segno_data.wear_detection.max_accl[2]);

    // Publish wear data ove zbus channel
    int ret = zbus_chan_pub(&segno_chan, &wear_detect, K_FOREVER);
    if(ret != 0) {
        LOG_ERR("Failed to publish wear data = %d", ret);
    }
    k_sem_give(&mem_mngr_sem);

    return 0;
}
#endif

#ifdef CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM
static int check_activity_detect_status(int64_t *segno_time_stamp_storage)
{
    sh_segno_thread::msg_t activity_detect;
    memset(&activity_detect, 0, sizeof(activity_detect));

    if(segno_time_stamp_storage == NULL)
    {
        LOG_ERR("Failed to get time for activity detect");
        return -EINVAL;
    }

    activity_detect.length = sizeof(activity_detect.segno_data.activity_index);
    activity_detect.type = sh_segno_thread::segno_eve_type::SEGNO_ACTIVITY_INDEX_DETECT;
    activity_detect.timestamp_storage = *segno_time_stamp_storage;

    activity_detect.segno_data.activity_index = 
        segno_runtime_get_vital(segno_processor, SEGNO_VITAL_TYPE_LIMB_SLEEP_ACTIVITY);
    
    LOG_DBG("Segno activity index is %f", activity_detect.segno_data.activity_index);

    // Publish wear data ove zbus channel
    int ret = zbus_chan_pub(&segno_chan, &activity_detect, K_FOREVER);
    if(ret != 0) {
        LOG_ERR("Failed to publish activity index data = %d", ret);
    }
    k_sem_give(&mem_mngr_sem);

    return 0;
}
#endif

#ifdef CONFIG_SEGNO_ANGLE_DETECT_PROGRAM
static int check_angle_detect_status(int64_t *segno_time_stamp_storage)
{
    sh_segno_thread::msg_t angle_detect;
    memset(&angle_detect, 0, sizeof(angle_detect));

    if(segno_time_stamp_storage == NULL)
    {
        LOG_ERR("Failed to get time for wear detect");
        return -EINVAL;
    }

    angle_detect.length = sizeof(angle_detect.segno_data.angle_data);
    angle_detect.type = sh_segno_thread::segno_eve_type::SEGNO_ANGLE_DETECT;
    angle_detect.timestamp_storage = *segno_time_stamp_storage;

    angle_detect.segno_data.angle_data = 
        segno_runtime_get_vital(segno_processor, SEGNO_VITAL_TYPE_LIMB_SLEEP_ARM_ANGLE);
    
    LOG_DBG("Segno angle detect is %f", angle_detect.segno_data.angle_data);

     // Publish wear data ove zbus channel
    int ret = zbus_chan_pub(&segno_chan, &angle_detect, K_FOREVER);
    if(ret != 0) {
        LOG_ERR("Failed to publish angle detect data = %d", ret);
    }
    k_sem_give(&mem_mngr_sem);
    return 0;
}
#endif

static void segno_data_store_work_handler(struct k_work *work)
{
    #ifdef CONFIG_SEGNO_SCRATCH_PROGRAM
    if(g_shrd_config_data.scratch)
    {
    if(check_scratch_status(&haptic_stats.segno_time_stamp_storage) != 0)
    {
        LOG_ERR("Not able to get scratch data from segno lib");
    }
    }
    #endif

    #ifdef CONFIG_SEGNO_WEAR_DETECT_PROGRAM 
    if(segno_timer_cnt.wear_detct_cnt == SEGNO_WEAR_DETECT_CNT)
    {
        if(check_wear_detect_status(&haptic_stats.segno_time_stamp_storage) != 0)
        {
            LOG_ERR("Not able to get wear data from segno lib");
        }
        segno_timer_cnt.wear_detct_cnt = 0;
    }
    segno_timer_cnt.wear_detct_cnt++;
    #endif

    #ifdef CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM
    if(segno_timer_cnt.activity_detct_cnt == SEGNO_ACTIVITY_INDEX_CNT)
    {
        if(check_activity_detect_status(&haptic_stats.segno_time_stamp_storage) != 0)
        {
            LOG_ERR("Not able to get activity index data from segno lib");
        }
        segno_timer_cnt.activity_detct_cnt = 0;
    }
    segno_timer_cnt.activity_detct_cnt++;
    #endif

    #ifdef CONFIG_SEGNO_ANGLE_DETECT_PROGRAM
    if(segno_timer_cnt.angle_detect_cnt == SEGNO_ANGLE_DETECT_CNT)
    {
        if(check_angle_detect_status(&haptic_stats.segno_time_stamp_storage) != 0)
        {
            LOG_ERR("Not able to get angle detect data from segno lib");
        }
        segno_timer_cnt.angle_detect_cnt = 0;
    }
    segno_timer_cnt.angle_detect_cnt++;
    #endif
}

void segno_event_timer_handler(struct k_timer *arg)
{
    #if defined(CONFIG_SH_TIME_MODULE)
    if (app_time::get_time_ticks(TimeResolution::MILLISECOND_TICKS_8192, &haptic_stats.segno_time_stamp_storage) < 0) 
    {
        LOG_ERR("Failed to get time for wear detect");
    }
    #endif

   k_work_submit(&k_work_segno_data_store);
}

uint8_t sh_segno_thread::check_lead_on_status(void)
{
    uint8_t lead_on_status = 0;
    lead_on_status = (uint8_t) segno_runtime_get_vital(segno_processor, SEGNO_VITAL_TYPE_ON_BODY);
    LOG_DBG("Lead On = %d", lead_on_status);
    return lead_on_status;
}

void sh_segno_thread::start_sensing(void)
{
    if((g_shrd_config_data.scratch == 0) && (g_shrd_config_data.arm_angle == 0) && 
            (g_shrd_config_data.activity == 0) && (g_shrd_config_data.imu_aggregated == 0))
    {
        return;
    }
    LOG_INF("entry segno start");
    // Resume the sensor thread 
    k_thread_resume(&segno_thread);
    thread_suspend_flag = false;
    LOG_INF("segno start resume");

    //Start 1 seconds timer for segno events
    k_timer_start(&segno_event_timer, K_MSEC(200),  \
    K_MSEC(SEGNO_EVENT_TIMER_IN_MS));
    LOG_INF("segno start end");
   
}

void sh_segno_thread::stop_sensing(void)
{
    if((g_shrd_config_data.scratch == 0) && (g_shrd_config_data.arm_angle == 0) && 
            (g_shrd_config_data.activity == 0) && (g_shrd_config_data.imu_aggregated == 0))
    {
        return;
    }
    LOG_INF("entry segno stop");
    //Stop 1 second timer for segno events
    k_timer_stop(&segno_event_timer);
    LOG_INF("segno stop timer");

    // Suspend the sensor thread
    if(segno_state == SEGNO_STATE_IDLE) {
        LOG_INF("segno stop thread suspend");
        k_thread_suspend(&segno_thread);
    } else {
        LOG_INF("segno stop flag thread suspend");
        thread_suspend_flag = true; //set flag to suspend the thread when thread is in idle
    }
    

    //Clear the timer instances
    #ifdef CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM
    segno_timer_cnt.activity_detct_cnt = 0;
    #endif
    #ifdef CONFIG_SEGNO_ANGLE_DETECT_PROGRAM
    segno_timer_cnt.angle_detect_cnt = 0;
    #endif
    #ifdef CONFIG_SEGNO_WEAR_DETECT_PROGRAM
    segno_timer_cnt.wear_detct_cnt = 0;
    #endif

    LOG_INF("segno stop end");

}

//TODO: Call body detect API from segno library and also 
//enable the segno_runtime_activate_agent for lead on/off support
uint8_t sh_segno_thread::check_body_detect_status(void)
{
    return 0;
}

int sh_segno_thread::config_imu_segno_w_low_odr(void)
{
    segno_runtime_set_sampling_rate(segno_processor, SEGNO_CHANNEL_TYPE_ACCL, 25);
#ifdef CONFIG_SH_BMI323
    if(!sh_bmi323_set_low_odr()) {
        LOG_ERR("Failed to set IMU to LOW ODR");
        return -1;
    }
#endif

    return 0;
}

int sh_segno_thread::config_imu_segno_w_default_odr(void)
{
#if defined (CONFIG_SH_BMI323)
    segno_runtime_set_sampling_rate(segno_processor, SEGNO_CHANNEL_TYPE_ACCL, SH_BMI323_DEFAULT_ACCEL_ODR);
    if(!sh_bmi323_set_default_odr()) {
        LOG_ERR("Failed to set IMU to LOW ODR");
        return -1;
    }
#endif 

#if defined (CONFIG_BMA456)
    segno_runtime_set_sampling_rate(segno_processor, SEGNO_CHANNEL_TYPE_ACCL, SH_BMI456_DEFAULT_ACCEL_ODR);
#endif

    return 0;
}

uint32_t sh_segno_thread::init(void)
{
    uint32_t ret = 0;

    sh_shrd_config::get_shrd_conf(&g_shrd_config_data);
    sh_shrd_config::print_config_data();

    if((g_shrd_config_data.scratch == 0) && (g_shrd_config_data.arm_angle == 0) && 
            (g_shrd_config_data.activity == 0) && (g_shrd_config_data.imu_aggregated == 0))
    {
        //Swallow profile is selected hense not init the swallo profile
    }
    else
    {
        #if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
        // Add thread as a zbus observer to get accelerometer data
        ret = zbus_chan_add_obs(&imu_segno_chan, &segno_imu_sub, K_MSEC(200));
        if(ret){
            LOG_ERR("Failed to add thread as zbus observer to cmd chan (err: %d)", ret);
        }
        #endif 

        // Create the segno lib thread
        segno_thread_id = k_thread_create(&segno_thread, segno_thread_stack,
                K_THREAD_STACK_SIZEOF(segno_thread_stack),
                segno_thread_func, NULL, NULL, NULL,
                6, K_USER, K_NO_WAIT);

        sh_segno_thread::stop_sensing();
    }
    return ret;
}
#endif /* CONFIG_SEGNO_LIBRARY */
