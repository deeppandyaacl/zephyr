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

#ifndef __SIBEL_SESSION_THREAD_H__
#define __SIBEL_SESSION_THREAD_H__

#include <zephyr/kernel.h> 

#define DATA_MAX_LEN 200

ZBUS_CHAN_DECLARE(mem_mngr_chan);
extern struct k_sem dl_session_sem;

#ifdef __cplusplus
namespace sh_session_thread{

     /**
     * @brief Structure contains the memory manager session data elements
     * 
     * cmd: Command type specific for session
     * len: Data buffer lenght
     * data: Buffer holding the session data for memory manager
     */
    typedef struct{
        uint8_t cmd;
        uint16_t len;
        uint8_t data[DATA_MAX_LEN];
    }msg_t;

    /**
     * @brief Initializes the temperature streaming thread 
     * and adds zbus observers for temperature channel.
     *
     * @return uint32_t Success indicates 0 and failure is non-zero.
     */
    uint32_t init(void);

    /** 
     * @brief get current session state
     *
     * This function return the current session state
     */
    bool get_current_session_state(void);

}
#endif /* __cplusplus */

#endif /* __SIBEL_BAROMETER_THREAD_H__ */