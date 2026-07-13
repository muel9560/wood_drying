#include "ads1220_moisture.h"
#include "esphome/core/log.h"
#include <cmath>

// Register values follow the ADS1220 datasheet (SBAS501). setup() reads them back and
// refuses to run if they do not stick -- a mis-wired SPI bus shows up as a loud log
// line instead of plausible-looking garbage.
//
// STILL NOT VALIDATED AGAINST A REAL CHIP. Bench-check against 0.1% resistors before
// trusting a reading. See BENCH.md.

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
static const float RAIL = 0.98f;       // treat >=98% of full scale as railed

// 20 SPS + the 50/60 Hz FIR takes ~50 ms. Allow generous slack, then give up rather
// than read a stale conversion.
static const uint32_t CONV_TIMEOUT_MS = 250;
static const uint32_t CONV_NO_DRDY_MS = 80;   // fixed wait when no DRDY pin is wired

void ADS1220Moisture::setup() {
  this->spi_setup();
  if (this->drdy_pin_ != nullptr)
    this->drdy_pin_->setup();
  for (auto *p : this->mux_pins_)
    p->setup();
  if (this->mux_enable_ != nullptr)
    this->mux_enable_->setup();
  this->park_mux_();

  this->reset_chip_();
  delay(5);

  // REG1 = 0x00 : 20 SPS, normal mode, single-shot, temp sensor and burnout off.
  const uint8_t reg1 = 0x00;
  // REG2 : VREF=01b (external REFP0/REFN0), 50/60 rejection, PSW=0, IDAC current.
  const uint8_t reg2 = 0x40 | ((this->line_freq_code_ & 0x03) << 4) | (this->idac_code_ & 0x07);
  // REG3 : I1MUX=001b (IDAC1 -> AIN0), I2MUX=000b (IDAC2 off), DRDYM=0 (data-ready on
  // the dedicated DRDY pin only).
  const uint8_t reg3 = 0x20;

  this->write_reg_(0x01, reg1);
  this->write_reg_(0x02, reg2);
  // The datasheet requires the IDAC *current* to be programmed before its routing, and
  // gives the IDACs up to 200us to start up. Give them that before setting I1MUX.
  delay(1);
  this->write_reg_(0x03, reg3);
  this->configure_(0);  // gain = 1

  // Read the registers back. If SPI is mis-wired (or MISO is floating) this catches it
  // immediately, instead of producing readings that look plausible but are noise.
  const uint8_t want[4] = {0x00, reg1, reg2, reg3};
  this->healthy_ = true;
  for (uint8_t r = 0; r < 4; r++) {
    uint8_t got = this->read_reg_(r);
    if (got != want[r]) {
      ESP_LOGE(TAG, "REG%u readback 0x%02X, expected 0x%02X", r, got, want[r]);
      this->healthy_ = false;
    }
  }
  if (!this->healthy_) {
    ESP_LOGE(TAG, "ADS1220 did not accept its configuration - check SPI wiring, CS, and "
                  "that the CLK pin is tied to DGND.");
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "ADS1220 configured (REG1=0x%02X REG2=0x%02X REG3=0x%02X)", reg1, reg2, reg3);
}

