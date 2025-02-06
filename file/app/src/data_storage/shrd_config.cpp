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

#if defined (CONFIG_NVS) && defined (CONFIG_DATA_LOGGER)

/* Zephyr include files */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Sibel SDK files */
#include <sh/sh_nvs_handler.h>

#include "shrd_config.h"
#include "imu_config.h"
#include "mic_config.h"
#include "barometer_config.h"

LOG_MODULE_REGISTER(sh_shrd_config, LOG_LEVEL_INF);

// SHRD structure instance
shrd_config_t g_shrd_config;

/**
 * @brief Load the SHRD config structure with default values
 *
 * @return None.
 */
void sh_shrd_config::load_default_shrd_conf(shrd_config_t *shrd_config)
{
    g_shrd_config.activity = SHRD_DISABLE_FEATURE;
    g_shrd_config.arm_angle = SHRD_DISABLE_FEATURE;
    g_shrd_config.baro = SHRD_DISABLE_FEATURE;
    g_shrd_config.cough = SHRD_DISABLE_FEATURE;
    g_shrd_config.events = SHRD_DISABLE_FEATURE;
    g_shrd_config.imu = SHRD_DISABLE_FEATURE;
    g_shrd_config.imu_aggregated = SHRD_DISABLE_FEATURE;
    g_shrd_config.mic = SHRD_DISABLE_FEATURE;
    g_shrd_config.steps = SHRD_DISABLE_FEATURE;

    *shrd_config = g_shrd_config;
}

/**
 * @brief Read data from the data_storage and store the NVS 
 * data in global structure
 * 
 * @param shrd_config    Instance of shrd_config
 *
 * @return None.
 */
void sh_shrd_config::set_shrd_conf(shrd_config_t *shrd_config)
{
    g_shrd_config = *shrd_config;
}

/**
 * @brief Pass the current structure data to data_storage to
 * store it into NVS
 * 
 * @param shrd_config    Instance of shrd_config
 *
 * @return None.
 */
void sh_shrd_config::get_shrd_conf(shrd_config_t *shrd_config)
{
    *shrd_config = g_shrd_config;
}

/**
 * @brief Print the configuration parameters
 *  
 * @return None.
 */
void sh_shrd_config::print_config_data(void)
{
    LOG_INF("\ng_shrd_config.activity       : %d", g_shrd_config.activity);
    LOG_INF("g_shrd_config.arm_angle        : %d", g_shrd_config.arm_angle);
    LOG_INF("g_shrd_config.baro             : %d", g_shrd_config.baro);
    LOG_INF("g_shrd_config.cough            : %d", g_shrd_config.cough);
    LOG_INF("g_shrd_config.events           : %d", g_shrd_config.events);
    LOG_INF("g_shrd_config.imu              : %d", g_shrd_config.imu);
    LOG_INF("g_shrd_config.imu_aggregated   : %d", g_shrd_config.imu_aggregated);
    LOG_INF("g_shrd_config.mic              : %d", g_shrd_config.mic);
    LOG_INF("g_shrd_config.temp             : %d", g_shrd_config.temp);
    LOG_INF("g_shrd_config.scratch          : %d", g_shrd_config.scratch);
    LOG_INF("g_shrd_config.step             : %d", g_shrd_config.steps);
}

/**
 * @brief Set shrd configuration parameters based on the profile selection
 * 
 * @param profile    Profile to be set 
 *
 * @return integer success > 0, error <= 0.
 */
