#include "laser_control.h"
#include "../MarlinCore.h"

// Define the I2C addresses for DamX 10 lasers
const uint8_t laser_i2c_addresses[10] = { 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29 };

LaserControl laserControl;

void LaserControl::init() {

  SERIAL_ECHOLNPGM("LaserControl: I2C Init");

  #ifdef I2C_SDA_PIN
    SET_INPUT_PULLUP(I2C_SDA_PIN);
  #endif

  #ifdef I2C_SCL_PIN
    SET_INPUT_PULLUP(I2C_SCL_PIN);
  #endif

  // Set up the AVR TWI bit rate register (100kHz standard mode at 16MHz CPU)
  TWBR = 72;
  TWSR = 0x00;

    // Initialize the power cache array to 0 (all lasers off)
  for (uint8_t i = 0; i < 10; i++) {
    last_powers[i] = 0;
  }

  SERIAL_ECHOLNPGM("LaserControl: I2C Pins configured (pullups enabled)");

  // DamX Laser - Raspberry Pi synchronization output
  pinMode(LASER_SYNC_PIN, OUTPUT);
  digitalWrite(LASER_SYNC_PIN, LOW);
  SERIAL_ECHOLNPGM("LaserControl: LASER_SYNC_PIN initialized: D66 = LOW");
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

// Low-level helper to write a single byte payload to a specific I2C slave address
bool LaserControl::twi_write_byte(const uint8_t addr, const uint8_t data) {
  // 1. Send START Condition
  TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
  while (!(TWCR & (1 << TWINT)));
  if ((TWSR & 0xF8) != 0x08 && (TWSR & 0xF8) != 0x10) return false; // Fail if START or Repeated START not sent

  // 2. Send Slave Address + Write Bit (0)
  TWDR = (addr << 1);
  TWCR = (1 << TWINT) | (1 << TWEN);
  while (!(TWCR & (1 << TWINT)));
  if ((TWSR & 0xF8) != 0x18) { // Fail if SLA+W ACK not received
    // Emergency STOP to clear the bus
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    return false;
  }

  // 3. Send Data Byte (Power Value)
  TWDR = data;
  TWCR = (1 << TWINT) | (1 << TWEN);
  while (!(TWCR & (1 << TWINT)));
  if ((TWSR & 0xF8) != 0x28) { // Fail if Data ACK not received
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    return false;
  }

  // 4. Send STOP Condition
  TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
  delayMicroseconds(10); // Brief hold for bus stabilization
  return true;
}

void LaserControl::set_power(const uint8_t pwr) {

  // Raspberry Pi laser synchronization
  // HIGH = laser power requested
  // LOW  = laser off
  digitalWrite(LASER_SYNC_PIN, pwr > 0 ? HIGH : LOW);

  SERIAL_ECHOPGM("{LASER:");
  SERIAL_ECHO((int)pwr);
  SERIAL_ECHOLNPGM("}");
}

void LaserControl::set_power(const uint8_t laser_idx, const uint8_t pwr) {

  // Raspberry Pi laser synchronization
  // HIGH = laser power requested
  // LOW  = laser off
  digitalWrite(LASER_SYNC_PIN, pwr > 0 ? HIGH : LOW);

  SERIAL_ECHOPGM("{LASER:");
  SERIAL_ECHO((int)laser_idx);
  SERIAL_ECHOPGM("{PWR:");
  SERIAL_ECHO((int)pwr);
  SERIAL_ECHOLNPGM("}");
  /*
  // Boundary safety check
  if (laser_idx >= 10) return;

  // Cache filter: skip transmission if this specific laser hasn't changed power states
  if (pwr == last_powers[laser_idx]) return;

  // Retrieve the unique hardware I2C address for this laser
  const uint8_t target_address = laser_i2c_addresses[laser_idx];

  // Execute the I2C physical transfer
  bool success = twi_write_byte(target_address, pwr);

  if (success) {
    last_powers[laser_idx] = pwr; // Update cache on successful transmission
  } else {
    // Optional: Log bus communication failures for tracking down electrical noise
    SERIAL_ECHOPGM("LaserControl Error: Comm failed on Laser ");
    SERIAL_ECHOLN((int)laser_idx);
  }
  */
}