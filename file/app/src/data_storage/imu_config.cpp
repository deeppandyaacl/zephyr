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

#if defined (CONFIG_NVS)

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)

/* Zephyr include files */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Sibel SDK files */
#include <sh/sh_nvs_handler.h>
#include <sh/sh_bmi323.h>

#include "imu_config.h"

#define IMU_PROFILE_SWALLOW_SCRATCH_MODE    2   // High performance mode
#define IMU_PROFILE_IBD_MODE                2   // High performance mode
#define IMU_PROFILE_COUGH_MODE              2   // High performance mode
#define IMU_PROFILE_SWALLOW_SCRATCH_ODR     12  // 1600Hz
#define IMU_PROFILE_IBD_ODR                 12  // 1600Hz
#define IMU_PROFILE_COUGH_ODR               12  // 1600Hz
#define IMU_PROFILE_SWALLOW_RANGE           1   // +/-2g
#define IMU_PROFILE_SCRATCH_RANGE           2   // +/-4g
#define IMU_PROFILE_IBD_RANGE               2   // +/-4g
#define IMU_PROFILE_COUGH_RANGE             2   // +/-4g

LOG_MODULE_REGISTER(sh_imu_config, LOG_LEVEL_ERR);

// IMU structure instance
imu_config_t g_imu_config;

/**
 * @brief Load the IMU sensor config structure with default values
 *
 * @param imu_config    Instance of imu_config
 * 
 * @return None.
 */
void sh_imu_config::load_default_imu_conf(imu_config_t *imu_config)
{
#if defined (CONFIG_BMA456)
    g_imu_config.enabled = IMU_ACC_ENABLE;
    g_imu_config.acc_mode = CONFIG_BMA456_ACCEL_PERF_MODE;
    g_imu_config.acc_odr = CONFIG_BMA456_ACCEL_ODR;
    g_imu_config.acc_range = CONFIG_BMA456_ACCEL_RANGE;
    g_imu_config.gyro_mode = 0;
    g_imu_config.gyro_odr = 0;
    g_imu_config.gyro_range = 0;
#else 
    g_imu_config.enabled = IMU_ACC_ENABLE;
    g_imu_config.acc_mode = CONFIG_SH_BMI323_ACCEL_MODE;
    g_imu_config.acc_odr = CONFIG_SH_BMI323_ACCEL_ODR;
    g_imu_config.acc_range = CONFIG_SH_BMI323_ACCEL_RANGE;
    g_imu_config.gyro_mode = CONFIG_SH_BMI323_GYRO_MODE;
    g_imu_config.gyro_odr = CONFIG_SH_BMI323_GYRO_ODR;
    g_imu_config.gyro_range = CONFIG_SH_BMI323_GYRO_RANGE;
#endif // CONFIG_SH_BMI323

    *imu_config = g_imu_config;
}

/**
 * @brief Read data from the data_storage and store the NVS 
 * data in global structure
 * 
 * @param imu_config    Instance of imu_config
 *
 * @return None.
 */
void sh_imu_config::set_imu_conf(imu_config_t *imu_config)
{
    g_imu_config = *imu_config;
}

/**
 * @brief Pass the current structure data to data_storage to
 * store it into NVS
 * 
 * @param imu_config    Instance of imu_config
 *
 * @return None.
 */
void sh_imu_config::get_imu_conf(imu_config_t *imu_config)
{
    *imu_config = g_imu_config;
}

/**
 * @brief Print the configuration parameters
 *  
 * @return None.
 */
void sh_imu_config::print_config_data(void)
{
    LOG_INF("\ng_imu_config.acc_odr : %d", g_imu_config.enabled);
    LOG_INF("g_imu_config.acc_mode  : %d", g_imu_config.acc_mode);
    LOG_INF("g_imu_config.acc_odr   : %d", g_imu_config.acc_odr);
    LOG_INF("g_imu_config.acc_range : %d", g_imu_config.acc_range);
    LOG_INF("g_imu_config.gyro_mode : %d", g_imu_config.gyro_mode);
    LOG_INF("g_imu_config.gyro_odr  : %d", g_imu_config.gyro_odr);
    LOG_INF("g_imu_config.gyro_range: %d", g_imu_config.gyro_range);
}

/**
 * @brief Set IMU configuration parameters based on the profile selection
 * 
 * @param profile    Profile to be set 
 *
 * @return integer success > 0, error <= 0.
 */
