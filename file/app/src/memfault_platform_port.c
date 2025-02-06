#include "memfault/components.h"
bool battery_is_discharging(){
	uint8_t battery_level=0;
	battery_level = sh_bas_get_battery_level();
	bool discharging;
	if((battery_level & 0x80)==1){
		discharging = 1;
	}else {
		discharging = 0;
	}
	return discharging;
}
static bool s_battery_is_discharging = false;

// This function is called on system initialization, to set the initial
// discharging state
void battery_set_initial_discharging_state(void) {
  s_battery_is_discharging = battery_is_discharging();
}

// This function is called by when the battery discharging state changes, i.e.:
// CHARGING → DISCHARGING or DISCHARGING → CHARGING
void battery_charge_state_changed_callback(void) {
  const bool discharging = battery_is_discharging();
  if (discharging != s_battery_is_discharging) {
    // update the state of the battery
    s_battery_is_discharging = discharging;

    if(!discharging) {
      // signal the Memfault SDK that the battery has stopped discharging
      memfault_metrics_battery_stopped_discharging();
    }
  }
}


int memfault_platform_get_stateofcharge(sMfltPlatformBatterySoc *soc){
	uint8_t battery_level=0;
	battery_level = sh_bas_get_battery_level();
	bool chargingStatus;
	if((battery_level & 0x80)==1){
		chargingStatus = 1;
	}else {
		chargingStatus = 0;
	}
	*soc = (sMfltPlatformBatterySoc){
	    // scale up the floating-point percentage to be an integer with 2 decimal
	    // places of precision, matching the selection of MEMFAULT_METRICS_BATTERY_SOC_PCT_SCALE_VALUE
	    .soc = (battery_level & 0x7F)*100 ,
	    .discharging = chargingStatus,
	  };
	  return 0;
}

