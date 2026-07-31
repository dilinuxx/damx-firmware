#pragma once

#include "../inc/MarlinConfigPre.h"

// ------------------------
// Laser Control Class
// ------------------------

class LaserControl {

public:

  void init();

  // I2C diagnostics
  void i2c_scanner();
  void bme280_probe();
  void pca_probe();
  void ads7138_probe();

private:

  void i2c_start();
  void i2c_stop();
  uint8_t i2c_write(uint8_t data);
};

// ------------------------
// Global instance
// ------------------------

extern LaserControl laserControl;