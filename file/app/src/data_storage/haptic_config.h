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

#ifndef __SIBEL_HAPTIC_CONFIG_H__
#define __SIBEL_HAPTIC_CONFIG_H__

#if defined (CONFIG_NVS)

#include <sh/sh_utils.h>

#define HAPTIC_DISABLE              0x00
#define HAPTIC_SCRATCH_ENABLE       0x01
#define HAPTIC_SWALLOW_ENABLE       0x02
#define HAPTIC_SWALLOW_RAW_ENABLE   0x03
#define HAPTIC_SCRATCH_RAW_ENABLE   0x04
#define HAPTIC_IBD_ENABLE           0x05
#define HAPTIC_COUGH_ENABLE         0x06

#define HAPTIC_DEFAULT_VALUE        0x00

typedef enum{	
	HAPTIC_ENABLED,
	HAPTIC_INTENSITY,
	HAPTIC_PULSE_WIDTH,
	HAPTIC_INTERVAL,
	HAPTIC_DURATION
}haptic_config_type_t;

/**
 * @brief Structure defines the vibration pattern of the hapic motor
 */
typedef struct{
    /** Intensity of the motor vibration */
	uint8_t 		intensity;

    /** Pulse value of the motor vibration */
	uint16_t		pulse_width;

    /** Interval of the motor vibration */
	uint16_t		interval;

    /** Duration of the motor vibration */
	uint16_t		duration;
}pattern_type_t;

/**
 * @brief Structure defines the hapic motor configuration
 */
typedef struct{
    /** Indicate the enable feature of haptic motor */
	uint8_t 		enabled;	// 0x01 - Scratch mode, 0x02 - Session mode, 0x04 - Swallow mode, 0xF8 - future use
	
    /** Hold the configuration of vibration pattern of the motor */
    pattern_type_t	pattern;
}haptic_config_t;


#ifdef __cplusplus
namespace sh_haptic_config{
    
    void load_default_haptic_conf(haptic_config_t *haptic_config);

    void set_haptic_conf(haptic_config_t *haptic_config);

    void get_haptic_conf(haptic_config_t *haptic_config);

    void print_config_data(void);

    void set_profile(uint8_t profile);

    void set_enabled(uint8_t enabled);
    uint8_t get_enabled(void);
    
    int set_intensity(uint8_t intensity);
    uint8_t get_intensity(void);

    int set_pulse_width(uint16_t pulse_width);
    uint16_t get_pulse_width(void);

    int set_interval(uint16_t interval);
    uint16_t get_interval(void);

    int set_duration(uint16_t duration);
    uint16_t get_duration(void);

}
#endif /* __cplusplus */

#endif // CONFIG_NVS

#endif /* __SIBEL_HAPTIC_CONFIG_H__ */
