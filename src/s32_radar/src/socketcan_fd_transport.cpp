/**
 * @file socketcan_fd_transport.cpp
 * @author antonioko@au-sensor.com
 * @brief SocketCAN FD transport layer — hardware I/O, frame routing and callback wrappers.
 * @version 1.0
 * @date 2026-06
 *
 * @details Uses the Linux kernel SocketCAN interface (PF_CAN / SOCK_RAW / CAN_RAW) with
 *          CAN_RAW_FD_FRAMES enabled.  No proprietary library is required; any hardware
 *          with a standard Linux CAN driver is supported.
 *
 * @copyright Copyright AU (c) 2026
 */

#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include "rclcpp/rclcpp.hpp"
#include "socketcan_fd_transport.hpp"

enum class CanRxKind : uint8_t 
{ 
    SHORT, 
    LONG, 
    ACK, 
    UNKNOWN 
};

/* ========================================================================= */
/* Construction / destruction                                                 */
/* ========================================================================= */

SocketCanFdTransport::SocketCanFdTransport(const Config& cfg)
    : cfg_(cfg)
{
    short_frame_ = std::make_unique<CanShortFrame>(*this, cfg_.short_frame);
    long_frame_  = std::make_unique<CanLongFrame>(*this,  cfg_.long_frame);
}

SocketCanFdTransport::~SocketCanFdTransport()
{
    stop_rx();
    shutdown();
}

/* ========================================================================= */
/* CAN link-up helper                                                         */
/* ========================================================================= */

bool SocketCanFdTransport::can_link_up()
{
    /* Validate interface name to prevent command injection. */
    for (char c : cfg_.interface_name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            RCLCPP_ERROR(rclcpp::get_logger("SocketCanFdTransport"),
                         "can_link_up: invalid interface name '%s'",
                         cfg_.interface_name.c_str());
            return false;
        }
    }

    char cmd[512];
    const char* iface = cfg_.interface_name.c_str();

    /* 1. Down */
    snprintf(cmd, sizeof(cmd), "sudo -n ip link set %s down", iface);
    if (system(cmd) != 0) {
        RCLCPP_WARN(rclcpp::get_logger("SocketCanFdTransport"),
                    "can_link_up: '%s' returned non-zero (may be already down)", cmd);
    }

    /* 2. Bitrate, sample-point, FD mode, restart-ms */
    snprintf(cmd, sizeof(cmd),
             "sudo -n ip link set %s type can bitrate %d sample-point %.3f "
             "dbitrate %d dsample-point %.3f fd on restart-ms %d",
             iface,
             cfg_.bitrate,  cfg_.sample_point,
             cfg_.dbitrate, cfg_.dsample_point,
             cfg_.restart_ms);
    if (system(cmd) != 0) {
        RCLCPP_ERROR(rclcpp::get_logger("SocketCanFdTransport"),
                     "can_link_up: failed to configure CAN params: %s", cmd);
        return false;
    }

    /* 3. Extend TX queue (required at 8 Mbps) */
    snprintf(cmd, sizeof(cmd), "sudo -n ip link set %s txqueuelen %d", iface, cfg_.txqueuelen);
    if (system(cmd) != 0) {
        RCLCPP_WARN(rclcpp::get_logger("SocketCanFdTransport"),
                    "can_link_up: txqueuelen failed: %s", cmd);
    }

    /* 4. Set CAN FD MTU (72 bytes) */
    snprintf(cmd, sizeof(cmd), "sudo -n ip link set %s mtu 72", iface);
    if (system(cmd) != 0) {
        RCLCPP_WARN(rclcpp::get_logger("SocketCanFdTransport"),
                    "can_link_up: mtu set failed: %s", cmd);
    }

    /* 5. Up */
    snprintf(cmd, sizeof(cmd), "sudo -n ip link set %s up", iface);
    if (system(cmd) != 0) {
        RCLCPP_ERROR(rclcpp::get_logger("SocketCanFdTransport"),
                     "can_link_up: failed to bring up interface: %s", cmd);
        return false;
    }

    RCLCPP_INFO(rclcpp::get_logger("SocketCanFdTransport"),
                "can_link_up: %s up — bitrate=%d dbitrate=%d fd=on txqueuelen=%d",
                iface, cfg_.bitrate, cfg_.dbitrate, cfg_.txqueuelen);
    return true;
}

/* ========================================================================= */
/* Channel initialisation / release                                           */
/* ========================================================================= */

