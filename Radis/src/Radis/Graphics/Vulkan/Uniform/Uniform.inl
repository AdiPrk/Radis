/*****************************************************************//**
 * \file   Uniform.inl
 * \brief  Implementation of the Uniform class template methods for setting uniform data.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

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

} // namespace Radis
