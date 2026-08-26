#include "dds_writer.h"

#include <fstream>
#include <iostream>
#include <limits>

namespace
{
constexpr uint32_t kDdsdCaps = 0x00000001;
constexpr uint32_t kDdsdHeight = 0x00000002;
constexpr uint32_t kDdsdWidth = 0x00000004;
constexpr uint32_t kDdsdPixelFormat = 0x00001000;
constexpr uint32_t kDdsdMipmapCount = 0x00020000;
constexpr uint32_t kDdsdLinearSize = 0x00080000;
constexpr uint32_t kDdpfFourCc = 0x00000004;
constexpr uint32_t kDdsCapsComplex = 0x00000008;
constexpr uint32_t kDdsCapsTexture = 0x00001000;
constexpr uint32_t kDdsCapsMipmap = 0x00400000;
constexpr uint32_t kDx10FourCc = 0x30315844;
constexpr uint32_t kD3d10ResourceDimensionTexture2D = 3;

uint32_t GetDxgiFormat(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::kBc1:
        return 71; // DXGI_FORMAT_BC1_UNORM
    case TextureFormat::kBc3:
        return 77; // DXGI_FORMAT_BC3_UNORM
    case TextureFormat::kBc4:
        return 80; // DXGI_FORMAT_BC4_UNORM
    case TextureFormat::kBc5:
        return 83; // DXGI_FORMAT_BC5_UNORM
    case TextureFormat::kBc7:
        return 98; // DXGI_FORMAT_BC7_UNORM
    default:
        return 0;
    }
}

size_t GetLevelSize(uint32_t width, uint32_t height, uint32_t block_size)
{
    const size_t block_width = (static_cast<size_t>(width) + 3) / 4;
    const size_t block_height = (static_cast<size_t>(height) + 3) / 4;
    return block_width * block_height * block_size;
}

bool WriteBytes(std::ofstream &file, const uint8_t *data, size_t size)
{
    if (size > static_cast<size_t>(std::numeric_limits<std::streamsize>::max()))
        return false;

    file.write(reinterpret_cast<const char *>(data),
               static_cast<std::streamsize>(size));
    return static_cast<bool>(file);
}
} // namespace

bool WriteDds(const std::string &file_path,
              Extent2D extent,
              TextureFormat format,
              const std::vector<uint8_t> &base_level_blocks,
              const MipmapLayout &mipmap_layout)
{
    const uint32_t block_size = GetBlockSize(format);
    const uint32_t dxgi_format = GetDxgiFormat(format);

    if (extent.width == 0 || extent.height == 0)
    {
        std::cerr << "[DdsWriter] Texture extent must not be zero.\n";
        return false;
    }

    if (block_size == 0 || dxgi_format == 0)
    {
        std::cerr << "[DdsWriter] Unsupported texture format.\n";
        return false;
    }

    if (mipmap_layout.base_width() != extent.width ||
        mipmap_layout.base_height() != extent.height ||
        mipmap_layout.format() != format)
    {
        std::cerr << "[DdsWriter] Mipmap layout does not match the base texture.\n";
        return false;
    }

    const size_t level_0_size =
        GetLevelSize(extent.width, extent.height, block_size);
    if (base_level_blocks.size() != level_0_size)
    {
        std::cerr << "[DdsWriter] Base-level data size is invalid.\n";
        return false;
    }

    for (uint32_t index = 0; index < mipmap_layout.num_levels(); ++index)
    {
        const MipLevelData &level = mipmap_layout.GetLevel(index);
        const size_t expected_size =
            GetLevelSize(level.width, level.height, block_size);
        if (level.raw_blocks.size() != expected_size)
        {
            std::cerr << "[DdsWriter] Mipmap level " << index + 1
                      << " has an invalid data size.\n";
            return false;
        }
    }

    if (level_0_size > std::numeric_limits<uint32_t>::max())
    {
        std::cerr << "[DdsWriter] Base level is too large for a DDS header.\n";
        return false;
    }

    const uint32_t mip_count = mipmap_layout.num_levels() + 1;

    DdsHeader header{};
    header.size = sizeof(DdsHeader);
    header.flags = kDdsdCaps | kDdsdHeight | kDdsdWidth |
                   kDdsdPixelFormat | kDdsdLinearSize;
    header.height = extent.height;
    header.width = extent.width;
    header.pitch_or_linear_size = static_cast<uint32_t>(level_0_size);
    header.mip_map_count = mip_count;
    header.ddspf.size = sizeof(DdsPixelFormat);
    header.ddspf.flags = kDdpfFourCc;
    header.ddspf.four_cc = kDx10FourCc;
    header.caps = kDdsCapsTexture;

    if (mip_count > 1)
    {
        header.flags |= kDdsdMipmapCount;
        header.caps |= kDdsCapsComplex | kDdsCapsMipmap;
    }

    DdsHeaderDxt10 dx10_header{};
    dx10_header.dxgi_format = dxgi_format;
    dx10_header.resource_dimension = kD3d10ResourceDimensionTexture2D;
    dx10_header.array_size = 1;

    static_assert(sizeof(DdsPixelFormat) == 32, "Unexpected DDS pixel format size");
    static_assert(sizeof(DdsHeader) == 124, "Unexpected DDS header size");
    static_assert(sizeof(DdsHeaderDxt10) == 20, "Unexpected DDS DX10 header size");

    std::ofstream file(file_path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        std::cerr << "[DdsWriter] Failed to create file: " << file_path << "\n";
        return false;
    }

    file.write(reinterpret_cast<const char *>(&kDdsMagic), sizeof(kDdsMagic));
    file.write(reinterpret_cast<const char *>(&header), sizeof(header));
    file.write(reinterpret_cast<const char *>(&dx10_header), sizeof(dx10_header));

    if (!file || !WriteBytes(file, base_level_blocks.data(), base_level_blocks.size()))
    {
        std::cerr << "[DdsWriter] Failed to write file: " << file_path << "\n";
        return false;
    }

    for (uint32_t index = 0; index < mipmap_layout.num_levels(); ++index)
    {
        const std::vector<uint8_t> &blocks =
            mipmap_layout.GetLevel(index).raw_blocks;
        if (!WriteBytes(file, blocks.data(), blocks.size()))
        {
            std::cerr << "[DdsWriter] Failed to write file: " << file_path << "\n";
            return false;
        }
    }

    return true;
}
