#include "backend_cpu.h"
#include "codec/bc1.h"
#include "task_dispatch.h"
#include <algorithm>
#include <iostream>
#include <thread>
#include <vector>

bool CpuBackend::GenerateChain(TextureContext &context)
{
    if (!context.layout || context.layout->num_levels() == 0)
    {
        std::cerr << "[CpuBackend] Invalid layout or no mipmaps to generate.\n";
        return false;
    }

    uint32_t num_threads = std::thread::hardware_concurrency();
    TaskDispatch dispatcher(num_threads);

    // Set Level 0 data as the initial source
    const uint8_t *current_src_blocks = context.src_raw_blocks;
    uint32_t current_src_width = context.src_width;
    uint32_t current_src_height = context.src_height;
    uint32_t block_stride = GetBlockSize(context.format);

    const uint32_t level0_block_width = (context.src_width + 3) / 4;
    const uint32_t level0_block_height = (context.src_height + 3) / 4;
    // M: the level-0 linear block-mean image (Theorem S input).
    // `mean_scratch` is the ping-pong target used to halve M once per level;
    // a quarter of M is large enough for every halving that follows.
    std::vector<LinearBlockMean> block_mean_image(static_cast<size_t>(level0_block_width) * level0_block_height);
    std::vector<LinearBlockMean> mean_scratch(
        static_cast<size_t>((level0_block_width + 1) / 2) * ((level0_block_height + 1) / 2));

    LinearBlockMean *mean_front = block_mean_image.data();
    LinearBlockMean *mean_back = mean_scratch.data();
    uint32_t mean_width = level0_block_width;
    uint32_t mean_height = level0_block_height;

    // 2. Mipmap Generation Loop (From Level 1 to the end)
    for (uint32_t i = 1; i <= context.layout->num_levels(); ++i)
    {
        // MipmapLayout::GetLevel(index) starts from 0, so we use i-1
        MipLevelData &dst_level = context.layout->GetLevel(i - 1);

        uint8_t *dst_blocks = dst_level.raw_blocks.data();
        uint32_t dst_block_width = dst_level.block_width;
        uint32_t dst_block_height = dst_level.block_height;
        const uint32_t src_block_width = (current_src_width + 3) / 4;
        const uint32_t src_block_height = (current_src_height + 3) / 4;

        LinearBlockMean *block_mean_output = (i == 1) ? block_mean_image.data() : nullptr;

        // Level 2 consumes M directly (s_2 = 1). Every later level consumes M
        // halved once more, which is the same box mean at a quarter of the cost.
        if (i >= 3)
        {
            const uint32_t next_mean_width = (mean_width + 1) / 2;
            const uint32_t next_mean_height = (mean_height + 1) / 2;

            for (uint32_t y = 0; y < next_mean_height; ++y)
            {
                TaskDispatch::Queue(
                    [mean_front, mean_width, mean_height,
                     mean_back, next_mean_width, y]()
                    {
                        DownsampleLinearMeanRow(
                            mean_front,
                            mean_width,
                            mean_height,
                            mean_back,
                            next_mean_width,
                            y);
                    });
            }
            TaskDispatch::Sync();

            std::swap(mean_front, mean_back);
            mean_width = next_mean_width;
            mean_height = next_mean_height;
        }

        // Split workloads by Y-axis and assign them to the thread pool.
        for (uint32_t y = 0; y < dst_block_height; ++y)
        {
            if (i >= 2)
            {
                const LinearBlockMean *mean_texels = mean_front;

                TaskDispatch::Queue(
                    [mean_texels,
                     mean_width,
                     mean_height,
                     dst_blocks,
                     dst_block_width,
                     y]()
                    {
                        // Generate this mip level from the linear mean image,
                        // never from an already encoded BC1 mip.
                        ProcessLinearRowBC1(
                            mean_texels,
                            mean_width,
                            mean_height,
                            dst_blocks,
                            dst_block_width,
                            y);
                    });
            }
            else
            {
                TaskDispatch::Queue(
                    [current_src_blocks,
                     src_block_width,
                     src_block_height,
                     dst_blocks,
                     dst_block_width,
                     block_mean_output,
                     y]()
                    {
                        ProcessRowBC1(
                            current_src_blocks,
                            src_block_width,
                            src_block_height,
                            dst_blocks,
                            dst_block_width,
                            y,
                            block_mean_output);
                    });
            }
        }
        // Wait until all rows in the current level are generated (Prevents Data Hazard)
        TaskDispatch::Sync();

        // Swap destination (dst) to source (src) for the next level calculation
        current_src_blocks = dst_blocks;
        current_src_width = dst_level.width;
        current_src_height = dst_level.height;
    }

    return true;
}
