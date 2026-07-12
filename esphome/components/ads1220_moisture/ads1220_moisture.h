#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/sensor/sensor.h"
#include <vector>

namespace esphome {
namespace ads1220_moisture {

class MoistureChannel;

// Ratiometric constant-current front end for pin-type wood-resistance probes.
// IDAC -> node_top -[R_wood]- node_mid -[R_ref]- AGND, reference taken across R_ref,
// so the ADC code is R_wood / R_ref and the excitation current cancels out.
//
// A reading is a settle delay plus one or two ~50ms conversions. That is far too long
// to sit inside a blocking update(): all sixteen channels share one update_interval
// and fire in the same loop iteration, which would stall the main loop for seconds.
// So a channel only *queues* a request; the work is driven from loop() as a state
// machine and the answer is pushed back when it lands.
class ADS1220Moisture : public Component,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                              spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_r_ref(float r) { this->r_ref_ = r; }
  void set_idac_code(uint8_t c) { this->idac_code_ = c; }
  void set_line_freq_code(uint8_t c) { this->line_freq_code_ = c; }
  void set_mux_settle_ms(uint32_t ms) { this->mux_settle_ms_ = ms; }
  void set_drdy_pin(GPIOPin *p) { this->drdy_pin_ = p; }
  void add_mux_pin(GPIOPin *p) { this->mux_pins_.push_back(p); }
  void set_mux_enable_pin(GPIOPin *p) { this->mux_enable_ = p; }

  // Queue a channel. The result arrives via MoistureChannel::publish_result().
  void request_measurement(MoistureChannel *ch);

 protected:
  enum class State : uint8_t { IDLE, SETTLE, CONVERT1, CONVERT2 };
  enum class Conv : uint8_t { PENDING, DONE, TIMEOUT };

  void reset_chip_();
  void write_reg_(uint8_t reg, uint8_t value);
  uint8_t read_reg_(uint8_t reg);
  void send_command_(uint8_t cmd);
  int32_t read_conversion_();          // signed 24-bit, sign-extended to int32
  void configure_(uint8_t gain_code);  // REG0: AINP=AIN0, AINN=AIN1, gain, PGA on
  void start_conversion_(uint8_t gain_code);
  Conv poll_conversion_(int32_t *code);
  void select_mux_(uint8_t ch);
  void park_mux_();                    // disconnect every channel between readings
  void finish_(float r);

  float r_ref_{200000.0f};       // 0.1% reference resistor -- the calibration anchor
  uint8_t idac_code_{1};         // REG2 IDAC[2:0]: 1 = 10 uA
  uint8_t line_freq_code_{3};    // REG2 50/60[1:0]: 3 = 60 Hz rejection only
  uint32_t mux_settle_ms_{50};
  GPIOPin *drdy_pin_{nullptr};
  GPIOPin *mux_enable_{nullptr};
  std::vector<GPIOPin *> mux_pins_;

  std::vector<MoistureChannel *> queue_;
  MoistureChannel *active_{nullptr};
  State state_{State::IDLE};
  uint32_t t0_{0};              // settle timer
  uint32_t conv_start_{0};      // conversion timer
  float ratio1_{0.0f};          // gain-1 estimate, kept as the fallback
  uint8_t gain_code_{0};
  bool healthy_{false};         // register readback matched at setup
};

class MoistureChannel : public PollingComponent, public sensor::Sensor {
 public:
  explicit MoistureChannel(ADS1220Moisture *parent) : parent_(parent) {}
  void update() override;         // enqueues only; never blocks
  void publish_result(float r);   // called by the hub when the reading lands
  void dump_config() override;

  uint8_t channel() const { return this->channel_; }

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