void sh_imu_config::set_profile(uint8_t profile)
{
    switch (profile)
    {
    case IMU_SWALLOW_ENABLE:
        {
            g_imu_config.enabled = IMU_ACC_ENABLE;
            g_imu_config.acc_mode = IMU_PROFILE_SWALLOW_SCRATCH_MODE;
            g_imu_config.acc_odr = IMU_PROFILE_SWALLOW_SCRATCH_ODR;
            g_imu_config.acc_range = IMU_PROFILE_SWALLOW_RANGE;

        #if defined (CONFIG_BMA456)
            g_imu_config.gyro_mode = 0;
            g_imu_config.gyro_odr = 0;
            g_imu_config.gyro_range = 0;
        #else 
            g_imu_config.gyro_mode = CONFIG_SH_BMI323_GYRO_MODE;
            g_imu_config.gyro_odr = CONFIG_SH_BMI323_GYRO_ODR;
            g_imu_config.gyro_range = CONFIG_SH_BMI323_GYRO_RANGE;
        #endif 

        }
        break;

    case IMU_SCRATCH_ENABLE:
        {
            g_imu_config.enabled = IMU_ACC_ENABLE;
            g_imu_config.acc_mode = IMU_PROFILE_SWALLOW_SCRATCH_MODE;
            g_imu_config.acc_odr = IMU_PROFILE_SWALLOW_SCRATCH_ODR;
            g_imu_config.acc_range = IMU_PROFILE_SCRATCH_RANGE;

        #if defined (CONFIG_BMA456)
            g_imu_config.gyro_mode = 0;
            g_imu_config.gyro_odr = 0;
            g_imu_config.gyro_range = 0;
        #else 
            g_imu_config.gyro_mode = CONFIG_SH_BMI323_GYRO_MODE;
            g_imu_config.gyro_odr = CONFIG_SH_BMI323_GYRO_ODR;
            g_imu_config.gyro_range = CONFIG_SH_BMI323_GYRO_RANGE;
        #endif 

        }
        break;
    
    case IMU_SWALLOW_RAW_ENABLE:
        {
            g_imu_config.enabled = IMU_ACC_ENABLE;
            g_imu_config.acc_mode = IMU_PROFILE_SWALLOW_SCRATCH_MODE;
            g_imu_config.acc_odr = IMU_PROFILE_SWALLOW_SCRATCH_ODR;
            g_imu_config.acc_range = IMU_PROFILE_SWALLOW_RANGE;

        #if defined (CONFIG_BMA456)
            g_imu_config.gyro_mode = 0;
            g_imu_config.gyro_odr = 0;
            g_imu_config.gyro_range = 0;
        #else 
            g_imu_config.gyro_mode = CONFIG_SH_BMI323_GYRO_MODE;
            g_imu_config.gyro_odr = CONFIG_SH_BMI323_GYRO_ODR;
            g_imu_config.gyro_range = CONFIG_SH_BMI323_GYRO_RANGE;
        #endif 

        }
        break;

    case IMU_SCRATCH_RAW_ENABLE:
        {
            g_imu_config.enabled = IMU_ACC_ENABLE;
            g_imu_config.acc_mode = IMU_PROFILE_SWALLOW_SCRATCH_MODE;
            g_imu_config.acc_odr = IMU_PROFILE_SWALLOW_SCRATCH_ODR;
            g_imu_config.acc_range = IMU_PROFILE_SCRATCH_RANGE;

        #if defined (CONFIG_BMA456)
            g_imu_config.gyro_mode = 0;
            g_imu_config.gyro_odr = 0;
            g_imu_config.gyro_range = 0;
        #else 
            g_imu_config.gyro_mode = CONFIG_SH_BMI323_GYRO_MODE;
            g_imu_config.gyro_odr = CONFIG_SH_BMI323_GYRO_ODR;
            g_imu_config.gyro_range = CONFIG_SH_BMI323_GYRO_RANGE;
        #endif 

        }
        break;
    
    case IMU_IBD_ENABLE:
        {
            g_imu_config.enabled = IMU_ACC_ENABLE;
            g_imu_config.acc_mode = IMU_PROFILE_IBD_MODE;
            g_imu_config.acc_odr = IMU_PROFILE_IBD_ODR;
            g_imu_config.acc_range = IMU_PROFILE_IBD_RANGE;

        #if defined (CONFIG_BMA456)
            g_imu_config.gyro_mode = 0;
            g_imu_config.gyro_odr = 0;
            g_imu_config.gyro_range = 0;
        #else 
            g_imu_config.gyro_mode = CONFIG_SH_BMI323_GYRO_MODE;
            g_imu_config.gyro_odr = CONFIG_SH_BMI323_GYRO_ODR;
            g_imu_config.gyro_range = CONFIG_SH_BMI323_GYRO_RANGE;
        #endif 

        }
        break;
    
    case IMU_COUGH_ENABLE:
        {
            g_imu_config.enabled = IMU_ACC_ENABLE;
            g_imu_config.acc_mode = IMU_PROFILE_COUGH_MODE;
            g_imu_config.acc_odr = IMU_PROFILE_COUGH_ODR;
            g_imu_config.acc_range = IMU_PROFILE_COUGH_RANGE;

        #if defined (CONFIG_BMA456)
            g_imu_config.gyro_mode = 0;
            g_imu_config.gyro_odr = 0;
            g_imu_config.gyro_range = 0;
        #else 
            g_imu_config.gyro_mode = CONFIG_SH_BMI323_GYRO_MODE;
            g_imu_config.gyro_odr = CONFIG_SH_BMI323_GYRO_ODR;
            g_imu_config.gyro_range = CONFIG_SH_BMI323_GYRO_RANGE;
        #endif 

        }
        break;
    
    default:
        break;
    }
}



