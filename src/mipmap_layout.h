#pragma once

#include <cstdint>
#include <vector>
#include "types.h"

struct MipLevelData
{
    uint32_t width;
    uint32_t height;
    uint32_t block_width;
    uint32_t block_height;

    // Format-independent raw byte array (8 bytes for BC1, 16 bytes for BC7, etc.)
    std::vector<uint8_t> raw_blocks;
};

class MipmapLayout
{
public:
    MipmapLayout() = default;
    ~MipmapLayout() = default;

    // Allocates correct memory size based on the provided format.
    void Allocate(uint32_t base_width, uint32_t base_height, TextureFormat format);

    uint32_t base_width() const { return base_width_; }
    uint32_t base_height() const { return base_height_; }
    uint32_t num_levels() const { return static_cast<uint32_t>(levels_.size()); }
    TextureFormat format() const { return format_; }

    const MipLevelData &GetLevel(uint32_t index) const;
    MipLevelData &GetLevel(uint32_t index);

private:
    uint32_t base_width_ = 0;
    uint32_t base_height_ = 0;
    TextureFormat format_ = TextureFormat::kUnknown;
    uint32_t block_stride_bytes_ = 0;

    std::vector<MipLevelData> levels_;
};
