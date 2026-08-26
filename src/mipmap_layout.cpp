#include "mipmap_layout.h"

#include <cmath>
#include <algorithm>
#include <iostream>

void MipmapLayout::Allocate(uint32_t base_width, uint32_t base_height, TextureFormat format)
{
    base_width_ = base_width;
    base_height_ = base_height;
    format_ = format;
    block_stride_bytes_ = GetBlockSize(format_);

    uint32_t max_dim = std::max(base_width_, base_height_);
    uint32_t total_mip_levels = static_cast<uint32_t>(std::log2(max_dim)) + 1;

    levels_.clear();
    if (total_mip_levels <= 1)
        return;

    levels_.reserve(total_mip_levels - 1);

    uint32_t current_width = base_width_;
    uint32_t current_height = base_height_;

    std::cout << "[MipmapLayout] Allocating for Format " << static_cast<int>(format_)
              << " (Stride: " << block_stride_bytes_ << " bytes)\n";

    for (uint32_t i = 1; i < total_mip_levels; ++i)
    {
        current_width = std::max(1u, current_width / 2);
        current_height = std::max(1u, current_height / 2);

        uint32_t block_width = std::max(1u, (current_width + 3) / 4);
        uint32_t block_height = std::max(1u, (current_height + 3) / 4);
        size_t num_blocks = block_width * block_height;

        MipLevelData level_data;
        level_data.width = current_width;
        level_data.height = current_height;
        level_data.block_width = block_width;
        level_data.block_height = block_height;

        // Allocate raw bytes based on the block count and stride
        level_data.raw_blocks.resize(num_blocks * block_stride_bytes_);

        levels_.push_back(std::move(level_data));
    }
}

const MipLevelData &MipmapLayout::GetLevel(uint32_t index) const
{
    return levels_[index];
}

MipLevelData &MipmapLayout::GetLevel(uint32_t index)
{
    return levels_[index];
}
