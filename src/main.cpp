#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "dds_loader.h"
#include "dds_writer.h"
#include "mipmap_layout.h"
#include "backend_cpu.h"
#include "png_writer.h"

namespace
{
    double GetTime()
    {
        using Clock = std::chrono::steady_clock;
        return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
    }

    struct Rgb8
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    Rgb8 DecodeRgb565(uint16_t color)
    {
        const uint8_t r5 = static_cast<uint8_t>((color >> 11) & 0x1f);
        const uint8_t g6 = static_cast<uint8_t>((color >> 5) & 0x3f);
        const uint8_t b5 = static_cast<uint8_t>(color & 0x1f);
        return {
            static_cast<uint8_t>((r5 << 3) | (r5 >> 2)),
            static_cast<uint8_t>((g6 << 2) | (g6 >> 4)),
            static_cast<uint8_t>((b5 << 3) | (b5 >> 2)),
        };
    }

    bool WriteBc1Png(const std::filesystem::path &file_path,
                     Extent2D extent,
                     const std::vector<uint8_t> &blocks)
    {
        const size_t block_width = (static_cast<size_t>(extent.width) + 3) / 4;
        const size_t block_height = (static_cast<size_t>(extent.height) + 3) / 4;
        if (blocks.size() != block_width * block_height * sizeof(Bc1Block))
        {
            std::cerr << "Invalid BC1 data for PNG: " << file_path << "\n";
            return false;
        }

        std::vector<uint8_t> pixels(
            static_cast<size_t>(extent.width) * extent.height * 3);

        for (size_t block_y = 0; block_y < block_height; ++block_y)
        {
            for (size_t block_x = 0; block_x < block_width; ++block_x)
            {
                Bc1Block block{};
                const size_t block_offset =
                    (block_y * block_width + block_x) * sizeof(Bc1Block);
                std::memcpy(&block, blocks.data() + block_offset, sizeof(block));

                std::array<Rgb8, 4> palette{};
                palette[0] = DecodeRgb565(block.color_0);
                palette[1] = DecodeRgb565(block.color_1);

                if (block.color_0 > block.color_1)
                {
                    palette[2] = {
                        static_cast<uint8_t>((2u * palette[0].r + palette[1].r) / 3u),
                        static_cast<uint8_t>((2u * palette[0].g + palette[1].g) / 3u),
                        static_cast<uint8_t>((2u * palette[0].b + palette[1].b) / 3u),
                    };
                    palette[3] = {
                        static_cast<uint8_t>((palette[0].r + 2u * palette[1].r) / 3u),
                        static_cast<uint8_t>((palette[0].g + 2u * palette[1].g) / 3u),
                        static_cast<uint8_t>((palette[0].b + 2u * palette[1].b) / 3u),
                    };
                }
                else
                {
                    palette[2] = {
                        static_cast<uint8_t>((palette[0].r + palette[1].r) / 2u),
                        static_cast<uint8_t>((palette[0].g + palette[1].g) / 2u),
                        static_cast<uint8_t>((palette[0].b + palette[1].b) / 2u),
                    };
                    palette[3] = {0, 0, 0};
                }

                for (uint32_t local_y = 0; local_y < 4; ++local_y)
                {
                    const uint32_t y = static_cast<uint32_t>(block_y * 4) + local_y;
                    if (y >= extent.height)
                        continue;

                    for (uint32_t local_x = 0; local_x < 4; ++local_x)
                    {
                        const uint32_t x = static_cast<uint32_t>(block_x * 4) + local_x;
                        if (x >= extent.width)
                            continue;

                        const uint32_t texel_index = local_y * 4 + local_x;
                        const uint32_t selector =
                            (block.selectors >> (texel_index * 2)) & 0x3;
                        const Rgb8 color = palette[selector];
                        const size_t pixel_offset =
                            (static_cast<size_t>(y) * extent.width + x) * 3;
                        pixels[pixel_offset] = color.r;
                        pixels[pixel_offset + 1] = color.g;
                        pixels[pixel_offset + 2] = color.b;
                    }
                }
            }
        }

        return WriteRgb8Png(file_path.string(), extent, pixels);
    }
} // namespace

void Help()
{
    std::cout << "Usage: gen_mipmap <input_dds_file>\n";
    std::cout << "Example: gen_mipmap input/test_image.dds\n";
}

