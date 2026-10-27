#include "app_logic.h"

#include <cstring>
#include <limits>

namespace app {
namespace {

constexpr std::uint8_t kCanFdLengths[] = {0U, 8U, 12U, 16U, 20U, 24U,
                                            32U, 48U, 64U};

std::uint32_t read_be24(const std::uint8_t *data) {
  return (static_cast<std::uint32_t>(data[0]) << 16U) |
         (static_cast<std::uint32_t>(data[1]) << 8U) |
         static_cast<std::uint32_t>(data[2]);
}

}  // namespace

GroupPayloadInfo validate_group_payload(const MessageGroupConfig &group) {
  if (group.sensors == nullptr || group.sensor_count == 0U) {
    return {GroupValidationError::kNoSensors, 0U};
  }

  std::size_t total = 0U;
  for (std::size_t index = 0U; index < group.sensor_count; ++index) {
    const SensorContext *const context = group.sensors[index].context;
    if (context == nullptr) {
      return {GroupValidationError::kNullContext, 0U};
    }
    total += context->payload_size;
    if (total > kMaxCanFdPayloadBytes) {
      return {GroupValidationError::kPayloadTooLarge, 0U};
    }
  }

  return {GroupValidationError::kNone, static_cast<std::uint8_t>(total)};
}

bool can_fd_padded_length(const std::size_t raw_length,
                          std::uint8_t *const padded_length) {
  if (padded_length == nullptr) {
    return false;
  }
  for (const std::uint8_t valid_length : kCanFdLengths) {
    if (raw_length <= valid_length) {
      *padded_length = valid_length;
      return true;
    }
  }
  return false;
}

bool can_fd_zero_pad(std::uint8_t *const payload, const std::size_t raw_length,
                     std::uint8_t *const padded_length) {
  std::uint8_t rounded_length = 0U;
  if (!can_fd_padded_length(raw_length, &rounded_length) ||
      (payload == nullptr && rounded_length != 0U)) {
    return false;
  }
  if (rounded_length > raw_length) {
    std::memset(payload + raw_length, 0,
                static_cast<std::size_t>(rounded_length) - raw_length);
  }
  *padded_length = rounded_length;
  return true;
}

std::uint32_t stagger_offset_ms(const std::uint16_t interval_ms,
                                const std::size_t active_index,
                                const std::size_t active_count) {
  if (interval_ms == 0U || active_count <= 1U || active_index == 0U) {
    return 0U;
  }
  return static_cast<std::uint32_t>(
      (static_cast<std::uint64_t>(interval_ms) * active_index) / active_count);
}

bool tick_reached(const std::uint32_t now, const std::uint32_t deadline) {
  return static_cast<std::int32_t>(now - deadline) >= 0;
}

std::uint32_t next_poll_deadline(const std::uint32_t now,
                                 const std::uint32_t scheduled_deadline,
                                 const std::uint32_t interval_ms) {
  const std::uint32_t next = scheduled_deadline + interval_ms;
  return tick_reached(now, next) ? now + interval_ms : next;
}

bool matches_sleep_control(const CanFrameView &frame,
                           const ControlMessageConfig &control,
                           const bool use_extended_ids) {
  return frame.extended == use_extended_ids &&
         frame.id == control.sleep_command_id &&
         control.command_byte_index < frame.length && frame.data != nullptr &&
         frame.data[control.command_byte_index] == control.sleep_command_byte;
}

std::int32_t sign_extend_24(const std::uint32_t value) {
  const std::uint32_t masked = value & 0x00FFFFFFUL;
  return static_cast<std::int32_t>((masked ^ 0x00800000UL) - 0x00800000UL);
}

Ads131M08Status decode_ads131m08_status(const std::uint16_t status_word) {
  return {
      (status_word & (1U << 15U)) != 0U,
      (status_word & (1U << 14U)) != 0U,
      (status_word & (1U << 13U)) != 0U,
      (status_word & (1U << 12U)) != 0U,
      (status_word & (1U << 11U)) != 0U,
      (status_word & (1U << 10U)) != 0U,
      static_cast<std::uint8_t>((status_word >> 8U) & 0x03U),
      static_cast<std::uint8_t>(status_word & 0x00FFU),
  };
}

std::uint16_t ads131m08_crc_ccitt(const std::uint8_t *const data,
                                  const std::size_t length) {
  if (data == nullptr && length != 0U) {
    return 0U;
  }

  std::uint16_t crc = 0xFFFFU;
  for (std::size_t index = 0U; index < length; ++index) {
    crc ^= static_cast<std::uint16_t>(data[index]) << 8U;
    for (std::uint8_t bit = 0U; bit < 8U; ++bit) {
      crc = (crc & 0x8000U) != 0U
                ? static_cast<std::uint16_t>((crc << 1U) ^ 0x1021U)
                : static_cast<std::uint16_t>(crc << 1U);
    }
  }
  return crc;
}

bool parse_ads131m08_frame(const std::uint8_t *const frame,
                           const std::size_t length,
                           Ads131M08Frame *const parsed) {
  if (frame == nullptr || parsed == nullptr || length != kAds131M08FrameBytes ||
      frame[2] != 0U || frame[29] != 0U) {
    return false;
  }

  const std::uint16_t received_crc =
      static_cast<std::uint16_t>((static_cast<std::uint16_t>(frame[27]) << 8U) |
                                 frame[28]);
  if (ads131m08_crc_ccitt(frame, 27U) != received_crc) {
    return false;
  }

  parsed->status = decode_ads131m08_status(
      static_cast<std::uint16_t>((static_cast<std::uint16_t>(frame[0]) << 8U) |
                                 frame[1]));
  for (std::size_t channel = 0U; channel < kAds131M08ChannelCount; ++channel) {
    parsed->channels[channel] =
        sign_extend_24(read_be24(&frame[3U + channel * 3U]));
  }
  return true;
}

void SampleAccumulator::add(const std::int32_t sample) {
  if (count_ == std::numeric_limits<std::uint32_t>::max()) {
    return;
  }
  sum_ += sample;
  ++count_;
}

bool SampleAccumulator::take_mean_and_reset(std::int32_t *const mean) {
  if (mean == nullptr || count_ == 0U) {
    return false;
  }
  *mean = static_cast<std::int32_t>(sum_ / static_cast<std::int64_t>(count_));
  sum_ = 0;
  count_ = 0U;
  return true;
}

}  // namespace app
