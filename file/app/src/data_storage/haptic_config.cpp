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

#if defined (CONFIG_NVS) && defined (CONFIG_DRV2624)

/* Zephyr include files */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Sibel SDK files */
#include <sh/sh_nvs_handler.h>

#include "haptic_config.h"

#define HAPTIC_PROFILE_SWALLOW_DURATION     7210    // 2Hr 10 seconds
#define HAPTIC_PROFILE_SWALLOW_INTERVAL     60      



LOG_MODULE_REGISTER(sh_haptic_config, LOG_LEVEL_ERR);

// Haptic structure instance
haptic_config_t g_haptic_config;

/**
 * @brief Load the Haptic motor config structure with default values
 *
 * @return None.
 */
void sh_haptic_config::load_default_haptic_conf(haptic_config_t *haptic_config)
{
    g_haptic_config.enabled = HAPTIC_DISABLE;
    g_haptic_config.pattern.intensity = HAPTIC_DEFAULT_VALUE;
    g_haptic_config.pattern.pulse_width = HAPTIC_DEFAULT_VALUE;
    g_haptic_config.pattern.interval = HAPTIC_DEFAULT_VALUE;
    g_haptic_config.pattern.duration = HAPTIC_DEFAULT_VALUE;

    *haptic_config = g_haptic_config;
}

/**
 * @brief Read data from the data_storage and store the NVS 
 * data in global structure
 * 
 * @param haptic_config    Instance of haptic_config
 *
 * @return None.
 */
void sh_haptic_config::set_haptic_conf(haptic_config_t *haptic_config)
{
    g_haptic_config = *haptic_config;
}

/**
 * @brief Pass the current structure data to data_storage to
 * store it into NVS
 * 
 * @param haptic_config    Instance of haptic_config
 *
 * @return None.
 */
void sh_haptic_config::get_haptic_conf(haptic_config_t *haptic_config)
{
    *haptic_config = g_haptic_config;
}

/**
 * @brief Print the configuration parameters
 *  
 * @return None.
 */
void sh_haptic_config::print_config_data(void)
{
    LOG_INF("\ng_haptic_config.enabled          : %d", g_haptic_config.enabled);
    LOG_INF("g_haptic_config.pattern.duration   : %d", g_haptic_config.pattern.duration);
    LOG_INF("g_haptic_config.pattern.intensity  : %d", g_haptic_config.pattern.intensity);
    LOG_INF("g_haptic_config.pattern.interval   : %d", g_haptic_config.pattern.interval);
    LOG_INF("g_haptic_config.pattern.pulse_width: %d", g_haptic_config.pattern.pulse_width);
}

/**
 * @brief Set haptic configuration parameters based on the profile selection
 * 
 * @param profile    Profile to be set 
 *
 * @return integer success > 0, error <= 0.
 */
void sh_haptic_config::set_profile(uint8_t profile)
{
    switch (profile)
    {
    case HAPTIC_SWALLOW_ENABLE:
        {
            g_haptic_config.enabled = HAPTIC_ENABLED;
            g_haptic_config.pattern.duration = HAPTIC_PROFILE_SWALLOW_DURATION;
            g_haptic_config.pattern.interval = HAPTIC_PROFILE_SWALLOW_INTERVAL;
            g_haptic_config.pattern.intensity = HAPTIC_DEFAULT_VALUE;
            g_haptic_config.pattern.pulse_width = HAPTIC_DEFAULT_VALUE;
        }
        break;

    case HAPTIC_SCRATCH_ENABLE:
        {
            g_haptic_config.enabled = HAPTIC_ENABLED;
            g_haptic_config.pattern.duration = HAPTIC_DEFAULT_VALUE;
            g_haptic_config.pattern.interval = HAPTIC_DEFAULT_VALUE;
            g_haptic_config.pattern.intensity = HAPTIC_DEFAULT_VALUE;
            g_haptic_config.pattern.pulse_width = HAPTIC_DEFAULT_VALUE;
        }
        break;
    
    case HAPTIC_SWALLOW_RAW_ENABLE:
        {
            g_haptic_config.enabled = HAPTIC_ENABLED;
            g_haptic_config.pattern.duration = HAPTIC_PROFILE_SWALLOW_DURATION;
            g_haptic_config.pattern.interval = HAPTIC_PROFILE_SWALLOW_INTERVAL;
            g_haptic_config.pattern.intensity = HAPTIC_DEFAULT_VALUE;
            g_haptic_config.pattern.pulse_width = HAPTIC_DEFAULT_VALUE;
        }
        break;

    case HAPTIC_SCRATCH_RAW_ENABLE:
        {
            g_haptic_config.enabled = HAPTIC_ENABLED;
            g_haptic_config.pattern.duration = HAPTIC_DEFAULT_VALUE;
            g_haptic_config.pattern.interval = HAPTIC_DEFAULT_VALUE;
            g_haptic_config.pattern.intensity = HAPTIC_DEFAULT_VALUE;
            g_haptic_config.pattern.pulse_width = HAPTIC_DEFAULT_VALUE;
        }
        break;
    
    case HAPTIC_IBD_ENABLE:
    case HAPTIC_COUGH_ENABLE:
        {
            g_haptic_config.enabled = HAPTIC_DISABLE;
            g_haptic_config.pattern.duration = HAPTIC_DEFAULT_VALUE;
            g_haptic_config.pattern.interval = HAPTIC_DEFAULT_VALUE;
            g_haptic_config.pattern.intensity = HAPTIC_DEFAULT_VALUE;
            g_haptic_config.pattern.pulse_width = HAPTIC_DEFAULT_VALUE;
        }
        break;
    
    default:
        break;
    }
}


