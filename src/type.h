#ifndef __TYPE_HEADER__
#define __TYPE_HEADER__

#include <stdint.h>
#include <iostream>

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

// DDS file and BC block
constexpr uint32_t kDdsMagic = 0x20534444;

struct DdsPixelFormat
{
    uint32_t size;
    uint32_t flags;
    uint32_t four_cc;
    uint32_t rgb_bit_count;
    uint32_t r_bit_mask;
    uint32_t g_bit_mask;
    uint32_t b_bit_mask;
    uint32_t a_bit_mask;
};

struct DdsHeader
{
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

struct DdsHeaderDxt10
{
    uint32_t dxgi_format;
    uint32_t resource_dimension;
    uint32_t misc_flag;
    uint32_t array_size;
    uint32_t misc_flags_2;
};

struct Bc1Block
{
    uint16_t color_0;
    uint16_t color_1;
    uint32_t selectors; 
};

#endif