bool SocketCanFdTransport::init()
{
    if (initialized_.load(std::memory_order_acquire)) {
        return true;
    }

    if (cfg_.auto_link_up && !can_link_up()) {
        return false;
    }

    std::lock_guard<std::mutex> lk(io_mtx_);

    if (initialized_.load(std::memory_order_relaxed)) {
        return true;
    }

    sock_fd_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock_fd_ < 0) {
        RCLCPP_ERROR(rclcpp::get_logger("SocketCanFdTransport"),
                     "socket() failed: %s", strerror(errno));
        return false;
    }

    /* Enable CAN FD frames on this socket. */
    const int enable = 1;
    if (setsockopt(sock_fd_, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
                   &enable, sizeof(enable)) < 0)
    {
        RCLCPP_ERROR(rclcpp::get_logger("SocketCanFdTransport"),
                     "setsockopt CAN_RAW_FD_FRAMES failed: %s", strerror(errno));
        close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }

    struct ifreq ifr{};
    std::strncpy(ifr.ifr_name, cfg_.interface_name.c_str(), IFNAMSIZ - 1);
    if (ioctl(sock_fd_, SIOCGIFINDEX, &ifr) < 0) {
        RCLCPP_ERROR(rclcpp::get_logger("SocketCanFdTransport"),
                     "ioctl SIOCGIFINDEX failed for '%s': %s",
                     cfg_.interface_name.c_str(), strerror(errno));
        close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }

    struct sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(sock_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        RCLCPP_ERROR(rclcpp::get_logger("SocketCanFdTransport"),
                     "bind() failed for '%s': %s",
                     cfg_.interface_name.c_str(), strerror(errno));
        close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }

    initialized_.store(true, std::memory_order_release);

    if (!cfg_.long_frame.quiet) {
        RCLCPP_INFO(
            rclcpp::get_logger("SocketCanFdTransport"),
            "SocketCAN init OK. iface=%s dev=%u short_tx=0x%03X short_rx=0x%03X "
            "long_tx=0x%03X long_rx=0x%03X",
            cfg_.interface_name.c_str(),
            cfg_.long_frame.device_count,
            cfg_.short_frame.tx_base_id,
            cfg_.short_frame.rx_base_id,
            cfg_.long_frame.tx_base_id,
            cfg_.long_frame.rx_base_id);
    }

    return true;
}

void SocketCanFdTransport::shutdown()
{
    if (!initialized_.load(std::memory_order_acquire)) {
        return;
    }

    std::lock_guard<std::mutex> lk(io_mtx_);

    if (!initialized_.load(std::memory_order_relaxed)) {
        return;
    }

    close(sock_fd_);
    sock_fd_ = -1;
    initialized_.store(false, std::memory_order_release);
}

/* ========================================================================= */
/* Shared write path                                                          */
/* ========================================================================= */

bool SocketCanFdTransport::do_can_write(uint32_t can_id,
                                         const uint8_t* data,
                                         uint8_t len)
{
    struct canfd_frame frame{};
    frame.can_id = can_id & CAN_SFF_MASK;
    frame.len    = len;
    if (cfg_.brs_on) {
        frame.flags = CANFD_BRS;
    }
    std::memcpy(frame.data, data, len);

    for (int retry = 0; retry < TX_MAX_RETRIES; ++retry) {
        ssize_t n;
        {
            std::lock_guard<std::mutex> lk(io_mtx_);
            n = write(sock_fd_, &frame, sizeof(frame));
        }

        if (n == static_cast<ssize_t>(sizeof(frame))) {
            return true;
        }

        if (!cfg_.long_frame.quiet) {
            RCLCPP_ERROR(rclcpp::get_logger("SocketCanFdTransport"),
                         "write() failed (retry %d): %s", retry, strerror(errno));
        }

        usleep(static_cast<unsigned>(TX_RETRY_DELAY_US));
    }

    return false;
}

/* ========================================================================= */
/* Public send API                                                            */
/* ========================================================================= */

bool SocketCanFdTransport::send_data(uint16_t can_id,
                                      const uint8_t* data,
                                      uint8_t length)
{
    if (!initialized_.load(std::memory_order_acquire) ||
        data   == nullptr ||
        length  > 64u)
    {
        return false;
    }

    return do_can_write(can_id, data, length);
}

bool SocketCanFdTransport::send_frame64(uint16_t can_id, const uint8_t data64[64])
{
    if (!initialized_.load(std::memory_order_acquire) || data64 == nullptr) {
        return false;
    }

    return do_can_write(can_id, data64, 64u);
}

/* ========================================================================= */
/* Frame dispatch                                                             */
/* ========================================================================= */

