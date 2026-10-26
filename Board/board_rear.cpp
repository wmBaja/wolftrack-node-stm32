#include "board.h"

namespace {

constexpr app::CanConfig kRearCanConfig{
    500000U,
    1000000U,
    true,
};

constexpr app::ControlMessageConfig kRearControl{
    0U,  // TODO_ID: sleep command CAN ID.
    0U,  // TODO: sleep command byte.
    0U,  // TODO: sleep command payload index.
};

constexpr app::BoardHooks kRearHooks{nullptr, nullptr, nullptr};

constexpr app::BoardConfig kRearBoardConfig{
    kRearCanConfig,
    1000U,
    kRearControl,
    kRearHooks,
    nullptr,  // TODO: rear sensor message groups.
    0U,
};

static_assert(app::all_group_payloads_fit(kRearBoardConfig.groups,
                                          kRearBoardConfig.group_count),
              "Every rear message group must fit in one CAN FD payload");

}  // namespace

const app::BoardConfig &board::config() { return kRearBoardConfig; }
