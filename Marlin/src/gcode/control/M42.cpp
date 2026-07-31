/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "../../inc/MarlinConfig.h"
#include "../../MarlinCore.h"

#if ENABLED(DIRECT_PIN_CONTROL)

#include "../gcode.h"

#if HAS_FAN
#include "../../module/temperature.h"
#endif

#ifdef MAPLE_STM32F1
// these are enums on the F1...
#define INPUT_PULLDOWN INPUT_PULLDOWN
#define INPUT_ANALOG INPUT_ANALOG
#define OUTPUT_OPEN_DRAIN OUTPUT_OPEN_DRAIN
#endif

bool pin_is_protected(const pin_t pin);

void protected_pin_err()
{
  SERIAL_ERROR_MSG(STR_ERR_PROTECTED_PIN);
}

/**
 * M42: Change pin status via G-Code
 *
 *  P<pin>  Pin number (LED if omitted)
 *          For LPC1768 specify pin P1_02 as M42 P102,
 *                                  P1_20 as M42 P120, etc.
 *
 *  S<byte> Pin status from 0 - 255
 *  I       Flag to ignore Marlin's pin protection
 *
 *  T<mode> Pin mode: 0=INPUT  1=OUTPUT  2=INPUT_PULLUP  3=INPUT_PULLDOWN
 *                    4=INPUT_ANALOG  5=OUTPUT_OPEN_DRAIN
 */
void GcodeSuite::M42()
{
  const int pin_index = PARSED_PIN_INDEX('P', GET_PIN_MAP_INDEX(LED_PIN));
  if (pin_index < 0)
    return;

  const pin_t pin = GET_PIN_MAP_PIN(pin_index);

  /* EMEM - UNCOMMENT FOR ARGON CONTROL
  if (!parser.boolval('I') && pin_is_protected(pin))
    return protected_pin_err();
  */

  bool avoidWrite = false;
  if (parser.seenval('T'))
  {
    switch (parser.value_byte())
    {
    case 0:
      pinMode(pin, INPUT);
      avoidWrite = true;
      break;
    case 1:
      pinMode(pin, OUTPUT);
      break;
    case 2:
      pinMode(pin, INPUT_PULLUP);
      avoidWrite = true;
      break;
#ifdef INPUT_PULLDOWN
    case 3:
      pinMode(pin, INPUT_PULLDOWN);
      avoidWrite = true;
      break;
#endif
#ifdef INPUT_ANALOG
    case 4:
      pinMode(pin, INPUT_ANALOG);
      avoidWrite = true;
      break;
#endif
#ifdef OUTPUT_OPEN_DRAIN
    case 5:
      pinMode(pin, OUTPUT_OPEN_DRAIN);
      break;
#endif
    default:
      SERIAL_ECHOLNPGM("Invalid Pin Mode");
      return;
    }
  }

  if (!parser.seenval('S'))
    return;
  const byte pin_status = parser.value_byte();

#if HAS_FAN
  switch (pin)
  {
#define _CASE(N)                              \
  case FAN##N##_PIN:                          \
    thermalManager.fan_speed[N] = pin_status; \
    return;
    REPEAT(FAN_COUNT, _CASE)
  }
#endif

  if (avoidWrite)
  {
    SERIAL_ECHOLNPGM("?Cannot write to INPUT");
    return;
  }

// An OUTPUT_OPEN_DRAIN should not be changed to normal OUTPUT (STM32)
// Use M42 Px T1/5 S0/1 to set the output type and then set value
#ifndef OUTPUT_OPEN_DRAIN
  pinMode(pin, OUTPUT);
#endif
  extDigitalWrite(pin, pin_status);

#ifdef ARDUINO_ARCH_STM32
  // A simple I/O will be set to 0 by hal.set_pwm_duty()
  if (pin_status <= 1 && !PWM_PIN(pin))
    return;
#endif
  hal.set_pwm_duty(pin, pin_status);
}

#endif // DIRECT_PIN_CONTROL

#if PIN_EXISTS(PRESSURE_SENSOR) && PIN_EXISTS(VACUUM_RELAY)
bool vacuum_setup_complete = false;

// Analog değeri doğrudan bara çevir
float readPressureBar()
{
  int analogValue = analogRead(PRESSURE_SENSOR_PIN);
  analogValue = constrain(analogValue, 0, 1023); // Güvenlik sınırlandırması
  return (analogValue - 511) / 410.0;            // 511 = 0 bar, 410 = ±1 bar aralığı
}

