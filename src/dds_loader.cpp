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

    bool is_bc1 = false;
    if (header.ddspf.flags & 0x00000004)
    {
        if (header.ddspf.four_cc == MAKE_FOUR_CC('D', 'X', 'T', '1'))
        {
            is_bc1 = true;
        }
        else if (header.ddspf.four_cc == MAKE_FOUR_CC('D', 'X', '1', '0'))
        {
            DdsHeaderDxt10 header_dx10;
            file.read(reinterpret_cast<char *>(&header_dx10), sizeof(DdsHeaderDxt10));
            if (header_dx10.dxgi_format == 71 || header_dx10.dxgi_format == 72)
            {
                is_bc1 = true;
            }
        }
    }

    if (!is_bc1)
    {
        std::cerr << "[DdsLoader] Currently only BC1 format is supported.\n";
        return false;
    }

    auto current_pos = file.tellg();
    file.seekg(0, std::ios::end);
    auto file_size = file.tellg();
    file.seekg(current_pos, std::ios::beg);

    auto data_size = file_size - current_pos;
    if (data_size > 0)
    {
        size_t block_count = static_cast<size_t>(data_size) / sizeof(Bc1Block);
        bc1_blocks_.resize(block_count);
        file.read(reinterpret_cast<char *>(bc1_blocks_.data()), data_size);
    }

    std::cout << "[DdsLoader] Loaded BC1. Size: " << width_ << "x" << height_
              << ", Blocks: " << bc1_blocks_.size() << "\n";

    return true;
}