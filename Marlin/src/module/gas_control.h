#pragma once

#include "../inc/MarlinConfigPre.h"

// ------------------------
// Configuration
// ------------------------

#ifndef GAS_CONTROL_DEFAULT_OFF
  #define GAS_CONTROL_DEFAULT_OFF true
#endif

#ifndef VACUUM_RELAY_ACTIVE_STATE
  #define VACUUM_RELAY_ACTIVE_STATE HIGH
#endif

#ifndef ARGON_INLET_ACTIVE_STATE
  #define ARGON_INLET_ACTIVE_STATE HIGH
#endif

#ifndef ARGON_PUMP_ACTIVE_STATE
  #define ARGON_PUMP_ACTIVE_STATE HIGH
#endif

// ------------------------
// GasControl Class
// ------------------------

class GasControl {
  
public:

  // Init
  void init();

  // Vacuum relay
  void vacuum_on();
  void vacuum_off();
  bool vacuum_state() const;

  // Argon inlet
  void argon_inlet_on();
  void argon_inlet_off();
  bool argon_inlet_state() const;

  // Argon pump
  void argon_pump_on();
  void argon_pump_off();
  bool argon_pump_state() const;

  // Safety
  void all_off();

private:

  bool _vacuum_state = false;
  bool _argon_inlet_state = false;
  bool _argon_pump_state = false;

  void write_pin(const int pin, const bool on, const uint8_t active_state);
};

// ------------------------
// Global instance
// ------------------------

extern GasControl gasControl;