// Pompa kontrolü
void setVacuumPump(bool state)
{
  WRITE(VACUUM_RELAY_PIN, state ? HIGH : LOW);
  SERIAL_ECHOPGM("Vacuum pump ");
  if (state)
    SERIAL_ECHOLNPGM("ON");
  else
    SERIAL_ECHOLNPGM("OFF");
}

// Vakum işlemini gerçekleştir
void performVacuumSetup()
{
  if (vacuum_setup_complete)
    return;

  const float TARGET_BAR = -0.5;         // Hedef basınç
  const int PRESSURE_CHECK_DELAY = 5000; // ms

  setVacuumPump(true);
  SERIAL_ECHOLNPGM("Target: -0.5 bar");
  SERIAL_ECHOLNPGM("Monitoring pressure...");

  while (true)
  {
    safe_delay(PRESSURE_CHECK_DELAY);
    idle(); // ➕ Watchdog resetini önlemek için marlin döngüsünü sürdür

    float bar = readPressureBar();
    SERIAL_ECHOPGM("  → Pressure: ");
    SERIAL_ECHO_F(bar, 3);
    SERIAL_ECHOLNPGM(" bar");

    if (bar <= TARGET_BAR)
      break;
  }

  setVacuumPump(false);
  vacuum_setup_complete = true;
  SERIAL_ECHOLNPGM("✅ Vacuum process complete.\n");
  delay(500);
}

// G-code: M750 - vakum işlemini başlat
void GcodeSuite::M750()
{
  SERIAL_ECHOLNPGM("\n== M750: Vacuum Start ==");

  static bool pins_initialized = false;
  if (!pins_initialized)
  {
    SET_OUTPUT(VACUUM_RELAY_PIN);
    SET_INPUT(PRESSURE_SENSOR_PIN);
    setVacuumPump(false);
    pins_initialized = true;
  }

  performVacuumSetup();
  setVacuumPump(false); // Güvenlik
  SERIAL_ECHOLNPGM("== M750: Vacuum End ==\n");
}
#endif

#if PIN_EXISTS(VACUUM_RELAY)
/**
 * M751: Toggle relay pin
 * Toggles the VACUUM_RELAY_PIN state (HIGH <-> LOW).
 */
void GcodeSuite::M751()
{
  static bool pin_initialized = false;

  if (!pin_initialized)
  {
    SET_OUTPUT(VACUUM_RELAY_PIN);
    WRITE(VACUUM_RELAY_PIN, LOW); // Start OFF
    pin_initialized = true;
  }

  // Toggle the pin
  const bool current_state = READ(VACUUM_RELAY_PIN);
  WRITE(VACUUM_RELAY_PIN, !current_state);

  if (!current_state)
    SERIAL_ECHOLNPGM("Relay ON (M751)");
  else
    SERIAL_ECHOLNPGM("Relay OFF (M751)");
}
#endif

#if PIN_EXISTS(EZO_O2_SIGNAL) && PIN_EXISTS(ARGON_INLET) && PIN_EXISTS(ARGON_PUMP)

static bool o2_monitoring_active = false;

void readOxygenAndControlArgon()
{
  while (Serial2.available())
    Serial2.read();

  Serial2.print("r\r");

  unsigned long start = millis();
  const unsigned long timeout = 5000;
  while (digitalRead(EZO_O2_SIGNAL_PIN) == LOW)
  {
    if (millis() - start > timeout)
    {
      SERIAL_ECHOLNPGM("Timeout waiting for signal pin");
      return;
    }
    safe_delay(10);
  }

  String response = "";
  start = millis();
  while (millis() - start < 200)
  {
    while (Serial2.available())
    {
      char c = Serial2.read();
      response += c;
    }
    safe_delay(10);
  }

  SERIAL_ECHOPGM("EZO-O2 Response: ");
  SERIAL_ECHOLN(response.c_str());

  int commaIndex = response.indexOf(',');
  if (commaIndex > 0)
  {
    String valueStr = response.substring(commaIndex + 1);
    float o2 = valueStr.toFloat();

    SERIAL_ECHOPGM("Parsed O2 %: ");
    SERIAL_ECHOLN(o2);

    const float O2_THRESHOLD = 0.1;

    if (o2 > O2_THRESHOLD)
    {
      WRITE(ARGON_PUMP_PIN, HIGH);
      WRITE(ARGON_INLET_PIN, HIGH);
      SERIAL_ECHOLNPGM("Argon system ON (O2 > 0.1%)");
    }
    else
    {
      WRITE(ARGON_PUMP_PIN, LOW);
      WRITE(ARGON_INLET_PIN, LOW);
      SERIAL_ECHOLNPGM("Argon system OFF (O2 <= 0.1%)");
    }
  }
  else
  {
    SERIAL_ECHOLNPGM("Invalid response format");
  }
}

