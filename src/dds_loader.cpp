#include "dds_loader.h"

#include <fstream>
#include <iostream>

#define MAKE_FOUR_CC(ch0, ch1, ch2, ch3)                        \
    ((static_cast<uint32_t>(static_cast<uint8_t>(ch0))) |       \
     (static_cast<uint32_t>(static_cast<uint8_t>(ch1)) << 8) |  \
     (static_cast<uint32_t>(static_cast<uint8_t>(ch2)) << 16) | \
     (static_cast<uint32_t>(static_cast<uint8_t>(ch3)) << 24))

bool DdsLoader::Load(const std::string &file_path)
{
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "[DdsLoader] Failed to open file: " << file_path << "\n";
        return false;
    }

    uint32_t magic = 0;
    file.read(reinterpret_cast<char *>(&magic), sizeof(magic));
    if (magic != kDdsMagic)
    {
        std::cerr << "[DdsLoader] Invalid DDS magic number.\n";
        return false;
    }

    DdsHeader header;
    file.read(reinterpret_cast<char *>(&header), sizeof(DdsHeader));

    width_ = header.width;
    height_ = header.height;
    mip_map_count_ = header.mip_map_count;
    format_ = TextureFormat::kUnknown;

    if (header.ddspf.flags & 0x00000004)
    {
        if (header.ddspf.four_cc == MAKE_FOUR_CC('D', 'X', 'T', '1'))
        {
            format_ = TextureFormat::kBc1;
        }
        else if (header.ddspf.four_cc == MAKE_FOUR_CC('D', 'X', '1', '0'))
        {
            DdsHeaderDxt10 header_dx10;
            file.read(reinterpret_cast<char *>(&header_dx10), sizeof(DdsHeaderDxt10));

            switch (header_dx10.dxgi_format)
            {
            case 71:
            case 72:
                format_ = TextureFormat::kBc1;
                break;
            case 77:
            case 78:
                format_ = TextureFormat::kBc3;
                break;
            case 80:
            case 81:
                format_ = TextureFormat::kBc4;
                break;
            case 83:
            case 84:
                format_ = TextureFormat::kBc5;
                break;
            case 98:
            case 99:
                format_ = TextureFormat::kBc7;
                break;
            }
        }
    }

    if (format_ == TextureFormat::kUnknown)
    {
        std::cerr << "[DdsLoader] Unsupported or unknown DDS format (BC6H is explicitly excluded).\n";
        return false;
    }

    auto current_pos = file.tellg();
    file.seekg(0, std::ios::end);
    auto file_size = file.tellg();
    file.seekg(current_pos, std::ios::beg);

    auto data_size = file_size - current_pos;
    if (data_size > 0)
    {
        raw_blocks_.resize(static_cast<size_t>(data_size));
        file.read(reinterpret_cast<char *>(raw_blocks_.data()), data_size);
    }

    uint32_t block_size = GetBlockSize(format_);
    std::cout << "[DdsLoader] Loaded Format: " << static_cast<int>(format_)
              << ", Size: " << width_ << "x" << height_
              << ", Blocks: " << raw_blocks_.size() / block_size << "\n";

    return true;
}