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

/* Zephyr include files */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>
#include <string.h>

/* Sibel SDK files */
#include <sh/sh_nvs_handler.h>
#include <sh/sh_utils.h>

#include "data_storage.h"

LOG_MODULE_REGISTER(sh_data_storage, LOG_LEVEL_INF);

/**
 * @brief Structure defines index of shell command argument array
 */
struct args_index {
	uint8_t conf_type;
	uint8_t conf_para;
	uint8_t conf_val;
};

static const struct args_index args_indx = {
	.conf_type = 1,
	.conf_para = 2,
	.conf_val = 3,
};


// Data configuration type string for shell command
const char *config_type_str[] = {
    "factory_config",
    "haptic_config",
    "imu_config",
    "mic_config",
    "barometer_config",
    "shrd_config",
    "power_config",
};

// Profile string for shell command
const char *profile_type_str[] = {
    "swallow",
    "scratch",
    "swallow_raw",
    "scratch_raw",
    "ibd",
    "cough"
};

// Factory configuration parameter string for shell command
const char *factory_config_para_str[] = {
    "hw_version",
    "serial_number",
    "vendor_name",
    "manuf_date",
};

// Haptic configuration parameter string for shell command
const char *haptic_config_para_str[] = {
    "enabled",
    "intensity",
    "pulse_width",
    "interval",
    "duration",
};

// IMU configuration parameter string for shell command
const char *imu_config_para_str[] = {
    "enabled",
    "acc_odr",
    "acc_mode",
    "acc_range",
    "gyro_odr",
    "gyro_mode",
    "gyro_range",
};

// Barometer configuration parameter string for shell command
const char *baro_config_para_str[] = {
    "enabled",
    "bmp5_odr",
    "bmp5_fifo_threshold",
    "bmp5_fifo_frame_sel",
};

// Micrphone configuration parameter string for shell command
const char *mic_config_para_str[] = {
    "enabled",
    "mic_lchannel_gain",
    "mic_rchannel_gain",
};

// SHRD configuration parameter string for shell command
const char *shrd_config_para_str[] = {
    "temperature",
    "imu",
    "barometer",
    "microphone",
    "scratch",
    "cough",
    "activity",
    "arm_angle",
    "imu_aggregated",
    "events",
    "step"
};

// Power mode configuration parameter string for shell command
const char *power_config_type_str[] = {
    "normal",
    "low_power",
};


/**
 * @brief Load the sensor config data with default values
 *
 * @return None.
 */
static void load_default_data_conf(config_v1_t *config)
{
#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
    sh_imu_config::load_default_imu_conf(&config->data_config.imu_config);
#endif // CONFIG_SH_BMI323, CONFIG_BMA456

#if defined (CONFIG_SH_BMP5)
    sh_barometer_config::load_default_barometer_conf(&config->data_config.baro_config);
#endif // CONFIG_SH_BMP5

#if defined (CONFIG_DRV2624)
    sh_haptic_config::load_default_haptic_conf(&config->data_config.haptic_config);
#endif // CONFIG_DRV2624

#if defined (CONFIG_MICROPHONE)
    sh_mic_config::load_default_mic_conf(&config->data_config.mic_config);
#endif // CONFIG_MICROPHONE

#if defined (CONFIG_DATA_LOGGER)
    sh_shrd_config::load_default_shrd_conf(&config->data_config.shrd_config);
#endif // CONFIG_DATA_LOGGER

#if defined (CONFIG_PM_DEVICE)
    config->data_config.power_mode = NORMAL;
#endif // CONFIG_PM_DEVICE
}

/**
 * @brief Print all the configuration parameters
 *  
 * @return None.
 */
static void print_config_data(void)
{
    LOG_INF("Config version :: %d", CONFIG_CURRENT_VERSION);
    sh_factory_config::print_config_data();

#if defined (CONFIG_SH_BMP5)
    sh_barometer_config::print_config_data();
#endif //CONFIG_SH_BMP5

#if defined (CONFIG_DRV2624)
    sh_haptic_config::print_config_data();
#endif // CONFIG_DRV2624

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
    sh_imu_config::print_config_data();
#endif // CONFIG_SH_BMI323, CONFIG_BMA456

#if defined (CONFIG_MICROPHONE)
    sh_mic_config::print_config_data();
#endif // CONFIG_MICROPHONE

#if defined (CONFIG_DATA_LOGGER)
    sh_shrd_config::print_config_data();
#endif // CONFIG_DATA_LOGGER

}

/**
 * @brief Load all the configuration to respective data and factory config 
 *  
 * @return None.
 */
static void save_current_config(config_v1_t *curr_config)
{
    sh_factory_config::set_factory_conf(&curr_config->factory_config);

#if defined (CONFIG_SH_BMP5)
    sh_barometer_config::set_barometer_conf(&curr_config->data_config.baro_config);
#endif // CONFIG_SH_BMP5

#if defined (CONFIG_DRV2624)
    sh_haptic_config::set_haptic_conf(&curr_config->data_config.haptic_config);
#endif // CONFIG_DRV2624

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
    sh_imu_config::set_imu_conf(&curr_config->data_config.imu_config);
#endif // CONFIG_SH_BMI323, CONFIG_BMA456

#if defined (CONFIG_MICROPHONE)
    sh_mic_config::set_mic_conf(&curr_config->data_config.mic_config);
#endif // CONFIG_MICROPHONE

#if defined (CONFIG_DATA_LOGGER)
    sh_shrd_config::set_shrd_conf(&curr_config->data_config.shrd_config);
#endif // CONFIG_DATA_LOGGER

}

/**
 * @brief Save configuration into the NVS 
 * 
 * @param version   NVS key where config will be stored 
 * @param config    Configuration to be stored
 *  
 * @return int Success indicates 0 and failure is non-zero..
 */
static int set_config_from_version(uint8_t version, config_v1_t *config)
{
    uint8_t key;
    key = CONFIG_DEFAULT_KEY + version;

    return sh_nvs_handler::write_nvs_data(key, config, sizeof(config_v1_t));
}

/**
 * @brief Delete configuration into the NVS 
 * 
 * @param version   NVS key where config will be stored 
 *  
 * @return int Success indicates 0 and failure is non-zero..
 */
int sh_data_storage::delete_config_from_version(uint8_t version)
{
    uint8_t key;
    key = CONFIG_DEFAULT_KEY + version;

    return sh_nvs_handler::delete_nvs_data(key);
}

/**
 * @brief Read configuration from the NVS 
 * 
 * @param version   NVS key from where config will be read 
 * @param config    Configuration to be stored
 *  
 * @return int Success indicates 0 and failure is non-zero..
 */
static int get_config_from_version(uint8_t version, config_v1_t *config)
{
    uint8_t key;
    key = CONFIG_DEFAULT_KEY + version;

    return sh_nvs_handler::read_nvs_data(key, config, sizeof(config_v1_t));
}

/**
 * @brief Save power mode configuration. This is handle separatly instead of data 
 * config structure
 * 
 * @param pwr_mode  power mode to be updted
 *  
 * @return int Success indicates 0 and failure is non-zero..
 */
int sh_data_storage::set_power_mode(uint8_t pwr_mode)
{
    int ret = 0;
    config_v1_t local_config;

    ret = get_config_from_version(CONFIG_CURRENT_VERSION, &local_config);
    if(ret < 0) {
        LOG_ERR("Failed to read current config, unable to set power mode Error : %d", ret);
        return ret;
    }

    local_config.data_config.power_mode = (power_modes_t)pwr_mode;
    LOG_INF("Power mode updated to %d", local_config.data_config.power_mode);

    ret = set_config_from_version(CONFIG_CURRENT_VERSION, &local_config);
    if(ret < 0) {
        LOG_ERR("Power mode update failed, unable to write new power mode Error : %d", ret);
    }

    return ret;
}

/**
 * @brief Am example function that will be use to migrate when new configuration is added 
 * this function will take care of getting current config and load to the new config structure 
 * and then write the updated structure to the NVS at new location.
 * 
 * @param curr_config  current configuration read from the NVS
 *  
 * @return int Success indicates 0 and failure is non-zero..
 */
