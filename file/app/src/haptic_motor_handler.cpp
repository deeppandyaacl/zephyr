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

#if defined (CONFIG_DRV2624)

// Zephyr modules 
#include "haptic_motor_handler.h"
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "segno_thread.h"
#include <sh/sh_data_logger.h>
#include <sh/sh_time.h>
#include <mem_mgr_thread.h>
#include <sh/sh_shrd_data.h>
#include "fault_handler.h"
#include "memfault/core/trace_event.h"

LOG_MODULE_REGISTER(haptic_motor_handler, LOG_LEVEL_INF);

K_MUTEX_DEFINE(mutex_haptic_feedback);

typedef struct 
{
    /* Haptic status variable */
    uint8_t haptic_enable;
    bool haptic_status;
    uint16_t haptic_interval;
    uint16_t haptic_duration;

    /* pulse width for haptic pattern */
    uint16_t pulse_width;
}haptic_app_parameters;

static haptic_app_parameters haptic_param;

/* Handler function definition */
static void haptic_duration_handler(struct k_timer *timer_id);
static void haptic_start_handler(struct k_timer *timer_id);
void haptic_work_handler(struct k_work *work);

/* Get the drv2524 instance from drv2624 */
const struct device *const dev = DEVICE_DT_GET_ANY(ti_drv2624);

/* Define start timer for haptic pattern */
K_TIMER_DEFINE(haptic_duration_timer, haptic_duration_handler, NULL);

/* Define start timer for haptic pattern */
K_TIMER_DEFINE(haptic_start_timer, haptic_start_handler, NULL);

/* Define work to handle the pulse action */
K_WORK_DEFINE(haptic_pulse_work, haptic_work_handler);

static int process_haptic_cmnd(haptic_params_t* param)
{
    sh_shrd_evt_type event = HAPTIC_FEATURE_ENABLE_EVT;
    haptic_param.haptic_interval = param->interval;
    haptic_param.haptic_duration = param->duration;

    switch(param->index) {
    case HAPTIC_INDEX_ENABLE:
        {
            haptic_param.haptic_enable = 1;
            int ret = sh_mem_mngr::store_events_to_shrd(&event);
            if(ret) {
                LOG_ERR("Failed to store haptic feature events : %d", ret);
            }
            sh_data_logger::update_haptic_enable_status(haptic_param.haptic_enable, haptic_param.haptic_interval, haptic_param.haptic_duration);
        }
        break;
    
    case HAPTIC_INDEX_MANUAL_OFF:
        LOG_INF("Haptic pattern is turned off");
        k_timer_stop(&haptic_start_timer);
        k_timer_stop(&haptic_duration_timer);
        haptic_param.haptic_status = false;
        break;

    case HAPTIC_INDEX_CUSTOM_PULSE_PATTERN:
        /* Set the pulse width and intensity for haptic driver */
        if((param->pulse_width == 0) && (param->interval == 0)) {
            LOG_ERR("The pulse width and interval values are invalid");
#if defined(CONFIG_MEMFAULT) 
            // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"The pulse width and interval values are invalid");
#endif 
            return -1;
        }
        else {
            /* Set the haptic driver mode to run it in RTP mode */
            drv2624_set_mode(dev, DRV262x_mode_RTP);

            /* set the pulse width and intensity */
            haptic_param.pulse_width = param->pulse_width;
            drv2624_set_rtp_input(dev, param->intensity);
        }

        /* Start the intervala and duration timer */
        k_timer_start(&haptic_start_timer, K_SECONDS(0), K_SECONDS(param->interval));
        if(param->duration != 0) {
            k_timer_start(&haptic_duration_timer, K_SECONDS(param->duration), K_SECONDS(0));
        }

        haptic_param.haptic_status = true;

        break;

    case HAPTIC_INDEX_BUILT_IN_PULSE_1:
    case HAPITC_INDEX_CUSTOM_WAVEFORM_PATTERN_1:
        DRV262x_WaveformPlaylist_t playlist;
        memset(&playlist, 0, sizeof(DRV262x_WaveformPlaylist_t));

        /* Set the hatpic driver mode to run it in waveform sequence mode */
        drv2624_set_mode(dev, DRV262x_mode_waveformSeq);

        /* Load the waveform seuqence register */
        playlist.waveformList[0].b.bDelay = 0;
        playlist.waveformList[0].b.bParam = param->index;

        /* Setting waveform reqpeat values */
        playlist.waveformSeqLoop1.b.bWaveformSeqLoop1 = 0;
        playlist.waveformSeqLoop1.b.bWaveformSeqLoop2 = 0;
        playlist.waveformSeqMainLoop.b.bWaveSeqMainLoop = 0;

        /* Fire the playlist */
        if(drv2624_load_waveform_playlist(dev, &playlist) < 0) {
            LOG_ERR("Failed to load the waveform playlist");
#if defined(CONFIG_MEMFAULT) 
            // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to load the waveform playlist");
#endif 
            return -1;
        }

        /* Start the interval and duration timer */
        if(param->interval == 0) {
             drv2624_set_go(dev);
        }
        else {
            k_timer_start(&haptic_start_timer, K_SECONDS(0), K_SECONDS(param->interval));
        }

        if(param->duration != 0) {
            k_timer_start(&haptic_duration_timer, K_SECONDS(param->duration), K_SECONDS(0));
        }

        haptic_param.haptic_status = true;

        break;

    case HAPTIC_INDEX_SWALLOW_PARAMETER_CHANGE:
        sh_data_logger::update_haptic_enable_status(haptic_param.haptic_enable, haptic_param.haptic_interval, haptic_param.haptic_duration);
    break;

    case HAPTIC_INDEX_DISABLE:
    {
        haptic_param.haptic_enable = 0;
        event = HAPTIC_FEATURE_DISABLE_EVT;
        int ret = sh_mem_mngr::store_events_to_shrd(&event);
        if(ret) {
            LOG_ERR("Failed to store haptic feature events : %d", ret);
        }
        sh_data_logger::update_haptic_enable_status(haptic_param.haptic_enable, haptic_param.haptic_interval, haptic_param.haptic_duration);
    }
    break;

    default:
        LOG_ERR("Haptic pattern index is not valid");
        break;

    }

    return 0;
}