void ADS1220Moisture::dump_config() {
  ESP_LOGCONFIG(TAG, "ADS1220 Moisture AFE:");
  ESP_LOGCONFIG(TAG, "  R_ref: %.0f ohm", this->r_ref_);
  ESP_LOGCONFIG(TAG, "  IDAC code: %u", this->idac_code_);
  ESP_LOGCONFIG(TAG, "  Line-freq rejection code: %u", this->line_freq_code_);
  ESP_LOGCONFIG(TAG, "  External mux pins: %u", (unsigned) this->mux_pins_.size());
  ESP_LOGCONFIG(TAG, "  Mux settle: %u ms", (unsigned) this->mux_settle_ms_);
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

// REG0: MUX=0000b (AINP=AIN0, AINN=AIN1), GAIN=gain_code, PGA_BYPASS=0 (PGA enabled).
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

void ADS1220Moisture::start_conversion_(uint8_t gain_code) {
  this->configure_(gain_code);
  this->send_command_(CMD_START);
  this->conv_start_ = millis();
}

// Non-blocking. Reading the result also releases DRDY back high, which is what arms
// the next conversion -- so every path that sees DONE must consume the code.
ADS1220Moisture::Conv ADS1220Moisture::poll_conversion_(int32_t *code) {
  if (this->drdy_pin_ != nullptr) {
    if (this->drdy_pin_->digital_read()) {  // DRDY is active LOW
      if (millis() - this->conv_start_ > CONV_TIMEOUT_MS)
        return Conv::TIMEOUT;
      return Conv::PENDING;
    }
  } else if (millis() - this->conv_start_ < CONV_NO_DRDY_MS) {
    return Conv::PENDING;
  }
  *code = this->read_conversion_();
  return Conv::DONE;
}

void ADS1220Moisture::select_mux_(uint8_t ch) {
  if (this->mux_pins_.size() == 4) {
    for (uint8_t i = 0; i < 4; i++)
      this->mux_pins_[i]->digital_write((ch >> i) & 0x01);
  }
  if (this->mux_enable_ != nullptr)
    this->mux_enable_->digital_write(false);  // active-low: LOW = enabled
}

// Between readings, disconnect every channel. Otherwise the last-selected probe carries
// the 10 uA IDAC continuously (the IDACs stay alive between single-shot conversions),
// which DC-polarises the electrodes for the whole update_interval.
void ADS1220Moisture::park_mux_() {
  if (this->mux_enable_ != nullptr)
    this->mux_enable_->digital_write(true);  // HIGH = all channels off
}

void ADS1220Moisture::request_measurement(MoistureChannel *ch) {
  if (this->is_failed())
    return;
  if (this->active_ == ch)
    return;
  for (auto *q : this->queue_) {
    if (q == ch) {
      ESP_LOGW(TAG, "CH%u still queued from the last cycle; measurements are falling "
                    "behind the update_interval", ch->channel());
      return;
    }
  }
  this->queue_.push_back(ch);
}

void ADS1220Moisture::finish_(float r) {
  this->park_mux_();
  if (this->active_ != nullptr) {
    this->active_->publish_result(r);
    this->active_ = nullptr;
  }
  this->state_ = State::IDLE;
}

void ADS1220Moisture::loop() {
  switch (this->state_) {
    case State::IDLE: {
      if (this->queue_.empty())
        return;
      this->active_ = this->queue_.front();
      this->queue_.erase(this->queue_.begin());
      this->select_mux_(this->active_->channel());
      this->t0_ = millis();
      this->state_ = State::SETTLE;
      return;
    }

    case State::SETTLE: {
      if (millis() - this->t0_ < this->mux_settle_ms_)
        return;
      this->start_conversion_(0);  // gain 1: estimate the ratio
      this->state_ = State::CONVERT1;
      return;
    }

    case State::CONVERT1: {
      int32_t code;
      Conv c = this->poll_conversion_(&code);
      if (c == Conv::PENDING)
        return;
      if (c == Conv::TIMEOUT) {
        ESP_LOGW(TAG, "CH%u: DRDY never asserted", this->active_->channel());
        this->finish_(NAN);
        return;
      }

      float norm = (float) code / (float) FS;  // at gain 1, norm == R_wood / R_ref
      if (std::fabs(norm) >= RAIL) {
        ESP_LOGD(TAG, "CH%u railed at gain 1 (open probe, or R_wood >= R_ref)",
                 this->active_->channel());
        this->finish_(NAN);
        return;
      }
      this->ratio1_ = norm;

      // Largest gain that still keeps the reading near ~0.75 of full scale.
      float aratio = std::fabs(this->ratio1_);
      this->gain_code_ = 0;
      uint16_t gain = 1;
      float target = (aratio > 1e-7f) ? (0.75f / aratio) : 128.0f;
      while (this->gain_code_ < 7 && (float) (gain * 2) <= target) {
        gain *= 2;
        this->gain_code_++;
      }

      // Already near full scale, so there is nothing to gain by re-reading: the gain-1
      // conversion IS the answer. Log it the same way CONVERT2 does -- an unlogged
      // success path is indistinguishable from a hang when you are watching the log.
      if (this->gain_code_ == 0) {
        float r = this->r_ref_ * aratio;
        ESP_LOGD(TAG, "CH%u gain=1 ratio=%.6f R=%.0f", this->active_->channel(), this->ratio1_, r);
        this->finish_(r);
        return;
      }
      this->start_conversion_(this->gain_code_);
      this->state_ = State::CONVERT2;
      return;
    }

    case State::CONVERT2: {
      int32_t code;
      Conv c = this->poll_conversion_(&code);
      if (c == Conv::PENDING)
        return;

      float ratio = this->ratio1_;  // fall back to the gain-1 reading
      if (c == Conv::DONE) {
        float norm = (float) code / (float) FS;
        if (std::fabs(norm) < RAIL)
          ratio = norm / (float) (1u << this->gain_code_);
      } else {
        ESP_LOGW(TAG, "CH%u: DRDY never asserted at gain %u; using the gain-1 reading",
                 this->active_->channel(), 1u << this->gain_code_);
      }

      float r = this->r_ref_ * std::fabs(ratio);
      ESP_LOGD(TAG, "CH%u gain=%u ratio=%.6f R=%.0f", this->active_->channel(),
               1u << this->gain_code_, ratio, r);
      this->finish_(r);
      return;
    }
  }
}

void MoistureChannel::update() { this->parent_->request_measurement(this); }

void MoistureChannel::publish_result(float r) {
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
    mc = 28.0f;  // fibre-saturation ceiling (clamp last)
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