/**
 * @brief Set IMU config value individually
 * 
 * @param enabled    set value of enabled
 *
 * @return None.
 */
void sh_imu_config::set_enabled(uint8_t enabled)
{
    g_imu_config.enabled = enabled;
}
/**
 * @brief Get IMU config value individually
 *
 * @return enabled value of IMU config
 */
uint8_t sh_imu_config::get_enabled(void)
{
    return g_imu_config.enabled;
}

/**
 * @brief Set IMU config value individually
 * 
 * @param acc_odr    set value of acc_odr
 *
 * @return integer success > 0, error <= 0.
 */
int sh_imu_config::set_acc_odr(uint8_t acc_odr)
{
    int ret = 1;

    if( (acc_odr >= 0) &&  (acc_odr <= 14) )
        g_imu_config.acc_odr = acc_odr;
    else 
        ret = CONFIG_SHELL_UNKNOWN_VALUE;

    return ret;
}
/**
 * @brief Get IMU config value individually
 *
 * @return acc_odr value of IMU config
 */
uint8_t sh_imu_config::get_acc_odr(void)
{
    return g_imu_config.acc_odr;
}

/**
 * @brief Set IMU config value individually
 * 
 * @param acc_mode    set value of acc_mode
 *
 * @return integer success > 0, error <= 0.
 */
int sh_imu_config::set_acc_mode(uint8_t acc_mode)
{
    int ret = 1;

    if( (acc_mode >= 0) &&  (acc_mode <= 2) )
        g_imu_config.acc_mode = acc_mode;
    else 
        ret = CONFIG_SHELL_UNKNOWN_VALUE;

    return ret;
}
/**
 * @brief Get IMU config value individually
 *
 * @return acc_mode value of IMU config
 */
uint8_t sh_imu_config::get_acc_mode(void)
{
    return g_imu_config.acc_mode;
}

/**
 * @brief Set IMU config value individually
 * 
 * @param acc_mode    set value of acc_mode
 *
 * @return integer success > 0, error <= 0.
 */
int sh_imu_config::set_acc_range(uint8_t acc_range)
{
    int ret = 1;

    if( (acc_range >= 0) &&  (acc_range <= 4) )
        g_imu_config.acc_range = acc_range;
    else 
        ret = CONFIG_SHELL_UNKNOWN_VALUE;

    return ret;
}
/**
 * @brief Get IMU config value individually
 *
 * @return acc_mode value of IMU config
 */
uint8_t sh_imu_config::get_acc_range(void)
{
    return g_imu_config.acc_range;
}

/**
 * @brief Set IMU config value individually
 * 
 * @param gyro_odr    set value of gyro_odr
 *
 * @return integer success > 0, error <= 0.
 */
int sh_imu_config::set_gyro_odr(uint8_t gyro_odr)
{
    int ret = 1;

    if( (gyro_odr >= 0) &&  (gyro_odr <= 14) )
        g_imu_config.gyro_odr = gyro_odr;
    else 
        ret = CONFIG_SHELL_UNKNOWN_VALUE;

    return ret;
}
/**
 * @brief Get IMU config value individually
 *
 * @return gyro_odr value of IMU config
 */
uint8_t sh_imu_config::get_gyro_odr(void)
{
    return g_imu_config.gyro_odr;
}

/**
 * @brief Set IMU config value individually
 * 
 * @param gyro_mode    set value of gyro_mode
 *
 * @return integer success > 0, error <= 0.
 */
int sh_imu_config::set_gyro_mode(uint8_t gyro_mode)
{
    int ret = 1;
    
    if( (gyro_mode >= 0) &&  (gyro_mode <= 2) )
        g_imu_config.gyro_mode = gyro_mode;
    else 
        ret = CONFIG_SHELL_UNKNOWN_VALUE;

    return ret;
}
/**
 * @brief Get IMU config value individually
 *
 * @return gyro_mode value of IMU config
 */
uint8_t sh_imu_config::get_gyro_mode(void)
{
    return g_imu_config.gyro_mode;
}

/**
 * @brief Set IMU config value individually
 * 
 * @param gyro_range    set value of gyro_range
 *
 * @return integer success > 0, error <= 0.
 */
int sh_imu_config::set_gyro_range(uint8_t gyro_range)
{
    int ret = 1;

    if( (gyro_range >= 0) &&  (gyro_range <= 4) )
        g_imu_config.gyro_range = gyro_range;
    else 
        ret = CONFIG_SHELL_UNKNOWN_VALUE;

    return ret;
}
/**
 * @brief Get IMU config value individually
 *
 * @return gyro_range value of IMU config
 */
uint8_t sh_imu_config::get_gyro_range(void)
{
    return g_imu_config.gyro_range;
}

#endif // CONFIG_BMA456, CONFIG_SH_BMI323

#endif // CONFIG_NVS
