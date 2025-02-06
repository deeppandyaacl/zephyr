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

#ifndef __EVENT_HANDLER_T__
#define __EVENT_HANDLER_T__

#include <zephyr/kernel.h>
#include <sh/drv2624.h>
#include <zephyr/zbus/zbus.h>

extern struct k_sem mem_mngr_sem;

#ifdef __cplusplus
namespace sh_haptic_motor_handler{

    typedef enum
    {
        /* Different types of pattern in haptic handler*/
        ON,
        OFF,
        SWALLOW,
        SCRATCH,
        CUSTOM,
        MAX,
    }type_t;

    /**
     * @brief Enable or disable the haptic motor
     * state : On/off state
     * params : Give custom haptic parameters
     *
     * @return int Success indicates 0 and failure is non-zero.
     */
    int operation(type_t stat, haptic_params_t *params);

    /**
     * @brief Get the haptic engine status
     * 
     * @return uint8_t 1 = Enable, 0 = Disable
     */
    uint8_t get_haptic_status(void);

    /**
     * @brief Get the haptic pattern status
     * 
     * @return uint8_t 1 = Enable, 0 = Disable
     */
    bool get_haptic_pattern_status(void);

}
#endif /* __cplusplus */

#endif /* __EVENT_HANDLER_T__ */