void sh_shrd_config::set_profile(uint8_t profile)
{
    switch (profile)
    {
    case SHRD_SWALLOW_ENABLE:
        {
            g_shrd_config.temp = SHRD_ENABLE_FEATURE;
            g_shrd_config.imu = SHRD_DISABLE_FEATURE;
            g_shrd_config.baro = SHRD_DISABLE_FEATURE;
            g_shrd_config.mic = SHRD_ENABLE_FEATURE;
            g_shrd_config.scratch = SHRD_DISABLE_FEATURE;
            g_shrd_config.cough = SHRD_DISABLE_FEATURE;
            g_shrd_config.activity = SHRD_DISABLE_FEATURE;
            g_shrd_config.arm_angle = SHRD_DISABLE_FEATURE;
            g_shrd_config.imu_aggregated = SHRD_DISABLE_FEATURE;
            g_shrd_config.events = SHRD_ENABLE_FEATURE;
            g_shrd_config.steps = SHRD_DISABLE_FEATURE;
        }
        break;

    case SHRD_SCRATCH_ENABLE:
        {
            g_shrd_config.temp = SHRD_ENABLE_FEATURE;
            g_shrd_config.imu = SHRD_DISABLE_FEATURE;
            g_shrd_config.baro = SHRD_DISABLE_FEATURE;
            g_shrd_config.mic = SHRD_DISABLE_FEATURE;
            g_shrd_config.scratch = SHRD_ENABLE_FEATURE;
            g_shrd_config.cough = SHRD_DISABLE_FEATURE;
            g_shrd_config.activity = SHRD_ENABLE_FEATURE;
            g_shrd_config.arm_angle = SHRD_ENABLE_FEATURE;
            g_shrd_config.imu_aggregated = SHRD_ENABLE_FEATURE;
            g_shrd_config.events = SHRD_ENABLE_FEATURE;
            g_shrd_config.steps = SHRD_DISABLE_FEATURE;
        }
        break;
    
    case SHRD_SWALLOW_RAW_ENABLE:
        {
            g_shrd_config.temp = SHRD_ENABLE_FEATURE;
            g_shrd_config.imu = SHRD_ENABLE_FEATURE;
            g_shrd_config.baro = SHRD_ENABLE_FEATURE;
            g_shrd_config.mic = SHRD_ENABLE_FEATURE;
            g_shrd_config.scratch = SHRD_DISABLE_FEATURE;
            g_shrd_config.cough = SHRD_DISABLE_FEATURE;
            g_shrd_config.activity = SHRD_DISABLE_FEATURE;
            g_shrd_config.arm_angle = SHRD_DISABLE_FEATURE;
            g_shrd_config.imu_aggregated = SHRD_DISABLE_FEATURE;
            g_shrd_config.events = SHRD_ENABLE_FEATURE;
            g_shrd_config.steps = SHRD_DISABLE_FEATURE;
        }
        break;

    case SHRD_SCRATCH_RAW_ENABLE:
        {
            g_shrd_config.temp = SHRD_ENABLE_FEATURE;
            g_shrd_config.imu = SHRD_ENABLE_FEATURE;
            g_shrd_config.baro = SHRD_ENABLE_FEATURE;
            g_shrd_config.mic = SHRD_DISABLE_FEATURE;
            g_shrd_config.scratch = SHRD_ENABLE_FEATURE;
            g_shrd_config.cough = SHRD_DISABLE_FEATURE;
            g_shrd_config.activity = SHRD_ENABLE_FEATURE;
            g_shrd_config.arm_angle = SHRD_ENABLE_FEATURE;
            g_shrd_config.imu_aggregated = SHRD_ENABLE_FEATURE;
            g_shrd_config.events = SHRD_ENABLE_FEATURE;
            g_shrd_config.steps = SHRD_DISABLE_FEATURE;
        }
        break;
    
    case SHRD_IBD_ENABLE:
        {
            g_shrd_config.temp = SHRD_ENABLE_FEATURE;
            g_shrd_config.imu = SHRD_ENABLE_FEATURE;
            g_shrd_config.baro = SHRD_ENABLE_FEATURE;
            g_shrd_config.mic = SHRD_ENABLE_FEATURE;
            g_shrd_config.scratch = SHRD_DISABLE_FEATURE;
            g_shrd_config.cough = SHRD_DISABLE_FEATURE;
            g_shrd_config.activity = SHRD_DISABLE_FEATURE;
            g_shrd_config.arm_angle = SHRD_DISABLE_FEATURE;
            g_shrd_config.imu_aggregated = SHRD_DISABLE_FEATURE;
            g_shrd_config.events = SHRD_DISABLE_FEATURE;
            g_shrd_config.steps = SHRD_ENABLE_FEATURE;
        }
        break;
    
    case SHRD_COUGH_ENABLE:
        {
            g_shrd_config.temp = SHRD_DISABLE_FEATURE;
            g_shrd_config.imu = SHRD_ENABLE_FEATURE;
            g_shrd_config.baro = SHRD_DISABLE_FEATURE;
            g_shrd_config.mic = SHRD_ENABLE_FEATURE;
            g_shrd_config.scratch = SHRD_DISABLE_FEATURE;
            g_shrd_config.cough = SHRD_ENABLE_FEATURE;
            g_shrd_config.activity = SHRD_DISABLE_FEATURE;
            g_shrd_config.arm_angle = SHRD_DISABLE_FEATURE;
            g_shrd_config.imu_aggregated = SHRD_DISABLE_FEATURE;
            g_shrd_config.events = SHRD_DISABLE_FEATURE;
            g_shrd_config.steps = SHRD_DISABLE_FEATURE;
        }
        break;
    
    default:
        break;
    }
}