static int migrate_prev_with_current_config(config_v1_t *curr_config)
{
    // Add default value of a new config parameter here 
    // curr_config->data_config.mic_config.mic_mode = MIC_MODE_MONO;
    // curr_config->version = CONFIG_CURRENT_VERSION;
    // Once new config is saved, write the new config on current version location in NVS
    // return set_config_from_version(CONFIG_CURRENT_VERSION, curr_config);

    return 0;
}

/**
 * @brief Load the default configuration when no data found in the NVS at the 
 * asked location
 * 
 * @param version   current version to store default config 
 * @param config    configuration to be stored
 *  
 * @return int Success indicates 0 and failure is non-zero..
 */
static int load_default_config_from_version(uint8_t version, config_v1_t *config)
{
    int ret = 0;

    config->version = version;
    sh_factory_config::load_default_factory_conf(&config->factory_config);

    LOG_INF("Config default profile %d", SCRATCH_PROFILE);
    ret = sh_data_storage::config_default_profile((profile_type_t)SCRATCH_PROFILE, version, &config->factory_config);
    if(ret < 0) {
        LOG_ERR("Failed to configure default profile :: %d", ret);
    }

    return ret;
}

/**
 * @brief Fetch individual data configuration from the config structure
 *
 * @param type  Type of data configuration structure to be read
 * @param data  data pointer to hold the result
 * 
 * @return int Success indicates 0 and failure is non-zero..
 */
int sh_data_storage::fetch_data(config_type_t type, uint8_t *data)
{
    int ret = 0;

    switch (type)
    {
    case FACTORY_CONFIG:
        {
            factory_config_t local_conf;
            sh_factory_config::get_factory_conf(&local_conf);
            memcpy(data, &local_conf, sizeof(factory_config_t));
            ret = 1;
        }
        break;

#if defined (CONFIG_DRV2624)
    case HAPTIC_CONFIG:
        {
            haptic_config_t local_conf;
            sh_haptic_config::get_haptic_conf(&local_conf);
            memcpy(data, &local_conf, sizeof(haptic_config_t));
            ret = 1;
        }
        break;
#endif // CONFIG_DRV2624

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
    case IMU_CONFIG:
        {
            imu_config_t local_conf;
            sh_imu_config::get_imu_conf(&local_conf);
            memcpy(data, &local_conf, sizeof(imu_config_t));
            ret = 1;
        }
        break;
#endif // CONFIG_SH_BMI323, CONFIG_BMA456

#if defined (CONFIG_MICROPHONE)
    case MIC_CONFIG:
        {
            mic_data_config_t local_conf;
            sh_mic_config::get_mic_conf(&local_conf);
            memcpy(data, &local_conf, sizeof(mic_data_config_t));
            ret = 1;
        }
        break;
#endif // CONFIG_MICROPHONE

#if defined (CONFIG_SH_BMP5)
    case BARO_CONFIG:
        {
            baro_config_t local_conf;
            sh_barometer_config::get_barometer_conf(&local_conf);
            memcpy(data, &local_conf, sizeof(baro_config_t));
            ret = 1;
        }
        break;
#endif // CONFIG_SH_BMP5

#if defined (CONFIG_DATA_LOGGER)
    case SHRD_CONFIG:
        {
            shrd_config_t local_conf;
            sh_shrd_config::get_shrd_conf(&local_conf);
            memcpy(data, &local_conf, sizeof(shrd_config_t));
            ret = 1;
        }
        break;
#endif // CONFIG_DATA_LOGGER

#if defined (CONFIG_PM_DEVICE)
    case POWER_CONFIG:
        {
            config_v1_t local_config;
            ret = get_config_from_version(CONFIG_CURRENT_VERSION, &local_config);
            if(ret < 0) {
                LOG_ERR("Failed to read current config, unable to set power mode Error : %d", ret);
                return ret;
            }
            data[0] = local_config.data_config.power_mode;
        }
        break;
#endif // CONFIG_PM_DEVICE
	
    default:
        ret = -1;
        break;
    }

    return ret;
}

/**
 * @brief Store individual data configuration to the config structure
 *
 * @param type  Type of data configuration structure to be read
 * 
 * @return int Success indicates 0 and failure is non-zero..
 */
int sh_data_storage::store_data(config_type_t type)
{
    int ret = 0;
    config_v1_t local_conf;
    uint8_t data_buff[sizeof(config_v1_t)] = {0};

    // Read the currently stored config data
    ret = get_config_from_version(CONFIG_CURRENT_VERSION, &local_conf);
    if(ret < 0){
        LOG_ERR("Failed to read config, cannot store data. Error : %d", ret);
        return ret;
    }
    
    switch (type)
    {
    case FACTORY_CONFIG:
        {
            // Check if the factory configuration is editable or not 
            if(sh_factory_config::get_valid_config()) {
                LOG_ERR("Valid configuration present, cannot edit factory configuration");
                return CONFIG_SHELL_VALID_FACTORY;
            }

            ret = sh_data_storage::fetch_data(FACTORY_CONFIG, data_buff);
            memcpy(&local_conf.factory_config, data_buff, sizeof(factory_config_t));
            local_conf.factory_config.is_valid_config = VALID_FACTORY_DATA;
            sh_factory_config::set_valid_config(VALID_FACTORY_DATA);
        }
        break;
        
#if defined (CONFIG_DRV2624)
    case HAPTIC_CONFIG:
        {
            ret = sh_data_storage::fetch_data(HAPTIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.haptic_config, data_buff, sizeof(haptic_config_t));
        }
        break;
#endif // CONFIG_DRV2624

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
    case IMU_CONFIG:
        {
            ret = sh_data_storage::fetch_data(IMU_CONFIG, data_buff);
            memcpy(&local_conf.data_config.imu_config, data_buff, sizeof(imu_config_t));
        }
        break;
#endif // CONFIG_SH_BMI323, CONFIG_BMA456

#if defined (CONFIG_MICROPHONE)
    case MIC_CONFIG:
        {
            ret = sh_data_storage::fetch_data(MIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.mic_config, data_buff, sizeof(mic_data_config_t));
        }
        break;
#endif // CONFIG_MICROPHONE

#if defined (CONFIG_SH_BMP5)
    case BARO_CONFIG:
        {
            ret = sh_data_storage::fetch_data(BARO_CONFIG, data_buff);
            memcpy(&local_conf.data_config.baro_config, data_buff, sizeof(baro_config_t));
        }
        break;
#endif // CONFIG_SH_BMP5

#if defined (CONFIG_DATA_LOGGER)
    case SHRD_CONFIG:
        {
            ret = sh_data_storage::fetch_data(SHRD_CONFIG, data_buff);
            memcpy(&local_conf.data_config.shrd_config, data_buff, sizeof(shrd_config_t));
        }
        break;	
#endif // CONFIG_DATA_LOGGER

#if defined (CONFIG_PM_DEVICE)
    case POWER_CONFIG:
        {
            ret = sh_data_storage::fetch_data(POWER_CONFIG, data_buff);
            memcpy(&local_conf.data_config.power_mode, data_buff, sizeof(power_modes_t));
        }
        break;	
#endif // CONFIG_PM_DEVICE

    case ALL_CONFIG:
        {
#if defined (CONFIG_DRV2624)
            sh_data_storage::fetch_data(HAPTIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.haptic_config, data_buff, sizeof(haptic_config_t));
            memset(data_buff, 0, sizeof(data_buff));
#endif // CONFIG_DRV2624

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
            sh_data_storage::fetch_data(IMU_CONFIG, data_buff);
            memcpy(&local_conf.data_config.imu_config, data_buff, sizeof(imu_config_t));
            memset(data_buff, 0, sizeof(data_buff));
#endif // CONFIG_SH_BMI323, CONFIG_BMA456

#if defined (CONFIG_MICROPHONE)
            sh_data_storage::fetch_data(MIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.mic_config, data_buff, sizeof(mic_data_config_t));
            memset(data_buff, 0, sizeof(data_buff));
#endif // CONFIG_MICROPHONE

#if defined (CONFIG_SH_BMP5)
            sh_data_storage::fetch_data(BARO_CONFIG, data_buff);
            memcpy(&local_conf.data_config.baro_config, data_buff, sizeof(baro_config_t));
            memset(data_buff, 0, sizeof(data_buff));
#endif // CONFIG_SH_BMP5

#if defined (CONFIG_DATA_LOGGER)
            sh_data_storage::fetch_data(SHRD_CONFIG, data_buff);
            memcpy(&local_conf.data_config.shrd_config, data_buff, sizeof(shrd_config_t));
            memset(data_buff, 0, sizeof(data_buff));
#endif // CONFIG_DATA_LOGGER

#if defined (CONFIG_PM_DEVICE)
            sh_data_storage::fetch_data(POWER_CONFIG, data_buff);
            memcpy(&local_conf.data_config.power_mode, data_buff, sizeof(power_modes_t));
            memset(data_buff, 0, sizeof(data_buff));
#endif // CONFIG_PM_DEVICE

            ret = 1;
        }
        break;
    
    default:
        ret = -1;
        break;
    }

    if(ret > 0) {
        ret = set_config_from_version(CONFIG_CURRENT_VERSION, &local_conf);
        if(ret < 0) {
            LOG_ERR("Failed to get the configuration data from NVS Error :: %d", ret);
            return ret;
        }
    }

    LOG_DBG("Config store success");

    return ret;
}