void GcodeSuite::M752()
{
  static bool initialized = false;

  // S parametresi kontrolü (varsayılan: 1)
  int s_param = parser.seen('S') ? parser.value_int() : 1;

  if (s_param == 0)
  {
    o2_monitoring_active = false;
    WRITE(ARGON_PUMP_PIN, LOW);
    WRITE(ARGON_INLET_PIN, LOW);
    SERIAL_ECHOLNPGM("Stopped O2 monitoring and turned OFF argon system");
    return;
  }

  if (!initialized)
  {
    SET_INPUT(EZO_O2_SIGNAL_PIN);
    SET_OUTPUT(ARGON_PUMP_PIN);
    SET_OUTPUT(ARGON_INLET_PIN);
    SET_OUTPUT(ARGON_OUTLET_PIN);
    WRITE(ARGON_PUMP_PIN, LOW);
    WRITE(ARGON_INLET_PIN, LOW);
    WRITE(ARGON_OUTLET_PIN, LOW);
    Serial2.begin(9600);
    safe_delay(300);
    initialized = true;
    SERIAL_ECHOLNPGM("EZO-O2 system initialized using Serial2 (pins 16/17)");
  }

  if (o2_monitoring_active)
  {
    SERIAL_ECHOLNPGM("O2 monitoring already active.");
    return;
  }

  o2_monitoring_active = true;
  SERIAL_ECHOLNPGM("Started O2 monitoring loop");

  while (o2_monitoring_active)
  {
    readOxygenAndControlArgon();
    safe_delay(1000);
  }

  SERIAL_ECHOLNPGM("Exited O2 monitoring loop");
}

#endif

#if PIN_EXISTS(ARGON_PUMP) && PIN_EXISTS(ARGON_INLET)

/**
 * M753: Toggle Argon pump and inlet state
 */
void GcodeSuite::M753()
{
  static bool pins_initialized = false;

  if (!pins_initialized)
  {
    SET_OUTPUT(ARGON_PUMP_PIN);
    SET_OUTPUT(ARGON_INLET_PIN);
    WRITE(ARGON_PUMP_PIN, LOW);
    WRITE(ARGON_INLET_PIN, LOW);
    pins_initialized = true;
    SERIAL_ECHOLNPGM("Argon pins initialized");
  }

  const bool current_state = READ(ARGON_PUMP_PIN); // assume both are synced
  const bool new_state = !current_state;

  WRITE(ARGON_PUMP_PIN, new_state);
  WRITE(ARGON_INLET_PIN, new_state);

  SERIAL_ECHOPGM("Argon system toggled ");
  SERIAL_ECHOLN(new_state ? "ON" : "OFF");
}
#endif

// #if PIN_EXISTS(MAX6675_SO) && PIN_EXISTS(MAX6675_CS) && PIN_EXISTS(MAX6675_SCK)

// #include <max6675.h>

// MAX6675 thermocouple(MAX6675_SCK_PIN, MAX6675_CS_PIN, MAX6675_SO_PIN);

// /**
//  * M754: Read temperature from MAX6675 thermocouple
//  */
// void GcodeSuite::M754()
// {
//   static bool initialized = false;

//   if (!initialized)
//   {
//     SET_INPUT(MAX6675_SO_PIN);
//     SET_OUTPUT(MAX6675_CS_PIN);
//     SET_OUTPUT(MAX6675_SCK_PIN);

//     WRITE(MAX6675_CS_PIN, HIGH);
//     WRITE(MAX6675_SCK_PIN, LOW);

//     delay(500); // Allow MAX6675 to stabilize
//     SERIAL_ECHOLNPGM("MAX6675 initialized");
//     initialized = true;
//   }

