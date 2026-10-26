#pragma once

#include <cstddef>
#include <cstdint>

namespace app {

constexpr std::size_t kMaxCanFdPayloadBytes = 64U;

struct SensorContext {
  const char *name;
  std::uint8_t payload_size;
};

struct SensorDescriptor {
  const SensorContext *context;
  bool (*begin)(const void *ctx);
  bool (*sample)(const void *ctx, std::uint8_t *dst);
  void (*suspend)(const void *ctx);
  void (*resume)(const void *ctx);
};

struct MessageGroupConfig {
  const char *name;
  std::uint32_t can_id;
  std::uint16_t poll_interval_ms;
  const SensorDescriptor *sensors;
  std::size_t sensor_count;
};

struct ControlMessageConfig {
  std::uint32_t sleep_command_id;
  std::uint8_t sleep_command_byte;
  std::uint8_t command_byte_index;
};

struct BoardHooks {
  void (*pre_setup)();
  void (*before_sleep)();
  void (*after_wake)();
};

struct CanConfig {
  std::uint32_t arbitration_bitrate;
  std::uint32_t data_bitrate;
  bool use_extended_ids;
};

struct BoardConfig {
  CanConfig can;
  std::uint16_t startup_delay_ms;
  ControlMessageConfig control;
  BoardHooks hooks;
  const MessageGroupConfig *groups;
  std::size_t group_count;
};

constexpr std::size_t group_payload_bytes(const MessageGroupConfig &group) {
  if (group.sensors == nullptr || group.sensor_count == 0U) {
    return 0U;
  }

  std::size_t total = 0U;
  for (std::size_t index = 0U; index < group.sensor_count; ++index) {
    if (group.sensors[index].context == nullptr) {
      return kMaxCanFdPayloadBytes + 1U;
    }
    total += group.sensors[index].context->payload_size;
  }
  return total;
}

constexpr bool group_payload_fits(const MessageGroupConfig &group) {
  return group.sensors != nullptr && group.sensor_count != 0U &&
         group_payload_bytes(group) <= kMaxCanFdPayloadBytes;
}

template <std::size_t GroupCount>
constexpr bool all_group_payloads_fit(
    const MessageGroupConfig (&groups)[GroupCount]) {
  for (std::size_t index = 0U; index < GroupCount; ++index) {
    if (!group_payload_fits(groups[index])) {
      return false;
    }
  }
  return true;
}

constexpr bool all_group_payloads_fit(const MessageGroupConfig *groups,
                                      const std::size_t group_count) {
  if (group_count == 0U) {
    return true;
  }
  if (groups == nullptr) {
    return false;
  }
  for (std::size_t index = 0U; index < group_count; ++index) {
    if (!group_payload_fits(groups[index])) {
      return false;
    }
  }
  return true;
}

}  // namespace app
