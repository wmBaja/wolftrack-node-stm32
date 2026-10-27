#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>

#include "app_logic.h"

namespace {

constexpr app::SensorContext kOneByte{"one", 1U};
constexpr app::SensorContext kSixtyFourBytes{"sixty-four", 64U};
constexpr app::SensorContext kSixtyFiveBytes{"sixty-five", 65U};
constexpr app::SensorDescriptor kOneSensor{&kOneByte, nullptr, nullptr, nullptr,
                                            nullptr};
constexpr app::SensorDescriptor kSixtyFourSensor{&kSixtyFourBytes, nullptr, nullptr,
                                                  nullptr, nullptr};
constexpr app::SensorDescriptor kSixtyFiveSensor{&kSixtyFiveBytes, nullptr, nullptr,
                                                  nullptr, nullptr};
constexpr app::SensorDescriptor kNullContextSensor{nullptr, nullptr, nullptr, nullptr,
                                                    nullptr};

app::MessageGroupConfig group(const app::SensorDescriptor *sensors,
                              const std::size_t count) {
  return {"test", 0U, 100U, sensors, count};
}

void finalize_frame(std::uint8_t *frame) {
  const std::uint16_t crc = app::ads131m08_crc_ccitt(frame, 27U);
  frame[27] = static_cast<std::uint8_t>(crc >> 8U);
  frame[28] = static_cast<std::uint8_t>(crc);
  frame[29] = 0U;
}

void test_group_validation() {
  assert(app::validate_group_payload(group(nullptr, 0U)).error ==
         app::GroupValidationError::kNoSensors);
  assert(app::validate_group_payload(group(&kNullContextSensor, 1U)).error ==
         app::GroupValidationError::kNullContext);

  const app::GroupPayloadInfo exactly_64 =
      app::validate_group_payload(group(&kSixtyFourSensor, 1U));
  assert(exactly_64.valid());
  assert(exactly_64.payload_bytes == 64U);
  assert(app::validate_group_payload(group(&kSixtyFiveSensor, 1U)).error ==
         app::GroupValidationError::kPayloadTooLarge);
}

void test_can_fd_lengths() {
  std::uint8_t length = 0U;
  for (const std::uint8_t valid : {0U, 1U, 8U, 9U, 12U, 13U, 16U, 17U, 20U,
                                   21U, 24U, 25U, 32U, 33U, 48U, 49U, 64U}) {
    assert(app::can_fd_padded_length(valid, &length));
    assert(length == (valid <= 8U   ? (valid == 0U ? 0U : 8U)
                      : valid <= 12U ? 12U
                      : valid <= 16U ? 16U
                      : valid <= 20U ? 20U
                      : valid <= 24U ? 24U
                      : valid <= 32U ? 32U
                      : valid <= 48U ? 48U
                                     : 64U));
  }
  assert(!app::can_fd_padded_length(65U, &length));

  std::uint8_t payload[64];
  std::memset(payload, 0xA5, sizeof(payload));
  assert(app::can_fd_zero_pad(payload, 9U, &length));
  assert(length == 12U);
  assert(payload[8] == 0xA5U);
  assert(payload[9] == 0U && payload[10] == 0U && payload[11] == 0U);
}

void test_schedule_logic() {
  assert(app::stagger_offset_ms(1000U, 0U, 3U) == 0U);
  assert(app::stagger_offset_ms(1000U, 1U, 3U) == 333U);
  assert(app::stagger_offset_ms(1000U, 2U, 3U) == 666U);
  assert(app::stagger_offset_ms(1000U, 0U, 0U) == 0U);

  assert(app::tick_reached(0x00000010U, 0xFFFFFFF0U));
  assert(!app::tick_reached(0xFFFFFFF0U, 0x00000010U));
  assert(app::next_poll_deadline(0x00000010U, 0xFFFFFFF0U, 20U) ==
         0x00000024U);
  assert(app::next_poll_deadline(120U, 100U, 50U) == 150U);
  assert(app::next_poll_deadline(160U, 100U, 50U) == 210U);
}

void test_control_matching() {
  const app::ControlMessageConfig control{0x1ABCDEU, 0x5AU, 1U};
  const std::uint8_t data[] = {0x00U, 0x5AU};
  const app::CanFrameView match{0x1ABCDEU, true, data, 2U};
  assert(app::matches_sleep_control(match, control, true));
  assert(!app::matches_sleep_control({0x1ABCDEU, false, data, 2U}, control,
                                     true));
  assert(!app::matches_sleep_control({0x1ABCDFU, true, data, 2U}, control,
                                     true));
  assert(!app::matches_sleep_control({0x1ABCDEU, true, data, 1U}, control,
                                     true));
  const std::uint8_t wrong_byte[] = {0x00U, 0x00U};
  assert(!app::matches_sleep_control({0x1ABCDEU, true, wrong_byte, 2U}, control,
                                     true));
}

void test_ads131m08_frame() {
  assert(app::sign_extend_24(0x800000U) == -8388608);
  assert(app::sign_extend_24(0x7FFFFFU) == 8388607);
  assert(app::sign_extend_24(0xFFFFFFU) == -1);
  assert(app::ads131m08_crc_ccitt(
             reinterpret_cast<const std::uint8_t *>("123456789"), 9U) ==
         0x29B1U);

  std::uint8_t frame[app::kAds131M08FrameBytes] = {};
  frame[0] = 0x25U;  // REG_MAP, RESET, 24-bit words, DRDY0 and DRDY2.
  frame[1] = 0x05U;
  const std::uint8_t channel_data[app::kAds131M08ChannelCount][3] = {
      {0x80U, 0x00U, 0x00U}, {0x7FU, 0xFFU, 0xFFU}, {0xFFU, 0xFFU, 0xFFU},
      {0x00U, 0x00U, 0x00U}, {0x00U, 0x00U, 0x01U}, {0x12U, 0x34U, 0x56U},
      {0xEDU, 0xCBU, 0xAAU}, {0x00U, 0x00U, 0x02U},
  };
  for (std::size_t channel = 0U; channel < app::kAds131M08ChannelCount;
       ++channel) {
    std::memcpy(&frame[3U + channel * 3U], channel_data[channel], 3U);
  }
  finalize_frame(frame);

  app::Ads131M08Frame parsed{};
  assert(app::parse_ads131m08_frame(frame, sizeof(frame), &parsed));
  assert(parsed.status.register_map_crc_error);
  assert(parsed.status.reset_occurred);
  assert(parsed.status.word_length == 1U);
  assert(parsed.status.data_ready_mask == 0x05U);
  assert(parsed.channels[0] == -8388608);
  assert(parsed.channels[1] == 8388607);
  assert(parsed.channels[2] == -1);
  assert(parsed.channels[5] == 0x123456);
  assert(parsed.channels[6] == -0x123456);

  frame[12] ^= 0x01U;
  assert(!app::parse_ads131m08_frame(frame, sizeof(frame), &parsed));
  assert(!app::parse_ads131m08_frame(frame, sizeof(frame) - 1U, &parsed));
}

void test_accumulator() {
  app::SampleAccumulator accumulator;
  std::int32_t mean = 123;
  assert(!accumulator.take_mean_and_reset(&mean));
  assert(mean == 123);

  accumulator.add(2);
  accumulator.add(-5);
  accumulator.add(6);
  assert(accumulator.count() == 3U);
  assert(accumulator.take_mean_and_reset(&mean));
  assert(mean == 1);
  assert(accumulator.count() == 0U);

  constexpr std::uint32_t kMaxPollIntervalMs = 65535U;
  constexpr std::uint32_t kSamplesPerSecond = 1954U;
  constexpr std::uint32_t kMaxExpectedSamples =
      (kMaxPollIntervalMs * kSamplesPerSecond + 999U) / 1000U;
  for (std::uint32_t index = 0U; index < kMaxExpectedSamples; ++index) {
    accumulator.add(8388607);
  }
  assert(accumulator.take_mean_and_reset(&mean));
  assert(mean == 8388607);
}

}  // namespace

int main() {
  test_group_validation();
  test_can_fd_lengths();
  test_schedule_logic();
  test_control_matching();
  test_ads131m08_frame();
  test_accumulator();
  return 0;
}
