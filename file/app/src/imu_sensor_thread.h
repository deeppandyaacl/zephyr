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

#ifndef __SIBEL_IMU_SENSOR_THREAD_H__
#define __SIBEL_IMU_SENSOR_THREAD_H__

#include <zephyr/kernel.h> 

#define SENSOR_IMU                  ( 2 )
#define IMU_DATA_LEN                ( 60 ) 
#define IMU_MAX_FAIL_CNT            ( 3 )       // Maximum failure counts while reading imu data
#define IMU_STEP_EVENT_TIMER_IN_MS  ( 1000 )    // 1 seconds
#define IMU_STEP_DATA_LEN           ( 2 )

ZBUS_CHAN_DECLARE(imu_chan);
ZBUS_CHAN_DECLARE(imu_segno_chan);
ZBUS_CHAN_DECLARE(imu_step_chan);
extern struct k_pipe imu_pipe;
extern struct k_pipe imu_segno_pipe;

#ifdef __cplusplus
namespace sh_imu_sensor{

    /**
     * @brief Structure contains the IMU data elements
     * 
     * type: Specifies the sensor type
     * length: Specifies the data buffer length
     * timestamp: Specifies respective data timestamp
     * data: Buffer holding the data
     */
    typedef struct{
        uint8_t type;
        uint16_t length;
        uint32_t timestamp_storage;
        uint32_t timestamp_streaming;
        uint8_t data[IMU_DATA_LEN];
    }msg_t;

    #if defined (CONFIG_BMA456)
        typedef struct{
        int64_t timestamp_segno;
        int16_t segno_data[CONFIG_BMA456_WATERMARK];
    }segno_msg_t;
    #endif
    
    #if defined (CONFIG_SH_BMI323)
        typedef struct{
        int64_t timestamp_segno;
        int16_t segno_data[CONFIG_SH_BMI323_WATERMARK];
    }segno_msg_t;
    #endif

    #if defined (CONFIG_SH_BMI323_STEP_COUNT_ENABLE)
        typedef struct {
        uint8_t type;
        uint16_t length;
        uint32_t timestamp_storage;
        uint32_t timestamp_streaming;
        uint16_t step_count;
    }step_count_msg_t;
    #endif

    typedef enum{
        IMU_NON_RECOVERABLE_ERROR = -1,
        IMU_RECOVERABLE_ERROR = -2,
        IMU_ERROR_MAX
    }error_t;

    /**@brief Function for thread initalization for sensor.
     *
     * @retval 0 If success. Otherwise, an error code is returned.
     */
    uint32_t init(void);

       /**
     * @brief Start sensing of IMU 
     * @param uint16_t accel_odr set sampling rate of accel. If zero no change will happen. Only supported accel ODR is 400Hz and 1600Hz
     * @return int Success indicates 0 and failure is non-zero.
     */ 
    int start_sensing(uint16_t accl_odr = 0);

    /**
     * @brief Stop sensing of IMU 
     *
     * @return int Success indicates 0 and failure is non-zero.
     */
    int stop_sensing(void);
}
#endif /* __cplusplus */

#endif /* __SIBEL_IMU_SENSOR_THREAD_H__ */