/**
 * @brief Set SHRD config value individually
 * 
 * @param temp    set value of temp
 *
 * @return None.
 */
void sh_shrd_config::set_temp(bool temp)
{
    g_shrd_config.temp = temp;
}
/**
 * @brief Get SHRD config value individually
 *
 * @return temp value of SHRD config
 */
bool sh_shrd_config::get_temp(void)
{
    return g_shrd_config.temp;
}

/**
 * @brief Set SHRD config value individually
 * 
 * @param imu    set value of imu
 *
 * @return integer success > 0, error <= 0.
 */
int sh_shrd_config::set_imu(bool imu)
{
    int ret = CONFIG_SHELL_CONFIG_NOT_ENABLED;

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
    if(sh_imu_config::get_enabled())
        g_shrd_config.imu = imu;
#endif // CONFIG_SH_BMI323, CONFIG_BMA456

    return ret;
}
/**
 * @brief Get SHRD config value individually
 *
 * @return imu value of SHRD config
 */
bool sh_shrd_config::get_imu(void)
{
    return g_shrd_config.imu;
}

/**
 * @brief Set SHRD config value individually
 * 
 * @param baro    set value of baro
 *
 * @return integer success > 0, error <= 0.
 */
int sh_shrd_config::set_baro(bool baro)
{
    int ret = CONFIG_SHELL_CONFIG_NOT_ENABLED;

#if defined (CONFIG_SH_BMP5)
    if(sh_barometer_config::get_enabled())
        g_shrd_config.baro = baro;
#endif // CONFIG_SH_BMP5
    
    return ret;
}
/**
 * @brief Get SHRD config value individually
 *
 * @return baro value of SHRD config
 */
bool sh_shrd_config::get_baro(void)
{
    return g_shrd_config.baro;
}

/**
 * @brief Set SHRD config value individually
 * 
 * @param mic    set value of mic
 *
 * @return integer success > 0, error <= 0.
 */
int sh_shrd_config::set_mic(bool mic)
{
    int ret = CONFIG_SHELL_CONFIG_NOT_ENABLED;

#if defined (CONFIG_MICROPHONE)
    if(sh_mic_config::get_enabled())
        g_shrd_config.mic = mic;
#endif // CONFIG_MICROPHONE
    
    return ret;
}
/**
 * @brief Get SHRD config value individually
 *
 * @return mic value of SHRD config
 */
bool sh_shrd_config::get_mic(void)
{
    return g_shrd_config.mic;
}

/**
 * @brief Set SHRD config value individually
 * 
 * @param scratch    set value of scratch
 *
 * @return integer success > 0, error <= 0.
 */
int sh_shrd_config::set_scratch(bool scratch)
{
    int ret = CONFIG_SHELL_CONFIG_NOT_ENABLED;

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
    if(sh_imu_config::get_enabled())
        g_shrd_config.scratch = scratch;
#endif // CONFIG_SH_BMI323, CONFIG_BMA456
    
    return ret;
}
/**
 * @brief Get SHRD config value individually
 *
 * @return scratch value of SHRD config
 */
bool sh_shrd_config::get_scratch(void)
{
    return g_shrd_config.scratch;
}

