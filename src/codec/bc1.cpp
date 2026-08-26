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

float Lane(const float4 &value, uint32_t lane)
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

void SetLane(float4 &value, uint32_t lane, float component)
{
    switch (lane)
    {
    case 0:
        value.x = component;
        break;
    case 1:
        value.y = component;
        break;
    case 2:
        value.z = component;
        break;
    default:
        value.w = component;
        break;
    }
}

void SetQuadrantSample(
    QuadrantMeans &quadrants,
    uint32_t sample,
    uint32_t lane,
    const LinearBlockMean &color)
{
    switch (sample)
    {
    case 0:
        SetLane(quadrants.q0_r, lane, color.r);
        SetLane(quadrants.q0_g, lane, color.g);
        SetLane(quadrants.q0_b, lane, color.b);
        break;
    case 1:
        SetLane(quadrants.q1_r, lane, color.r);
        SetLane(quadrants.q1_g, lane, color.g);
        SetLane(quadrants.q1_b, lane, color.b);
        break;
    case 2:
        SetLane(quadrants.q2_r, lane, color.r);
        SetLane(quadrants.q2_g, lane, color.g);
        SetLane(quadrants.q2_b, lane, color.b);
        break;
    default:
        SetLane(quadrants.q3_r, lane, color.r);
        SetLane(quadrants.q3_g, lane, color.g);
        SetLane(quadrants.q3_b, lane, color.b);
        break;
    }
}

// Store one SIMD lane as a scalar texel in the block-mean image.
void StoreBlockMean(
    const BlockMeans &means,
    uint32_t lane,
    LinearBlockMean &destination)
{
    destination.r = Lane(means.r, lane);
    destination.g = Lane(means.g, lane);
    destination.b = Lane(means.b, lane);
}

// Clamp-to-edge fetch from the linear mean image.
const LinearBlockMean &FetchLinearMeanTexel(
    const LinearBlockMean *src_texels,
    uint32_t src_width,
    uint32_t src_height,
    uint32_t target_x,
    uint32_t target_y)
{
    const uint32_t x = std::min(target_x, src_width - 1);
    const uint32_t y = std::min(target_y, src_height - 1);
    return src_texels[static_cast<size_t>(y) * src_width + x];
}

void DownsampleLinearMeanRow(
    const LinearBlockMean *src_texels,
    uint32_t src_width,
    uint32_t src_height,
    LinearBlockMean *dst_texels,
    uint32_t dst_width,
    uint32_t dst_row_y)
{
    const uint32_t src_y0 = std::min(dst_row_y * 2, src_height - 1);
    const uint32_t src_y1 = std::min(src_y0 + 1, src_height - 1);

    const size_t src_row0 = static_cast<size_t>(src_y0) * src_width;
    const size_t src_row1 = static_cast<size_t>(src_y1) * src_width;
    const size_t dst_row = static_cast<size_t>(dst_row_y) * dst_width;

    for (uint32_t dst_x = 0; dst_x < dst_width; ++dst_x)
    {
        const uint32_t src_x0 = std::min(dst_x * 2, src_width - 1);
        const uint32_t src_x1 = std::min(src_x0 + 1, src_width - 1);

        const LinearBlockMean &s00 = src_texels[src_row0 + src_x0];
        const LinearBlockMean &s10 = src_texels[src_row0 + src_x1];
        const LinearBlockMean &s01 = src_texels[src_row1 + src_x0];
        const LinearBlockMean &s11 = src_texels[src_row1 + src_x1];

        LinearBlockMean &destination = dst_texels[dst_row + dst_x];
        destination.r = (s00.r + s10.r + s01.r + s11.r) * 0.25f;
        destination.g = (s00.g + s10.g + s01.g + s11.g) * 0.25f;
        destination.b = (s00.b + s10.b + s01.b + s11.b) * 0.25f;
    }
}

