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

/* Sibel SDK files */
#include <sh/sh_nvs_handler.h>
#include <sh/sh_utils.h>

#include "factory_config.h"

LOG_MODULE_REGISTER(sh_factory_config, LOG_LEVEL_ERR);

// Factory config structure instance
factory_config_t g_factory_config;


/**
 * @brief Load the Factory config structure with default values
 *
 * @return None.
 */
void sh_factory_config::load_default_factory_conf(factory_config_t *factory_config)
{    
    g_factory_config.is_valid_config = INVALID_FACTORY_DATA; 
    memset(g_factory_config.hw_version, 0, HW_VER_SIZE);
    strcpy((char *)g_factory_config.manuf_date, FACTORY_DEFAULT_MANUF_DATE);
    strcpy((char *)g_factory_config.vendor_name, FACTORY_DEFAULT_VENDOR);
    strcpy((char *)g_factory_config.serial_number, FACTORY_DEFAULT_SERIAL_NUM);
    #ifdef CONFIG_BT_DIS_HW_REV_STR
        strcpy((char *)g_factory_config.hw_version, CONFIG_BT_DIS_HW_REV_STR);
    #else  
        strcpy((char *)g_factory_config.hw_version, "0.1");
    #endif
    *factory_config = g_factory_config;
}

/**
 * @brief Read data from the data_storage and store the NVS 
 * data in global structure
 * 
 * @param factory_config    Instance of factory_config
 *
 * @return None.
 */
void sh_factory_config::set_factory_conf(factory_config_t *factory_config)
{
    g_factory_config = *factory_config;
}

/**
 * @brief Pass the current structure data to data_storage to
 * store it into NVS
 * 
 * @param factory_config    Instance of factory_config
 *
 * @return None.
 */
void sh_factory_config::get_factory_conf(factory_config_t *factory_config)
{
    *factory_config = g_factory_config;
}

/**
 * @brief Print the configuration parameters
 *  
 * @return None.
 */
void sh_factory_config::print_config_data(void)
{
    LOG_INF("\ng_factory_config.is_valid_config : %d", g_factory_config.is_valid_config);
    LOG_INF("g_factory_config.hw_version      : %s", g_factory_config.hw_version);
    LOG_INF("g_factory_config.serial_number   : %s", g_factory_config.serial_number);
    LOG_INF("g_factory_config.vendor_name     : %s", g_factory_config.vendor_name);
    LOG_INF("g_factory_config.manuf_date      : %s", g_factory_config.manuf_date);
}

/**
 * @brief Set factory config value individually
 * 
 * @param valid_config    set value of valid_config
 *
 * @return integer success > 0, error <= 0.
 */
int sh_factory_config::set_valid_config(uint8_t valid_config)
{
    g_factory_config.is_valid_config = valid_config;
    return valid_config;
}
/**
 * @brief Get factory config value individually
 *
 * @return valid_config value of factory config
 */
uint8_t sh_factory_config::get_valid_config(void)
{
    return g_factory_config.is_valid_config;
}

/**
 * @brief Set factory config value individually
 * 
 * @param *serial_number    set value of serial_number
 *
 * @return integer success > 0, error <= 0.
 */
int sh_factory_config::set_serial_number(uint8_t *serial_number)
{
    int config_value = 0;
    
    config_value = strlen((const char *)serial_number);
    if( (config_value > 0) && (config_value <= SERIAL_NUMBER_SIZE))
        strcpy((char *)g_factory_config.serial_number, (const char *)serial_number);
    else 
        config_value = CONFIG_SHELL_UNKNOWN_VALUE;
    
    return config_value;
}
/**
 * @brief Get factory config value individually
 *
 * @return serial_number value of factory config
 */
uint8_t *sh_factory_config::get_serial_number(void)
{
    return g_factory_config.serial_number;
}

/**
 * @brief Set factory config value individually
 * 
 * @param *hw_version    set value of hw_version
 *
 * @return integer success > 0, error <= 0.
 */
int sh_factory_config::set_hw_version(uint8_t *hw_version)
{
    int config_value = 0;

    config_value = strlen((const char *)hw_version);
    if( (config_value > 0) && (config_value <= HW_VER_SIZE))
        strcpy((char *)g_factory_config.hw_version, (const char *)hw_version);
    else 
        config_value = CONFIG_SHELL_UNKNOWN_VALUE;
    
    return config_value;
}
/**
 * @brief Get factory config value individually
 *
 * @return hw_version value of factory config
 */
uint8_t *sh_factory_config::get_hw_version(void)
{
    return g_factory_config.hw_version;
}

/**
 * @brief Set factory config value individually
 * 
 * @param *vendor    set value of vendor
 *
 * @return integer success > 0, error <= 0.
 */
int sh_factory_config::set_vendor(uint8_t *vendor)
{
    int config_value = 0;
    
    config_value = strlen((const char *)vendor);
    if( (config_value > 0) && (config_value <= VENDOR_NAME_SIZE))
        strcpy((char *)g_factory_config.vendor_name, (const char *)vendor);
    else 
        config_value = CONFIG_SHELL_UNKNOWN_VALUE;
    
    return config_value;
}
/**
 * @brief Get factory config value individually
 *
 * @return vendor value of factory config
 */
uint8_t *sh_factory_config::get_vendor(void)
{
    return g_factory_config.vendor_name;
}

/**
 * @brief Set factory config value individually
 * 
 * @param *manufacturer    set value of manufacturer
 *
 * @return integer success > 0, error <= 0.
 */
int sh_factory_config::set_manufacturer(uint8_t *manufacturer)
{
    int config_value = 0;
    
    config_value = strlen((const char *)manufacturer);
    if( (config_value > 0) && (config_value <= MANUF_DATE_SIZE))
        strcpy((char *)g_factory_config.manuf_date, (const char *)manufacturer);
    else 
        config_value = CONFIG_SHELL_UNKNOWN_VALUE;
    
    return config_value;
}
/**
 * @brief Get factory config value individually
 *
 * @return manufacturer value of factory config
 */
uint8_t *sh_factory_config::get_manufacturer(void)
{
    return g_factory_config.manuf_date;
}

#endif // CONFIG_NVS