void SocketCanFdTransport::poll_rx()
{
    if (!initialized_.load(std::memory_order_acquire)) {
        return;
    }

    while (true) {
        struct canfd_frame frame{};
        ssize_t n;
        {
            std::lock_guard<std::mutex> lk(io_mtx_);
            /* MSG_DONTWAIT: non-blocking — return immediately if no frame. */
            n = recv(sock_fd_, &frame, sizeof(frame), MSG_DONTWAIT);
        }

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  /* queue drained */
            }
            if (!cfg_.long_frame.quiet) {
                RCLCPP_ERROR(rclcpp::get_logger("SocketCanFdTransport"),
                             "recv() failed: %s", strerror(errno));
            }
            break;
        }

        /* Determine actual payload length from the received bytes. */
        uint8_t data_len = 0u;
        if (n == static_cast<ssize_t>(CANFD_MTU)) {
            data_len = frame.len;               /* CAN FD frame */
        } else if (n == static_cast<ssize_t>(CAN_MTU)) {
            data_len = frame.len & 0x0Fu;       /* classic CAN frame */
        } else {
            continue;                           /* unexpected size — skip */
        }

        const uint32_t can_id = frame.can_id & CAN_SFF_MASK;
        const uint8_t  tp_id  = static_cast<uint8_t>(can_id & ICanFdTransport::CAN_TP_ID_MASK);

        CanRxKind kind = CanRxKind::UNKNOWN;
        if      (short_frame_ && tp_id == cfg_.short_frame.rx_base_id)   kind = CanRxKind::SHORT;
        else if (long_frame_  && tp_id == cfg_.long_frame.rx_base_id)    kind = CanRxKind::LONG;
        else if (short_frame_ && tp_id == cfg_.short_frame.ack_base_id)  kind = CanRxKind::ACK;

        switch (kind) {
            case CanRxKind::SHORT:
                short_frame_->handle_short_can_frame(can_id, frame.data, data_len);
                break;
            case CanRxKind::LONG:
                long_frame_->handle_long_can_frame(can_id, frame.data, data_len);
                break;
            case CanRxKind::ACK:
                short_frame_->handle_ack_can_frame(can_id, frame.data, data_len);
                break;
            default:
                if (!cfg_.long_frame.quiet) {
                    RCLCPP_WARN(rclcpp::get_logger("SocketCanFdTransport"),
                                "Unknown CAN ID: 0x%03X len=%u", can_id, data_len);
                }
                break;
        }
    }
}

/* ========================================================================= */
/* epoll setup                                                                */
/* ========================================================================= */

int SocketCanFdTransport::setup_epoll()
{
    const int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        RCLCPP_WARN(rclcpp::get_logger("SocketCanFdTransport"),
                    "epoll_create1 failed: %s", strerror(errno));
        return -1;
    }

    struct epoll_event ev{};
    ev.events  = EPOLLIN;
    ev.data.fd = sock_fd_;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock_fd_, &ev) < 0) {
        RCLCPP_WARN(rclcpp::get_logger("SocketCanFdTransport"),
                    "epoll_ctl failed: %s", strerror(errno));
        close(epfd);
        return -1;
    }

    return epfd;
}

/* ========================================================================= */
/* Receive loop                                                               */
/* ========================================================================= */

void SocketCanFdTransport::start()
{
    if (init()) {
        start_rx();
    }
}

void SocketCanFdTransport::start_rx()
{
    if (rx_running_.load()) {
        return;
    }
    rx_running_.store(true);
    rx_thread_ = std::thread(&SocketCanFdTransport::receiveThread, this);
}

void SocketCanFdTransport::stop_rx()
{
    rx_running_.store(false);
    if (rx_thread_.joinable()) {
        rx_thread_.join();
    }

    if (rx_epoll_fd_ >= 0) {
        close(rx_epoll_fd_);
        rx_epoll_fd_ = -1;
    }
}

void SocketCanFdTransport::receiveThread()
{
    rx_epoll_fd_ = setup_epoll();
    const bool use_epoll = (rx_epoll_fd_ >= 0);

    if (!cfg_.long_frame.quiet) {
        RCLCPP_INFO(rclcpp::get_logger("SocketCanFdTransport"),
                    "Receive thread started — iface=%s mode=%s",
                    cfg_.interface_name.c_str(),
                    use_epoll ? "epoll" : "polling");
    }

    struct epoll_event events[8];

    while (rx_running_.load()) {
        if (use_epoll) {
            const int n = epoll_wait(rx_epoll_fd_, events, 8, EPOLL_TIMEOUT_MS);

            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                RCLCPP_ERROR(rclcpp::get_logger("SocketCanFdTransport"),
                             "epoll_wait error: %s", strerror(errno));
                break;
            }

            if (n > 0) {
                poll_rx();
            }
        } else {
            poll_rx();
            usleep(1000);
        }
    }

    if (!cfg_.long_frame.quiet) {
        RCLCPP_INFO(rclcpp::get_logger("SocketCanFdTransport"), "Receive thread stopped.");
    }
}

/* ========================================================================= */
/* Protocol layer accessors                                                   */
/* ========================================================================= */

CanLongFrame& SocketCanFdTransport::long_frame()
{
    return *long_frame_;
}

CanShortFrame& SocketCanFdTransport::short_frame()
{
    return *short_frame_;
}