int sh_haptic_motor_handler::operation(sh_haptic_motor_handler::type_t operation, haptic_params_t *haptic_parameters)
{
    int ret = 0;
    uint8_t haptic_enable = 0;
    uint16_t duration = 0;
    uint16_t interval = 0;
    haptic_params_t haptic_param;
    sh_shrd_evt_type event = HAPTIC_ENABLE_EVT;
    memset(&haptic_param, 0, sizeof(haptic_params_t));

    //TODO: Move below impelementation in NVS
    sh_data_logger::get_haptic_enable_status(&haptic_enable, &duration, &interval);
   
    switch(operation)
    {
        case ON:
        {
            haptic_param.index = HAPTIC_INDEX_BUILT_IN_PULSE_1;
        }
        break;

        case OFF:
        {
            event = HAPTIC_DISABLE_EVT;
            haptic_param.index = HAPTIC_INDEX_MANUAL_OFF;
        }
        break;

        case SWALLOW:
        {
            haptic_param.index = HAPTIC_INDEX_BUILT_IN_PULSE_1;
            
            #ifdef CONFIG_HAPTIC_SWALLOW_DETECT_PROGRAM
            if(duration == 0 || interval == 0)
            {
                duration = CONFIG_HAPTIC_DEFAULT_DURATION;
                interval = CONFIG_HAPTIC_DEFAULT_INTERVAL;
            }
            #endif

            haptic_param.duration = duration;
            haptic_param.interval = interval;
        }
        break;

        case SCRATCH:
        {
            haptic_param.index = HAPTIC_INDEX_BUILT_IN_PULSE_1;
        }
        break;

        case CUSTOM:
        {
            LOG_INF("Set custom haptic configuration");
            memcpy(&haptic_param, haptic_parameters, sizeof(haptic_params_t));
            k_mutex_lock(&mutex_haptic_feedback, K_MSEC(10));
            ret = process_haptic_cmnd(&haptic_param);
            if(ret) {
                LOG_ERR("Failed to start/stop the Haptic pattern (err = %d)", ret);
#if defined(CONFIG_MEMFAULT) 
                // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to start/stop the Haptic pattern (err = %d)", ret);
#endif 
            }
            k_mutex_unlock(&mutex_haptic_feedback);
            return ret;
        }
        break;

        default:
        LOG_ERR("Haptic invalid opertaions");
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Haptic invalid opertaions");
#endif 
        break;
    }
    
    if(haptic_enable)
    {
        ret = sh_mem_mngr::store_events_to_shrd(&event);
        if(ret) {
            LOG_ERR("Failed to store timestamp event : %d", ret);
#if defined(CONFIG_MEMFAULT) 
            // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to store timestamp event : %d", ret);
#endif 
        }
        k_mutex_lock(&mutex_haptic_feedback, K_MSEC(10));
        ret = process_haptic_cmnd(&haptic_param);
        if(ret) {
            LOG_ERR("Failed to start/stop the Haptic pattern (err = %d)", ret);
#if defined(CONFIG_MEMFAULT) 
            // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to start/stop the Haptic pattern (err = %d)", ret);
#endif 
            fault_handler::send_diagnostics(HAPTIC_FAULT);
        }
        k_mutex_unlock(&mutex_haptic_feedback);
    }
    // else
    // {
    //     LOG_ERR("haptic is not enabled");
    //     event = HAPTIC_FEATURE_ENABLE_EVT;
        
    //     int ret = sh_mem_mngr::store_events_to_shrd(&event);
    //     if(ret) {
    //         LOG_ERR("Failed to store haptic feature events : %d", ret);
    //     }
        
    //     sh_data_logger::update_haptic_enable_status(HAPTIC_FEATURE_ENABLE_EVT, interval, duration);
    // }
    return ret;
}

