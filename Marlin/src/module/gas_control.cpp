#include "gas_control.h"
#include "../inc/MarlinConfig.h"
#include <Arduino.h>

// ------------------------
// Global instance
// ------------------------

GasControl gasControl;


// ------------------------
// Internal helper
// ------------------------

void GasControl::write_pin(const int pin, const bool on, const uint8_t active_state) {
    const bool level = on ? active_state : !active_state;
  digitalWrite(pin, level ? HIGH : LOW);
}

// ------------------------
// Init
// ------------------------

void GasControl::init() {

  #ifdef VACUUM_RELAY_PIN
    SET_OUTPUT(VACUUM_RELAY_PIN);
  #endif

  #ifdef ARGON_INLET_PIN
    SET_OUTPUT(ARGON_INLET_PIN);
  #endif

  #ifdef ARGON_OUTLET_PIN
    SET_OUTPUT(ARGON_OUTLET_PIN);
  #endif

  #ifdef ARGON_PUMP_PIN
    SET_OUTPUT(ARGON_PUMP_PIN);
  #endif

  #ifdef S1_SIGNAL_PIN
    SET_OUTPUT(S1_SIGNAL_PIN);
  #endif

  if (GAS_CONTROL_DEFAULT_OFF) {
    all_off();
  }
}

// ------------------------
// Vacuum
// ------------------------

void GasControl::vacuum_on() {
  #ifdef VACUUM_RELAY_PIN
    write_pin(VACUUM_RELAY_PIN, true, VACUUM_RELAY_ACTIVE_STATE);
    _vacuum_state = true;
  #endif
}

void GasControl::vacuum_off() {
  #ifdef VACUUM_RELAY_PIN
    write_pin(VACUUM_RELAY_PIN, false, VACUUM_RELAY_ACTIVE_STATE);
    _vacuum_state = false;
  #endif
}

bool GasControl::vacuum_state() const {
  return _vacuum_state;
}

// ------------------------
// Argon inlet
// ------------------------

void GasControl::argon_inlet_on() {
  #ifdef ARGON_INLET_PIN
    write_pin(ARGON_INLET_PIN, true, ARGON_INLET_ACTIVE_STATE);
    _argon_inlet_state = true;
  #endif
}

void GasControl::argon_inlet_off() {
  #ifdef ARGON_INLET_PIN
    write_pin(ARGON_INLET_PIN, false, ARGON_INLET_ACTIVE_STATE);
    _argon_inlet_state = false;
  #endif
}

bool GasControl::argon_inlet_state() const {
  return _argon_inlet_state;
}

// ------------------------
// Argon pump
// ------------------------

void GasControl::argon_pump_on() {
  #ifdef ARGON_PUMP_PIN
    write_pin(ARGON_PUMP_PIN, true, ARGON_PUMP_ACTIVE_STATE);
    _argon_pump_state = true;
  #endif
}

void GasControl::argon_pump_off() {
  #ifdef ARGON_PUMP_PIN
    write_pin(ARGON_PUMP_PIN, false, ARGON_PUMP_ACTIVE_STATE);
    _argon_pump_state = false;
  #endif
}

bool GasControl::argon_pump_state() const {
  return _argon_pump_state;
}

// ------------------------
// Safety
// ------------------------

void GasControl::all_off() {
  vacuum_off();
  argon_inlet_off();
  argon_pump_off();
}
  