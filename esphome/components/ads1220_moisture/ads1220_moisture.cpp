#include "ads1220_moisture.h"
#include "esphome/core/log.h"
#include <cmath>

// DRAFT — the register values and conversion timing below follow the ADS1220
// datasheet but have NOT been validated against real hardware yet. Bench-check
// the readings against a known resistor before trusting them.

namespace esphome {
namespace ads1220_moisture {

static const char *const TAG = "ads1220_moisture";

// SPI commands
static const uint8_t CMD_RESET = 0x06;
static const uint8_t CMD_START = 0x08;
static const uint8_t CMD_RDATA = 0x10;
static const uint8_t CMD_WREG = 0x40;  // | (reg << 2) | (n - 1)
static const uint8_t CMD_RREG = 0x20;  // | (reg << 2) | (n - 1)

static const int32_t FS = 0x800000;    // 2^23 (full-scale magnitude)

void ADS1220Moisture::setup() {
  this->spi_setup();
  if (this->drdy_pin_ != nullptr)
    this->drdy_pin_->setup();
  for (auto *p : this->mux_pins_)
    p->setup();
  if (this->mux_enable_ != nullptr)
    this->mux_enable_->setup();

  this->reset_chip_();
  delay(5);
  // Static config that never changes between reads: REG1/REG2/REG3.
  // REG1 = 0x00 : 20 SPS, normal mode, single-shot, temp/burnout off.
  this->write_reg_(0x01, 0x00);
  // REG2 : VREF=01 (external REFP0/REFN0), 50/60 Hz FIR, IDAC current.
  this->write_reg_(0x02, 0x70 | (this->idac_code_ & 0x07));
  // REG3 : IDAC1 -> AIN0, IDAC2 off, DRDY on DOUT/DRDY.
  this->write_reg_(0x03, 0x20);
  this->configure_(0);  // gain = 1 to start
}

void ADS1220Moisture::dump_config() {
  ESP_LOGCONFIG(TAG, "ADS1220 Moisture AFE:");
  ESP_LOGCONFIG(TAG, "  R_ref: %.0f ohm", this->r_ref_);
  ESP_LOGCONFIG(TAG, "  IDAC code: %u", this->idac_code_);
  ESP_LOGCONFIG(TAG, "  External mux pins: %u", (unsigned) this->mux_pins_.size());
  LOG_PIN("  DRDY pin: ", this->drdy_pin_);
}

void ADS1220Moisture::reset_chip_() { this->send_command_(CMD_RESET); }

void ADS1220Moisture::send_command_(uint8_t cmd) {
  this->enable();
  this->transfer_byte(cmd);
  this->disable();
}

void ADS1220Moisture::write_reg_(uint8_t reg, uint8_t value) {
  this->enable();
  this->transfer_byte(CMD_WREG | (reg << 2));  // n - 1 = 0 (one register)
  this->transfer_byte(value);
  this->disable();
}

uint8_t ADS1220Moisture::read_reg_(uint8_t reg) {
  this->enable();
  this->transfer_byte(CMD_RREG | (reg << 2));
  uint8_t v = this->transfer_byte(0x00);
  this->disable();
  return v;
}

// REG0: MUX=0000 (AINP=AIN0, AINN=AIN1), GAIN=gain_code, PGA enabled.
void ADS1220Moisture::configure_(uint8_t gain_code) {
  this->write_reg_(0x00, (0x00 << 4) | ((gain_code & 0x07) << 1) | 0x00);
}

int32_t ADS1220Moisture::read_conversion_() {
  this->enable();
  this->transfer_byte(CMD_RDATA);
  uint8_t b0 = this->transfer_byte(0x00);
  uint8_t b1 = this->transfer_byte(0x00);
  uint8_t b2 = this->transfer_byte(0x00);
  this->disable();
  int32_t raw = ((int32_t) b0 << 16) | ((int32_t) b1 << 8) | (int32_t) b2;
  if (raw & 0x800000)
    raw |= 0xFF000000;  // sign-extend 24 -> 32 bit
  return raw;
}

// Runs one conversion at gain_code, returns ratio = R_wood / R_ref.
// Returns false if the reading rails (open probe or R_wood > R_ref).
bool ADS1220Moisture::convert_(uint8_t gain_code, float *ratio_out) {
  this->configure_(gain_code);
  this->send_command_(CMD_START);

  // Wait for DRDY (falls low when data is ready). ~50 ms at 20 SPS + FIR.
  uint32_t start = millis();
  if (this->drdy_pin_ != nullptr) {
    while (this->drdy_pin_->digital_read() && (millis() - start) < 250)
      yield();
  } else {
    delay(80);
  }

  int32_t code = this->read_conversion_();
  float norm = (float) code / (float) FS;  // fraction of full scale, signed
  if (std::fabs(norm) >= 0.98f)
    return false;  // railed
  const uint8_t gain = 1u << gain_code;
  *ratio_out = norm / (float) gain;
  return true;
}

void ADS1220Moisture::select_mux_(uint8_t ch) {
  if (this->mux_pins_.size() == 4) {
    for (uint8_t i = 0; i < 4; i++)
      this->mux_pins_[i]->digital_write((ch >> i) & 0x01);
  }
  if (this->mux_enable_ != nullptr)
    this->mux_enable_->digital_write(false);  // active-low: LOW = enabled
}

float ADS1220Moisture::measure_resistance(uint8_t ch) {
  this->select_mux_(ch);
  delay(this->mux_settle_ms_);

  // First pass at gain 1 to estimate the ratio.
  float ratio1;
  if (!this->convert_(0, &ratio1)) {
    ESP_LOGD(TAG, "CH%u railed at gain 1 (open or R_wood >= R_ref)", ch);
    return NAN;
  }
  float aratio = std::fabs(ratio1);

  // Pick the largest gain that keeps the reading near ~0.75 of full scale.
  uint8_t gain_code = 0;
  uint16_t gain = 1;
  float target = (aratio > 1e-7f) ? (0.75f / aratio) : 128.0f;
  while (gain_code < 7 && (float) (gain * 2) <= target) {
    gain *= 2;
    gain_code++;
  }

  float ratio = ratio1;
  if (gain_code > 0)
    this->convert_(gain_code, &ratio);  // if it rails, keep the gain-1 value

  float r = this->r_ref_ * std::fabs(ratio);
  ESP_LOGD(TAG, "CH%u gain=%u ratio=%.6f R=%.0f", ch, gain, ratio, r);
  return r;
}

void MoistureChannel::update() {
  float r = this->parent_->measure_resistance(this->channel_);

  if (this->resistance_sensor_ != nullptr)
    this->resistance_sensor_->publish_state(r);  // NAN on open, published as-is

  if (std::isnan(r) || r <= 0.0f) {
    this->publish_state(NAN);
    return;
  }

  float mc = this->cal_a_ - this->cal_b_ * log10f(r);

  if (this->temperature_sensor_ != nullptr && this->temperature_sensor_->has_state()) {
    float t_c = this->temperature_sensor_->state;  // expects degrees C
    mc -= ((t_c - this->temp_baseline_c_) / 5.6f) * 0.5f;
  }

  if (mc > 28.0f)
    mc = 28.0f;  // fiber-saturation ceiling (clamp last)
  if (mc < 6.0f)
    mc = 6.0f;  // dry floor
  this->publish_state(mc);
}

void MoistureChannel::dump_config() {
  LOG_SENSOR("", "ADS1220 Moisture Channel", this);
  ESP_LOGCONFIG(TAG, "  Channel: %u  cal_a=%.3f cal_b=%.4f", this->channel_, this->cal_a_,
                this->cal_b_);
}

}  // namespace ads1220_moisture
}  // namespace esphome
