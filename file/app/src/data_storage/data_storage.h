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

#ifndef __SIBEL_DATA_STORAGE_H__
#define __SIBEL_DATA_STORAGE_H__

#if defined (CONFIG_NVS)

#include "factory_config.h"
#include "haptic_config.h"
#include "imu_config.h"
#include "mic_config.h"
#include "barometer_config.h"
#include "shrd_config.h"

#define CONFIG_DEFAULT_KEY          (100)
#define CONFIG_V1_KEY               (101)
#define CONFIG_V2_KEY               (102)
#define CONFIG_MAX_KEY_RANGE        (150)

#define MAX_CONFIG_KEY_STORAGE      50
#define CONFIG_VERSION_1            1
#define CONFIG_VERSION_2            2
#define CONFIG_CURRENT_VERSION      1
#define CONFIG_DATA_RFU_SIZE        100

typedef enum{
	LOW_POWER,
	NORMAL
}power_modes_t;

typedef enum{	
	FACTORY_CONFIG,
	HAPTIC_CONFIG,
	IMU_CONFIG,
	MIC_CONFIG,
	BARO_CONFIG,
	SHRD_CONFIG,
	POWER_CONFIG,
    ALL_CONFIG
}config_type_t;

typedef enum{
    SWALLOW_PROFILE,
    SCRATCH_PROFILE,
    SWALLOW_RAW_DATA_PROFILE,
    SCRATCH_RAW_DATA_PROFILE,
    IBD_PROFILE,
    COUGH_PROFILE
}profile_type_t;

//////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Structure defines the sensor data configurations
 */
typedef struct{
    /** Structure to hold Haptic configuration */
	haptic_config_t	    haptic_config;

    /** Structure to hold IMU configuration */
	imu_config_t	    imu_config;

    /** Structure to hold Microphone configuration */
	mic_data_config_t   mic_config;

    /** Structure to hold Barometer configuration */
	baro_config_t	    baro_config;

    /** Structure to hold SHARD configuration */
	shrd_config_t	    shrd_config;

    /** Structure to hold Power Module configuration */
	power_modes_t		power_mode;
}data_config_t;


/**
 * @brief Structure defines the factory and device data configuration 
 */
typedef struct{
    /** Version of the structure */
    uint8_t				version;

    /** structure contains factory configuration parameters */
    factory_config_t	factory_config;

    /** structure contains data configuration parameters */
    data_config_t		data_config;
}config_v1_t;


/**
 * Note: For adding the new configuration config_v2_t, this structure should have the version
 * and the updated data_config_v2_t only.
 * 
 * For example, in version 2 configuration, microphone has a new configuration named mic_mode.
 * With this new member there should be new config_v2_t structure which will have two members
 * version and data_config_v2_t. The data_config_v2_t will only have one member mic_config_v2_t
 * which has one member mic_mode.
 */


#ifdef __cplusplus
namespace sh_data_storage{

    int init(void);

    int fetch_data(config_type_t type, uint8_t *data);

    int store_data(config_type_t type);

    int set_power_mode(uint8_t pwr_mode);

    int handle_shell_write_cmd(char *cmd_arg[]);

    int handle_shell_profile_cmd(char *cmd_arg[]);

    int config_default_profile(profile_type_t profile, uint8_t version, factory_config_t *factory_config);

    int delete_config_from_version(uint8_t version);


}
#endif /* __cplusplus */

#endif // CONFIG_NVS

#endif /* __SIBEL_DATA_STORAGE_H__ */
