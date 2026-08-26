#pragma once

#include "types.h"
#include "mipmap_layout.h"

// Context containing all information needed by the backend (CPU/GPU) to generate mipmaps
struct TextureContext
{
    const uint8_t *src_raw_blocks; // Pointer to original (Level 0) compressed data array
    uint32_t src_width;            // Original pixel width
    uint32_t src_height;           // Original pixel height
    TextureFormat format;          // Compression format (e.g., BC1, BC7)

    // Layout containing pre-allocated memory for sub-mipmaps (Level 1~N)
    MipmapLayout *layout;
};

// Common interface for all backends (Polymorphism)
class IMipmapBackend
{
public:
    virtual ~IMipmapBackend() = default;

    // Executes the generation of the entire mipmap chain. Returns true on success.
    virtual bool GenerateChain(TextureContext &context) = 0;
};
