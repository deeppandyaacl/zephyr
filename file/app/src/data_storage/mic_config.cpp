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

#if defined (CONFIG_NVS) && defined (CONFIG_MICROPHONE)

/* Zephyr include files */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Sibel SDK files */
#include <sh/sh_nvs_handler.h>

#include "mic_config.h"

#define MIC_PROFILE_SWALLOW_GAIN    80
#define MIC_PROFILE_IBD_GAIN        80
#define MIC_PROFILE_COUGH_GAIN      80

LOG_MODULE_REGISTER(sh_mic_config, LOG_LEVEL_ERR);

// Microphone structure instance
mic_data_config_t g_mic_config;

/**
 * @brief Load the Microphone config structure with default values
 *
 * @return None.
 */
void sh_mic_config::load_default_mic_conf(mic_data_config_t *mic_config)
{
    g_mic_config.enabled = MIC_NO_CHANNEL;
    g_mic_config.mic_lchannel_gain = CONFIG_MICROPHONE_L_GAIN;
    g_mic_config.mic_rchannel_gain = CONFIG_MICROPHONE_R_GAIN;
    
    *mic_config = g_mic_config;
}

/**
 * @brief Read data from the data_storage and store the NVS 
 * data in global structure
 * 
 * @param mic_config    Instance of mic_config
 *
 * @return None.
 */
void sh_mic_config::set_mic_conf(mic_data_config_t *mic_config)
{
    g_mic_config = *mic_config;
}
/**
 * @brief Pass the current structure data to data_storage to
 * store it into NVS
 * 
 * @param mic_config    Instance of mic_config
 *
 * @return None.
 */
void sh_mic_config::get_mic_conf(mic_data_config_t *mic_config)
{
    *mic_config = g_mic_config;
}

/**
 * @brief Print the configuration parameters
 *  
 * @return None.
 */
void sh_mic_config::print_config_data(void)
{
    LOG_INF("\ng_mic_config.enabled         : %d", g_mic_config.enabled);
    LOG_INF("g_mic_config.mic_lchannel_gain : %d", g_mic_config.mic_lchannel_gain);
    LOG_INF("g_mic_config.mic_rchannel_gain : %d", g_mic_config.mic_rchannel_gain);
    // LOG_INF("g_mic_config.mic_mode : %d", g_mic_config.mic_mode);
}

/**
 * @brief Set IMU configuration parameters based on the profile selection
 * 
 * @param profile    Profile to be set 
 *
 * @return integer success > 0, error <= 0.
 */
void sh_mic_config::set_profile(uint8_t profile)
{
    switch (profile)
    {
    case MIC_SWALLOW_ENABLE:
        {
            g_mic_config.enabled = MIC_ALL_CHANNEL;//MIC_ALL_CHANNEL;
            g_mic_config.mic_lchannel_gain = MIC_PROFILE_SWALLOW_GAIN;
            g_mic_config.mic_rchannel_gain = MIC_PROFILE_SWALLOW_GAIN;
        }
        break;

    case MIC_SCRATCH_ENABLE:
        {
            g_mic_config.enabled = MIC_NO_CHANNEL;
            g_mic_config.mic_lchannel_gain = CONFIG_MICROPHONE_L_GAIN;
            g_mic_config.mic_rchannel_gain = CONFIG_MICROPHONE_R_GAIN;
        }
        break;
    
    case MIC_SWALLOW_RAW_ENABLE:
        {
            g_mic_config.enabled = MIC_ALL_CHANNEL;//MIC_ALL_CHANNEL;
            g_mic_config.mic_lchannel_gain = MIC_PROFILE_SWALLOW_GAIN;
            g_mic_config.mic_rchannel_gain = MIC_PROFILE_SWALLOW_GAIN;
        }
        break;

    case MIC_SCRATCH_RAW_ENABLE:
        {
            g_mic_config.enabled = MIC_NO_CHANNEL;
            g_mic_config.mic_lchannel_gain = CONFIG_MICROPHONE_L_GAIN;
            g_mic_config.mic_rchannel_gain = CONFIG_MICROPHONE_R_GAIN;
        }
        break;
    
    case MIC_IBD_ENABLE:
        {
            g_mic_config.enabled = MIC_ALL_CHANNEL;//MIC_ALL_CHANNEL;
            g_mic_config.mic_lchannel_gain = MIC_PROFILE_IBD_GAIN;
            g_mic_config.mic_rchannel_gain = MIC_PROFILE_IBD_GAIN;
        }
        break;
    
    case MIC_COUGH_ENABLE:
        {
            g_mic_config.enabled = MIC_RIGHT_CHANNEL;//MIC_RIGHT_CHANNEL;
            g_mic_config.mic_lchannel_gain = MIC_PROFILE_COUGH_GAIN;
            g_mic_config.mic_rchannel_gain = MIC_PROFILE_COUGH_GAIN;
        }
        break;
    
    default:
        break;
    }
}



/**
 * @brief Set MIC config value individually
 * 
 * @param enabled    set value of enabled
 *
 * @return None.
 */
void sh_mic_config::set_enabled(uint8_t enabled)
{
    g_mic_config.enabled = enabled;
}
/**
 * @brief Get MIC config value individually
 *
 * @return enabled value of MIC config
 */
uint8_t sh_mic_config::get_enabled(void)
{
    return g_mic_config.enabled;
}

/**
 * @brief Set MIC config value individually
 * 
 * @param lchannel_gain    set value of lchannel_gain
 *
 * @return integer success > 0, error <= 0.
 */
int sh_mic_config::set_lchannel_gain(uint8_t lchannel_gain)
{
    int ret = 1;

    if( (lchannel_gain >= 0) &&  (lchannel_gain <= 255) )
        g_mic_config.mic_lchannel_gain = lchannel_gain;
    else 
        ret = CONFIG_SHELL_UNKNOWN_VALUE;

    return ret;
}
/**
 * @brief Get MIC config value individually
 *
 * @return lchannel_gain value of MIC config
 */
uint8_t sh_mic_config::get_lchannel_gain(void)
{
    return g_mic_config.mic_lchannel_gain;
}

/**
 * @brief Set MIC config value individually
 * 
 * @param rchannel_gain    set value of rchannel_gain
 *
 * @return integer success > 0, error <= 0.
 */
int sh_mic_config::set_rchannel_gain(uint8_t rchannel_gain)
{
    int ret = 1;

    if( (rchannel_gain >= 0) &&  (rchannel_gain <= 255) )
        g_mic_config.mic_rchannel_gain = rchannel_gain;
    else 
        ret = CONFIG_SHELL_UNKNOWN_VALUE;

    return ret;
}
/**
 * @brief Get MIC config value individually
 *
 * @return rchannel_gain value of MIC config
 */
uint8_t sh_mic_config::get_rchannel_gain(void)
{
    return g_mic_config.mic_rchannel_gain;
}

#endif // CONFIG_NVS
