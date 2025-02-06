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

#ifndef __SIBEL_MICROPHONE_THREAD_H__
#define __SIBEL_MICROPHONE_THREAD_H__

#if defined(CONFIG_MICROPHONE)

#include <zephyr/kernel.h> 
#include <sh/sh_microphone.h>

#define SENSOR_MIC      10
#if CONFIG_MIC_STREAMING
#define MAX_MIC_DATA_LEN 160
#else
#define MAX_MIC_DATA_LEN    800
#endif
#define MAX_MIC_FAIL_CNT 3
ZBUS_CHAN_DECLARE(microphone_chan);
extern struct k_pipe microphone_pipe;

#define CMD_MIC_CONFIGURE_LEN       ( 2 )

#ifdef __cplusplus

namespace sh_microphone{

     /**
     * @brief Structure contains the mcirophone data elements
     * 
     * type: Specifies the type of sensor
     * length: Specifies the length of the data buffer
     * timestamp: Specifies the timestamp for the associated data
     * data: Buffer holding the temp data of length equal to MAX_MIC_DATA_LEN
     */
    typedef struct{
        uint8_t type;
        uint16_t length;
        uint32_t timestamp_storage;
        uint32_t timestamp_streaming;
        uint8_t data[MAX_MIC_DATA_LEN];
    }msg_t;

    /**
     * @brief Initializes the mcirophone streaming thread 
     * and adds zbus observers for mcirophone channel.
     *
     * @return uint32_t Success indicates 0 and failure is non-zero.
     */
    uint32_t thread_init(void);

    /**
     * @brief Start sensing of Microphone 
     *
     * @return int Success indicates 0 and failure is non-zero.
     */
    int start_sensing(void);

    /**
     * @brief Stop sensing of Microphone 
     *
     * @return int Success indicates 0 and failure is non-zero.
     */
    int stop_sensing(void);

    /**
     * @brief Get current confi of Microphone 
     * @param config : pointer to read the value of config in microphone_config_t
     * @return int Success indicates 0 and failure is non-zero.
     */
    int get_current_config(microphone_config_t *config);
    /**
     * @brief Set current config of Microphone 
     * @param config : pointer to write the value of config in microphone_config_t
     * @return int Success indicates 0 and failure is non-zero.
     */
    int set_current_config(microphone_config_t *config);

}
#endif /* __cplusplus */

#endif /* (CONFIG_MICROPHONE) */

#endif /* __SIBEL_MICROPHONE_THREAD_H__ */