/**
 * @brief Set haptic config value individually
 * 
 * @param enabled    set value of enabled
 *
 * @return None.
 */
void sh_haptic_config::set_enabled(uint8_t enabled)
{
    g_haptic_config.enabled = enabled;
}
/**
 * @brief Get haptic config value individually
 *
 * @return enabled value of haptic config
 */
uint8_t sh_haptic_config::get_enabled(void)
{
    return g_haptic_config.enabled;
}

/**
 * @brief Set haptic config value individually
 * 
 * @param intensity    set value of intensity
 *
 * @return integer success > 0, error <= 0.
 */
int sh_haptic_config::set_intensity(uint8_t intensity)
{
    int ret = 1;

    if( (intensity >= 0) &&  (intensity <= 100) )
        g_haptic_config.pattern.intensity = intensity;
    else 
        ret = CONFIG_SHELL_UNKNOWN_VALUE;

    return ret;
}
/**
 * @brief Get haptic config value individually
 *
 * @return intensity value of haptic config
 */
uint8_t sh_haptic_config::get_intensity(void)
{
    return g_haptic_config.pattern.intensity;
}

/**
 * @brief Set haptic config value individually
 * 
 * @param pulse_width    set value of pulse_width
 *
 * @return integer success > 0, error <= 0.
 */
int sh_haptic_config::set_pulse_width(uint16_t pulse_width)
{
    int ret = 1;

    if(pulse_width > 0) 
        g_haptic_config.pattern.pulse_width = pulse_width;
    else 
        ret = CONFIG_SHELL_UNKNOWN_VALUE;

    return ret;
}
/**
 * @brief Get haptic config value individually
 *
 * @return pulse_width value of haptic config
 */
uint16_t sh_haptic_config::get_pulse_width(void)
{
    return g_haptic_config.pattern.pulse_width;
}

/**
 * @brief Set haptic config value individually
 * 
 * @param interval    set value of interval
 *
 * @return integer success > 0, error <= 0.
 */
int sh_haptic_config::set_interval(uint16_t interval)
{
    int ret = 1;

    if(interval > 0) 
        g_haptic_config.pattern.interval = interval;
    else 
        ret = CONFIG_SHELL_UNKNOWN_VALUE;

    return ret;
}
/**
 * @brief Get haptic config value individually
 *
 * @return interval value of haptic config
 */
uint16_t sh_haptic_config::get_interval(void)
{
    return g_haptic_config.pattern.interval;
}

/**
 * @brief Set haptic config value individually
 * 
 * @param duration    set value of duration
 *
 * @return integer success > 0, error <= 0.
 */
int sh_haptic_config::set_duration(uint16_t duration)
{
    int ret = 1;

    if(duration > 0)
        g_haptic_config.pattern.duration = duration;
    else 
        ret = CONFIG_SHELL_UNKNOWN_VALUE;

    return ret;
}
/**
 * @brief Get haptic config value individually
 *
 * @return duration value of haptic config
 */
uint16_t sh_haptic_config::get_duration(void)
{
    return g_haptic_config.pattern.duration;
}

#endif // CONFIG_NVS
