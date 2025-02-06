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

#if defined(CONFIG_MICROPHONE)

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>

#include <zephyr/zbus/zbus.h>

#include "cmd_service/ble_cmd.h"
#include "pd_service/ble_pd.h"

#include "network_thread.h"
#include "mem_mgr_thread.h"
#include "memfault/core/trace_event.h"
#include <sh/sh_time.h>
#include "microphone_thread.h"
#include "mic_config.h"

#ifdef CONFIG_WATCHDOG
#include "watchdog.h"
#endif
#include "fault_handler.h"
#include "shrd_config.h"

#if CONFIG_MIC_STREAMING
#define LC3_RATIO_FOR_10MS_DATA 2
#define MIC_BUFFER_SIZE         1280
#else
#define LC3_RATIO_FOR_10MS_DATA 10
#define MIC_BUFFER_SIZE         6400
#endif

LOG_MODULE_REGISTER(microphone_thread, LOG_LEVEL_INF);

typedef enum {
    MIC_STATE_IDLE,     // Idle state
    MIC_STATE_START,    // Start Microphone
    MIC_STATE_STOP,     // Stop Microphone
    MIC_STATE_STREAM,   // Stream Microphone data
    MIC_STATE_FAULT     // Fault state in Microphone
}mic_states_t;


ZBUS_MSG_SUBSCRIBER_DEFINE(microphone_cmd_sub);
K_PIPE_DEFINE(microphone_pipe, sizeof(sh_microphone::msg_t)*2, 4);

ZBUS_CHAN_DEFINE(microphone_chan, sh_microphone::msg_t, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(0));

// K_MSGQ_DEFINE(microphone_queue, sizeof(sh_microphone::msg_t), 10, 4);
typedef struct {
    mic_states_t      mic_state;
    uint8_t           mic_fail_cnt;
}mic_sensor_struct_t;

mic_sensor_struct_t mic_status = {
    .mic_state = MIC_STATE_IDLE,
    .mic_fail_cnt = 0
};

microphone_config_t g_mic_cnfg;

#ifdef CONFIG_WATCHDOG
/* Watchdog Channel ID for Microphone thread */
int8_t wdt_mic_channel_id = WATCHDOG_CHANNEL_DEFAULT_ID;
#endif


static k_tid_t microphone_thread_id;
static struct k_thread microphone_thread;
K_THREAD_STACK_DEFINE(microphone_stack, 16384);

static void suspend_thread_work_handler(struct k_work * work);
K_WORK_DELAYABLE_DEFINE(k_suspend_thread, suspend_thread_work_handler);

static bool thread_suspend_flag = false;

#ifdef CONFIG_WATCHDOG
/** 
 * @brief Start Stop watchdog for Microphone threads
 *
 * This function starts the watchdog for thread when sensor has been started to 
 * monitor its health. Once the session has been stopped, watchdog of the threads
 * will be stopped.
 * 
 * @param sh_microphone: Start/Stop watchdog flag
 */
static void start_stop_wdt(bool start_watchdog) 
{
    LOG_INF("Microphone Receiving start watchdog flag : %d and channel id is : %d", start_watchdog, wdt_mic_channel_id);

    if(start_watchdog) {
        // Starting watchdog for all sensor threads 
        if(wdt_mic_channel_id < 0) {
            wdt_mic_channel_id = task_wdt_add_thread(WATCHDOG_LOW_TIMEOUT_MSEC);
            LOG_INF("Microphone watchdog enabled with channel id :%d", wdt_mic_channel_id);
        } else {
            LOG_INF("Microphone watchdog already enabled");
        }
    } else {
        // Stop watchdog to monitor all sensor threads 
        if(wdt_mic_channel_id >= 0) {
            LOG_INF("Microphone watchdog disabled with channel id :%d", wdt_mic_channel_id);
            task_wdt_delete_thread(wdt_mic_channel_id);
            wdt_mic_channel_id = WATCHDOG_CHANNEL_DEFAULT_ID;
        } else {
            LOG_INF("Microphone watchdog already disabled ");
        }
    }
}
#endif

static void suspend_thread_work_handler(struct k_work * work)
{
    int ret = 0;

    thread_suspend_flag = true;

#ifdef CONFIG_WATCHDOG
    // Turn off watchdog channel
    start_stop_wdt(false);
#endif

    /* This needs to be done in next release need to update MIC driver to have this feature */
    ret = mic::mic_suspend_power();
    if (ret < 0) {
        LOG_ERR("Failed to suspend MIC power (err %d)\n", ret);
#if defined(CONFIG_MEMFAULT)
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to suspend MIC power (err %d)\n", ret);
#endif
    }
    LOG_INF("sh_microphone :: power supend");
}