uint8_t sh_haptic_motor_handler::get_haptic_status(void)
{
    uint8_t status = 0;
    uint16_t interval = 0;
    uint16_t duration = 0;
    //TODO: Move below impelementation in NVS
    sh_data_logger::get_haptic_enable_status(&status, &interval, &duration);
    return status;
}

/**
 * @brief Handler to stop the haptic pattern after duration timer is
 *        expired.
 *        Function stops all running timers.
 * 
 * @param timer_id 
 */
static void haptic_duration_handler(struct k_timer *timer_id)
{
     LOG_INF("Haptic pattern is turned off");
     k_timer_stop(&haptic_start_timer);
     k_timer_stop(&haptic_duration_timer);
}

/**
 * @brief Handler to execute the pulse after the interval timer
 *        is expired
 * 
 * @param timer_id 
 */
static void haptic_start_handler(struct k_timer *timer_id)
{
     k_work_submit(&haptic_pulse_work);
}

/**
 * @brief Handler to trigger the haptic pulse
 * 
 * @param work 
 */
void haptic_work_handler(struct k_work *work)
{
    uint8_t haptic_enable = 0;
    uint16_t duration = 0;
    uint16_t interval = 0;
    sh_data_logger::get_haptic_enable_status(&haptic_enable, &duration, &interval);

    if(haptic_enable)
    {
        sh_shrd_evt_type event = HAPTIC_ENABLE_EVT;
        int rc = drv2624_set_go(dev);
        if(haptic_param.pulse_width) {
            k_msleep(haptic_param.pulse_width);
            rc = drv2624_set_stop(dev);
        }

        //Store Haptic pulse event into SHRD
        int ret = sh_mem_mngr::store_events_to_shrd(&event);
        if(ret) {
            LOG_ERR("Failed to store timestamp event : %d", ret);
    #if defined(CONFIG_MEMFAULT) 
            // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to store timestamp event : %d", ret);
    #endif 
        }
    }
    else
    {
        LOG_ERR("Haptic is not enabled");
    }
}

bool sh_haptic_motor_handler::get_haptic_pattern_status(void)
{
    return haptic_param.haptic_status;
}

#endif // CONFIG_DRV2624