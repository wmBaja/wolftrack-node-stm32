#pragma once

#include <cstddef>
#include <cstdint>

#include "config.h"

namespace app {

enum class GroupValidationError : std::uint8_t {
  kNone,
  kNoSensors,
  kNullContext,
  kPayloadTooLarge,
};

struct GroupPayloadInfo {
  GroupValidationError error;
  std::uint8_t payload_bytes;

  constexpr bool valid() const { return error == GroupValidationError::kNone; }
};

GroupPayloadInfo validate_group_payload(const MessageGroupConfig &group);

// Returns false when raw_length cannot be represented by a CAN FD frame.
bool can_fd_padded_length(std::size_t raw_length, std::uint8_t *padded_length);

// Rounds a payload to a legal CAN FD length and clears the added bytes.
bool can_fd_zero_pad(std::uint8_t *payload, std::size_t raw_length,
                     std::uint8_t *padded_length);

std::uint32_t stagger_offset_ms(std::uint16_t interval_ms,
                                std::size_t active_index,
                                std::size_t active_count);
bool tick_reached(std::uint32_t now, std::uint32_t deadline);
std::uint32_t next_poll_deadline(std::uint32_t now,
                                 std::uint32_t scheduled_deadline,
                                 std::uint32_t interval_ms);

struct CanFrameView {
  std::uint32_t id;
  bool extended;
  const std::uint8_t *data;
  std::uint8_t length;
};

bool matches_sleep_control(const CanFrameView &frame,
                           const ControlMessageConfig &control,
                           bool use_extended_ids);

constexpr std::size_t kAds131M08ChannelCount = 8U;
constexpr std::size_t kAds131M08FrameBytes = 30U;

struct Ads131M08Status {
  bool locked;
  bool fast_resync;
  bool register_map_crc_error;
  bool input_crc_error;
  bool crc_is_ansi;
  bool reset_occurred;
  std::uint8_t word_length;
  std::uint8_t data_ready_mask;
};

struct Ads131M08Frame {
  Ads131M08Status status;
  std::int32_t channels[kAds131M08ChannelCount];
};

std::int32_t sign_extend_24(std::uint32_t value);
Ads131M08Status decode_ads131m08_status(std::uint16_t status_word);
std::uint16_t ads131m08_crc_ccitt(const std::uint8_t *data,
                                  std::size_t length);
bool parse_ads131m08_frame(const std::uint8_t *frame, std::size_t length,
                           Ads131M08Frame *parsed);

class SampleAccumulator {
 public:
  void add(std::int32_t sample);
  bool take_mean_and_reset(std::int32_t *mean);
  std::uint32_t count() const { return count_; }

 private:
  std::int64_t sum_ = 0;
  std::uint32_t count_ = 0;
};

}  // namespace app
