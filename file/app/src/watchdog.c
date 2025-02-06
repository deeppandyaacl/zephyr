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

#ifdef CONFIG_WATCHDOG
#include <zephyr/kernel.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/task_wdt/task_wdt.h>
#include "watchdog.h"


/**
 * @brief Task callback handler calls whenever any thread fails to feed 
 * watchdog. The callback will indicate the watchdog channel ID to 
 * identify the thread that fails to feed.
 *  
 * @param channel_id: Watchdog channel ID specific to thread/loop
 * @param user_data: Pointer to the thread user data 
 * 
 * @return none
 */
static void task_wdt_callback(int channel_id, void *user_data)
{
	/*
	 * If the issue could be resolved, call task_wdt_feed(channel_id) here
	 * to continue operation.
	 *
	 * Otherwise we can perform some cleanup and reset the device.
	 */
	printk("\n********************************************\n");
	printk("********************************************\n");
	printk("Task watchdog triggered with channel %d\n", channel_id);
	
	task_wdt_feed(channel_id);

	printk("Feeding the channel id : %d...\n", channel_id);
	printk("********************************************\n");
	printk("********************************************\n");
	// sys_reboot(SYS_REBOOT_COLD);
}

/**
 * @brief Initialize watchdog interface and task watchdog structure
 *   
 * @return integer Success indicates 0 and failure is non-zero.
 */
int task_watchdog_init(void)
{
	int ret;
	const struct device *const hw_wdt_dev = DEVICE_DT_GET(DT_NODELABEL(wdt));

	printk("Task watchdog Started.\n");

	if (!device_is_ready(hw_wdt_dev)) {
		printk("Hardware watchdog not ready; ignoring it.\n");
	}

	ret = task_wdt_init(hw_wdt_dev);
	if (ret != 0) {
		printk("task wdt init failure: %d\n", ret);
	}

	return 0;
}

/**
 * @brief Add watchdog timeout and create a new watchdog channel specific
 * to a thread
 *  
 * @param timeout_pr: Watchdog timeout between feeds 
 * 
 * @return integer watchdog channel ID
 */
int task_wdt_add_thread(uint32_t timeout_pr)
{
	/* passing NULL instead of callback to trigger system reset */
	int task_wdt_id = task_wdt_add(timeout_pr, task_wdt_callback, NULL);

	return task_wdt_id;
}

/**
 * @brief Remove the created watchdog channel to ignore the watchdog 
 * feeds from that specific thread
 *  
 * @param channel_id: Watchdog channel ID of a thread
 * 
 * @return integer Success indicates 0 and failure is non-zero.
 */
int task_wdt_delete_thread(int channel_id)
{
	/* passing NULL instead of callback to trigger system reset */
	int ret;

	ret = task_wdt_delete(channel_id);
	if(ret < 0) {
		printk("Failed to disable the watchdog for channel : %d", ret);
	}

	return ret;
}

/**
 * @brief Watchdog feed function of specific channel ID
 *  
 * @param channel_id: Watchdog channel ID of a thread
 * 
 * @return integer Success indicates 0 and failure is non-zero.
 */
int task_wdt_fed_from_thread(int channel_id)
{
	task_wdt_feed(channel_id);
	
	return 0;
}

#endif // end of CONFIG_WATCHDOG

