#ifndef __TYPE_HEADER__
#define __TYPE_HEADER__

#include <stdint.h>
#include <iostream>
#include <vector>

struct Extent2D 
{
    uint32_t width;
    uint32_t height;
};  

struct TexelCoord
{
    uint32_t x;
    uint32_t y;
};

struct BlockCoord 
{
    uint32_t x;
    uint32_t y;
};

// ============================================================================
// Texture Format Enum and Utilities for Scalability (Excluding BC6H)
// ============================================================================
enum class TextureFormat {
    kUnknown,
    kBc1,
    kBc3,
    kBc4,
    kBc5,
    kBc7
};

inline uint32_t GetBlockSize(TextureFormat format) {
    switch (format) {
        case TextureFormat::kBc1:
        case TextureFormat::kBc4:
            return 8;   // BC1, BC4 use 8 bytes per 4x4 block
        case TextureFormat::kBc3:
        case TextureFormat::kBc5:
        case TextureFormat::kBc7:
            return 16;  // Others use 16 bytes per 4x4 block
        default:
            return 0;
    }
}

constexpr uint32_t kDdsMagic = 0x20534444;

struct DdsPixelFormat {
    uint32_t size;
    uint32_t flags;
    uint32_t four_cc;
    uint32_t rgb_bit_count;
    uint32_t r_bit_mask;
    uint32_t g_bit_mask;
    uint32_t b_bit_mask;
    uint32_t a_bit_mask;
};

struct DdsHeader {
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitch_or_linear_size;
    uint32_t depth;
    uint32_t mip_map_count;
    uint32_t reserved_1[11];
    DdsPixelFormat ddspf;
    uint32_t caps;
    uint32_t caps_2;
    uint32_t caps_3;
    uint32_t caps_4;
    uint32_t reserved_2;
};

struct DdsHeaderDxt10 {
    uint32_t dxgi_format;
    uint32_t resource_dimension;
    uint32_t misc_flag;
    uint32_t array_size;
    uint32_t misc_flags_2;
};

#pragma pack(push, 1)
struct Bc1Block {
    uint16_t color_0;
    uint16_t color_1;
    uint32_t selectors;
};
#pragma pack(pop)

#endif // __TYPE_HEADER__
