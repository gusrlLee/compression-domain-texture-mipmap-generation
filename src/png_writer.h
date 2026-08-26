#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "types.h"

// Writes tightly packed, row-major RGB8 pixels to a PNG file.
bool WriteRgb8Png(const std::string &file_path,
                  Extent2D extent,
                  const std::vector<uint8_t> &pixels);
