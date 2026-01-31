/*****************************************************************//**
 * \file   ITexture.cpp
 * \brief  Implementation of the ITexture interface for texture resources.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

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
