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

#ifndef __SIBEL_MIC_CONFIG_H__
#define __SIBEL_MIC_CONFIG_H__

#if defined (CONFIG_NVS)

#include <sh/sh_utils.h>

#define MIC_NO_CHANNEL      0x00
#define MIC_LEFT_CHANNEL    0x01
#define MIC_RIGHT_CHANNEL   0x02
#define MIC_ALL_CHANNEL     0x03

#define MIC_SWALLOW_ENABLE      0x00
#define MIC_SCRATCH_ENABLE      0x01
#define MIC_SWALLOW_RAW_ENABLE  0x02
#define MIC_SCRATCH_RAW_ENABLE  0x03
#define MIC_IBD_ENABLE          0x04
#define MIC_COUGH_ENABLE        0x05

typedef enum{	
	MIC_ENABLED,
	MIC_LCHANNEL_GAIN,
	MIC_RCHANNEL_GAIN
}mic_config_type_t;

/**
 * @brief Structure defines the Microphone configurations
 */
typedef struct{
    /** Indicate the enable feature of microphone */
	uint8_t 		enabled;	// 0x01 -> left_mic , 0x02 -> right_mic, 0x03 -> left_right_mic, 0xFC - future use

    /** Hold the gain of left channel of the microphone */
    uint8_t			mic_lchannel_gain;

    /** Hold the gain of right channel of the microphone */
    uint8_t			mic_rchannel_gain;

    /** Hold the mode of the microphone */
    // uint8_t			mic_mode;   // 0x01 -> mono mode, 0x02 -> stereo mode 
}mic_data_config_t;


/**
 * To handle the multiple configuration of individual data configuration, consider the 
 * below example of mic_data_config_t structure as a union.
 * 
 * // Define tagged struct for versioned config
 * typedef struct {
 *     uint8_t version;  // Version of the structure
 *     union {
 *         struct {
 *             uint8_t enabled;
 *             uint8_t mic_lchannel_gain;
 *             uint8_t mic_rchannel_gain;
 *         } v1;
 * 
 *         struct {
 *             uint8_t mic_mode;
 *         } v2;
 *     } data;
 * } mic_data_config_t;
 * 
 * With this example the mic_data_config_t will be the same across all the operation with 
 * the data storage and v1 amd v2 will be handled individually.
 * 
 * The storage of v1 and v2 structure inside the union should be handled separatly and the 
 * get/set function of all the members should be handled such that the user of these APIs
 * does not know which version is currently accessed.
 * 
 */

#ifdef __cplusplus
namespace sh_mic_config{

    void load_default_mic_conf(mic_data_config_t *mic_config);

    void set_mic_conf(mic_data_config_t *mic_config);

    void get_mic_conf(mic_data_config_t *mic_config);

    void print_config_data(void);

    void set_profile(uint8_t profile);

    void set_enabled(uint8_t enabled);
    uint8_t get_enabled(void);

    int set_lchannel_gain(uint8_t lchannel_gain);
    uint8_t get_lchannel_gain(void);
    
    int set_rchannel_gain(uint8_t rchannel_gain);
    uint8_t get_rchannel_gain(void);
}
#endif /* __cplusplus */

#endif // CONFIG_NVS

#endif /* __SIBEL_MIC_CONFIG_H__ */
