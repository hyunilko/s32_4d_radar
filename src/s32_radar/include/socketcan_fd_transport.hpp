#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "can_fd_transport.hpp"
#include "can_long_frame.hpp"
#include "can_short_frame.hpp"

/**
 * @file socketcan_fd_transport.hpp
 * @brief Generic SocketCAN FD transport layer.
 *
 * Implements ICanFdTransport using the Linux kernel SocketCAN interface.
 * Works with any CAN hardware that has a Linux kernel driver
 * (PEAK PCAN, Kvaser, Vector, 8devices, etc.).
 *
 * The CAN interface (bitrate, dbitrate, fd mode) must be configured externally
 * before starting the node, e.g.:
 *   ip link set can0 up type can bitrate 1000000 dbitrate 8000000 fd on
 */
class SocketCanFdTransport : public ICanFdTransport
{
public:
    struct Config
    {
        std::string interface_name = "can0";
        bool brs_on = true;

        /* link-up automation */
        bool  auto_link_up    = true;
        int   bitrate         = 1000000;
        float sample_point    = 0.800f;
        int   dbitrate        = 8000000;
        float dsample_point   = 0.800f;
        int   restart_ms      = 100;
        int   txqueuelen      = 1000;

        CanLongFrameConfig  long_frame{};
        CanShortFrameConfig short_frame{};
    };

    explicit SocketCanFdTransport(const Config& cfg);
    ~SocketCanFdTransport();

    /* ----- Channel initialisation / release -------------------------------- */
    bool init();
    void shutdown();

    /**
     * @brief Calls init() then start_rx() in order.
     *        Call after all handler callbacks have been registered.
     */
    void start();

    /* ----- Receive loop control ------------------------------------------- */
    void start_rx();
    void stop_rx();

    /* ----- Protocol layer accessors --------------------------------------- */
    CanLongFrame&  long_frame();
    CanShortFrame& short_frame();

    /* ----- ICanFdTransport ------------------------------------------------ */
    bool send_data(uint16_t can_id, const uint8_t* data, uint8_t length) override;
    bool send_frame64(uint16_t can_id, const uint8_t data64[64]) override;

private:
    static constexpr int TX_MAX_RETRIES    = 3;
    static constexpr int TX_RETRY_DELAY_US = 1000;
    static constexpr int EPOLL_TIMEOUT_MS  = 50;

    bool can_link_up();
    bool do_can_write(uint32_t can_id, const uint8_t* data, uint8_t len);
    void poll_rx();
    void receiveThread();
    int  setup_epoll();

private:
    Config cfg_;

    std::atomic<bool> initialized_{false};
    std::mutex        io_mtx_;
    int               sock_fd_{-1};

    std::unique_ptr<CanShortFrame> short_frame_;
    std::unique_ptr<CanLongFrame>  long_frame_;

    std::thread       rx_thread_;
    std::atomic<bool> rx_running_{false};
    int               rx_epoll_fd_{-1};
};
