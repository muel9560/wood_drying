#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/sensor/sensor.h"
#include <vector>

namespace esphome {
namespace ads1220_moisture {

// Ratiometric constant-current front end for pin-type wood-resistance probes.
// IDAC -> node_top -[R_wood]- node_mid -[R_ref]- GND, ref taken across R_ref,
// so the ADC code is R_wood / R_ref and the excitation current cancels.
class ADS1220Moisture : public Component,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                              spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_r_ref(float r) { this->r_ref_ = r; }
  void set_idac_code(uint8_t c) { this->idac_code_ = c; }
  void set_mux_settle_ms(uint32_t ms) { this->mux_settle_ms_ = ms; }
  void set_drdy_pin(GPIOPin *p) { this->drdy_pin_ = p; }
  void add_mux_pin(GPIOPin *p) { this->mux_pins_.push_back(p); }
  void set_mux_enable_pin(GPIOPin *p) { this->mux_enable_ = p; }

  // Selects channel `ch` (via the external mux, if configured), auto-ranges the
  // PGA, and returns R_wood in ohms. Returns NAN if out of range / open.
  float measure_resistance(uint8_t ch);

 protected:
  void reset_chip_();
  void write_reg_(uint8_t reg, uint8_t value);
  uint8_t read_reg_(uint8_t reg);
  void send_command_(uint8_t cmd);
  int32_t read_conversion_();   // signed 24-bit, sign-extended to int32
  void configure_(uint8_t gain_code);
  bool convert_(uint8_t gain_code, float *ratio_out);  // ratio = R_wood / R_ref
  void select_mux_(uint8_t ch);

  float r_ref_{100000.0f};
  uint8_t idac_code_{1};        // default 10 µA
  uint32_t mux_settle_ms_{50};
  GPIOPin *drdy_pin_{nullptr};
  GPIOPin *mux_enable_{nullptr};
  std::vector<GPIOPin *> mux_pins_;
};

class MoistureChannel : public PollingComponent, public sensor::Sensor {
 public:
  explicit MoistureChannel(ADS1220Moisture *parent) : parent_(parent) {}
  void update() override;
  void dump_config() override;

  void set_channel(uint8_t ch) { this->channel_ = ch; }
  void set_cal(float a, float b) {
    this->cal_a_ = a;
    this->cal_b_ = b;
  }
  void set_temperature_baseline(float t) { this->temp_baseline_c_ = t; }
  void set_resistance_sensor(sensor::Sensor *s) { this->resistance_sensor_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { this->temperature_sensor_ = s; }

 protected:
  ADS1220Moisture *parent_;
  uint8_t channel_{0};
  float cal_a_{45.8f};
  float cal_b_{7.53f};
  float temp_baseline_c_{21.1f};
  sensor::Sensor *resistance_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
};

}  // namespace ads1220_moisture
}  // namespace esphome
