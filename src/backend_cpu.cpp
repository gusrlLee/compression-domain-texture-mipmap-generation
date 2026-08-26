#include "backend_cpu.h"
#include "codec/bc1.h"
#include "task_dispatch.h"
#include <iostream>
#include <thread>

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

        // Split workloads by Y-axis (Row) and assign to the thread pool
        for (uint32_t y = 0; y < dst_block_height; ++y)
        {

            TaskDispatch::Queue([current_src_blocks, src_block_width, src_block_height,
                                 dst_blocks, dst_block_width, y]()
                                { ProcessRowBC1(current_src_blocks, src_block_width, src_block_height,
                                                dst_blocks, dst_block_width, y); });
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
