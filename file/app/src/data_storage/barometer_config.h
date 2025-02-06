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

#ifndef __SIBEL_BAROMETER_CONFIG_H__
#define __SIBEL_BAROMETER_CONFIG_H__

#if defined (CONFIG_NVS) && defined (CONFIG_SH_BMP5)

#include <sh/sh_utils.h>

#define BAROMETER_DISABLE       0x00
#define BAROMETER_ENABLE        0x01

#define BARO_SWALLOW_ENABLE     0x00
#define BARO_SCRATCH_ENABLE     0x01
#define BARO_SWALLOW_RAW_ENABLE 0x02
#define BARO_SCRATCH_RAW_ENABLE 0x03
#define BARO_IBD_ENABLE         0x04
#define BARO_COUGH_ENABLE       0x05

typedef enum{	
	BARO_ENABLED,
	BARO_ODR,
	BARO_FIFO_THRESHOLD,
	BARO_FIFO_FRAME_SEL
}baro_config_type_t;

/**
 * @brief Structure defines the Barometer sensor configurations
 */
typedef struct{
    /** Indicate the enable feature of Barometer sensor */
	uint8_t 		enabled;	// 0x01 -> enabled , 0xFE - future use

    /** Hold the ODR data of barometer */
	uint8_t 		bmp5_odr;

    /** Hold the FIFO threshold value of barometer */
    uint8_t 		bmp5_fifo_threshold;

    /** Hold the FIFO frame selection value of barometer */
    uint8_t 		bmp5_fifo_frame_sel;
}baro_config_t;

#ifdef __cplusplus
namespace sh_barometer_config{

    void load_default_barometer_conf(baro_config_t *baro_config);

    void set_barometer_conf(baro_config_t *baro_config);

    void get_barometer_conf(baro_config_t *baro_config);

    void print_config_data(void);

    void set_profile(uint8_t profile);

    void set_enabled(uint8_t enabled);
    uint8_t get_enabled(void);

    int set_odr(uint8_t odr);
    uint8_t get_odr(void);

    int set_fifo_threshold(uint8_t fifo_threshold);
    uint8_t get_fifo_threshold(void);

    int set_frame_sel(uint8_t frame_sel);
    uint8_t get_frame_sel(void);
}
#endif /* __cplusplus */

#endif // CONFIG_NVS

#endif /* __SIBEL_BAROMETER_CONFIG_H__ */