//   float tempC = thermocouple.readCelsius();

//   SERIAL_ECHOPGM("MAX6675 Temp: ");
//   SERIAL_ECHO(tempC);
//   SERIAL_ECHOLNPGM(" °C");
// }
// #endif

/** EMEM
 * read_pressure_sensor
 * -------------------------
 * Reads the XDB305 0–5V 3-wire pressure sensor.
 * 
 * Important change: 
 * - The sensor outputs ~2.5V at 0 bar (not 0V), so we subtract ADC_ZERO 
 *   to ensure zero pressure is correctly reported.
 * - ADC readings are constrained between ADC_ZERO and ADC_MAX to prevent
 *   spurious negative or excessive pressure values.
 * - Optional smoothing via multiple samples to reduce noise.
 */

#define P_MAX 10.0               // Full-scale pressure (bar)
#define ADC_ZERO 512             // ADC reading at 0 bar (~2.5V)
#define ADC_MAX 1023             // ADC reading at full scale (~5V)
#define ADC_SAMPLES 10           // Number of samples to average for smoothing

float read_pressure_sensor() {
  long sum = 0;

  // Take multiple samples for smoothing
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(PRESSURE_SENSOR_PIN);
  }

  int analogValue = sum / ADC_SAMPLES;

  // Constrain ADC value to sensor range
  analogValue = constrain(analogValue, ADC_ZERO, ADC_MAX);

  // Convert ADC to pressure (linear mapping)
  float pressure = (analogValue - ADC_ZERO) * (P_MAX / float(ADC_MAX - ADC_ZERO));

  // Print pressure for debugging/console
  //SERIAL_ECHO("Pressure: ");
  //SERIAL_ECHOLN(pressure);  // prints in bar

  return pressure;  // pressure in bar
}

#include <Arduino.h>

#define OXYGEN_DEBUG 1   // Set to 1 to print debug messages

/**
 * read_oxygen_level
 * -----------------
 * Reads oxygen sensor data from Serial2.
 * Expects lines starting with "*UV" followed by a numeric value.
 * Caches the last oxygen value for repeated queries.
 *
 * Returns:
 *   oxygen level as float
 */
float read_oxygen_level() {
  static float oxygen_value = 0.0;  // cached last value
  static String uart2Line = "";

  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\r' || c == '\n') {  // end of line
      if (uart2Line.length() > 0) {
        // Try to convert any line to float
        float val = uart2Line.toFloat();
        if (val != 0.0 || uart2Line == "0" || uart2Line == "0.0") {
          oxygen_value = val;

          //SERIAL_ECHO("Oxygen reading: ");
          //SERIAL_ECHOLN(oxygen_value);
        } else {
          //SERIAL_ECHO("Ignored line: ");
          //SERIAL_ECHOLN(uart2Line.c_str());
        }
        uart2Line = "";
      }
    } else {
      uart2Line += c;  // accumulate chars
    }
  }

  return oxygen_value;  // always return last valid reading
}

/**
 * Old formula (kept for reference)
 * --------------------------------
 * This formula assumed 511 = 0 bar and ±1 bar range.
 * It is not suitable for the XDB305 sensor that outputs ~2.5V at 0 bar.
 */
// Old Formular from Code
float read_pressure_sensor_previous()
{
  int analogValue = analogRead(PRESSURE_SENSOR_PIN);
  SERIAL_ECHOLN(analogValue);
  analogValue = constrain(analogValue, 0, 1023);  // Güvenlik sınırlandırması
  return (analogValue - 511) / 410.0;            // 511 = 0 bar, 410 = ±1 bar aralığı
}

// G-code: M621
void GcodeSuite::M621() {
  float oxygen_value = read_oxygen_level();        // cached or latest value
  float pressure_value = read_pressure_sensor(); // 0–10 bar

  // --- Optional debug to console ---
  #if ENABLED(PRESSURE_DEBUG)
    SERIAL_ECHO("Pressure debug: ");
    SERIAL_ECHOLN(pressure_value);
  #endif

  // --- Send host-readable response ---
  SERIAL_ECHOLNPGM(" O2:");
  SERIAL_ECHO_F(oxygen_value, 3);
  SERIAL_ECHOLNPGM(" P:");
  SERIAL_ECHO_F(pressure_value, 3);
}