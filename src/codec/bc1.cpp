#include "codec/bc1.h"

#include <algorithm>
#include <cstddef>

constexpr uint32_t kLaneCount = 4;

SymbolicDataBC1x4 PackBlocks(const SymbolicDataBC1 blocks[kLaneCount])
{
    SymbolicDataBC1x4 packed;
    packed.color_0 = uint4(blocks[0].color_0, blocks[1].color_0,
                           blocks[2].color_0, blocks[3].color_0);
    packed.color_1 = uint4(blocks[0].color_1, blocks[1].color_1,
                           blocks[2].color_1, blocks[3].color_1);
    packed.selectors = uint4(blocks[0].selectors, blocks[1].selectors,
                             blocks[2].selectors, blocks[3].selectors);
    return packed;
}

uint Lane(const uint4 &value, uint32_t lane)
{
    switch (lane)
    {
    case 0:
        return value.x;
    case 1:
        return value.y;
    case 2:
        return value.z;
    default:
        return value.w;
    }
}

void ProcessRowBC1(
    const uint8_t *src_blocks,
    uint32_t src_block_width,
    uint32_t src_block_height,
    uint8_t *dst_blocks,
    uint32_t dst_block_width,
    uint32_t dst_row_y)
{
    const auto *src = reinterpret_cast<const SymbolicDataBC1 *>(src_blocks);
    auto *dst = reinterpret_cast<SymbolicDataBC1 *>(dst_blocks);

    const uint32_t src_y0 = dst_row_y * 2;
    const uint32_t src_y1 = std::min(src_y0 + 1, src_block_height - 1);
    const size_t src_row0 = static_cast<size_t>(src_y0) * src_block_width;
    const size_t src_row1 = static_cast<size_t>(src_y1) * src_block_width;
    const size_t dst_row = static_cast<size_t>(dst_row_y) * dst_block_width;

    for (uint32_t dst_x = 0; dst_x < dst_block_width; dst_x += kLaneCount)
    {
        SymbolicDataBC1 p00[kLaneCount];
        SymbolicDataBC1 p10[kLaneCount];
        SymbolicDataBC1 p01[kLaneCount];
        SymbolicDataBC1 p11[kLaneCount];

        const uint32_t valid_lanes = std::min(kLaneCount, dst_block_width - dst_x);
        for (uint32_t lane = 0; lane < kLaneCount; ++lane)
        {
            const uint32_t child_x = dst_x + std::min(lane, valid_lanes - 1);
            const uint32_t src_x0 = child_x * 2;
            const uint32_t src_x1 = std::min(src_x0 + 1, src_block_width - 1);

            p00[lane] = src[src_row0 + src_x0];
            p10[lane] = src[src_row0 + src_x1];
            p01[lane] = src[src_row1 + src_x0];
            p11[lane] = src[src_row1 + src_x1];
        }

        const SymbolicDataBC1x4 encoded = EncodeMipBlocksBC1x4(PackBlocks(p00), PackBlocks(p10), PackBlocks(p01), PackBlocks(p11));

        for (uint32_t lane = 0; lane < valid_lanes; ++lane)
        {
            SymbolicDataBC1 &result = dst[dst_row + dst_x + lane];
            result.color_0 = static_cast<ushort>(Lane(encoded.color_0, lane));
            result.color_1 = static_cast<ushort>(Lane(encoded.color_1, lane));
            result.selectors = Lane(encoded.selectors, lane);
        }
    }
}
