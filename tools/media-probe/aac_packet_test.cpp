#include "media/base/arbiter_aac_packet.h"
#include <array>
#include <cstdlib>

int main() {
    auto require = [](bool value) { if (!value) std::abort(); };
    std::array<uint8_t, 3> raw{0x21, 0x10, 0x56};
    require(media::ArbiterRawAacPacket(raw)->size() == raw.size());
    std::array<uint8_t, 10> adts{0xff, 0xf1, 0x50, 0x80, 0x01, 0x5f, 0xfc, 1, 2, 3};
    auto result = media::ArbiterRawAacPacket(adts);
    require(result && result->size() == 3 && (*result)[0] == 1);
    for (size_t size = 2; size < 10; ++size)
        require(!media::ArbiterRawAacPacket(std::span(adts).first(size)));
    auto invalid = adts;
    invalid[6] |= 1; // Multiple access units cannot be passed as one raw packet.
    require(!media::ArbiterRawAacPacket(invalid));
    invalid = adts;
    invalid[4] = 0; // Length must match, including any CRC bytes.
    require(!media::ArbiterRawAacPacket(invalid));
    std::array<uint8_t, 12> crc{0xff, 0xf0, 0x50, 0x80, 0x01, 0x9f, 0xfc, 0, 0, 4, 5, 6};
    result = media::ArbiterRawAacPacket(crc);
    require(result && result->size() == 3 && (*result)[0] == 4);
    require(media::ArbiterRawAacPacket(std::span<const uint8_t>{})->empty());
}
