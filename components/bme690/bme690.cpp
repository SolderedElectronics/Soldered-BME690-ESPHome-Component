/**
 * @file bme690.cpp
 * @brief Implementation for the bme690 ESPHome sensor component
 * @author Soldered Electronics
 */

#include "bme690.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bme690 {

static const char *const TAG = "bme690";

/// Interval the data registers are polled at in parallel and sequential mode.
static const uint32_t POLL_INTERVAL_MS = 100;

/// Status mask a data field has to match for its gas reading to be usable.
static const uint8_t GAS_VALID_MASK = BME69X_GASM_VALID_MSK | BME69X_HEAT_STAB_MSK;

/// Maximum number of data fields the sensor buffers.
static const uint8_t MAX_FIELDS = 3;

void BME690Component::add_heater_profile_step(uint16_t temperature, uint16_t duration) {
  BME690HeaterProfileStep step;
  step.temperature = temperature;
  step.duration = duration;
  this->heater_profile_.push_back(step);
}

void BME690Component::set_profile_gas_resistance_sensor(uint8_t index, sensor::Sensor *sensor) {
  if (index >= this->heater_profile_.size())
    return;
  this->heater_profile_[index].gas_resistance_sensor = sensor;
}

void BME690Component::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");

  this->dev_.intf = BME69X_I2C_INTF;
  this->dev_.read = BME690Component::i2c_read;
  this->dev_.write = BME690Component::i2c_write;
  this->dev_.delay_us = BME690Component::delay_us;
  this->dev_.intf_ptr = this;
  // Typical room temperature in degrees Celsius, used to refine the heater resistance.
  this->dev_.amb_temp = 25;

  int8_t status = bme69x_init(&this->dev_);
  if (status != BME69X_OK) {
    ESP_LOGE(TAG, "Initialization failed: %s", BME690Component::status_string(status));
    this->mark_failed();
    return;
  }

  if (!this->configure_sensor_()) {
    this->mark_failed();
    return;
  }
}

bool BME690Component::configure_sensor_() {
  int8_t status = bme69x_get_conf(&this->conf_, &this->dev_);
  if (status != BME69X_OK) {
    ESP_LOGE(TAG, "Reading the configuration failed: %s", BME690Component::status_string(status));
    return false;
  }

  this->conf_.os_temp = this->temperature_oversampling_;
  this->conf_.os_pres = this->pressure_oversampling_;
  this->conf_.os_hum = this->humidity_oversampling_;
  this->conf_.filter = this->iir_filter_;
  this->conf_.odr = this->odr_;

  status = bme69x_set_conf(&this->conf_, &this->dev_);
  if (status != BME69X_OK) {
    ESP_LOGE(TAG, "Writing the configuration failed: %s", BME690Component::status_string(status));
    return false;
  }

  this->heatr_conf_ = {};
  this->heatr_conf_.enable = BME69X_ENABLE;

  if (this->operation_mode_ == BME690_OPERATION_MODE_FORCED) {
    this->heatr_conf_.heatr_temp = this->heater_temperature_;
    this->heatr_conf_.heatr_dur = this->heater_duration_;
  } else {
    this->heater_temperature_profile_.clear();
    this->heater_duration_profile_.clear();
    for (auto &step : this->heater_profile_) {
      this->heater_temperature_profile_.push_back(step.temperature);
      this->heater_duration_profile_.push_back(step.duration);
    }

    this->heatr_conf_.heatr_temp_prof = this->heater_temperature_profile_.data();
    this->heatr_conf_.heatr_dur_prof = this->heater_duration_profile_.data();
    this->heatr_conf_.profile_len = this->heater_profile_.size();

    if (this->operation_mode_ == BME690_OPERATION_MODE_PARALLEL) {
      // The shared heating duration is the duration of one profile step minus the time
      // the temperature, pressure and humidity measurement itself takes.
      uint32_t meas_dur_ms = bme69x_get_meas_dur(BME69X_PARALLEL_MODE, &this->conf_, &this->dev_) / 1000;
      if (this->shared_heater_duration_ <= meas_dur_ms) {
        ESP_LOGE(TAG, "Shared heater duration of %ums is shorter than the %ums measurement takes",
                 (unsigned) this->shared_heater_duration_, (unsigned) meas_dur_ms);
        return false;
      }
      this->heatr_conf_.shared_heatr_dur = this->shared_heater_duration_ - meas_dur_ms;
    }
  }

  status = bme69x_set_heatr_conf(this->operation_mode_, &this->heatr_conf_, &this->dev_);
  if (status != BME69X_OK) {
    ESP_LOGE(TAG, "Writing the heater configuration failed: %s", BME690Component::status_string(status));
    return false;
  }

  // Forced mode is triggered once per update, the continuous modes are started here and
  // keep measuring on their own.
  if (this->operation_mode_ != BME690_OPERATION_MODE_FORCED) {
    status = bme69x_set_op_mode(this->operation_mode_, &this->dev_);
    if (status != BME69X_OK) {
      ESP_LOGE(TAG, "Setting the operation mode failed: %s", BME690Component::status_string(status));
      return false;
    }
  }

  return true;
}

