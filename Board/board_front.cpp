#include "board.h"

namespace {

constexpr app::CanConfig kFrontCanConfig{
    500000U,
    1000000U,
    true,
};

constexpr app::ControlMessageConfig kFrontControl{
    0U,  // TODO_ID: sleep command CAN ID.
    0U,  // TODO: sleep command byte.
    0U,  // TODO: sleep command payload index.
};

constexpr app::BoardHooks kFrontHooks{nullptr, nullptr, nullptr};

constexpr app::BoardConfig kFrontBoardConfig{
    kFrontCanConfig,
    1000U,
    kFrontControl,
    kFrontHooks,
    nullptr,  // TODO: front sensor message groups.
    0U,
};

static_assert(app::all_group_payloads_fit(kFrontBoardConfig.groups,
                                          kFrontBoardConfig.group_count),
              "Every front message group must fit in one CAN FD payload");

}  // namespace

const app::BoardConfig &board::config() { return kFrontBoardConfig; }
