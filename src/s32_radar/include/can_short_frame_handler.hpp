#pragma once

#include <cstdint>
#include <vector>

#include "rclcpp/logger.hpp"
#include "can_short_frame.hpp"

namespace s32_radar
{
    class device_radar_node;

    class CanShortFrameHandler
    {
    public:
        explicit CanShortFrameHandler(
            device_radar_node* node,
            CanShortFrame& can,
            rclcpp::Logger logger = rclcpp::get_logger("CanShortFrameHandler"),
            bool quiet = false);

        void start(void);
        void stop(void);

    private:
        void handle_short_frame(uint8_t dev_id, ShortCanCmd cmd, uint32_t uniq_id,
                                const std::vector<uint8_t>& data);

        bool send_time_sync(uint8_t dev_id, uint32_t uniq_id);

        CanShortFrame& can_short_;
        rclcpp::Logger  logger_;
        bool            quiet_ = true;

        device_radar_node* radar_node_;
    };

} // namespace s32_radar
