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

#ifndef __SIBEL_SHRD_CONFIG_H__
#define __SIBEL_SHRD_CONFIG_H__

#if defined (CONFIG_NVS) && defined (CONFIG_DATA_LOGGER)

#include <sh/sh_utils.h>

#define SHRD_ENABLE_FEATURE 	true
#define SHRD_DISABLE_FEATURE	false

#define SHRD_SWALLOW_ENABLE         0x00
#define SHRD_SCRATCH_ENABLE       	0x01
#define SHRD_SWALLOW_RAW_ENABLE	    0x02
#define SHRD_SCRATCH_RAW_ENABLE     0x03
#define SHRD_IBD_ENABLE				0X04
#define SHRD_COUGH_ENABLE			0x05

typedef enum{	
	SHRD_TEMPERATURE,
	SHRD_IMU,
	SHRD_BAROMETER,
	SHRD_MIC,
	SHRD_SCRATCH,
	SHRD_COUGH,
	SHRD_ACTIVITY,
	SHRD_ARM_ANGLE,
	SHRD_IMU_AGGREGATED,
	SHRD_EVENTS,
	SHRD_STEP
}shrd_config_type_t;

/**
 * @brief Structure defines the SHRD configuration
 */
typedef struct{
    bool	 		temp;               // Storage of temperature in shrd
	bool	 		imu;                // Storage of imu in shrd
	bool	 		baro;               // Storage of barometer in shrd
	bool	 		mic;                // Storage of microphone in shrd
	bool	 		scratch;            // Storage of scratch count in shrd
	bool	 		cough;              // Storage of cough count in shrd
	bool	 		activity;           // Storage of activity count in shrd
	bool	 		arm_angle;          // Storage of arm angle in shrd
	bool	 		imu_aggregated;     // Storage of imu aggregate data in shrd
	bool	 		events;             // shrd events 
	bool			steps;				// Storage of step data in shrd
}shrd_config_t;

#ifdef __cplusplus
namespace sh_shrd_config{

	void load_default_shrd_conf(shrd_config_t *shrd_config);

	void set_shrd_conf(shrd_config_t *shrd_config);

    void get_shrd_conf(shrd_config_t *shrd_config);

	void print_config_data(void);

	void set_profile(uint8_t profile);

	void set_temp(bool temp);
    bool get_temp(void);

	int set_imu(bool imu);
    bool get_imu(void);

	int set_baro(bool baro);
    bool get_baro(void);
	
	int set_mic(bool mic);
    bool get_mic(void);

	int set_scratch(bool scratch);
    bool get_scratch(void);
	
	int set_cough(bool cough);
    bool get_cough(void);

	int set_activity(bool activity);
    bool get_activity(void);

	int set_arm_angle(bool arm_angle);
    bool get_arm_angle(void);

	int set_imu_aggregated(bool imu_aggregated);
    bool get_imu_aggregated(void);

	int set_events(bool events);
    bool get_events(void);

	int set_steps(bool steps);
    bool get_steps(void);

}
#endif /* __cplusplus */

#endif // CONFIG_NVS

#endif /* __SIBEL_SHRD_CONFIG_H__ */
