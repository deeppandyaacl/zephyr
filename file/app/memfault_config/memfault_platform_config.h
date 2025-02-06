//As of now, we have disabled the matrices. If required then enable it in future. And commenct out CONFIG_MEMFAULT_METRICS_HEARTBEAT_INTERVAL_SECS
//in project config file

// #pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Platform overrides for the default configuration settings in the memfault-firmware-sdk.
//! Default configuration settings can be found in "memfault/config.h"

// Enable capture of entire ISR state at time of crash
// #ifdef CONFIG_MEMFAULT_NVIC_INTERRUPTS_TO_COLLECT
//   #define MEMFAULT_NVIC_INTERRUPTS_TO_COLLECT CONFIG_MEMFAULT_NVIC_INTERRUPTS_TO_COLLECT
// #else
//   #undef MEMFAULT_NVIC_INTERRUPTS_TO_COLLECT  // Undefine the macro if the config option is not set
// #endif

// #ifdef CONFIG_MEMFAULT_METRICS_BATTERY_ENABLE
//   #define MEMFAULT_METRICS_BATTERY_ENABLE CONFIG_MEMFAULT_METRICS_BATTERY_ENABLE
// #else
//   #undef MEMFAULT_METRICS_BATTERY_ENABLE  // Undefine the macro if the config option is not set
// #endif

// #ifdef CONFIG_MEMFAULT_METRICS_BATTERY_SOC_PCT_SCALE_VALUE
//   #define MEMFAULT_METRICS_BATTERY_SOC_PCT_SCALE_VALUE CONFIG_MEMFAULT_METRICS_BATTERY_SOC_PCT_SCALE_VALUE
// #else
//   #undef MEMFAULT_METRICS_BATTERY_SOC_PCT_SCALE_VALUE  // Undefine the macro if the config option is not set
// #endif

// #ifdef CONFIG_MEMFAULT_METRICS_HEARTBEAT_INTERVAL_SECS
//   #define MEMFAULT_METRICS_HEARTBEAT_INTERVAL_SECS CONFIG_MEMFAULT_METRICS_HEARTBEAT_INTERVAL_SECS
// #else
//   #undef MEMFAULT_METRICS_HEARTBEAT_INTERVAL_SECS  // Undefine the macro if the config option is not set
// #endif
