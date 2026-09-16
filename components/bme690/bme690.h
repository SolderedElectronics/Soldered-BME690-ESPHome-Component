/**
 * @file bme690.h
 * @brief Public API for the bme690 ESPHome sensor component
 * @author Soldered Electronics
 */

#pragma once

#include <vector>

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include "bme69x.h"

namespace esphome {
namespace bme690 {

/// Over-sampling settings of the temperature, pressure and humidity measurement.
enum BME690Oversampling : uint8_t {
  BME690_OVERSAMPLING_NONE = BME69X_OS_NONE,
  BME690_OVERSAMPLING_1X = BME69X_OS_1X,
  BME690_OVERSAMPLING_2X = BME69X_OS_2X,
  BME690_OVERSAMPLING_4X = BME69X_OS_4X,
  BME690_OVERSAMPLING_8X = BME69X_OS_8X,
  BME690_OVERSAMPLING_16X = BME69X_OS_16X,
};

/// Coefficient of the IIR filter applied to the temperature and pressure readings.
enum BME690IIRFilter : uint8_t {
  BME690_IIR_FILTER_OFF = BME69X_FILTER_OFF,
  BME690_IIR_FILTER_1X = BME69X_FILTER_SIZE_1,
  BME690_IIR_FILTER_3X = BME69X_FILTER_SIZE_3,
  BME690_IIR_FILTER_7X = BME69X_FILTER_SIZE_7,
  BME690_IIR_FILTER_15X = BME69X_FILTER_SIZE_15,
  BME690_IIR_FILTER_31X = BME69X_FILTER_SIZE_31,
  BME690_IIR_FILTER_63X = BME69X_FILTER_SIZE_63,
  BME690_IIR_FILTER_127X = BME69X_FILTER_SIZE_127,
};

/// Standby duration the sensor sleeps for between two profile steps in sequential mode.
enum BME690ODR : uint8_t {
  BME690_ODR_0_59_MS = BME69X_ODR_0_59_MS,
  BME690_ODR_10_MS = BME69X_ODR_10_MS,
  BME690_ODR_20_MS = BME69X_ODR_20_MS,
  BME690_ODR_62_5_MS = BME69X_ODR_62_5_MS,
  BME690_ODR_125_MS = BME69X_ODR_125_MS,
  BME690_ODR_250_MS = BME69X_ODR_250_MS,
  BME690_ODR_500_MS = BME69X_ODR_500_MS,
  BME690_ODR_1000_MS = BME69X_ODR_1000_MS,
  BME690_ODR_NONE = BME69X_ODR_NONE,
};

/// Operation mode the sensor is run in.
enum BME690OperationMode : uint8_t {
  BME690_OPERATION_MODE_FORCED = BME69X_FORCED_MODE,
  BME690_OPERATION_MODE_PARALLEL = BME69X_PARALLEL_MODE,
  BME690_OPERATION_MODE_SEQUENTIAL = BME69X_SEQUENTIAL_MODE,
};

/// One step of the gas heater profile used in parallel and sequential mode.
struct BME690HeaterProfileStep {
  /// Heater plate temperature in degrees Celsius.
  uint16_t temperature;
  /// Heating duration in milliseconds (sequential mode) or shared duration multiplier (parallel mode).
  uint16_t duration;
  /// Optional sensor publishing the gas resistance measured during this step.
  sensor::Sensor *gas_resistance_sensor{nullptr};
  /// Last gas resistance measured during this step, in Ohm.
  float gas_resistance{NAN};
};

/**
 * @brief Soldered BME690 temperature, humidity, pressure and gas sensor breakout board.
 *
 * Thin ESPHome wrapper around the official Bosch BME69x Sensor API, which is vendored
 * unmodified in bme69x.c, bme69x.h and bme69x_defs.h.
 */
class BME690Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }
  void set_pressure_sensor(sensor::Sensor *sensor) { this->pressure_sensor_ = sensor; }
  void set_humidity_sensor(sensor::Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_gas_resistance_sensor(sensor::Sensor *sensor) { this->gas_resistance_sensor_ = sensor; }

  void set_temperature_oversampling(BME690Oversampling oversampling) { this->temperature_oversampling_ = oversampling; }
  void set_pressure_oversampling(BME690Oversampling oversampling) { this->pressure_oversampling_ = oversampling; }
  void set_humidity_oversampling(BME690Oversampling oversampling) { this->humidity_oversampling_ = oversampling; }
  void set_iir_filter(BME690IIRFilter iir_filter) { this->iir_filter_ = iir_filter; }
  void set_odr(BME690ODR odr) { this->odr_ = odr; }
  void set_operation_mode(BME690OperationMode operation_mode) { this->operation_mode_ = operation_mode; }

  void set_heater_temperature(uint16_t temperature) { this->heater_temperature_ = temperature; }
  void set_heater_duration(uint16_t duration) { this->heater_duration_ = duration; }
  void set_shared_heater_duration(uint16_t duration) { this->shared_heater_duration_ = duration; }

  /// Appends one step to the gas heater profile used in parallel and sequential mode.
  void add_heater_profile_step(uint16_t temperature, uint16_t duration);
  /// Attaches a gas resistance sensor to an already added heater profile step.
  void set_profile_gas_resistance_sensor(uint8_t index, sensor::Sensor *sensor);

 protected:
  /// Read callback handed to the Bosch API.
  static BME69X_INTF_RET_TYPE i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr);
  /// Write callback handed to the Bosch API.
  static BME69X_INTF_RET_TYPE i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr);
  /// Delay callback handed to the Bosch API.
  static void delay_us(uint32_t period_us, void *intf_ptr);
  /// Returns a brief text description of a Bosch API status code.
  static const char *status_string(int8_t status);

  /// Applies the over-sampling, filter, ODR and heater configuration to the sensor.
  bool configure_sensor_();
  /// Reads all data fields the sensor has ready and caches the values of the valid ones.
  void read_data_();
  /// Caches the values of a single data field.
  void handle_field_(const bme69x_data &field);
  /// Publishes the cached values to the sensors.
  void publish_();

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *gas_resistance_sensor_{nullptr};

  BME690Oversampling temperature_oversampling_{BME690_OVERSAMPLING_2X};
  BME690Oversampling pressure_oversampling_{BME690_OVERSAMPLING_16X};
  BME690Oversampling humidity_oversampling_{BME690_OVERSAMPLING_1X};
  BME690IIRFilter iir_filter_{BME690_IIR_FILTER_OFF};
  BME690ODR odr_{BME690_ODR_0_59_MS};
  BME690OperationMode operation_mode_{BME690_OPERATION_MODE_FORCED};

  uint16_t heater_temperature_{320};
  uint16_t heater_duration_{150};
  uint16_t shared_heater_duration_{140};
  std::vector<BME690HeaterProfileStep> heater_profile_;

  bme69x_dev dev_{};
  bme69x_conf conf_{};
  bme69x_heatr_conf heatr_conf_{};
  /// Heater temperature profile in the flat layout the Bosch API expects.
  std::vector<uint16_t> heater_temperature_profile_;
  /// Heating duration or multiplier profile in the flat layout the Bosch API expects.
  std::vector<uint16_t> heater_duration_profile_;

  float temperature_{NAN};
  float pressure_{NAN};
  float humidity_{NAN};
  float gas_resistance_{NAN};
  /// Set once a new valid data field has been read since the last publish.
  bool has_new_data_{false};
  uint32_t last_poll_{0};
};

}  // namespace bme690
}  // namespace esphome