void ProcessRowBC1(
    const uint8_t *src_blocks,
    uint32_t src_block_width,
    uint32_t src_block_height,
    uint8_t *dst_blocks,
    uint32_t dst_block_width,
    uint32_t dst_row_y,
    LinearBlockMean *source_block_means)
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

        SourceBlockMeans source_means;
        const SymbolicDataBC1x4 encoded = EncodeMipBlocksBC1x4(PackBlocks(p00), PackBlocks(p10), PackBlocks(p01), PackBlocks(p11), source_means);

        for (uint32_t lane = 0; lane < valid_lanes; ++lane)
        {
            SymbolicDataBC1 &result = dst[dst_row + dst_x + lane];
            result.color_0 = static_cast<ushort>(Lane(encoded.color_0, lane));
            result.color_1 = static_cast<ushort>(Lane(encoded.color_1, lane));
            result.selectors = Lane(encoded.selectors, lane);

            if (source_block_means != nullptr)
            {
                const uint32_t child_x = dst_x + lane;
                const uint32_t src_x0 = child_x * 2;
                const uint32_t src_x1 = std::min(src_x0 + 1, src_block_width - 1);

                StoreBlockMean(source_means.p00, lane, source_block_means[src_row0 + src_x0]);

                if (src_x1 != src_x0)
                {
                    StoreBlockMean(source_means.p10, lane, source_block_means[src_row0 + src_x1]);
                }

                if (src_y1 != src_y0)
                {
                    StoreBlockMean(source_means.p01, lane, source_block_means[src_row1 + src_x0]);

                    if (src_x1 != src_x0)
                    {
                        StoreBlockMean(source_means.p11, lane, source_block_means[src_row1 + src_x1]);
                    }
                }
            }
        }
    }
}

void ProcessLinearRowBC1(
    const LinearBlockMean *src_texels,
    uint32_t src_width,
    uint32_t src_height,
    uint8_t *dst_blocks,
    uint32_t dst_block_width,
    uint32_t dst_row_y)
{
    auto *dst = reinterpret_cast<SymbolicDataBC1 *>(dst_blocks);
    const size_t dst_row =
        static_cast<size_t>(dst_row_y) * dst_block_width;

    for (uint32_t dst_x = 0;
         dst_x < dst_block_width;
         dst_x += kLaneCount)
    {
        QuadrantMeans regions[4];

        const uint32_t valid_lanes =
            std::min(kLaneCount, dst_block_width - dst_x);

        // Only fill the lanes that map to a real destination block. The
        // remaining lanes stay zero-initialised; their results are discarded.
        for (uint32_t lane = 0; lane < valid_lanes; ++lane)
        {
            const uint32_t block_x = dst_x + lane;

            const uint32_t dst_texel_base_x = block_x * 4;
            const uint32_t dst_texel_base_y = dst_row_y * 4;

            for (uint32_t local_y = 0; local_y < 4; ++local_y)
            {
                for (uint32_t local_x = 0; local_x < 4; ++local_x)
                {
                    const uint32_t target_x =
                        dst_texel_base_x + local_x;
                    const uint32_t target_y =
                        dst_texel_base_y + local_y;

                    const LinearBlockMean &color =
                        FetchLinearMeanTexel(
                            src_texels,
                            src_width,
                            src_height,
                            target_x,
                            target_y);

                    const uint32_t region =
                        ((local_y >> 1) << 1) + (local_x >> 1);

                    const uint32_t sample =
                        ((local_y & 1) << 1) + (local_x & 1);

                    SetQuadrantSample(
                        regions[region],
                        sample,
                        lane,
                        color);
                }
            }
        }

        SourceBlockMeans unused_source_means;

        const SymbolicDataBC1x4 encoded =
            EncodeLinearBlocksBC1x4(
                regions[0],
                regions[1],
                regions[2],
                regions[3],
                unused_source_means);

        for (uint32_t lane = 0; lane < valid_lanes; ++lane)
        {
            SymbolicDataBC1 &result =
                dst[dst_row + dst_x + lane];

            result.color_0 =
                static_cast<ushort>(Lane(encoded.color_0, lane));
            result.color_1 =
                static_cast<ushort>(Lane(encoded.color_1, lane));
            result.selectors =
                Lane(encoded.selectors, lane);
        }
    }
}
