// Copyright 2026 Arbiter contributors
// SPDX-License-Identifier: GPL-3.0-only
#ifndef MEDIA_BASE_ARBITER_AAC_PACKET_H_
#define MEDIA_BASE_ARBITER_AAC_PACKET_H_

#include <cstdint>
#include <optional>
#include <span>

namespace media {

// MSE prepends ADTS before CDM decryption, whereas native AAC decoders expect
// raw access units. Strip only after decryption so encrypted subsample offsets
// remain intact. Ordinary MP4 demuxing can already supply raw access units.
inline std::optional<std::span<const uint8_t>> ArbiterRawAacPacket(
    std::span<const uint8_t> packet) {
  if (packet.size() < 2 || packet[0] != 0xff || (packet[1] & 0xf6) != 0xf0)
    return packet;
  const size_t header = (packet[1] & 1) ? 7 : 9;
  if (packet.size() <= header)
    return std::nullopt;
  const size_t size = ((packet[3] & 3) << 11) | (packet[4] << 3) | (packet[5] >> 5);
  if (size != packet.size() || (packet[6] & 3) != 0)
    return std::nullopt;
  return packet.subspan(header);
}

}  // namespace media
#endif
