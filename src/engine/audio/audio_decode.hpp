#ifndef CLAY_ENGINE_AUDIO_AUDIO_DECODE_HPP
#define CLAY_ENGINE_AUDIO_AUDIO_DECODE_HPP

#include "audio_mixer.hpp"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace clay {

namespace audio_detail {

[[nodiscard]] inline bool fourcc(std::span<const std::uint8_t> bytes,
                                 std::size_t offset, char a, char b, char c,
                                 char d) noexcept {
    return offset <= bytes.size() && bytes.size() - offset >= 4 &&
           bytes[offset] == static_cast<std::uint8_t>(a) &&
           bytes[offset + 1] == static_cast<std::uint8_t>(b) &&
           bytes[offset + 2] == static_cast<std::uint8_t>(c) &&
           bytes[offset + 3] == static_cast<std::uint8_t>(d);
}

[[nodiscard]] inline std::uint16_t
read_u16_le(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(bytes[offset]) |
           (static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

[[nodiscard]] inline std::uint32_t
read_u32_le(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

struct WavFormat {
    std::uint16_t encoding = 0;
    std::uint16_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::uint16_t block_align = 0;
    std::uint16_t bits_per_sample = 0;
};

[[nodiscard]] inline std::optional<float>
decode_sample(std::span<const std::uint8_t> bytes, std::size_t offset,
              const WavFormat &format) noexcept {
    if (format.encoding == 1) {
        switch (format.bits_per_sample) {
        case 8:
            return (static_cast<float>(bytes[offset]) - 128.0F) / 128.0F;
        case 16: {
            const auto raw = std::bit_cast<std::int16_t>(read_u16_le(bytes, offset));
            return static_cast<float>(raw) / 32768.0F;
        }
        case 24: {
            const std::uint32_t raw =
                static_cast<std::uint32_t>(bytes[offset]) |
                (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
                (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U);
            const std::int32_t signed_raw =
                (raw & 0x00800000U) != 0
                    ? static_cast<std::int32_t>(raw) - 0x01000000
                    : static_cast<std::int32_t>(raw);
            return static_cast<float>(signed_raw) / 8388608.0F;
        }
        case 32: {
            const auto raw = std::bit_cast<std::int32_t>(read_u32_le(bytes, offset));
            return static_cast<float>(static_cast<double>(raw) / 2147483648.0);
        }
        default:
            return std::nullopt;
        }
    }

    if (format.encoding == 3 && format.bits_per_sample == 32) {
        const float sample = std::bit_cast<float>(read_u32_le(bytes, offset));
        if (!std::isfinite(sample)) {
            return std::nullopt;
        }
        return sample;
    }

    return std::nullopt;
}

} // namespace audio_detail

// Decodes an in-memory RIFF/WAVE clip into the mixer-owned float PCM format.
// PCM 8/16/24/32-bit integer and 32-bit IEEE-float mono/stereo WAV are
// supported. The native sample rate is preserved; resampling is a separate
// concern and AudioMixer::add_clip() enforces rate compatibility.
[[nodiscard]] inline std::optional<AudioClip>
decode_wav(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 12 || !audio_detail::fourcc(bytes, 0, 'R', 'I', 'F', 'F') ||
        !audio_detail::fourcc(bytes, 8, 'W', 'A', 'V', 'E')) {
        return std::nullopt;
    }

    const std::uint32_t riff_size = audio_detail::read_u32_le(bytes, 4);
    if (riff_size < 4 || riff_size > bytes.size() - 8) {
        return std::nullopt;
    }
    const std::size_t riff_end = static_cast<std::size_t>(riff_size) + 8;

    std::optional<audio_detail::WavFormat> format;
    std::span<const std::uint8_t> data;

    std::size_t offset = 12;
    while (offset <= riff_end && riff_end - offset >= 8) {
        const std::uint32_t chunk_size = audio_detail::read_u32_le(bytes, offset + 4);
        const std::size_t payload = offset + 8;
        if (chunk_size > riff_end - payload) {
            return std::nullopt;
        }

        if (audio_detail::fourcc(bytes, offset, 'f', 'm', 't', ' ')) {
            if (chunk_size < 16) {
                return std::nullopt;
            }
            format = audio_detail::WavFormat{
                audio_detail::read_u16_le(bytes, payload),
                audio_detail::read_u16_le(bytes, payload + 2),
                audio_detail::read_u32_le(bytes, payload + 4),
                audio_detail::read_u16_le(bytes, payload + 12),
                audio_detail::read_u16_le(bytes, payload + 14),
            };
        } else if (audio_detail::fourcc(bytes, offset, 'd', 'a', 't', 'a') &&
                   data.empty()) {
            data = bytes.subspan(payload, chunk_size);
        }

        const std::size_t padded_size = static_cast<std::size_t>(chunk_size) +
                                        (static_cast<std::size_t>(chunk_size) & 1U);
        if (padded_size > riff_end - payload) {
            return std::nullopt;
        }
        offset = payload + padded_size;
    }

    if (!format.has_value() || data.empty()) {
        return std::nullopt;
    }

    const audio_detail::WavFormat &fmt = *format;
    if ((fmt.encoding != 1 && fmt.encoding != 3) ||
        (fmt.channels != 1 && fmt.channels != 2) || fmt.sample_rate == 0 ||
        fmt.bits_per_sample == 0 || fmt.bits_per_sample % 8 != 0) {
        return std::nullopt;
    }

    const std::size_t bytes_per_sample = fmt.bits_per_sample / 8;
    if (bytes_per_sample == 0 ||
        fmt.block_align != fmt.channels * bytes_per_sample ||
        data.size() % fmt.block_align != 0) {
        return std::nullopt;
    }
    if ((fmt.encoding == 1 && fmt.bits_per_sample != 8 &&
         fmt.bits_per_sample != 16 && fmt.bits_per_sample != 24 &&
         fmt.bits_per_sample != 32) ||
        (fmt.encoding == 3 && fmt.bits_per_sample != 32)) {
        return std::nullopt;
    }

    std::vector<float> samples;
    samples.reserve(data.size() / bytes_per_sample);
    for (std::size_t sample_offset = 0; sample_offset < data.size();
         sample_offset += bytes_per_sample) {
        auto sample = audio_detail::decode_sample(data, sample_offset, fmt);
        if (!sample.has_value()) {
            return std::nullopt;
        }
        samples.push_back(*sample);
    }

    return AudioClip{fmt.sample_rate, static_cast<std::uint8_t>(fmt.channels),
                     std::move(samples)};
}

} // namespace clay

#endif /* CLAY_ENGINE_AUDIO_AUDIO_DECODE_HPP */
