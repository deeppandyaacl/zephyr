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

#ifndef __SH_WATCHDOG_H__
#define __SH_WATCHDOG_H__

#ifdef __cplusplus
extern "C" {
#endif

#define WATCHDOG_LOW_TIMEOUT_MSEC       2000 // Timeout to trigger watchdog for low timeout threshold for critical thread monitoring
#define WATCHDOG_DEFAULT_TIMEOUT_MSEC   4000 // Generic timeout to trigger watchdog suitable for all threads
#define WATCHDOG_TEMP_TIMEOUT_MSEC      5000 // Specific for Temperature sensor thread as it will go to sleep for 4 seconds after each measurement

#define WATCHDOG_CHANNEL_DEFAULT_ID (-1)

/**
 * @brief Start watchdog 
 *
 * @return int Success indicates 0 and failure is non-zero.
 */
int task_watchdog_init(void);

/**
 * @brief add thread in watchdog 
 * @param timeout_pr : watchdog timeout to configure
 * @return int channel id.
 */
int task_wdt_add_thread(uint32_t timeout_pr);

/**
 * @brief delete thread in watchdog
 * @param channel_id : channel id to received in task_wdt_add_thread
 * @return int Success indicates 0 and failure is non-zero.
 */
int task_wdt_delete_thread(int channel_id);

/**
 * @brief feed to watchdog 
 * @param channel_id : channel id to received in task_wdt_add_thread 
 * @return int Success indicates 0 and failure is non-zero.
 */
int task_wdt_fed_from_thread(int channel_id);

#ifdef __cplusplus
}
#endif

#endif // __SH_WATCHDOG_H__