/** 
 * @brief Microphone thread module.
 *
 * This function handles microphone operations 
 * and data publishing over zbus.
 */
void microphone_thread_func(void *arg1, void *arg2, void *arg3)
{
    int32_t ret;
    // const struct zbus_channel *chan;
    sh_microphone::msg_t microphone_data;            // Structure to hold microphone data
    uint16_t buf[MIC_BUFFER_SIZE];
    uint16_t buf_size;
    uint8_t *encoded_data;
    size_t encoded_data_size = 0;
    int64_t time_stamp_storage = 0;
    int64_t time_stamp_streaming = 0;

    const struct zbus_channel *chan;
    cmd_msg_t cmd_msg;
    
#ifdef CONFIG_TEST_ENCODE_DECODE_DATA    
    static void *pcm_raw_data;
	size_t pcm_block_size;
    size_t decoded_data_size;
    uint8_t sec_cnt;
    sec_cnt = 0;
#endif

    while(1)
    {
        if(thread_suspend_flag)
        {
            thread_suspend_flag = false;
            LOG_ERR("microphone thread suspend");
            k_thread_suspend(&microphone_thread);
        }
        // Check and process incoming messages from the microphone_cmd_sub
        if(!zbus_sub_wait_msg(&microphone_cmd_sub, &chan, &cmd_msg, K_NO_WAIT)) {
            switch(cmd_msg.cmd_type)
            {
                case CMD_MIC_CONFIG_RW:
                    microphone_config_t mic_cfg;
                    sh_microphone::get_current_config(&mic_cfg);
                    if(cmd_msg.data_len != CMD_MIC_CONFIGURE_LEN) {
                        LOG_INF("Read-only request for mic config");
                    }
                    else {
                        if (cmd_msg.data[0] >= MICS_DISABLED && cmd_msg.data[0] <= STEREO_MODE) {
                            mic_cfg.num_of_chan = (cmd_msg.data[0] == MICS_DISABLED) ? NONE : (cmd_msg.data[0] == STEREO_MODE) ? STEREO : MONO;
                            if (mic_cfg.num_of_chan == MONO) {
                                if (cmd_msg.data[0] == BONE_CONDUCTION) {
                                    mic_cfg.primary_chan = LEFT_CHANNEL;
                                }
                                else if (cmd_msg.data[0] == ACOUSTIC) {
                                    mic_cfg.primary_chan = RIGHT_CHANNEL;
                                }
                                else {
                                    LOG_ERR("Invalid microphone config: %d", cmd_msg.data[0]);
                                }
                            } 
                        }
                        else {
                            LOG_INF("No change request to mic mode ");
                        }
                        if (cmd_msg.data[1] >= MIN_MIC_GAIN && cmd_msg.data[1] <= MAX_MIC_GAIN) {
                            mic_cfg.mic_lchannel_gain = cmd_msg.data[1]; 
                            mic_cfg.mic_rchannel_gain = cmd_msg.data[1]; 
                        }
                        else {
                            LOG_ERR("Invalid microphone gain: %d", cmd_msg.data[1]);
                        }
                        ret = sh_microphone::set_current_config(&mic_cfg);
                        if(ret) {
                            LOG_ERR("Failed to set the mic config (err = %d)", ret);
                        }
                    }
                    uint8_t mic_cfg_resp[3];
                    mic_cfg_resp[0] = SH_DATA_TYPE_MIC_CONFIG;
                    mic_cfg_resp[1] = (mic_cfg.num_of_chan == NONE) ? MICS_DISABLED : \
                                      (mic_cfg.num_of_chan == STEREO) ? STEREO_MODE : \
                                      (mic_cfg.primary_chan == LEFT_CHANNEL) ? BONE_CONDUCTION : ACOUSTIC;
                    mic_cfg_resp[2] = mic_cfg.mic_lchannel_gain;
                    LOG_INF("Mic config = %d", mic_cfg_resp[1]);
                    LOG_INF("Mic gain = %d", mic_cfg_resp[2]);
                    ret = sh_ble_pd_meas_send(mic_cfg_resp, 3);
                    if(ret < 0) {
                        LOG_ERR("Failed send the microphone status on BLE");
#if defined(CONFIG_MEMFAULT) 
                        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed send the microphone status on BLE");
#endif 
                    }
                    break;
                default:
                    break;
            }
        }

#ifdef CONFIG_WATCHDOG
    if(wdt_mic_channel_id >= 0) {
        task_wdt_fed_from_thread(wdt_mic_channel_id);
    }
#endif 
        switch(mic_status.mic_state){
            case MIC_STATE_IDLE:
                    k_msleep(200);
                    break;

            case MIC_STATE_FAULT:
                    fault_handler::send_diagnostics(MIC_FAULT);
                    sh_microphone::stop_sensing();
                    //TODO: Add recover mechanism if possible
                    
                    break;

            case MIC_STATE_START:
                    ret = mic::init(&g_mic_cnfg);
                    if (ret < 0) {
                        LOG_ERR("Mic Initialization failed (err %d)\n", ret);
#if defined(CONFIG_MEMFAULT) 
                        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Mic Initialization failed (err %d)\n", ret);
#endif 
                        mic_status.mic_fail_cnt++;
                        if(mic_status.mic_fail_cnt >= MAX_MIC_FAIL_CNT)
                        {
                            mic_status.mic_state = MIC_STATE_FAULT; 
                            break;
                        }
                    }
                    else
                    {
                        mic_status.mic_fail_cnt = 0;
                    }

                    switch(g_mic_cnfg.num_of_chan)
                    {
                        case MONO:
                        {
                            if(LEFT_CHANNEL == (g_mic_cnfg.primary_chan))
                            {
                                mic::left_mic_en(MIC_ENABLE);
                                mic::right_mic_en(MIC_DISABLE);
                            }
                            else if(RIGHT_CHANNEL == (g_mic_cnfg.primary_chan))
                            {
                                mic::left_mic_en(MIC_DISABLE);
                                mic::right_mic_en(MIC_ENABLE);
                            }
                        }
                        break;

                        case STEREO:
                        {
                            mic::right_mic_en(MIC_ENABLE);
                            mic::left_mic_en(MIC_ENABLE);
                        }
                        break;

                        default:
                        LOG_ERR("Wrong mic configuration");
                        break;
                    }
                    ret = mic::start();
                    if (ret < 0) {
                        LOG_ERR("Mic start failed (err %d)\n", ret);
#if defined(CONFIG_MEMFAULT) 
                        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Mic start failed (err %d)\n", ret);
#endif 
                        mic_status.mic_fail_cnt++;
                        if(mic_status.mic_fail_cnt >= MAX_MIC_FAIL_CNT)
                        {
                            mic_status.mic_state = MIC_STATE_FAULT; 
                            break;
                        }
                    }
                    else
                    {
                        mic_status.mic_fail_cnt = 0;
                    }  

                    LOG_INF("Mic started");

                    mic_status.mic_state = MIC_STATE_STREAM; 
                    break;
            case MIC_STATE_STOP:

                    LOG_INF("Mic Stopped");

                    mic_status.mic_state = MIC_STATE_IDLE; 
                    break;
            case MIC_STATE_STREAM:
                    ret = mic::readdata(buf,&buf_size);
                    if (ret < 0) {
                        LOG_ERR("Mic read data failed (err %d)\n", ret);
#if defined(CONFIG_MEMFAULT) 
                        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Mic read data failed (err %d)\n", ret);
#endif 
                        mic_status.mic_fail_cnt++;
                        if(mic_status.mic_fail_cnt >= MAX_MIC_FAIL_CNT)
                        {
                            microphone_data.type = 0xFF;    // sensor fault

                            ret = zbus_chan_pub(&microphone_chan, &microphone_data, K_NO_WAIT);
                            if(ret != 0){
                                LOG_ERR("Failed to publish microphone data [%d]",ret);
#if defined(CONFIG_MEMFAULT) 
                                // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to publish microphone data [%d]",ret);
#endif 
                            }
                            k_sem_give(&mem_mngr_sem);

                            mic_status.mic_state = MIC_STATE_FAULT;
                            break;
                        }
                    }
                    else{
                        mic_status.mic_fail_cnt = 0;

#ifdef CONFIG_TEST_RAW_MIC_DATA
                        uint16_t num_samples = buf_size / sizeof(int16_t);
                        for (uint16_t j = 0; j < num_samples; j++)
                        {
                            printk("%x ",buf[j]);
                        }

                        sec_cnt++;

                        if(sec_cnt >= 10) {

                            LOG_INF("RAW_MIC_DATA_TEST_COMPLETE");
                            ret = mic::stop();
                            if (ret < 0) {
                                LOG_ERR("Mic stop failed (err %d)\n", ret);
                                mic_status.mic_fail_cnt++;
                                if(mic_status.mic_fail_cnt >= MAX_MIC_FAIL_CNT)
                                {
                                    mic_status.mic_state = MIC_STATE_FAULT;
                                    break;
                                }
                            }
                            else
                            {
                                mic_status.mic_fail_cnt = 0;
                            }

                            while(1){
                                k_msleep(1000);
                            }
                        }

#elif CONFIG_TEST_ENCODE_DECODE_DATA
                        int compress_len = buf_size/10;

                        for(int i =0; i<10 ; i++){
                                            
                            ret = sw_codec_encode( ( (void*) buf + (i * compress_len ) ), compress_len, &encoded_data, &encoded_data_size);
                            if (ret < 0) {
                                LOG_ERR("lc3 encoding failed (err %d)\n", ret);
#if defined(CONFIG_MEMFAULT) 
                                // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"lc3 encoding failed (err %d)\n", ret);
#endif 
                            }
                            else{
                                // data_size+=encoded_data_size;

                                // for (uint16_t j = 0; j < encoded_data_size ; j++)
                                // {
                                //     printk("%x ",encoded_data[j]);
                                // }

                                ret = sw_codec_decode(encoded_data, encoded_data_size, 0, &pcm_raw_data,
                                            &pcm_block_size);
                                if (ret) {
                                    LOG_ERR("Failed to decode (err %d)\n", ret);
#if defined(CONFIG_MEMFAULT) 
                                    // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to decode (err %d)\n", ret);
#endif 
                                }
                                else{

                                    uint16_t *pcm_data = (uint16_t *) pcm_raw_data;

                                    uint16_t num_samples = pcm_block_size / sizeof(int16_t);
                                    for (uint16_t j = 0; j < num_samples; j++)
                                    {
                                        printk("%x ",pcm_data[j]);
                                    }

                                    decoded_data_size+=pcm_block_size;
                                }
                            }

                        }

                        sec_cnt++;

                        if(sec_cnt >= 10) {

                            LOG_INF("LC3_MIC_DATA_TEST_COMPLETE");
                        
                            ret = mic::stop();
                            if (ret < 0) {
                                LOG_ERR("Mic stop failed (err %d)\n", ret);
#if defined(CONFIG_MEMFAULT) 
                                // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Mic stop failed (err %d)\n", ret);
#endif 
                                mic_status.mic_fail_cnt++;
                                if(mic_status.mic_fail_cnt >= MAX_MIC_FAIL_CNT)
                                {
                                    mic_status.mic_state = MIC_STATE_FAULT;
                                    break;
                                }
                            }
                            else
                            {
                                mic_status.mic_fail_cnt = 0;
                            }

                            while(1){
                                k_msleep(1000);
                            }
                        }
#else
                        #if defined(CONFIG_SH_TIME_MODULE)
                        app_time::get_time_ticks(TimeResolution::MILLISECOND_TICKS_4096, &time_stamp_streaming);
                        app_time::get_time_ticks(TimeResolution::MILLISECOND_TICKS_8192, &time_stamp_storage);
                        #endif

                            microphone_data.type = SENSOR_MIC;
                            microphone_data.timestamp_storage = time_stamp_storage;
                            microphone_data.timestamp_streaming = time_stamp_streaming;
                            microphone_data.length = 0;

                        int compress_len = buf_size/LC3_RATIO_FOR_10MS_DATA;

                        for(int i =0; i<LC3_RATIO_FOR_10MS_DATA ; i++){
                                            
                            ret = sw_codec_encode( ( (void*) buf + (i * compress_len ) ), compress_len, &encoded_data, &encoded_data_size);
                            if (ret < 0) {
                                LOG_ERR("lc3 encoding failed (err %d)\n", ret);
#if defined(CONFIG_MEMFAULT) 
                                // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"lc3 encoding failed (err %d)\n", ret);
#endif 
                            }
                            else{
                                memcpy(&microphone_data.data[microphone_data.length], encoded_data, encoded_data_size);
                                microphone_data.length+=encoded_data_size;
                                LOG_DBG("mic mode size: %d", microphone_data.length);
                            }

                        }
#if CONFIG_PIPES
                        size_t bytes_written = 0;
                        ret = k_pipe_put(&microphone_pipe, (void*) &microphone_data, sizeof(microphone_data), &bytes_written,
                                        sizeof(microphone_data), K_NO_WAIT);
                        if (ret < 0) {
                            /* Incomplete message header sent */
                            // LOG_ERR("Error when data was sent through mic pipe: %d", ret);
                        } else if (bytes_written < sizeof(microphone_data)) {
                            /* Some of the data was sent */
                            LOG_ERR("Not all data was sent through mic pipe");
#if defined(CONFIG_MEMFAULT) 
                            // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Not all data was sent through mic pipe");
#endif                             
                        }
#else
                        // Publish microphone data over microphone_chan zbus channel
                        ret = zbus_chan_pub(&microphone_chan, &microphone_data, K_NO_WAIT);
                        if(ret != 0){
                            LOG_ERR("Failed to publish microphone data [%d]",ret);
#if defined(CONFIG_MEMFAULT) 
                            // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to publish microphone data [%d]",ret);
#endif                             
                        }
#endif
                            //Release the semaphores
#if CONFIG_MIC_STREAMING
                            k_sem_give(&stream_sem);
#endif
                            k_sem_give(&mem_mngr_sem);
#endif
                    }

                    break;
            default:
                    LOG_INF("MIC_UNKOWN_STATE");
                    break;
        }
        
    }
}