int main(int argc, char *argv[])
{
    // 1. Command line parsing
    if (argc < 2)
    {
        Help();
        return 1;
    }

    std::string input_file = argv[1];

    // 2. Read DDS file & 3. Checking Codec of BC
    DdsLoader loader;
    if (!loader.Load(input_file))
    {
        std::cerr << "Failed to load DDS file: " << input_file << "\n";
        return 1;
    }

    // 4. Checking Size of Resoluation
    std::cout << "Original Size : " << loader.width() << "x" << loader.height() << "\n";

    // 5. Allocate memory of texture mipmap according to mipmap size
    MipmapLayout layout;
    layout.Allocate(loader.width(), loader.height(), loader.format());

    // 6. Prepare multi-threading or GPU backend
    std::cout << "[Ready for Backend Dispatch]\n";

    TextureContext context;
    context.src_raw_blocks = loader.raw_blocks().data();
    context.src_width = loader.width();
    context.src_height = loader.height();
    context.format = loader.format();
    context.layout = &layout;

    CpuBackend cpu_backend;

    // 7. Generate texture mipmap
    const double generation_start = GetTime();
    if (!cpu_backend.GenerateChain(context))
    {
        std::cerr << "Failed to generate mipmap chain.\n";
        return 1;
    }
    const double generation_seconds = GetTime() - generation_start;
    uint64_t generated_pixels = 0;
    uint64_t generated_blocks = 0;
    for (uint32_t index = 0; index < layout.num_levels(); ++index)
    {
        const MipLevelData &level = layout.GetLevel(index);
        generated_pixels += static_cast<uint64_t>(level.width) * level.height;
        generated_blocks +=
            static_cast<uint64_t>(level.block_width) * level.block_height;
    }

    const double megapixels_per_second =
        static_cast<double>(generated_pixels) / generation_seconds / 1'000'000.0;
    const double megablocks_per_second =
        static_cast<double>(generated_blocks) / generation_seconds / 1'000'000.0;
        
    std::cout << std::fixed << std::setprecision(3)
              << "[Performance] Mipmap generation: "
              << generation_seconds * 1000.0 << " ms, "
              << megapixels_per_second << " MPixel/s, "
              << megablocks_per_second << " MBlock/s\n";

    // 8. Store generated mipmap data dds file and png files
    const size_t base_block_width =
        (static_cast<size_t>(loader.width()) + 3) / 4;
    const size_t base_block_height =
        (static_cast<size_t>(loader.height()) + 3) / 4;
    const size_t base_level_size =
        base_block_width * base_block_height * GetBlockSize(loader.format());

    if (loader.raw_blocks().size() < base_level_size)
    {
        std::cerr << "Invalid base-level texture data.\n";
        return 1;
    }

    const std::vector<uint8_t> base_level_blocks(
        loader.raw_blocks().begin(),
        loader.raw_blocks().begin() + base_level_size);

    const std::filesystem::path input_path(input_file);
    const std::filesystem::path output_directory =
        std::filesystem::path("output") / input_path.stem();

    std::error_code directory_error;
    std::filesystem::create_directories(output_directory, directory_error);
    if (directory_error)
    {
        std::cerr << "Failed to create output directory: "
                  << output_directory << " (" << directory_error.message() << ")\n";
        return 1;
    }

    const std::filesystem::path output_file =
        output_directory / (input_path.stem().string() + "_mipmap.dds");

    if (!WriteDds(
            output_file.string(),
            {loader.width(), loader.height()},
            loader.format(),
            base_level_blocks,
            layout))
    {
        std::cerr << "Failed to save DDS file: " << output_file << "\n";
        return 1;
    }

    if (loader.format() != TextureFormat::kBc1)
    {
        std::cerr << "PNG output currently supports only BC1 textures.\n";
        return 1;
    }

    std::filesystem::path png_file = output_directory / "mip0.png";
    if (!WriteBc1Png(
            png_file,
            {loader.width(), loader.height()},
            base_level_blocks))
    {
        return 1;
    }

    for (uint32_t index = 0; index < layout.num_levels(); ++index)
    {
        const MipLevelData &level = layout.GetLevel(index);
        png_file = output_directory /
                   ("mip" + std::to_string(index + 1) + ".png");
        
        if (!WriteBc1Png(png_file, {level.width, level.height}, level.raw_blocks))
        {
            return 1;
        }
    }

    // 9. exit
    std::cout << "Saved mipmap texture: " << output_file << "\n";

    return 0;
}
