#pragma once

#include "../inc/MarlinConfigPre.h"

// ------------------------
// Laser Control Class
// ------------------------

class LaserControl {
public:
  uint8_t active_laser = 0; // Tracks target laser (0 to 9). Defaults to 0 for single-laser setups.
  void init();
  void i2c_scanner();
  void bme280_probe();
  void pca_probe();
  void ads7138_probe();
  void set_power(const uint8_t pwr);
  void set_power(const uint8_t laser_idx, const uint8_t pwr);

private:

  uint8_t last_powers[10]; 
  void i2c_start();
  void i2c_stop();
  uint8_t i2c_write(uint8_t data);
  bool twi_write_byte(const uint8_t addr, const uint8_t data);
};

// ------------------------
// Global instance
// ------------------------

extern LaserControl laserControl;