/**
 * @brief This function will resume the sensor thread and 
 * start the measurement of sensor data
 *
 * @return uint32_t Success indicates 0 and failure is non-zero.
 */
int sh_microphone::start_sensing(void)
{
    int ret = 0;

    if(sh_mic_config::get_enabled() == MIC_NO_CHANNEL)
    {
        //Swallow profile is selected hence not init the swallow profile
        return 0;
    }

    if (mic_status.mic_state == MIC_STATE_STREAM) {
        LOG_ERR("Mic already streaming, can't start");
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Mic already streaming, can't start");
#endif          
        return ret;
    }
    else if (g_mic_cnfg.num_of_chan == NONE) {
        LOG_ERR("Mics are disabled, they can't be used");
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Mics are disabled, they can't be used");
#endif          
        return ret;
    }
    LOG_INF("sh_microphone :: state: %d, chan %d",mic_status.mic_state,g_mic_cnfg.num_of_chan);

    /* This needs to be done in next release need to update MIC driver to have this feature */
    ret = mic::mic_resume_power();
    if (ret < 0) {
        LOG_ERR("Failed to resume MIC power (err %d)\n", ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to resume MIC power (err %d)\n", ret);
#endif          
    }
    mic_status.mic_state = MIC_STATE_START;
    thread_suspend_flag = false;
    LOG_INF("sh_microphone :: start_sensing");
    k_thread_resume(&microphone_thread);

    LOG_INF("sh_microphone :: resume");

#ifdef CONFIG_WATCHDOG
    // Turn on watchdog channel 
    start_stop_wdt(true);
#endif
    
    // MIC thread requires some time to resume its thread due to its stack size
    k_msleep(200);

    return ret;
}

/**
 * @brief This function will suspend the running sensor thread and 
 * stop the the sensor measurement
 *
 * @return uint32_t Success indicates 0 and failure is non-zero.
 */
int sh_microphone::stop_sensing(void)
{
    int ret = 0;

    if(sh_mic_config::get_enabled() == MIC_NO_CHANNEL)
    {
        //Swallow profile is selected hence not init the swallow profile
        return 0;
    }


    if (mic_status.mic_state == MIC_STATE_IDLE) {
        LOG_ERR("Mic already idle, can't stop");
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Mic already idle, can't stop");
#endif          
        return ret;
    }

    /**
     * Mic stop is ececuted prior to thread suspend as this will 
     * trigger the event handler to clear the flags the buffer. 
     */
    ret = mic::stop();
    if (ret < 0) {
        LOG_ERR("Mic stop failed (err %d)\n", ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Mic stop failed (err %d)\n", ret);
#endif          
        mic_status.mic_fail_cnt++;
        if(mic_status.mic_fail_cnt >= MAX_MIC_FAIL_CNT)
        {
            mic_status.mic_state = MIC_STATE_FAULT; 
        }
    }
    else
    {
        mic_status.mic_state = MIC_STATE_IDLE;
        mic_status.mic_fail_cnt = 0;
    }
    LOG_INF("sh_microphone :: mic::stop");

    // Schedule the work to suspend the thread. The thread will suspend after 200ms
    // to give some time to dmic to get deinitialized.
    k_work_reschedule(&k_suspend_thread, K_MSEC(200));

    return ret;
}

int sh_microphone::get_current_config(microphone_config_t *config)
{
    if(config == NULL)
    {
        return -1;
    }
    memcpy(config, &g_mic_cnfg, sizeof(microphone_config_t));
    return 0;
}

int sh_microphone::set_current_config(microphone_config_t *config)
{
    if(config == NULL)
    {
        return -1;
    }
    memcpy(&g_mic_cnfg, config,  sizeof(microphone_config_t));
    mic::set_last_configuration(config);
    return 0;
}

/**
 * @brief Read the current microphone configuration from the NVS and load 
 * the configuration in microphone application structure
 *
 * @return none
 */
static void load_microphone_config(void) 
{ 
    switch (sh_mic_config::get_enabled())
    {
    case MIC_NO_CHANNEL:
        {
            g_mic_cnfg.microphonel_en = MIC_DISABLE;
            g_mic_cnfg.microphoner_en = MIC_DISABLE;
            g_mic_cnfg.num_of_chan = MONO;
            g_mic_cnfg.primary_chan = LEFT_CHANNEL;
        }
        break;

    case MIC_LEFT_CHANNEL:
        {
            g_mic_cnfg.microphonel_en = MIC_ENABLE;
            g_mic_cnfg.microphoner_en = MIC_DISABLE;
            g_mic_cnfg.num_of_chan = MONO;
            g_mic_cnfg.primary_chan = LEFT_CHANNEL;
        }
        break;
    
    case MIC_RIGHT_CHANNEL:
        {
            g_mic_cnfg.microphonel_en = MIC_DISABLE;
            g_mic_cnfg.microphoner_en = MIC_ENABLE;
            g_mic_cnfg.num_of_chan = MONO;
            g_mic_cnfg.primary_chan = RIGHT_CHANNEL;
        }
        break;
    
    case MIC_ALL_CHANNEL:
        {
            g_mic_cnfg.microphonel_en = MIC_ENABLE;
            g_mic_cnfg.microphoner_en = MIC_ENABLE;
            g_mic_cnfg.num_of_chan = STEREO;
            g_mic_cnfg.primary_chan = RIGHT_CHANNEL;
        }
        break;
    
    default:
        break;
    }
    
    g_mic_cnfg.mic_lchannel_gain = sh_mic_config::get_lchannel_gain();
    g_mic_cnfg.mic_rchannel_gain = sh_mic_config::get_rchannel_gain();
}

/**
 * @brief Initializes the microphone streaming thread 
 * and adds zbus observers for microphone channel.
 *
 * @return uint32_t Success indicates 0 and failure is non-zero.
 */
uint32_t sh_microphone::thread_init(void)
{
    uint32_t ret = 0;
    
    //TODO: Read configuration from NVS block

        // Check the mic is enable or not 
    if(sh_mic_config::get_enabled() == MIC_NO_CHANNEL) {
        LOG_ERR("Microphone config disabled in the NVS. Cannot initialize microphone");
        return -1;
    }
    
    // Load the configuration from the NVS and set the configuration in mic
    load_microphone_config();
    mic::set_last_configuration(&g_mic_cnfg);

    microphone_thread_id = k_thread_create(&microphone_thread, microphone_stack,
			K_THREAD_STACK_SIZEOF(microphone_stack),
			microphone_thread_func, NULL, NULL, NULL,
			3, K_USER, K_NO_WAIT);

    k_thread_name_set(microphone_thread_id, "microphone");

    // Add thread as a zbus observer to temperature chan
    ret = zbus_chan_add_obs(&cmd_chan, &microphone_cmd_sub, K_MSEC(200));
    if(ret){
        LOG_ERR("Failed to add thread as zbus observer to ble streaming chan (err: %d)", ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to add thread as zbus observer to ble streaming chan (err: %d)", ret);
#endif          
    } 

    // Stop sensor thread until session starts
    mic_status.mic_state = MIC_STATE_STOP;
    ret = sh_microphone::stop_sensing();
    if(ret < 0) {
        LOG_ERR("Failed to Start sensor %d.\n",ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to Start sensor %d.\n",ret);
#endif          
    }

    return ret;
}

#endif  //Microphone