void BME690Component::loop() {
  // In forced mode the measurement is triggered by update() instead.
  if (this->operation_mode_ == BME690_OPERATION_MODE_FORCED || this->is_failed())
    return;

  const uint32_t now = millis();
  if (now - this->last_poll_ < POLL_INTERVAL_MS)
    return;
  this->last_poll_ = now;

  this->read_data_();
}

void BME690Component::update() {
  if (this->is_failed())
    return;

  if (this->operation_mode_ != BME690_OPERATION_MODE_FORCED) {
    // The continuous modes are read out in loop(), only publish what has been collected.
    this->publish_();
    return;
  }

  int8_t status = bme69x_set_op_mode(BME69X_FORCED_MODE, &this->dev_);
  if (status != BME69X_OK) {
    ESP_LOGW(TAG, "Triggering a measurement failed: %s", BME690Component::status_string(status));
    this->status_set_warning();
    return;
  }

  // Wait for the temperature, pressure and humidity measurement and the gas heater to finish.
  uint32_t meas_dur_us = bme69x_get_meas_dur(BME69X_FORCED_MODE, &this->conf_, &this->dev_);
  uint32_t delay_ms = meas_dur_us / 1000 + this->heater_duration_ + 1;

  this->set_timeout("read", delay_ms, [this]() {
    this->read_data_();
    this->publish_();
  });
}

void BME690Component::read_data_() {
  bme69x_data fields[MAX_FIELDS];
  uint8_t n_fields = 0;

  int8_t status = bme69x_get_data(this->operation_mode_, fields, &n_fields, &this->dev_);
  if (status < BME69X_OK) {
    ESP_LOGW(TAG, "Reading the data failed: %s", BME690Component::status_string(status));
    this->status_set_warning();
    return;
  }

  for (uint8_t i = 0; i < n_fields; i++)
    this->handle_field_(fields[i]);
}

void BME690Component::handle_field_(const bme69x_data &field) {
  if ((field.status & BME69X_NEW_DATA_MSK) == 0)
    return;

  this->temperature_ = field.temperature;
  this->pressure_ = field.pressure / 100.0f;
  this->humidity_ = field.humidity;
  this->has_new_data_ = true;

  // The gas reading is only meaningful once the heater has settled on its target temperature.
  if ((field.status & GAS_VALID_MASK) != GAS_VALID_MASK)
    return;

  this->gas_resistance_ = field.gas_resistance;
  if (field.gas_index < this->heater_profile_.size())
    this->heater_profile_[field.gas_index].gas_resistance = field.gas_resistance;
}

void BME690Component::publish_() {
  if (!this->has_new_data_) {
    ESP_LOGW(TAG, "No new data available");
    this->status_set_warning();
    return;
  }
  this->has_new_data_ = false;

  ESP_LOGD(TAG, "Got temperature=%.2f°C pressure=%.2fhPa humidity=%.2f%% gas_resistance=%.0fΩ", this->temperature_,
           this->pressure_, this->humidity_, this->gas_resistance_);

  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(this->temperature_);
  if (this->pressure_sensor_ != nullptr)
    this->pressure_sensor_->publish_state(this->pressure_);
  if (this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(this->humidity_);
  if (this->gas_resistance_sensor_ != nullptr)
    this->gas_resistance_sensor_->publish_state(this->gas_resistance_);

  for (auto &step : this->heater_profile_) {
    if (step.gas_resistance_sensor != nullptr)
      step.gas_resistance_sensor->publish_state(step.gas_resistance);
  }

  this->status_clear_warning();
}

BME69X_INTF_RET_TYPE BME690Component::i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr) {
  auto *component = static_cast<BME690Component *>(intf_ptr);
  if (component == nullptr)
    return BME69X_E_NULL_PTR;

  if (component->read_register(reg_addr, reg_data, length) != i2c::ERROR_OK)
    return BME69X_E_COM_FAIL;

  return BME69X_INTF_RET_SUCCESS;
}

