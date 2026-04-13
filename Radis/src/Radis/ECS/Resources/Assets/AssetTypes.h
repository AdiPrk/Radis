/*****************************************************************//**
 * \file   AssetTypes.h
 * \brief  Core type definitions for the asset system.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

namespace Radis
{
    using AssetID = uint32_t;
    static constexpr AssetID INVALID_ASSET_ID = UINT32_MAX;

    enum class AssetState : uint8_t
    {
        Unknown,
        Queued,
        Loading,
        Finalizing,
        Loaded,
        Failed,
        Missing,
    };

    inline bool IsInProgress(AssetState s)
    {
        return s == AssetState::Queued || s == AssetState::Loading || s == AssetState::Finalizing;
    }

    inline bool IsError(AssetState s)
    {
        return s == AssetState::Failed || s == AssetState::Missing;
    }

    enum class AssetPriority : uint8_t
    {
        Low = 0,
        Normal = 1,
        High = 2,
        Critical = 3,
    };

} // namespace Radis