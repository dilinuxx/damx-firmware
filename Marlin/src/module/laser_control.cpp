#include "laser_control.h"
#include "../MarlinCore.h"

LaserControl laserControl;

void LaserControl::init() {

  SERIAL_ECHOLNPGM("LaserControl: I2C Init");

  #ifdef I2C_SDA_PIN
    SET_INPUT_PULLUP(I2C_SDA_PIN);
  #endif

  #ifdef I2C_SCL_PIN
    SET_INPUT_PULLUP(I2C_SCL_PIN);
  #endif

  SERIAL_ECHOLNPGM("LaserControl: I2C Pins configured (pullups enabled)");
}

void LaserControl::i2c_scanner() {

  SERIAL_ECHOLNPGM("LaserControl: I2C Scanner Start");

  TWBR = 72;
  TWSR = 0x00;

  uint8_t found = 0;

  for (uint8_t address = 1; address < 127; address++) {

    // START
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    // SLA+W
    TWDR = (address << 1);
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    uint8_t status = TWSR & 0xF8;

    if (status == 0x18) {

      SERIAL_ECHOPGM("Device found at address ");
      SERIAL_ECHO((int)address);
      SERIAL_ECHOLNPGM(" (ACK)");

      found++;
    }

    // STOP
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    delayMicroseconds(50);
  }

  SERIAL_ECHOPGM("LaserControl: Scan complete. Devices = ");
  SERIAL_ECHO((int)found);
  SERIAL_EOL();

  if (!found) {
    SERIAL_ECHOLNPGM("LaserControl: No I2C devices detected");
  }
}

void LaserControl::bme280_probe() {

  SERIAL_ECHOLNPGM("LaserControl: BME280 Probe Start");

  TWBR = 72;
  TWSR = 0x00;

  uint8_t address = 0x77 << 1;

  TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
  while (!(TWCR & (1 << TWINT)));

  TWDR = address;
  TWCR = (1 << TWINT) | (1 << TWEN);
  while (!(TWCR & (1 << TWINT)));

  uint8_t status = TWSR & 0xF8;

  SERIAL_ECHOPGM("ADDR Status: ");
  SERIAL_ECHO((int)status);
  SERIAL_EOL();

  if (status == 0x18)
    SERIAL_ECHOLNPGM("LaserControl: BME280 ACK OK");
  else
    SERIAL_ECHOLNPGM("LaserControl: BME280 NO ACK");

  TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}