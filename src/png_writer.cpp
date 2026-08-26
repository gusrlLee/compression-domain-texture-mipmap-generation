#include "png_writer.h"

#include <iostream>
#include <limits>

#include "lodepng.h"

bool WriteRgb8Png(const std::string &file_path,
                  Extent2D extent,
                  const std::vector<uint8_t> &pixels)
{
    if (extent.width == 0 || extent.height == 0)
    {
        std::cerr << "[PngWriter] Image extent must not be zero.\n";
        return false;
    }

    constexpr size_t kChannelCount = 3;
    if (extent.width > std::numeric_limits<size_t>::max() /
                           extent.height / kChannelCount)
    {
        std::cerr << "[PngWriter] Image dimensions are too large.\n";
        return false;
    }

    const size_t expected_size =
        static_cast<size_t>(extent.width) * extent.height * kChannelCount;
    if (pixels.size() != expected_size)
    {
        std::cerr << "[PngWriter] RGB pixel data size does not match the image extent.\n";
        return false;
    }

    const unsigned error = lodepng::encode(file_path,
                                           pixels,
                                           extent.width,
                                           extent.height,
                                           LCT_RGB,
                                           8);
    if (error != 0)
    {
        std::cerr << "[PngWriter] Failed to write file: " << file_path
                  << " (" << lodepng_error_text(error) << ")\n";
        return false;
    }

    return true;
}
