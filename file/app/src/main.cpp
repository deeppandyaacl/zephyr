
/*

Testing purpose
Added again
*/


// Zephyr dependent files
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include <zephyr/drivers/usb/usb_dc.h>
#include <zephyr/device.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/dfu/mcuboot.h>

// Sibel Health modules
#include <shell_ble/shell_ble.h>
#include <sh/led.h>
#include <sh/factory_data_block.h>
#include "factory_data.h"
#include <sh/sh_microphone.h>
#include "memfault/metrics/battery.h"
// #include "memfault/components.h"
#include <sh/as6221.h>
#include <sh/sh_ptx30.h>
#include <sh/bma456.h>
#include <sh/sh_littlefs.h>
#include <sh/sh_time.h>
#include <sh/sh_nvs_handler.h>

// Threads
#include "imu_sensor_thread.h"
#include "temp_sensor_thread.h"
#include "ptx30_thread.h"
#include "barometer_thread.h"
#include "microphone_thread.h"
#include "session_thread.h"
#include "network_thread.h"
#include "microphone_thread.h"
#include "mem_mgr_thread.h"
#include "fault_handler.h"
#include "segno_thread.h"
#include "data_storage.h"
#include "haptic_motor_handler.h"

// BLE Modules
#include <ble_base/ble.h>

// BLE Modules

#if CONFIG_MEMFAULT
// Memfault
#include "memfault/core/trace_event.h"
#endif

#ifdef CONFIG_WATCHDOG
#include "watchdog.h"
#endif

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

// Memory manager
#include "mem_mgr_thread.h"
#include <sh/sh_data_logger.h>
#include <sh/sh_shrd_data.h>

#include <sh/sh_nvs_handler.h>

#include "ptxPlat.h"

/* size of stack area used by each thread */
#define STACKSIZE 512
/* scheduling priority used by each thread */
#define PRIORITY 7

/* Getting battery voltage to shut down the system in case of low bat */
#define PTX_BAT_VAL_SAMPLE_MAX_CNT      5
#define PTX_BAT_VAL_SAMPLE_PASS_CNT     3

// /* Check the availability of console node */
// BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
//              "Console device is not ACM CDC UART device");

// // /* Check the availability of shell node */
// BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_shell_uart), zephyr_cdc_acm_uart),
//              "Shell-uart device is not ACM CDC UART device");

// /* Check the availability of mcumgr node */
// BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_uart_mcumgr), zephyr_cdc_acm_uart),
//              "Console device is not ACM CDC UART device");

// const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(zephyr_console));

#if defined(CONFIG_FACTORY_DATA_BLOCK)
void copy_cbor_data(const struct fdata *data, struct copied_fdata *cdata)
{
    sprintf(cdata->json_ver, "%.*s", (int)data->values[JSON_VER_INDEX].len, data->values[JSON_VER_INDEX].value);
    sprintf(cdata->serial_number, "%.*s", (int)data->values[SER_INDEX].len, data->values[SER_INDEX].value);
    sprintf(cdata->vendor_name, "%.*s", (int)data->values[VENDOR_INDEX].len, data->values[VENDOR_INDEX].value);
    sprintf(cdata->date, "%.*s", (int)data->values[DATE_INDEX].len, data->values[DATE_INDEX].value);
	sprintf(cdata->hardware_version, "%.*s", (int)data->values[HW_VER_INDEX].len, data->values[HW_VER_INDEX].value);	
}

int display_factory_data(struct copied_fdata *copied_data) {
    if (copied_data != NULL) {
            printk("********************************************\n");
            printk("Json Version= %s.\n", copied_data->json_ver);
            printk("Hw_Version= %s.\n", copied_data->hardware_version);
            printk("Serial number= %s.\n", copied_data->serial_number);
            printk("Vendor name= %s.\n", copied_data->vendor_name);
            printk("Manuf date= %s.\n", copied_data->date);
            printk("********************************************\n");
    } else {
        LOG_ERR("Argument with a NULL pointer has been received.\n");
        return -2;
    }

    return 0;
}
#endif


