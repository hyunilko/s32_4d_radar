#pragma once

#include <cstdint>

/**
 * @brief Abstract CAN FD transport interface.
 *
 * Decouples protocol-layer classes (PcanLongFrame, PcanShortFrame) from any
 * specific hardware backend (PCAN, SocketCAN, …).
 */
class ICanFdTransport
{
public:
    static constexpr int CAN_DEVICE_ID_SHIFT = 8U;
    static constexpr int CAN_DEVICE_ID_MASK  = 0x700U;
    static constexpr int CAN_TP_ID_MASK      = 0xFFU;

    virtual ~ICanFdTransport() = default;

    /**
     * @brief Sends a CAN FD frame with a variable-length payload (up to 64 bytes).
     */
    virtual bool send_data(uint16_t can_id, const uint8_t* data, uint8_t length) = 0;

    /**
     * @brief Sends a fixed 64-byte CAN FD frame.
     */
    virtual bool send_frame64(uint16_t can_id, const uint8_t data64[64]) = 0;
};
