#include "snowimagecodecbridge.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>

int main() {
    constexpr std::array<uint8_t, 3U * 2U * 4U> pixels{
        255, 0,   0,   255, 0, 255, 0, 255, 0,  0,   255, 255,
        255, 255, 255, 255, 0, 0,   0, 255, 80, 100, 120, 255,
    };
    SnowShotImageCodecEncodeOptions options{};
    options.struct_size = static_cast<uint32_t>(sizeof(options));
    options.abi_version = SNOW_SHOT_IMAGE_CODEC_ABI_VERSION;
    options.format = SNOW_SHOT_IMAGE_CODEC_FORMAT_PNG;
    options.quality = 90;
    options.effort = 7;
    options.lossless_effort = 6;
    options.compression_level = 0;
    options.preserve_metadata = 0;

    SnowShotImageCodecBuffer output{};
    std::array<char, 512> error{};
    const int32_t succeeded = snow_shot_image_codec_encode_rgba8(
        pixels.data(), pixels.size(), 3, 2, 3U * 4U, &options, &output, error.data(), error.size());
    const bool hasPngSignature = output.size >= 8 && output.data != nullptr &&
                                 output.data[0] == 0x89 && output.data[1] == 'P' &&
                                 output.data[2] == 'N' && output.data[3] == 'G';

    uint8_t* const originalData = output.data;
    const uint64_t originalSize = output.size;
    error.fill('\0');
    const int32_t reusedWithoutRelease = snow_shot_image_codec_encode_rgba8(
        pixels.data(), pixels.size(), 3, 2, 3U * 4U, &options, &output, error.data(), error.size());
    const bool rejectedUnsafeReuse = reusedWithoutRelease == 0 && output.data == originalData &&
                                     output.size == originalSize && error[0] != '\0';

    snow_shot_image_codec_release_buffer(&output);
    snow_shot_image_codec_release_buffer(&output);
    if (snow_shot_image_codec_abi_version() != SNOW_SHOT_IMAGE_CODEC_ABI_VERSION ||
        succeeded == 0 || !hasPngSignature || !rejectedUnsafeReuse || output.data != nullptr ||
        output.size != 0) {
        std::cerr << (error[0] == '\0' ? "The C ABI PNG smoke test failed." : error.data()) << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
