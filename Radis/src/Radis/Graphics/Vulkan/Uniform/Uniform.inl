/*****************************************************************//**
 * \file   Uniform.inl
 * \author Adi (aditya.prakash@digipen.edu)
 * \date   October 27 2024
 * \Copyright @ 2024 Digipen (USA) Corporation *

 * \brief  Uniform template definitions
 *  *********************************************************************/

#pragma once
#include "Uniform.h"
#include "../Core/Buffer.h"
#include "../Core/AccelerationStructures.h"

namespace Radis 
{
    template<typename T>
    void Uniform::SetUniformData(const T& data, int bindingIndex, int frameIndex)
    {
        auto& buffer = mBuffersPerBinding[bindingIndex][frameIndex];
        memcpy(buffer.mapping, &data, sizeof(T));

    }

    template<typename T>
    void Uniform::SetUniformData(const std::vector<T>& data, int bindingIndex, int frameIndex)
    {
        auto& buffer = mBuffersPerBinding[bindingIndex][frameIndex];
        memcpy(buffer.mapping, data.data(), data.size() * sizeof(T));
    }

    template<typename T>
    void Uniform::SetUniformData(const std::vector<T>& data, int bindingIndex, int frameIndex, int count)
    {
        auto& buffer = mBuffersPerBinding[bindingIndex][frameIndex];
        memcpy(buffer.mapping, data.data(), count * sizeof(T));
    }

    template<typename T, std::size_t N>
    void Uniform::SetUniformData(const std::array<T, N>& data, int bindingIndex, int frameIndex)
    {
        auto& buffer = mBuffersPerBinding[bindingIndex][frameIndex];
        memcpy(buffer.mapping, data.data(), N * sizeof(T));
    }

} // namespace Dog
