#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "mipmap_layout.h"
#include "types.h"

// Writes a 2D BC texture containing the base level followed by every level in
// mipmap_layout. The DX10 DDS header can represent every project format.
bool WriteDds(const std::string &file_path,
              Extent2D extent,
              TextureFormat format,
              const std::vector<uint8_t> &base_level_blocks,
              const MipmapLayout &mipmap_layout);
