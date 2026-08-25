#ifndef __DDS_LOADER_HEADER__
#define __DDS_LOADER_HEADER__

#include <stdint.h>
#include <filesystem>
#include <vector>
#include <string>

#include "type.h"

class DdsLoader
{
public:
    DdsLoader() = default;
    ~DdsLoader() = default;

    bool Load(const std::string &file_path);

    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    uint32_t mip_map_count() const { return mip_map_count_; }

    const std::vector<Bc1Block> &bc1_blocks() const { return bc1_blocks_; }

private:
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t mip_map_count_ = 0;

    std::vector<Bc1Block> bc1_blocks_;
};

#endif // __DDS_LOADER_HEADER__