BME69X_INTF_RET_TYPE BME690Component::i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length,
                                                void *intf_ptr) {
  auto *component = static_cast<BME690Component *>(intf_ptr);
  if (component == nullptr)
    return BME69X_E_NULL_PTR;

  if (component->write_register(reg_addr, reg_data, length) != i2c::ERROR_OK)
    return BME69X_E_COM_FAIL;

  return BME69X_INTF_RET_SUCCESS;
}

void BME690Component::delay_us(uint32_t period_us, void *intf_ptr) {
  (void) intf_ptr;
  delay_microseconds_safe(period_us);
}

const char *BME690Component::status_string(int8_t status) {
  switch (status) {
    case BME69X_OK:
      return "OK";
    case BME69X_E_NULL_PTR:
      return "Null pointer";
    case BME69X_E_COM_FAIL:
      return "Communication failure";
    case BME69X_E_DEV_NOT_FOUND:
      return "Sensor not found";
    case BME69X_E_INVALID_LENGTH:
      return "Invalid length";
    case BME69X_E_SELF_TEST:
      return "Self test failed";
    case BME69X_W_DEFINE_OP_MODE:
      return "Set the operation mode";
    case BME69X_W_NO_NEW_DATA:
      return "No new data";
    case BME69X_W_DEFINE_SHD_HEATR_DUR:
      return "Set the shared heater duration";
    default:
      return "Undefined error code";
  }
}

void BME690Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BME690:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with BME690 failed!");
    return;
  }
  LOG_UPDATE_INTERVAL(this);

  const char *operation_mode = "forced";
  if (this->operation_mode_ == BME690_OPERATION_MODE_PARALLEL) {
    operation_mode = "parallel";
  } else if (this->operation_mode_ == BME690_OPERATION_MODE_SEQUENTIAL) {
    operation_mode = "sequential";
  }
  ESP_LOGCONFIG(TAG,
                "  Operation mode: %s\n"
                "  IIR Filter: %u\n"
                "  Temperature oversampling: %u\n"
                "  Pressure oversampling: %u\n"
                "  Humidity oversampling: %u",
                operation_mode, (unsigned) this->iir_filter_, (unsigned) this->temperature_oversampling_,
                (unsigned) this->pressure_oversampling_, (unsigned) this->humidity_oversampling_);

  if (this->operation_mode_ == BME690_OPERATION_MODE_FORCED) {
    ESP_LOGCONFIG(TAG,
                  "  Heater temperature: %u°C\n"
                  "  Heater duration: %ums",
                  (unsigned) this->heater_temperature_, (unsigned) this->heater_duration_);
  } else {
    if (this->operation_mode_ == BME690_OPERATION_MODE_SEQUENTIAL) {
      ESP_LOGCONFIG(TAG, "  ODR: %u", (unsigned) this->odr_);
    } else {
      ESP_LOGCONFIG(TAG, "  Shared heater duration: %ums", (unsigned) this->shared_heater_duration_);
    }
    ESP_LOGCONFIG(TAG, "  Heater profile:");
    for (size_t i = 0; i < this->heater_profile_.size(); i++) {
      ESP_LOGCONFIG(TAG, "    Step %u: %u°C, %u", (unsigned) i, (unsigned) this->heater_profile_[i].temperature,
                    (unsigned) this->heater_profile_[i].duration);
    }
  }

  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Gas resistance", this->gas_resistance_sensor_);
  for (auto &step : this->heater_profile_)
    LOG_SENSOR("  ", "Gas resistance", step.gas_resistance_sensor);
}

}  // namespace bme690
}  // namespace esphome
