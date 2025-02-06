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

#if defined (CONFIG_NVS) && defined (CONFIG_SH_BMP5)

/* Zephyr include files */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Sibel SDK files */
#include <sh/sh_nvs_handler.h>

#include "barometer_config.h"

LOG_MODULE_REGISTER(sh_barometer_config, LOG_LEVEL_ERR);

// Barometer structure instance
baro_config_t g_baro_config;

/**
 * @brief Load the Barometer sensor config structure with default values
 *  
 * @param baro_config    Instance of baro_config
 * 
 * @return None.
 */
void sh_barometer_config::load_default_barometer_conf(baro_config_t *baro_config)
{
    g_baro_config.enabled = BAROMETER_ENABLE;
    g_baro_config.bmp5_fifo_frame_sel = CONFIG_BMP5_FIFO_FRAME_SELECT;
    g_baro_config.bmp5_fifo_threshold = CONFIG_BMP5_FIFO_THRESHOLD;
    g_baro_config.bmp5_odr = CONFIG_BMP5_ORD;

    *baro_config = g_baro_config;
}

/**
 * @brief Print the configuration parameters
 *  
 * @return None.
 */
void sh_barometer_config::print_config_data(void)
{
    LOG_INF("\ng_baro_config.enabled            : %d", g_baro_config.enabled);
    LOG_INF("g_baro_config.bmp5_fifo_frame_sel  : %d", g_baro_config.bmp5_fifo_frame_sel);
    LOG_INF("g_baro_config.bmp5_fifo_threshold  : %d", g_baro_config.bmp5_fifo_threshold);
    LOG_INF("g_baro_config.bmp5_odr             : %d", g_baro_config.bmp5_odr);
}

/**
 * @brief Set Barometer configuration parameters based on the profile selection
 * 
 * @param profile    Profile to be set 
 *
 * @return integer success > 0, error <= 0.
 */
void sh_barometer_config::set_profile(uint8_t profile)
{
    switch (profile)
    {
    case BARO_SWALLOW_ENABLE:
    case BARO_SCRATCH_ENABLE:
        {
            g_baro_config.enabled = BAROMETER_DISABLE;
            g_baro_config.bmp5_fifo_frame_sel = CONFIG_BMP5_FIFO_FRAME_SELECT;
            g_baro_config.bmp5_fifo_threshold = CONFIG_BMP5_FIFO_THRESHOLD;
            g_baro_config.bmp5_odr = CONFIG_BMP5_ORD;
        }
        break;
    
    case BARO_SWALLOW_RAW_ENABLE:
    case BARO_SCRATCH_RAW_ENABLE:
    case BARO_IBD_ENABLE:
    case BARO_COUGH_ENABLE:
        {
            g_baro_config.enabled = BAROMETER_ENABLE;
            g_baro_config.bmp5_fifo_frame_sel = CONFIG_BMP5_FIFO_FRAME_SELECT;
            g_baro_config.bmp5_fifo_threshold = CONFIG_BMP5_FIFO_THRESHOLD;
            g_baro_config.bmp5_odr = CONFIG_BMP5_ORD;
        }
        break;
    
    default:
        break;
    }
}

/**
 * @brief Read data from the data_storage and store the NVS 
 * data in global structure
 * 
 * @param baro_config    Instance of baro_config
 *
 * @return None.
 */
void sh_barometer_config::set_barometer_conf(baro_config_t *baro_config)
{
    g_baro_config = *baro_config;
}

/**
 * @brief Pass the current structure data to data_storage to
 * store it into NVS
 * 
 * @param baro_config    Instance of baro_config
 *
 * @return None.
 */
void sh_barometer_config::get_barometer_conf(baro_config_t *baro_config)
{
    *baro_config = g_baro_config;
}

/**
 * @brief Set barometer config value individually
 * 
 * @param enabled    set value of enabled
 *
 * @return None.
 */
void sh_barometer_config::set_enabled(uint8_t enabled)
{
    g_baro_config.enabled = enabled;
}

/**
 * @brief Get barometer config value individually
 *
 * @return enabled value of barometer config
 */
uint8_t sh_barometer_config::get_enabled(void)
{
    return g_baro_config.enabled;
}

/**
 * @brief Set barometer config value individually
 * 
 * @param odr    set value of odr
 *
 * @return integer success > 0, error <= 0.
 */
int sh_barometer_config::set_odr(uint8_t odr)
{
    int ret = 1;

    if( (odr >= 0) &&  (odr <= 31) )
        g_baro_config.bmp5_odr = odr;
    else 
        ret = CONFIG_SHELL_UNKNOWN_VALUE;

    return ret;
}
/**
 * @brief Get barometer config value individually
 *
 * @return odr value of barometer config
 */
uint8_t sh_barometer_config::get_odr(void)
{
    return g_baro_config.bmp5_odr;
}

/**
 * @brief Set barometer config value individually
 * 
 * @param fifo_threshold    set value of fifo_threshold
 *
 * @return integer success > 0, error <= 0.
 */
int sh_barometer_config::set_fifo_threshold(uint8_t fifo_threshold)
{
    int ret = 1;

    if( (fifo_threshold > 0) &&  (fifo_threshold <= 31) )
        g_baro_config.bmp5_fifo_threshold = fifo_threshold;
    else 
        ret = CONFIG_SHELL_UNKNOWN_VALUE;

    return ret;
}   
/**
 * @brief Get barometer config value individually
 *
 * @return fifo_threshold value of barometer config
 */
uint8_t sh_barometer_config::get_fifo_threshold(void)
{
    return g_baro_config.bmp5_fifo_threshold;
}

/**
 * @brief Set barometer config value individually
 * 
 * @param frame_sel    set value of frame_sel
 *
 * @return integer success > 0, error <= 0.
 */
int sh_barometer_config::set_frame_sel(uint8_t frame_sel)
{
    int ret = 1;

    if( (frame_sel >= 0) &&  (frame_sel <= 3) )
        g_baro_config.bmp5_fifo_frame_sel = frame_sel;
    else 
        ret = CONFIG_SHELL_UNKNOWN_VALUE;
    
    return ret;
}
/**
 * @brief Get barometer config value individually
 *
 * @return frame_sel value of barometer config
 */
uint8_t sh_barometer_config::get_frame_sel(void)
{
    return g_baro_config.bmp5_fifo_frame_sel;
}

#endif // CONFIG_NVS