int main(void)
{
    int ret;
   
    LOG_INF("The current version of the application is : %s\n", CONFIG_BT_DIS_FW_REV_STR);

#ifdef CONFIG_BOOTLOADER_MCUBOOT
    if (!boot_is_img_confirmed()) {
        // Mark the ota image as installed so we don't revert
        LOG_INF("***Confirming OTA update\n****");

        boot_write_img_confirmed();

        //TODO: As of now we are deleting the configuration in OTA to set the default profile
        //In future add logic to delete the profile and set the profile based on version number update

        LOG_INF("*Now delete the confighration\n****");
        ret = sh_nvs_handler::init();
        if(ret < 0) {
            LOG_ERR("NVS init failed Error: %d", ret);
        }
        k_msleep(100);

        sh_data_storage::delete_config_from_version(CONFIG_CURRENT_VERSION);

        k_msleep(100);

        NVIC_SystemReset();
    }
    else
    {
        LOG_INF("Boot image is already confirmed....");
    }
#endif
#if defined(CONFIG_FACTORY_DATA_BLOCK)
    const struct device *flash_dev = FLASH_PARTITION_DEVICE ;
    struct copied_fdata copyData;
   
    ret = factory_data::init_factory_block(flash_dev);
    if(!ret) {
        ret = fetch_factory_data(flash_dev, copy_cbor_data, &copyData);
        if(!ret) {
            ret = display_factory_data(&copyData);
            if(ret) {
                 LOG_ERR("Failed to display the factory data.");
            }
        } else {
            LOG_ERR("Failed to fetch the factory data.");
        }
    } else {
            LOG_ERR("Failed to initialize factory block(err %d)\n", ret);
    }
#endif

#ifdef CONFIG_WATCHDOG
    ret = task_watchdog_init();
    if(ret < 0) {
        LOG_ERR("Watchdog init failed : %d", ret);
    }
#endif
    ret = sh_data_storage::init(); 
    if(ret < 0) {
        LOG_ERR("Data storage module init failed Error: %d", ret);
    }
#if defined(CONFIG_PTX30)
    if (powermngr::thread_init()) {
        LOG_ERR("Failed to start the PTX30 thread\n");
    }

    uint16_t bat_val = 0;
    uint8_t bat_val_sample = 0;

    //Check for the low battery having 3.5V
    for(int i=0; i<PTX_BAT_VAL_SAMPLE_MAX_CNT; i++) {
        bat_val = powermngr::get_battery_voltage();
        
        // Read the battery voltage multiple times to avoid failure results 
        if(bat_val <= DEAD_BATTERY_THRESHOLD)
            bat_val_sample++;           
    }

    if( bat_val_sample >= PTX_BAT_VAL_SAMPLE_PASS_CNT) {
        LOG_ERR("Sensor battery is critically low cannot start system");
        LOG_ERR("Put sensor on the charger");

        while(1) 
        {   
            // Monitor the battery voltage until it gets 3.7v and then continue system initialization
            bat_val = powermngr::get_battery_voltage();
            if(bat_val >= DEAD_BATTERY_RECOVERY)
                break;

            k_msleep(5000);
        }
    }
        
#endif
#ifdef CONFIG_MEMFAULT_METRICS_BATTERY_ENABLE
    memfault_metrics_battery_boot();
#endif
    ret = sh_network_thread::init();
    if(ret < 0) {
        LOG_ERR("Network thread init failed (err %d)\n", ret);
    }

#if defined(CONFIG_SEGNO_LIBRARY)
    ret = sh_segno_thread::init();
    if (ret < 0) {
        LOG_ERR("Segno Initialization failed (err %d)\n", ret);
    }
#endif 

    ret = fault_handler::initialize();
    if(ret < 0) {
        LOG_ERR("Error initalizing fault module (err %d)\n", ret);
    }
#if defined(CONFIG_MEMFAULT)   // keep this type of trace in errors to trace the issue
    MEMFAULT_TRACE_EVENT(critical_log);
    // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log, "Aria Memfault Critical Logs");
    // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log, "");
#endif    

#if defined(CONFIG_TEST_LITTLEFS)
	ret = sh_littlefs::init();
	if (ret < 0) {
		LOG_ERR("Littlefs file system init failed (err %d)\n", ret);
	}
    else{	
        ret = sh_littlefs::flash_functional_test((char *)Flash_Testbuffer, 
                                    strlen((char *)Flash_Testbuffer), 0, 2);
        if (ret < 0) {
            LOG_ERR("Flash read write failed (err %d)\n", ret);
        }        
    }  
#endif    

#if defined(CONFIG_TMP119SH) || defined(CONFIG_AS6221)
    ret = sh_temp_sensor::init();
    if(ret < 0){
        LOG_ERR("Failed to Initialize AS6221 Temp Sensor %d.", ret);
    } else {
        LOG_INF("Temperature sensor is initialize successfully");
    }
#endif /* CONFIG_TMP119SH or CONFIG_AS6221*/

#if defined(CONFIG_SH_BMP5)
    /* Init Barometer driver instance and start FIFO interrupt handler */
    ret = sh_barometer_thread::init();
    if(ret < 0){
        LOG_ERR("Failed to Initialize BMP5 Barometer sensor. %d", ret);
    }
#endif /* CONFIG_SH_BMP5 */

#if defined(CONFIG_SH_BMI323)
    ret = sh_imu_sensor::init();
    if(ret < 0){
        LOG_ERR("**Failed to Initialize BMI323 IMU sensor. %d**", ret);
    }
#endif /* CONFIG_BMA323 */

#if defined(CONFIG_BMA456)
    ret = sh_imu_sensor::init();
    if(ret < 0){
        LOG_ERR("**Failed to Initialize BMA456 IMU sensor. %d**", ret);
    }
#endif /* CONFIG_BMA456 */

#if defined(CONFIG_DRV2624)
    LOG_INF("Haptic Motor Testing Started");
    ret = drv2624::init();
    if(ret < 0){
        LOG_ERR("Failed to initialize DRV2624 haptic driver %d", ret);
        fault_handler::send_diagnostics(HAPTIC_FAULT);
    }
    else{
        LOG_INF("Haptic Motor Testing Completed Successfully");
    }
#endif /* CONFIG_DRB2624 */

#if defined(CONFIG_MICROPHONE)
    ret = sh_microphone::thread_init();
    if (ret < 0) {
        LOG_ERR("Mic Initialization failed (err %d)\n", ret);
    }
#endif

#if defined(CONFIG_SH_TIME_MODULE)
    // Start keeping track of time
    app_time::init();
#endif

    LOG_INF("Aria System Initialized");

    ret = sh_mem_mngr::thread_init();
    if(ret < 0)
    {
        LOG_ERR("Memory maager therad init failed (err %d)\n", ret);
    }

    ret = sh_session_thread::init();
    if (ret < 0) {
        LOG_ERR("Session thread start failed (err %d)\n", ret);
    }
    // ptxPlat_turn_on_off_irq(true);
    ret = ptx30w::handle_pmic_interrupt(true);
    if(ret < 0) {
        LOG_ERR("Failed to enable PTX interrupt : %d", ret);
    } else{
        LOG_INF("PTX interrupt enabled");
    }
    return 0;
}


// /* Thread to get the UART channel */
// void get_uart_channel(void)
// {
//     const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
// 	uint32_t dtr = 0;

// 	if (usb_enable(NULL)) {
// 		printk("\nUnable to Initialize USB CDC interface...\n");
//         return;
// 	}
    
//     while (true)
//     {
// #ifdef CONFIG_WATCHDOG
// 	task_wdt_fed_from_thread(wdt_main_channel_id);
// #endif 
//         uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
//         if (dtr)
//         {
//             break;
//         }
//         else
//         {
//             /* Give CPU resources to low priority threads. */
//             k_msleep(100);
//         }
//     }
// }

// /* Thread to get uart channel for mcumgr */
// void get_uart_channel_for_mcumgr()
// {
//     uint32_t dtr = 0;
//     const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_uart_mcumgr));

//     if (!device_is_ready(dev))
//     {
//         return;
//     }

//     while (true)
//     {
// #ifdef CONFIG_WATCHDOG
// 	task_wdt_fed_from_thread(wdt_main_channel_id);
// #endif 
//         uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
//         if (dtr)
//         {
//             break;
//         }
//         else
//         {
//             /* Give CPU resources to low priority threads. */
//             k_msleep(100);
//         }
//     }
// }