/**
 * @brief Set SHRD config value individually
 * 
 * @param cough    set value of cough
 *
 * @return integer success > 0, error <= 0.
 */
int sh_shrd_config::set_cough(bool cough)
{
    int ret = CONFIG_SHELL_CONFIG_NOT_ENABLED;

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
    if(sh_imu_config::get_enabled())
        g_shrd_config.cough = cough;
#endif // CONFIG_SH_BMI323, CONFIG_BMA456
    
    return ret;
}
/**
 * @brief Get SHRD config value individually
 *
 * @return cough value of SHRD config
 */
bool sh_shrd_config::get_cough(void)
{
    return g_shrd_config.cough;
}

/**
 * @brief Set SHRD config value individually
 * 
 * @param activity    set value of activity
 *
 * @return integer success > 0, error <= 0.
 */
int sh_shrd_config::set_activity(bool activity)
{
    int ret = CONFIG_SHELL_CONFIG_NOT_ENABLED;

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
    if(sh_imu_config::get_enabled())
        g_shrd_config.activity = activity;
#endif // CONFIG_SH_BMI323, CONFIG_BMA456
    
    return ret;
}
/**
 * @brief Get SHRD config value individually
 *
 * @return activity value of SHRD config
 */
bool sh_shrd_config::get_activity(void)
{
    return g_shrd_config.activity;
}

/**
 * @brief Set SHRD config value individually
 * 
 * @param arm_angle    set value of arm_angle
 *
 * @return integer success > 0, error <= 0.
 */
int sh_shrd_config::set_arm_angle(bool arm_angle)
{
    int ret = CONFIG_SHELL_CONFIG_NOT_ENABLED;

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
    if(sh_imu_config::get_enabled())
        g_shrd_config.arm_angle = arm_angle;
#endif // CONFIG_SH_BMI323, CONFIG_BMA456
    
    return ret;
}
/**
 * @brief Get SHRD config value individually
 *
 * @return arm_angle value of SHRD config
 */
bool sh_shrd_config::get_arm_angle(void)
{
    return g_shrd_config.arm_angle;
}

/**
 * @brief Set SHRD config value individually
 * 
 * @param imu_aggregated    set value of imu_aggregated
 *
 * @return integer success > 0, error <= 0.
 */
int sh_shrd_config::set_imu_aggregated(bool imu_aggregated)
{
    int ret = CONFIG_SHELL_CONFIG_NOT_ENABLED;

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
    if(sh_imu_config::get_enabled())
        g_shrd_config.imu_aggregated = imu_aggregated;
#endif // CONFIG_SH_BMI323, CONFIG_BMA456
    
    return ret;
}
/**
 * @brief Get SHRD config value individually
 *
 * @return imu_aggregated value of SHRD config
 */
bool sh_shrd_config::get_imu_aggregated(void)
{
    return g_shrd_config.imu_aggregated;
}

/**
 * @brief Set SHRD config value individually
 * 
 * @param events    set value of events
 *
 * @return integer success > 0, error <= 0.
 */
int sh_shrd_config::set_events(bool events)
{
    g_shrd_config.events = events;
    return events;
}
/**
 * @brief Get SHRD config value individually
 *
 * @return events value of SHRD config
 */
bool sh_shrd_config::get_events(void)
{
    return g_shrd_config.events;
}

/**
 * @brief Set SHRD config value individually
 * 
 * @param step    set value of ste count
 *
 * @return integer success > 0, error <= 0.
 */
int sh_shrd_config::set_steps(bool steps)
{
    int ret = CONFIG_SHELL_CONFIG_NOT_ENABLED;

#if defined (CONFIG_SH_BMI323_STEP_COUNT_ENABLE)
    if(sh_imu_config::get_enabled())
        g_shrd_config.steps = steps;
#endif /* CONFIG_SH_BMI323_STEP_COUNT_ENABLE */

    return ret;
}
/**
 * @brief Get SHRD config value individually
 *
 * @return step value of SHRD config
 */
bool sh_shrd_config::get_steps(void)
{
    return g_shrd_config.steps;
}

#endif // CONFIG_NVS
