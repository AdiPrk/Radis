#include <PCH/pch.h>
#include "ITexture.h"

namespace Dog
{
    ITexture::ITexture(const TextureData& data)
        : mData(data)
    {
    }

    ITexture::~ITexture()
    {
    }
}
