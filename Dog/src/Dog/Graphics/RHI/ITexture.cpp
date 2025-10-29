#include <PCH/pch.h>
#include "ITexture.h"

namespace Dog
{
    ITexture::ITexture()
        : mWidth(0)
        , mHeight(0)
        , mChannels(0)
        , mImageSize(0)
    {
    }

    ITexture::~ITexture()
    {
    }
}
