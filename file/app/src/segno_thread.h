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

#ifndef __SEGNO__THREAD_H__
#define __SEGNO__THREAD_H__

#if defined(CONFIG_SEGNO_LIBRARY)

#include <zephyr/kernel.h> 
#include <sh/sh_shrd_data.h>

#ifdef __cplusplus

extern struct k_sem segno_mngr_sem;

#if ((CONFIG_SEGNO_SCRATCH_PROGRAM) || (CONFIG_SEGNO_WEAR_DETECT_PROGRAM) || \
    (CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM) || (CONFIG_SEGNO_ANGLE_DETECT_PROGRAM))
ZBUS_CHAN_DECLARE(segno_chan);
#endif


#define SEGNO_EVENT_TIMER_IN_MS  ( 500 )    // 0.5 seconds
#define SEGNO_WEAR_DETECT_CNT    ( 120 )    // 1 minute
#define SEGNO_ACTIVITY_INDEX_CNT ( 120 )    // 1 minute
#define SEGNO_ANGLE_DETECT_CNT   ( 10 )     // 5 seconds

namespace sh_segno_thread{

    typedef enum{
        SEGNO_SCRATCH_DETECT,
        SEGNO_ANGLE_DETECT,
        SEGNO_ACTIVITY_INDEX_DETECT,
        SEGNO_WEAR_DETECT,
        SEGNO_EVT_MAX
    }segno_eve_type;

    typedef struct __attribute__((__packed__)) {
        uint8_t type;
        uint32_t timestamp_storage;
        uint16_t length;
        union
        {
        #ifdef CONFIG_SEGNO_SCRATCH_PROGRAM
            float scratch_data;
        #endif

        #ifdef CONFIG_SEGNO_ANGLE_DETECT_PROGRAM 
            float angle_data;
        #endif

        #ifdef CONFIG_SEGNO_ACTIVITY_DETECT_PROGRAM
            float activity_index;
        #endif

        #ifdef CONFIG_SEGNO_WEAR_DETECT_PROGRAM
            struct __attribute__((__packed__)){
                float mean_accl[3];
                float max_accl[3];
                float min_accl[3];
            }wear_detection;
        #endif
        }segno_data;
    }msg_t;

    /**
     * @brief Initializes the Application thread 
     * and adds zbus observers for application channel.
     *
     * @return uint32_t Success indicates 0 and failure is non-zero.
     */
    uint32_t init(void);

    /**
     * @brief Checks whether the sesnor is attached to body or not
     * 
     * @return uint8_t 0 = Lead Off, 1 = Lead On
     */
    uint8_t check_lead_on_status(void);

    /**
     * @brief Set body detection flag to have low sampling rate when
     *        lead Off
     * 
     */
    void start_body_detect(void);

    /**
     * @brief Reset body detection flag to set default sampling rate when
     *        lead On
     * 
     */
    void stop_body_detect(void);

    /**
     * @brief Start the scratch detection, wear detection and angle detection
     * 
     */
    void start_sensing(void);

     /**
     * @brief Stop the scratch detection, wear detection and angle detection
     * 
     */
    void stop_sensing(void);

    /**
     * @brief Return the value of body detection flag
     * 
     * @return uint8_t 0 = body detection Off, 1 = body detection On
     */
    uint8_t check_body_detect_status(void);

    /**
     * @brief Configure IMU and segno with 25Hz sampling rate
     * 
     * @return int 0 = Success, Negative value = Error
     */
    int config_imu_segno_w_low_odr(void);

    /**
     * @brief Configure IMU and segno with default sampling rate
     * 
     * @return int 0 = Success, Negative value = Error
     */
    int config_imu_segno_w_default_odr(void);

    int store_events_to_shrd(sh_shrd_evt_type *event);

}
#endif /* __cplusplus */

#endif /* CONFIG_SEGNO_LIBRARY */

#endif /* __SEGNO__THREAD_H__ */