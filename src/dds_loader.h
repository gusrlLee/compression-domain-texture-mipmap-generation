#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "types.h"

class DdsLoader
{
public:
    DdsLoader() = default;
    ~DdsLoader() = default;

    bool Load(const std::string &file_path);

    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    uint32_t mip_map_count() const { return mip_map_count_; }
    TextureFormat format() const { return format_; }

    const std::vector<uint8_t> &raw_blocks() const { return raw_blocks_; }

private:
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t mip_map_count_ = 0;
    TextureFormat format_ = TextureFormat::kUnknown;

    std::vector<uint8_t> raw_blocks_;
};