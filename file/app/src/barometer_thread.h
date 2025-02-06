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

#ifndef __SIBEL_BAROMETER_THREAD_H__
#define __SIBEL_BAROMETER_THREAD_H__

#if defined(CONFIG_SH_BMP5)

#include <zephyr/kernel.h> 

#define SENSOR_BAROMETER                1

/*Barometer (BMP581)*/
#define BAROMETER_SAMPLE_RATE           CONFIG_BMP5_ORD                 // Setting data rate to 80 samples per seconds
#define BAROMETER_THRESHOLD_LEVEL       CONFIG_BMP5_FIFO_THRESHOLD      // 20 samples at a time in FIFO
#define BAROMTER_FIFO_DATA_BUFFER_SIZE  BAROMETER_THRESHOLD_LEVEL       // FIFO buffer that can hold data upto threshold
#define BAROMTER_FIFO_FRAME_TYPE        CONFIG_BMP5_FIFO_FRAME_SELECT   // Enable FIFO only for Pressure data
#define BAROMTER_MAX_FAIL_CNT           3                               // Maximum failure counts while reading barometer data
ZBUS_CHAN_DECLARE(bmp5_chan);
extern struct k_pipe barometer_pipe;

#ifdef __cplusplus
namespace sh_barometer_thread{

     /**
     * @brief Structure contains the barometer data elements
     * 
     * type: Specifies the type of sensor
     * length: Specifies the length of the data buffer
     * timestamp: Specifies the timestamp for the associated data
     * data: Buffer holding the pressure data of length equal to BAROMETER_THRESHOLD_LEVEL
     */
    typedef struct{
        uint8_t type;
        uint16_t length;
        uint32_t timestamp_storage;
        uint32_t timestamp_streaming;
        float data[BAROMETER_THRESHOLD_LEVEL];
    }msg_t;

    /**
     * @brief Initializes the temperature streaming thread 
     * and adds zbus observers for temperature channel.
     *
     * @return int32_t Success indicates 0 and failure is non-zero.
     */
    int32_t init(void);

    /**
     * @brief Start sensing of Barometer 
     *
     * @return int Success indicates 0 and failure is non-zero.
     */
    int start_sensing(void);

    /**
     * @brief Stop sensing of Barometer
     *
     * @return int Success indicates 0 and failure is non-zero.
     */
    int stop_sensing(void);
}
#endif /* __cplusplus */

#endif //CONFIG_SH_BMP5

#endif /* __SIBEL_BAROMETER_THREAD_H__ */