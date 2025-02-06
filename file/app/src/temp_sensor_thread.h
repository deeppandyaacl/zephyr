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

#ifndef __SIBEL_TEMP_SENSOR_THREAD_H__
#define __SIBEL_TEMP_SENSOR_THREAD_H__

#include <zephyr/kernel.h> 

#define SENSOR_TEMP     1
#define TEMP_DATA_LEN   2
#define MAX_FAIL_CNT_TEMP_SENSOR 3
ZBUS_CHAN_DECLARE(temp_chan);

#ifdef __cplusplus
namespace sh_temp_sensor{

     /**
     * @brief Structure contains the temperature data elements
     * 
     * type: Specifies the type of sensor
     * length: Specifies the length of the data buffer
     * timestamp: Specifies the timestamp for the associated data
     * data: Buffer holding the temp data of length equal to TEMP_DATA_LEN
     */
    typedef struct{
        uint8_t type;
        uint16_t length;
        uint32_t timestamp_storage;
        uint32_t timestamp_streaming;
        int16_t data;
    }msg_t;

    /**
     * @brief Initializes the temperature streaming thread 
     * and adds zbus observers for temperature channel.
     *
     * @return uint32_t Success indicates 0 and failure is non-zero.
     */
    uint32_t init(void);

    /**
     * @brief Start sensing of temperature 
     *
     * @return int Success indicates 0 and failure is non-zero.
     */
    int start_sensing(void);

    /**
     * @brief Stop sensing of temperature 
     * 
     * @param flag default value is 0 it is for session, 1 is for streaming
     *
     * @return int Success indicates 0 and failure is non-zero.
     */
    int stop_sensing(uint8_t flag=0);

}
#endif /* __cplusplus */

#endif /* __SIBEL_TEMP_SENSOR_THREAD_H__ */