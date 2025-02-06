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

#ifndef __SIBEL_IMU_CONFIG_H__
#define __SIBEL_IMU_CONFIG_H__

#if defined (CONFIG_NVS)

#include <sh/sh_utils.h>

#define IMU_DISABLE         0x00
#define IMU_ACC_ENABLE      0x01
#define IMU_GYRO_ENABLE     0x02
#define IMU_ACC_GYRO_ENABLE 0x03

#define IMU_SWALLOW_ENABLE      0x00
#define IMU_SCRATCH_ENABLE      0x01
#define IMU_SWALLOW_RAW_ENABLE  0x02
#define IMU_SCRATCH_RAW_ENABLE  0x03
#define IMU_IBD_ENABLE          0x04
#define IMU_COUGH_ENABLE        0x05

typedef enum{	
	IMU_ENABLED,
	IMU_ACC_ODR,
	IMU_ACC_MODE,
	IMU_ACC_RANGE,
	IMU_GYRO_ODR,
	IMU_GYRO_MODE,
	IMU_GYRO_RANGE,
	
}imu_config_type_t;

/**
 * @brief Structure defines the IMU sensor configurations
 */
typedef struct{
    /** Indicate the enable feature of IMU sensor */
	uint8_t 		enabled;	// 0x01 -> acc , 0x02 -> gyro, 0xFC - future use

    /** Hold the ODR data of Accelerometer */
	uint8_t 		acc_odr;

    /** Hold the mode of operation of Accelerometer */
	uint8_t 		acc_mode;

    /** Hold the range of Accelerometer */
    
	uint8_t 		acc_range;

    /** Hold the ODR data of Gyro */
	uint8_t 		gyro_odr;

    /** Hold the mode of operation of Gyro */
	uint8_t 		gyro_mode;

    /** Hold the range of Gyro */
	uint8_t 		gyro_range;
}imu_config_t;

#ifdef __cplusplus
namespace sh_imu_config{

    void load_default_imu_conf(imu_config_t *imu_config);

    void set_imu_conf(imu_config_t *imu_config);

    void get_imu_conf(imu_config_t *imu_config);

    void print_config_data(void);

    void set_profile(uint8_t profile);

    void set_enabled(uint8_t enabled);
    uint8_t get_enabled(void);

    int set_acc_odr(uint8_t acc_odr);
    uint8_t get_acc_odr(void);

    int set_acc_mode(uint8_t acc_mode);
    uint8_t get_acc_mode(void);

    int set_acc_range(uint8_t acc_range);
    uint8_t get_acc_range(void);

    int set_gyro_odr(uint8_t gyro_odr);
    uint8_t get_gyro_odr(void);

    int set_gyro_mode(uint8_t gyro_mode);
    uint8_t get_gyro_mode(void);

    int set_gyro_range(uint8_t gyro_range);
    uint8_t get_gyro_range(void);

}
#endif /* __cplusplus */

#endif // CONFIG_NVS

#endif /* __SIBEL_IMU_CONFIG_H__ */
