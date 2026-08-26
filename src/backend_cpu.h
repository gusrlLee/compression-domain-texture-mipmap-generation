#pragma once

#include "backend.h"

// CPU backend implementation for generating mipmaps using multi-threading
class CpuBackend : public IMipmapBackend
{
public:
    CpuBackend() = default;
    virtual ~CpuBackend() = default;

    bool GenerateChain(TextureContext &context) override;
};
