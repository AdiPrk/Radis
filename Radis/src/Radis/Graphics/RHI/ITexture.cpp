#include <PCH/pch.h>
#include "ITexture.h"

namespace Radis
{
    ITexture::ITexture(const TextureData& data)
        : mData(data)
    {
    }

    ITexture::~ITexture()
    {
    }
}