/**
 * @brief parse the configuration type, parameter, and its value received from 
 * the shell command
 *
 * @param curr_config   pointer to the current configuration type text
 * @param config_arr    array containing the available configuration list 
 * @param count         size of array 
 * 
 * @return int Success indicates 0 and failure is non-zero..
 */
static int parse_config_data(char *curr_config, const char *config_arr[], int count)
{
    // Identify the index of configuration 
    for(int i=0; i<count; i++) {
        if( strcmp(curr_config, config_arr[i]) == 0 ) {
            return i;
        }
    }

    return -ENOTSUP;
}


/**
 * @brief Config default profile
 * 
 * @return int Success indicates 0 and failure is non-zero..
 */
int sh_data_storage::config_default_profile(profile_type_t profile, uint8_t version, factory_config_t *factory_config)
{
    int ret = 0;
    config_v1_t local_conf;
    uint8_t data_buff[sizeof(config_v1_t)] = {0};
    memcpy(&local_conf.factory_config, factory_config, sizeof(factory_config_t));

    switch (profile)
    {
    case SWALLOW_PROFILE:
        {
        #if defined (CONFIG_DRV2624)
            sh_haptic_config::set_profile(HAPTIC_SWALLOW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(HAPTIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.haptic_config, data_buff, sizeof(haptic_config_t));
        #endif // CONFIG_DRV2624
        
        #if defined (CONFIG_BMA456) || defined (CONFIG_SH_BMI323)
            sh_imu_config::set_profile(IMU_SWALLOW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(IMU_CONFIG, data_buff);
            memcpy(&local_conf.data_config.imu_config, data_buff, sizeof(imu_config_t));
        #endif // CONFIG_BMA456, CONFIG_SH_BMI323

        #if defined (CONFIG_MICROPHONE)
            sh_mic_config::set_profile(MIC_SWALLOW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(MIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.mic_config, data_buff, sizeof(mic_data_config_t));
        #endif // CONFIG_MICROPHONE

        #if defined (CONFIG_NVS) && defined (CONFIG_SH_BMP5)
            sh_barometer_config::set_profile(BARO_SWALLOW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(BARO_CONFIG, data_buff);
            memcpy(&local_conf.data_config.baro_config, data_buff, sizeof(baro_config_t));
        #endif // CONFIG_NVS && CONFIG_SH_BMP5

        #if defined (CONFIG_NVS) && defined (CONFIG_DATA_LOGGER)
            sh_shrd_config::set_profile(SHRD_SWALLOW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(SHRD_CONFIG, data_buff);
            memcpy(&local_conf.data_config.shrd_config, data_buff, sizeof(shrd_config_t));
        #endif // CONFIG_NVS && CONFIG_DATA_LOGGER

        }
        break;

    case SCRATCH_PROFILE:
        {
        #if defined (CONFIG_DRV2624) 
            sh_haptic_config::set_profile(HAPTIC_SCRATCH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(HAPTIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.haptic_config, data_buff, sizeof(haptic_config_t));
        #endif // CONFIG_DRV2624

        #if defined (CONFIG_BMA456) || defined (CONFIG_SH_BMI323)
            sh_imu_config::set_profile(IMU_SCRATCH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(IMU_CONFIG, data_buff);
            memcpy(&local_conf.data_config.imu_config, data_buff, sizeof(imu_config_t));
        #endif // CONFIG_BMA456, CONFIG_SH_BMI323

        #if defined (CONFIG_MICROPHONE)
            sh_mic_config::set_profile(MIC_SCRATCH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(MIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.mic_config, data_buff, sizeof(mic_data_config_t));
        #endif // CONFIG_MICROPHONE

        #if defined (CONFIG_NVS) && defined (CONFIG_SH_BMP5)
            sh_barometer_config::set_profile(BARO_SCRATCH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(BARO_CONFIG, data_buff);
            memcpy(&local_conf.data_config.baro_config, data_buff, sizeof(baro_config_t));
        #endif // CONFIG_NVS && CONFIG_SH_BMP5

        #if defined (CONFIG_NVS) && defined (CONFIG_DATA_LOGGER)
            sh_shrd_config::set_profile(SHRD_SCRATCH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(SHRD_CONFIG, data_buff);
            memcpy(&local_conf.data_config.shrd_config, data_buff, sizeof(shrd_config_t));
        #endif // CONFIG_NVS && CONFIG_DATA_LOGGER

        }
        break;
    
    case SWALLOW_RAW_DATA_PROFILE:
        {
           
        #if defined (CONFIG_DRV2624)
            sh_haptic_config::set_profile(HAPTIC_SWALLOW_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(HAPTIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.haptic_config, data_buff, sizeof(haptic_config_t));
        #endif // CONFIG_DRV2624
        
        #if defined (CONFIG_BMA456) || defined (CONFIG_SH_BMI323)
            sh_imu_config::set_profile(IMU_SWALLOW_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(IMU_CONFIG, data_buff);
            memcpy(&local_conf.data_config.imu_config, data_buff, sizeof(imu_config_t));
        #endif // CONFIG_BMA456, CONFIG_SH_BMI323

        #if defined (CONFIG_MICROPHONE)
            sh_mic_config::set_profile(MIC_SWALLOW_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(MIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.mic_config, data_buff, sizeof(mic_data_config_t));
        #endif // CONFIG_MICROPHONE

        #if defined (CONFIG_NVS) && defined (CONFIG_SH_BMP5)
            sh_barometer_config::set_profile(BARO_SWALLOW_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(BARO_CONFIG, data_buff);
            memcpy(&local_conf.data_config.baro_config, data_buff, sizeof(baro_config_t));
        #endif // CONFIG_NVS && CONFIG_SH_BMP5

        #if defined (CONFIG_NVS) && defined (CONFIG_DATA_LOGGER)
            sh_shrd_config::set_profile(SHRD_SWALLOW_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(SHRD_CONFIG, data_buff);
            memcpy(&local_conf.data_config.shrd_config, data_buff, sizeof(shrd_config_t));
        #endif // CONFIG_NVS && CONFIG_DATA_LOGGER

        }
        break;

    case SCRATCH_RAW_DATA_PROFILE:
        {
        #if defined (CONFIG_DRV2624) 
            sh_haptic_config::set_profile(HAPTIC_SCRATCH_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(HAPTIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.haptic_config, data_buff, sizeof(haptic_config_t));
        #endif // CONFIG_DRV2624

        #if defined (CONFIG_BMA456) || defined (CONFIG_SH_BMI323)
            sh_imu_config::set_profile(IMU_SCRATCH_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(IMU_CONFIG, data_buff);
            memcpy(&local_conf.data_config.imu_config, data_buff, sizeof(imu_config_t));
        #endif // CONFIG_BMA456, CONFIG_SH_BMI323

        #if defined (CONFIG_MICROPHONE)
            sh_mic_config::set_profile(MIC_SCRATCH_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(MIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.mic_config, data_buff, sizeof(mic_data_config_t));
        #endif // CONFIG_MICROPHONE

        #if defined (CONFIG_NVS) && defined (CONFIG_SH_BMP5)
            sh_barometer_config::set_profile(BARO_SCRATCH_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(BARO_CONFIG, data_buff);
            memcpy(&local_conf.data_config.baro_config, data_buff, sizeof(baro_config_t));
        #endif // CONFIG_NVS && CONFIG_SH_BMP5

        #if defined (CONFIG_NVS) && defined (CONFIG_DATA_LOGGER)
            sh_shrd_config::set_profile(SHRD_SCRATCH_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(SHRD_CONFIG, data_buff);
            memcpy(&local_conf.data_config.shrd_config, data_buff, sizeof(shrd_config_t));
        #endif // CONFIG_NVS && CONFIG_DATA_LOGGER
        }
        break;
    
    case IBD_PROFILE:
        {
        #if defined (CONFIG_DRV2624) 
            sh_haptic_config::set_profile(HAPTIC_IBD_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(HAPTIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.haptic_config, data_buff, sizeof(haptic_config_t));
        #endif // CONFIG_DRV2624

        #if defined (CONFIG_BMA456) || defined (CONFIG_SH_BMI323)
            sh_imu_config::set_profile(IMU_IBD_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(IMU_CONFIG, data_buff);
            memcpy(&local_conf.data_config.imu_config, data_buff, sizeof(imu_config_t));
        #endif // CONFIG_BMA456, CONFIG_SH_BMI323

        #if defined (CONFIG_MICROPHONE)
            sh_mic_config::set_profile(MIC_IBD_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(MIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.mic_config, data_buff, sizeof(mic_data_config_t));
        #endif // CONFIG_MICROPHONE

        #if defined (CONFIG_NVS) && defined (CONFIG_SH_BMP5)
            sh_barometer_config::set_profile(BARO_IBD_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(BARO_CONFIG, data_buff);
            memcpy(&local_conf.data_config.baro_config, data_buff, sizeof(baro_config_t));
        #endif // CONFIG_NVS && CONFIG_SH_BMP5

        #if defined (CONFIG_NVS) && defined (CONFIG_DATA_LOGGER)
            sh_shrd_config::set_profile(SHRD_IBD_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(SHRD_CONFIG, data_buff);
            memcpy(&local_conf.data_config.shrd_config, data_buff, sizeof(shrd_config_t));
        #endif // CONFIG_NVS && CONFIG_DATA_LOGGER
        }
        break;
    
    case COUGH_PROFILE:
        {
        #if defined (CONFIG_DRV2624)
            sh_haptic_config::set_profile(HAPTIC_COUGH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(HAPTIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.haptic_config, data_buff, sizeof(haptic_config_t));
        #endif // CONFIG_DRV2624

        #if defined (CONFIG_BMA456) || defined (CONFIG_SH_BMI323)
            sh_imu_config::set_profile(IMU_COUGH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(IMU_CONFIG, data_buff);
            memcpy(&local_conf.data_config.imu_config, data_buff, sizeof(imu_config_t));
        #endif // CONFIG_BMA456, CONFIG_SH_BMI323

        #if defined (CONFIG_MICROPHONE)
            sh_mic_config::set_profile(MIC_COUGH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(MIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.mic_config, data_buff, sizeof(mic_data_config_t));
        #endif // CONFIG_MICROPHONE

        #if defined (CONFIG_NVS) && defined (CONFIG_SH_BMP5)
            sh_barometer_config::set_profile(BARO_COUGH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(BARO_CONFIG, data_buff);
            memcpy(&local_conf.data_config.baro_config, data_buff, sizeof(baro_config_t));
        #endif // CONFIG_NVS && CONFIG_SH_BMP5

        #if defined (CONFIG_NVS) && defined (CONFIG_DATA_LOGGER)
            sh_shrd_config::set_profile(SHRD_COUGH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(SHRD_CONFIG, data_buff);
            memcpy(&local_conf.data_config.shrd_config, data_buff, sizeof(shrd_config_t));
        #endif // CONFIG_NVS && CONFIG_DATA_LOGGER
        }
        break;

    default:
        ret = CONFIG_SHELL_UNKNOWN_VALUE;
        break;
    }
    ret = set_config_from_version(version, &local_conf);
    if(ret < 0) {
        LOG_ERR("Failed to get the configuration data from NVS Error :: %d", ret);
    }

    return ret;
}


/**
 * @brief Store individual data configuration to the config structure
 *
 * @param type  Type of data configuration structure to be read
 * 
 * @return int Success indicates 0 and failure is non-zero..
 */
int sh_data_storage::handle_shell_profile_cmd(char *cmd_arg[])
{
    int ret = 0;
    config_v1_t local_conf;
    uint8_t data_buff[sizeof(config_v1_t)] = {0};

    // parse the configuration type 
    LOG_INF("Received write request of : %s", cmd_arg[args_indx.conf_type]);
    ret = parse_config_data(cmd_arg[args_indx.conf_type], profile_type_str, ARRAY_SIZE(profile_type_str));
    if(ret < 0) {
        LOG_ERR("Unknown configuration type");
        return CONFIG_SHELL_UNKNOWN_CONFIG;
    }

    switch (ret)
    {
    case SWALLOW_PROFILE:
        {
        #if defined (CONFIG_DRV2624)
            sh_haptic_config::set_profile(HAPTIC_SWALLOW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(HAPTIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.haptic_config, data_buff, sizeof(haptic_config_t));
        #endif // CONFIG_DRV2624
        
        #if defined (CONFIG_BMA456) || defined (CONFIG_SH_BMI323)
            sh_imu_config::set_profile(IMU_SWALLOW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(IMU_CONFIG, data_buff);
            memcpy(&local_conf.data_config.imu_config, data_buff, sizeof(imu_config_t));
        #endif // CONFIG_BMA456, CONFIG_SH_BMI323

        #if defined (CONFIG_MICROPHONE)
            sh_mic_config::set_profile(MIC_SWALLOW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(MIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.mic_config, data_buff, sizeof(mic_data_config_t));
        #endif // CONFIG_MICROPHONE

        #if defined (CONFIG_NVS) && defined (CONFIG_SH_BMP5)
            sh_barometer_config::set_profile(BARO_SWALLOW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(BARO_CONFIG, data_buff);
            memcpy(&local_conf.data_config.baro_config, data_buff, sizeof(baro_config_t));
        #endif // CONFIG_NVS && CONFIG_SH_BMP5

        #if defined (CONFIG_NVS) && defined (CONFIG_DATA_LOGGER)
            sh_shrd_config::set_profile(SHRD_SWALLOW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(SHRD_CONFIG, data_buff);
            memcpy(&local_conf.data_config.shrd_config, data_buff, sizeof(shrd_config_t));
        #endif // CONFIG_NVS && CONFIG_DATA_LOGGER

        }
        break;

    case SCRATCH_PROFILE:
        {
        #if defined (CONFIG_DRV2624) 
            sh_haptic_config::set_profile(HAPTIC_SCRATCH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(HAPTIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.haptic_config, data_buff, sizeof(haptic_config_t));
        #endif // CONFIG_DRV2624

        #if defined (CONFIG_BMA456) || defined (CONFIG_SH_BMI323)
            sh_imu_config::set_profile(IMU_SCRATCH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(IMU_CONFIG, data_buff);
            memcpy(&local_conf.data_config.imu_config, data_buff, sizeof(imu_config_t));
        #endif // CONFIG_BMA456, CONFIG_SH_BMI323

        #if defined (CONFIG_MICROPHONE)
            sh_mic_config::set_profile(MIC_SCRATCH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(MIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.mic_config, data_buff, sizeof(mic_data_config_t));
        #endif // CONFIG_MICROPHONE

        #if defined (CONFIG_NVS) && defined (CONFIG_SH_BMP5)
            sh_barometer_config::set_profile(BARO_SCRATCH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(BARO_CONFIG, data_buff);
            memcpy(&local_conf.data_config.baro_config, data_buff, sizeof(baro_config_t));
        #endif // CONFIG_NVS && CONFIG_SH_BMP5

        #if defined (CONFIG_NVS) && defined (CONFIG_DATA_LOGGER)
            sh_shrd_config::set_profile(SHRD_SCRATCH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(SHRD_CONFIG, data_buff);
            memcpy(&local_conf.data_config.shrd_config, data_buff, sizeof(shrd_config_t));
        #endif // CONFIG_NVS && CONFIG_DATA_LOGGER
        }
        break;
    
    case SWALLOW_RAW_DATA_PROFILE:
        {
           
        #if defined (CONFIG_DRV2624)
            sh_haptic_config::set_profile(HAPTIC_SWALLOW_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(HAPTIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.haptic_config, data_buff, sizeof(haptic_config_t));
        #endif // CONFIG_DRV2624
        
        #if defined (CONFIG_BMA456) || defined (CONFIG_SH_BMI323)
            sh_imu_config::set_profile(IMU_SWALLOW_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(IMU_CONFIG, data_buff);
            memcpy(&local_conf.data_config.imu_config, data_buff, sizeof(imu_config_t));
        #endif // CONFIG_BMA456, CONFIG_SH_BMI323

        #if defined (CONFIG_MICROPHONE)
            sh_mic_config::set_profile(MIC_SWALLOW_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(MIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.mic_config, data_buff, sizeof(mic_data_config_t));
        #endif // CONFIG_MICROPHONE

       #if defined (CONFIG_NVS) && defined (CONFIG_SH_BMP5)
            sh_barometer_config::set_profile(BARO_SWALLOW_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(BARO_CONFIG, data_buff);
            memcpy(&local_conf.data_config.baro_config, data_buff, sizeof(baro_config_t));
        #endif // CONFIG_NVS && CONFIG_SH_BMP5

        #if defined (CONFIG_NVS) && defined (CONFIG_DATA_LOGGER)
            sh_shrd_config::set_profile(SHRD_SWALLOW_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(SHRD_CONFIG, data_buff);
            memcpy(&local_conf.data_config.shrd_config, data_buff, sizeof(shrd_config_t));
        #endif // CONFIG_NVS && CONFIG_DATA_LOGGER
        }
        break;

    case SCRATCH_RAW_DATA_PROFILE:
        {
        #if defined (CONFIG_DRV2624) 
            sh_haptic_config::set_profile(HAPTIC_SCRATCH_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(HAPTIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.haptic_config, data_buff, sizeof(haptic_config_t));
        #endif // CONFIG_DRV2624

        #if defined (CONFIG_BMA456) || defined (CONFIG_SH_BMI323)
            sh_imu_config::set_profile(IMU_SCRATCH_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(IMU_CONFIG, data_buff);
            memcpy(&local_conf.data_config.imu_config, data_buff, sizeof(imu_config_t));
        #endif // CONFIG_BMA456, CONFIG_SH_BMI323

        #if defined (CONFIG_MICROPHONE)
            sh_mic_config::set_profile(MIC_SCRATCH_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(MIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.mic_config, data_buff, sizeof(mic_data_config_t));
        #endif // CONFIG_MICROPHONE

        #if defined (CONFIG_NVS) && defined (CONFIG_SH_BMP5)
            sh_barometer_config::set_profile(BARO_SCRATCH_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(BARO_CONFIG, data_buff);
            memcpy(&local_conf.data_config.baro_config, data_buff, sizeof(baro_config_t));
        #endif // CONFIG_NVS && CONFIG_SH_BMP5

        #if defined (CONFIG_NVS) && defined (CONFIG_DATA_LOGGER)
            sh_shrd_config::set_profile(SHRD_SCRATCH_RAW_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(SHRD_CONFIG, data_buff);
            memcpy(&local_conf.data_config.shrd_config, data_buff, sizeof(shrd_config_t));
        #endif // CONFIG_NVS && CONFIG_DATA_LOGGER
        }
        break;
    
    case IBD_PROFILE:
        {
        #if defined (CONFIG_DRV2624) 
            sh_haptic_config::set_profile(HAPTIC_IBD_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(HAPTIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.haptic_config, data_buff, sizeof(haptic_config_t));
        #endif // CONFIG_DRV2624

        #if defined (CONFIG_BMA456) || defined (CONFIG_SH_BMI323)
            sh_imu_config::set_profile(IMU_IBD_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(IMU_CONFIG, data_buff);
            memcpy(&local_conf.data_config.imu_config, data_buff, sizeof(imu_config_t));
        #endif // CONFIG_BMA456, CONFIG_SH_BMI323

        #if defined (CONFIG_MICROPHONE)
            sh_mic_config::set_profile(MIC_IBD_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(MIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.mic_config, data_buff, sizeof(mic_data_config_t));
        #endif // CONFIG_MICROPHONE

        #if defined (CONFIG_NVS) && defined (CONFIG_SH_BMP5)
            sh_barometer_config::set_profile(BARO_IBD_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(BARO_CONFIG, data_buff);
            memcpy(&local_conf.data_config.baro_config, data_buff, sizeof(baro_config_t));
        #endif // CONFIG_NVS && CONFIG_SH_BMP5

        #if defined (CONFIG_NVS) && defined (CONFIG_DATA_LOGGER)
            sh_shrd_config::set_profile(SHRD_IBD_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(SHRD_CONFIG, data_buff);
            memcpy(&local_conf.data_config.shrd_config, data_buff, sizeof(shrd_config_t));
        #endif // CONFIG_NVS && CONFIG_DATA_LOGGER
        }
        break;

    case COUGH_PROFILE:
        {
        #if defined (CONFIG_DRV2624)
            sh_haptic_config::set_profile(HAPTIC_COUGH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(HAPTIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.haptic_config, data_buff, sizeof(haptic_config_t));
        #endif // CONFIG_DRV2624

        #if defined (CONFIG_BMA456) || defined (CONFIG_SH_BMI323)
            sh_imu_config::set_profile(IMU_COUGH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(IMU_CONFIG, data_buff);
            memcpy(&local_conf.data_config.imu_config, data_buff, sizeof(imu_config_t));
        #endif // CONFIG_BMA456, CONFIG_SH_BMI323

        #if defined (CONFIG_MICROPHONE)
            sh_mic_config::set_profile(MIC_COUGH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(MIC_CONFIG, data_buff);
            memcpy(&local_conf.data_config.mic_config, data_buff, sizeof(mic_data_config_t));
        #endif // CONFIG_MICROPHONE

        #if defined (CONFIG_NVS) && defined (CONFIG_SH_BMP5)
            sh_barometer_config::set_profile(BARO_COUGH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(BARO_CONFIG, data_buff);
            memcpy(&local_conf.data_config.baro_config, data_buff, sizeof(baro_config_t));
        #endif // CONFIG_NVS && CONFIG_SH_BMP5

        #if defined (CONFIG_NVS) && defined (CONFIG_DATA_LOGGER)
            sh_shrd_config::set_profile(SHRD_COUGH_ENABLE);

            memset(data_buff, 0, sizeof(data_buff));
            sh_data_storage::fetch_data(SHRD_CONFIG, data_buff);
            memcpy(&local_conf.data_config.shrd_config, data_buff, sizeof(shrd_config_t));
        #endif // CONFIG_NVS && CONFIG_DATA_LOGGER
        }
        break;

    default:
        ret = CONFIG_SHELL_UNKNOWN_VALUE;
        break;
    }

    sh_factory_config::load_default_factory_conf(&local_conf.factory_config);

    if(ret >= 0) {
        ret = set_config_from_version(CONFIG_CURRENT_VERSION, &local_conf);
        if(ret < 0) {
            LOG_ERR("Failed to get the configuration data from NVS Error :: %d", ret);
            return ret;
        }
    }
    else
    {
        LOG_ERR("***Not able to store data into NVS");
    }

    LOG_DBG("New profile set and config stored in NVS");

    return ret;
}

/**
 * Note: The write command function should be updated to handle the multiple configurations. There
 * should be migration function for each version migration from new to old version and that function
 * should be responsible to load the new parameters with its default or stored value.
 * 
 * Also, while storing the individual configuration, each config version structure should store 
 * individually, i.e. if there is any change in config_v1_t structure data, it should be stored in 
 * 101, if there is any change in config_v3_t it should be stored in 103 and similar like that.
 * 
 * There should be a common function that can handle the version wise configuration read and loading
 * of individual data_config structure. For example, there should a switch case which has case statement
 * of 101, 102, 103, ... and so on. Each case shall have the migrte function for example 102 shall have 
 * migrate function of migrate_v1_to_v2 which will read the v1 data from NVS and set default value of v2
 * parameter. And similar for v3, v4, and so on.
 * 
 */

/**
 * @brief this function handles the write command to update the existing configuration
 *
 * @param cmd_arg   configuration type and sub commands
 *
 * @return int Success indicates 0 and failure is non-zero..
 */
int sh_data_storage::handle_shell_write_cmd(char *cmd_arg[])
{
    int config_param = 0;
    int config_value = 0;
    int ret = 0;
    
    // Parse the configuration type 
    LOG_DBG("Received write request of : %s", cmd_arg[args_indx.conf_type]);
    ret = parse_config_data(cmd_arg[args_indx.conf_type], config_type_str, ARRAY_SIZE(config_type_str));
    if(ret < 0) {
        LOG_ERR("Unknown configuration type");
        return CONFIG_SHELL_UNKNOWN_CONFIG;
    }

    switch (ret)
    {
    case FACTORY_CONFIG:
        {
            // Check if the factory configuration is editable or not 
            if(sh_factory_config::get_valid_config()) {
                LOG_ERR("Valid configuration present, cannot edit factory configuration");
                return CONFIG_SHELL_VALID_FACTORY;
            }

            // parse the configuration parameter 
            config_param = parse_config_data(cmd_arg[args_indx.conf_para], factory_config_para_str, ARRAY_SIZE(factory_config_para_str));
            if(config_param < 0) {
                LOG_ERR("Unknown configuraion parameter");
                return CONFIG_SHELL_UNKNOWN_PARAM;
            }

            LOG_DBG("FACTORY_CONFIG parameter name : %s", cmd_arg[args_indx.conf_para]);
            LOG_DBG("new value : %s", cmd_arg[args_indx.conf_val]);
            
            switch (config_param)
            {
            case FACTORY_HW_VERSION:
                {
                    config_value = sh_factory_config::set_hw_version((uint8_t *)cmd_arg[args_indx.conf_val]);
                }
                break;
            
            case FACTORY_SERIAL_NUMBER:
                {
                    config_value = sh_factory_config::set_serial_number((uint8_t *)cmd_arg[args_indx.conf_val]);
                }
                break;
            
            case FACTORY_MANUF_DATE:
                {
                    config_value = sh_factory_config::set_manufacturer((uint8_t *)cmd_arg[args_indx.conf_val]);      
                }
                break;
            
            case FACTORY_VENDOR_NAME:
                {
                    config_value = sh_factory_config::set_vendor((uint8_t *)cmd_arg[args_indx.conf_val]);
                }
                break;
            
            default:
                config_value = -EINVAL;
                break;
            }
        }
        break;

#if defined (CONFIG_DRV2624)
    case HAPTIC_CONFIG:
        {
            // parse the configuration parameter 
            config_param = parse_config_data(cmd_arg[args_indx.conf_para], haptic_config_para_str, ARRAY_SIZE(haptic_config_para_str));
            if(config_param < 0) {
                LOG_ERR("Unknown configuraion parameter");
                return CONFIG_SHELL_UNKNOWN_PARAM;
            }

            LOG_DBG("HAPTIC_CONFIG parameter name : %s", cmd_arg[args_indx.conf_para]);
            LOG_DBG("new value : %s", cmd_arg[args_indx.conf_val]);

            switch (config_param)
            {
            case HAPTIC_ENABLED:
                {
                    config_value = strtoul(cmd_arg[args_indx.conf_val], NULL, 0);
                    if(config_value >= 0)
                        sh_haptic_config::set_enabled(config_value);
                }
                break;
            
            case HAPTIC_DURATION:
                {
                    config_value = sh_haptic_config::set_duration(strtoul(cmd_arg[args_indx.conf_val], NULL, 0));
                }
                break;
            
            case HAPTIC_INTENSITY:
                {
                    config_value = sh_haptic_config::set_intensity(strtoul(cmd_arg[args_indx.conf_val], NULL, 0));
                }
                break;
            
            case HAPTIC_PULSE_WIDTH:
                {
                    config_value = sh_haptic_config::set_pulse_width(strtoul(cmd_arg[args_indx.conf_val], NULL, 0));
                }
                break;
            
            case HAPTIC_INTERVAL:
                {   
                    config_value = sh_haptic_config::set_interval(strtoul(cmd_arg[args_indx.conf_val], NULL, 0));
                }
                break;
            
            default:
                config_value = -EINVAL;
                break;
            }
        }
        break;
#endif // CONFIG_DRV2624

#if defined (CONFIG_SH_BMI323) || defined (CONFIG_BMA456)
    case IMU_CONFIG:
        {
            // parse the configuration parameter 
            config_param = parse_config_data(cmd_arg[args_indx.conf_para], imu_config_para_str, ARRAY_SIZE(imu_config_para_str));
            if(config_param < 0) {
                LOG_ERR("Unknown configuraion parameter");
                return CONFIG_SHELL_UNKNOWN_PARAM;
            }

            LOG_DBG("IMU_CONFIG parameter name : %s", cmd_arg[args_indx.conf_para]);
            LOG_DBG("new value : %s", cmd_arg[args_indx.conf_val]);

            switch (config_param)
            {
            case IMU_ENABLED:
                {
                    config_value = strtoul(cmd_arg[args_indx.conf_val], NULL, 0);
                    if(config_value >= 0)
                        sh_imu_config::set_enabled(config_value);
                }
                break;
            
            case IMU_ACC_ODR:
                {
                    config_value = sh_imu_config::set_acc_odr(strtoul(cmd_arg[args_indx.conf_val], NULL, 0));
                }
                break;
            
            case IMU_ACC_MODE:
                {
                    config_value = sh_imu_config::set_acc_mode(strtoul(cmd_arg[args_indx.conf_val], NULL, 0)); 
                }
                break;
            
            case IMU_ACC_RANGE:
                {
                    config_value = sh_imu_config::set_acc_range(strtoul(cmd_arg[args_indx.conf_val], NULL, 0));
                }
                break;
            
            case IMU_GYRO_ODR:
                {
                    config_value = sh_imu_config::set_gyro_odr(strtoul(cmd_arg[args_indx.conf_val], NULL, 0));
                }
                break;
            
            case IMU_GYRO_MODE:
                {
                    config_value = sh_imu_config::set_gyro_mode(strtoul(cmd_arg[args_indx.conf_val], NULL, 0));
                }
                break;
            
            case IMU_GYRO_RANGE:
                {
                    config_value = sh_imu_config::set_gyro_range(strtoul(cmd_arg[args_indx.conf_val], NULL, 0));
                }
                break;
            
            default:
                config_value = -EINVAL;
                break;
            }
        }
        break;
#endif // CONFIG_SH_BMI323, CONFIG_BMA456

#if defined (CONFIG_MICROPHONE)
    case MIC_CONFIG:
        {
            // parse the configuration parameter 
            config_param = parse_config_data(cmd_arg[args_indx.conf_para], mic_config_para_str, ARRAY_SIZE(mic_config_para_str));
            if(config_param < 0) {
                LOG_ERR("Unknown configuraion parameter");
                return CONFIG_SHELL_UNKNOWN_PARAM;
            }

            LOG_DBG("MIC_CONFIG parameter name : %s", cmd_arg[args_indx.conf_para]);
            LOG_DBG("new value : %s", cmd_arg[args_indx.conf_val]);

            switch (config_param)
            {
            case MIC_ENABLED:
                {
                    config_value = strtoul(cmd_arg[args_indx.conf_val], NULL, 0);
                    if(config_value >= 0)
                        sh_mic_config::set_enabled(config_value);
                }
                break;
            case MIC_LCHANNEL_GAIN:
                {   
                    config_value = sh_mic_config::set_lchannel_gain(strtoul(cmd_arg[args_indx.conf_val], NULL, 0));
                }
                break;
            case MIC_RCHANNEL_GAIN:
                {
                    config_value = sh_mic_config::set_rchannel_gain(strtoul(cmd_arg[args_indx.conf_val], NULL, 0));
                }
                break;
            
            default:
                config_value = -EINVAL;
                break;
            }
        }
        break;
#endif // CONFIG_MICROPHONE

#if defined (CONFIG_SH_BMP5)
    case BARO_CONFIG:
        {
            // parse the configuration parameter 
            config_param = parse_config_data(cmd_arg[args_indx.conf_para], baro_config_para_str, ARRAY_SIZE(baro_config_para_str));
            if(config_param < 0) {
                LOG_ERR("Unknown configuraion parameter");
                return CONFIG_SHELL_UNKNOWN_PARAM;
            }

            LOG_DBG("BARO_CONFIG parameter name : %s", cmd_arg[args_indx.conf_para]);
            LOG_DBG("new value : %s", cmd_arg[args_indx.conf_val]);

            switch (config_param)
            {
            case BARO_ENABLED:
                {
                    config_value = strtoul(cmd_arg[args_indx.conf_val], NULL, 0);
                    if(config_value >= 0)
                        sh_barometer_config::set_enabled(config_value);
                }
                break;

            case BARO_ODR:
                {
                    config_value = sh_barometer_config::set_odr(strtoul(cmd_arg[args_indx.conf_val], NULL, 0));
                }
                break;
            case BARO_FIFO_THRESHOLD:
                {
                    config_value = sh_barometer_config::set_fifo_threshold(strtoul(cmd_arg[args_indx.conf_val], NULL, 0));
                }
                break;

            case BARO_FIFO_FRAME_SEL:
                {
                    config_value = sh_barometer_config::set_frame_sel(strtoul(cmd_arg[args_indx.conf_val], NULL, 0));
                }
                break;
                
            
            default:
                config_value = -EINVAL;
                break;
            }
        }
        break;
#endif // CONFIG_SH_BMP5
    
#if defined (CONFIG_DATA_LOGGER)
    case SHRD_CONFIG:
        {
            // parse the configuration parameter 
            config_param = parse_config_data(cmd_arg[args_indx.conf_para], shrd_config_para_str, ARRAY_SIZE(shrd_config_para_str));
            if(config_param < 0) {
                LOG_ERR("Unknown configuraion parameter");
                return CONFIG_SHELL_UNKNOWN_PARAM;
            }

            LOG_DBG("SHRD_CONFIG parameter name : %s", cmd_arg[args_indx.conf_para]);
            LOG_DBG("new value : %s", cmd_arg[args_indx.conf_val]);

            config_value = 1;

            switch (config_param)
            {
            case SHRD_TEMPERATURE:
                {
                    if( strcmp(cmd_arg[args_indx.conf_val], "yes") == 0 ) {
                        sh_shrd_config::set_temp(SHRD_ENABLE_FEATURE);
                    } else if( strcmp(cmd_arg[args_indx.conf_val], "no") == 0 ) {
                        sh_shrd_config::set_temp(SHRD_DISABLE_FEATURE);
                    } else 
                        config_value = CONFIG_SHELL_UNKNOWN_PARAM;
                }
                break;
            
            case SHRD_IMU:
                {
                    if( strcmp(cmd_arg[args_indx.conf_val], "yes") == 0 ) {
                        sh_shrd_config::set_imu(SHRD_ENABLE_FEATURE);
                    } else if( strcmp(cmd_arg[args_indx.conf_val], "no") == 0 ) {
                        sh_shrd_config::set_imu(SHRD_DISABLE_FEATURE);
                    } else 
                        config_value = CONFIG_SHELL_UNKNOWN_PARAM;
                }
                break;
            
            case SHRD_SCRATCH:
                {
                    if( strcmp(cmd_arg[args_indx.conf_val], "yes") == 0 ) {
                        sh_shrd_config::set_scratch(SHRD_ENABLE_FEATURE);
                    } else if( strcmp(cmd_arg[args_indx.conf_val], "no") == 0 ) {
                        sh_shrd_config::set_scratch(SHRD_DISABLE_FEATURE);
                    } else 
                        config_value = CONFIG_SHELL_UNKNOWN_PARAM;
                }
                break;
            
            case SHRD_ACTIVITY:
                {
                    if( strcmp(cmd_arg[args_indx.conf_val], "yes") == 0 ) {
                        sh_shrd_config::set_activity(SHRD_ENABLE_FEATURE);
                    } else if( strcmp(cmd_arg[args_indx.conf_val], "no") == 0 ) {
                        sh_shrd_config::set_activity(SHRD_DISABLE_FEATURE);
                    } else 
                        config_value = CONFIG_SHELL_UNKNOWN_PARAM;
                }
                break;
            
            case SHRD_IMU_AGGREGATED:
                {
                    if( strcmp(cmd_arg[args_indx.conf_val], "yes") == 0 ) {
                        sh_shrd_config::set_imu_aggregated(SHRD_ENABLE_FEATURE);
                    } else if( strcmp(cmd_arg[args_indx.conf_val], "no") == 0 ) {
                        sh_shrd_config::set_imu_aggregated(SHRD_DISABLE_FEATURE);
                    } else 
                        config_value = CONFIG_SHELL_UNKNOWN_PARAM;
                }
                break;
            
            case SHRD_BAROMETER:
                {
                    if( strcmp(cmd_arg[args_indx.conf_val], "yes") == 0 ) {
                        sh_shrd_config::set_baro(SHRD_ENABLE_FEATURE);
                    } else if( strcmp(cmd_arg[args_indx.conf_val], "no") == 0 ) {
                        sh_shrd_config::set_baro(SHRD_DISABLE_FEATURE);
                    } else 
                        config_value = CONFIG_SHELL_UNKNOWN_PARAM;
                }
                break;
            
            case SHRD_MIC:
                {
                    if( strcmp(cmd_arg[args_indx.conf_val], "yes") == 0 ) {
                        sh_shrd_config::set_mic(SHRD_ENABLE_FEATURE);
                    } else if( strcmp(cmd_arg[args_indx.conf_val], "no") == 0 ) {
                        sh_shrd_config::set_mic(SHRD_DISABLE_FEATURE);
                    } else 
                        config_value = CONFIG_SHELL_UNKNOWN_PARAM;
                }
                break;
            
            case SHRD_COUGH:
                {
                    if( strcmp(cmd_arg[args_indx.conf_val], "yes") == 0 ) {
                        sh_shrd_config::set_cough(SHRD_ENABLE_FEATURE);
                    } else if( strcmp(cmd_arg[args_indx.conf_val], "no") == 0 ) {
                        sh_shrd_config::set_cough(SHRD_DISABLE_FEATURE);
                    } else 
                        config_value = CONFIG_SHELL_UNKNOWN_PARAM;
                }
                break;
            
            case SHRD_ARM_ANGLE:
                {
                    if( strcmp(cmd_arg[args_indx.conf_val], "yes") == 0 ) {
                        sh_shrd_config::set_arm_angle(SHRD_ENABLE_FEATURE);
                    } else if( strcmp(cmd_arg[args_indx.conf_val], "no") == 0 ) {
                        sh_shrd_config::set_arm_angle(SHRD_DISABLE_FEATURE);
                    } else 
                        config_value = CONFIG_SHELL_UNKNOWN_PARAM;
                }
                break;
            
            case SHRD_EVENTS:
                {
                    if( strcmp(cmd_arg[args_indx.conf_val], "yes") == 0 ) {
                        sh_shrd_config::set_events(SHRD_ENABLE_FEATURE);
                    } else if( strcmp(cmd_arg[args_indx.conf_val], "no") == 0 ) {
                        sh_shrd_config::set_events(SHRD_DISABLE_FEATURE);
                    } else 
                        config_value = CONFIG_SHELL_UNKNOWN_PARAM;
                }
                break;
            
            case SHRD_STEP:
                {
                    if( strcmp(cmd_arg[args_indx.conf_val], "yes") == 0 ) {
                        sh_shrd_config::set_steps(SHRD_ENABLE_FEATURE);
                    } else if( strcmp(cmd_arg[args_indx.conf_val], "no") == 0 ) {
                        sh_shrd_config::set_steps(SHRD_DISABLE_FEATURE);
                    } else 
                        config_value = CONFIG_SHELL_UNKNOWN_PARAM;
                }
                break;
            
            default:
                config_value = CONFIG_SHELL_UNKNOWN_PARAM;
                break;
            }
        }
        break;
#endif // CONFIG_DATA_LOGGER

#if defined (CONFIG_PM_DEVICE)
    case POWER_CONFIG:
        {
            LOG_DBG("POWER_CONFIG parameter name : %s", cmd_arg[args_indx.conf_para]);

            if( strcmp(cmd_arg[args_indx.conf_para], "normal") == 0 ) {
                LOG_DBG("setting power config to NORMAL");
                config_value = sh_data_storage::set_power_mode(NORMAL);
            } else if( strcmp(cmd_arg[args_indx.conf_para], "lowpower") == 0 ) {
                config_value = sh_data_storage::set_power_mode(LOW_POWER);
                LOG_DBG("setting power config to LOW POWER");
            } else {
                config_value = CONFIG_SHELL_UNKNOWN_PARAM;
                LOG_DBG("Invalid power config received");
            }
        }
        break;
#endif // CONFIG_PM_DEVICE
    
    default:
        config_value = CONFIG_SHELL_UNKNOWN_PARAM;
        break;
    }
    
    return config_value;
}

/**
 * @brief Read configuration from the NVS 
 * 
 * @param version   NVS key from where config will be read 
 * @param config    Configuration to be stored
 *  
 * @return int Success indicates 0 and failure is non-zero..
 */
static int read_data_from_storage(uint8_t version, uint8_t *conf_buff, uint16_t buff_len)
{
    uint8_t key;
    key = CONFIG_DEFAULT_KEY + version;

    return sh_nvs_handler::read_nvs_data(key, conf_buff, buff_len);
}

static int load_current_config_structure(uint8_t version, uint8_t *conf_buff, uint16_t buff_len, bool load_defaut)
{
    int ret = 0;

    switch (version)
    {
    case CONFIG_VERSION_1:
        {
            config_v1_t local_config;

            if(load_defaut) {
                ret = load_default_config_from_version(CONFIG_VERSION_1, &local_config);
                if(ret < 0) {
                    LOG_ERR("Failed to load default config version :: %d with Error : %d", CONFIG_VERSION_1, ret);
                } else {
                    LOG_INF("No data found of version %d default data loaded", CONFIG_VERSION_1);
                    print_config_data();
                }
            } else {
                // copy the read data from the NVS and load the respective structures 
                memcpy(&local_config, conf_buff, sizeof(config_v1_t));
                LOG_DBG("Save current config to respective data strcutre");
                save_current_config(&local_config);
                print_config_data();
                ret = 1;
            }
        }
        break;

    case CONFIG_VERSION_2:
        {
            // Instance of the configuration structure version 2
            // config_v2_t local_config;

            if(load_defaut) {
                /**
                 * Load default data of configuration version 2 
                 * Need to create new functions to load the default data config
                 * 
                 * Note that we do not need to update the factory configuration as it
                 * can only be updated once in the device life time so no need to 
                 * load the default configuration of factroy data 
                 * 
                 */
            } else {
                /**
                 * Copy the read data from NVS to the respective confg structure of version 2
                 * Save all the data config structure of version 2
                 * Print updated data
                 */
            }
        }
        break;
    
    /** 
     * Based on the upcoming version, there should be individual cases of each version 
     * and there should be individual handlers of respective data config structure that 
     * can load their global instaces based on the read data
     * 
     * There should be version wise config_vx_t structures that contains the updated 
     * data_config_vx_t structure having newly added parameters only 
     */
    
    
    default:
        break;
    }

    return ret;
}

static int read_config_from_storage(void)
{
    int ret = 0;
    uint8_t config_buff[256] = {0};
    bool load_default = false;

    for(int ver=CONFIG_VERSION_1; ver<=CONFIG_CURRENT_VERSION; ver++) {
        ret = read_data_from_storage(ver, config_buff, sizeof(config_buff));
        if(ret < 0) {
            load_default = true;
        } else {
            load_default = false;
        }   

        ret = load_current_config_structure(ver, config_buff, sizeof(config_buff), load_default);
        if(ret < 0) {
            LOG_ERR("Unable to load the current config :: %d", ret);
            break;
        }

        memset(config_buff, 0, sizeof(config_buff));
    }

    return ret;
}

/**
 * @brief Initializes the data storage of configurations 
 *
 * @return int32_t Success indicates 0 and failure is non-zero.
 */
int32_t sh_data_storage::init(void)
{
    int ret = 0;

    // Initialize the NVS 
    ret = sh_nvs_handler::init();
    if(ret < 0) {
        LOG_ERR("NVS init failed Error: %d", ret);
        return ret;
    }

    /**
     * Note: To handle the multiple configuration, there should be the a loop that reads the NVS
     * starting from the location 101 till the current configuration version.
     * 
     * This loop shall load all the config_vx_t structure and also load the individual data_config_vx_t 
     * structures as well. 
     * 
     * In the case of missing any config in between the version i.e. for instance a firmware is running 
     * on the device having config stored in 101, and an OTA has been performed having the version of 104,
     * than 102 and 103 location will found empty in NVS and thoes config structure should be load with 
     * their default values.
     */

    ret = read_config_from_storage();
    if(ret < 0) {
        LOG_ERR("Failed to read the config from storage");
    } else {
        LOG_INF("Config loaded successfully");
    }

#if 0
    ret = get_config_from_version(CONFIG_CURRENT_VERSION, &curr_config);
    if(ret < 0) {
        LOG_DBG("No data found of version %d", CONFIG_CURRENT_VERSION);
        if(CONFIG_CURRENT_VERSION == CONFIG_VERSION_1) {
            // load default config at location 101 with version 1 
            ret = load_default_config_from_version(CONFIG_CURRENT_VERSION, &curr_config);
            if(ret < 0) {
                LOG_ERR("Failed to load default config version :: %d with Error : %d", CONFIG_CURRENT_VERSION, ret);
            } else {
                LOG_DBG("No version found, hence loading default values with version %d", CONFIG_CURRENT_VERSION);
            }
        } else {
            // read last config to migrate with current config
            ret = get_config_from_version(CONFIG_CURRENT_VERSION-1, &curr_config);
            if(ret < 0){
                // load default with current version
                ret = load_default_config_from_version(CONFIG_CURRENT_VERSION, &curr_config);
                if(ret < 0) {
                    LOG_ERR("Failed to load default config version :: %d with Error : %d", CONFIG_CURRENT_VERSION, ret);
                } else {
                    LOG_DBG("No previous version found, hence loading default values with version %d", CONFIG_CURRENT_VERSION);
                }
            } else {
                // migrate previous config with current config
                ret = migrate_prev_with_current_config(&curr_config);
                if(ret < 0) {
                    LOG_ERR("Error in migration between previous and current config %d", ret);
                } else {
                    LOG_DBG("Migrating version %d to new version %d", CONFIG_CURRENT_VERSION-1, CONFIG_CURRENT_VERSION);
                }
            }
        }
    }
    
    if(ret > 0) {
        // load all data config 
        LOG_DBG("Data found of version %d", curr_config.version);
        LOG_DBG("Save current config to respective data strcutre");
        save_current_config(&curr_config);
        print_config_data();
    }
#endif   

    return ret;
}

#endif // CONFIG_NVS
