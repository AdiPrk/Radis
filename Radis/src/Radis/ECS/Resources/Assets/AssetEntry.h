/*****************************************************************//**
 * \file   AssetEntry.h
 * \brief  Internal per-asset state stored in the registry.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "AssetTypes.h"

namespace Radis
{
    template<typename T>
    struct AssetEntry
    {
        AssetID id = INVALID_ASSET_ID;
        std::string path;

        std::atomic<AssetState> state{ AssetState::Unknown };
        std::atomic<AssetPriority> priority{ AssetPriority::Normal };

        std::shared_ptr<T> data;
        std::shared_ptr<T> placeholder;

        std::string errorMessage;

        std::mutex                            callbackMutex;
        std::vector<std::function<void(T&)>>  callbacks;

        T& resolve() const
        {
            if (state.load(std::memory_order_acquire) == AssetState::Loaded && data)
                return *data;
            return *placeholder;
        }

        void markLoaded(std::shared_ptr<T> loaded)
        {
            data = std::move(loaded);
            state.store(AssetState::Loaded, std::memory_order_release);
        }

        void markFailed(std::string error)
        {
            errorMessage = std::move(error);
            state.store(AssetState::Failed, std::memory_order_release);
        }

        void markMissing()
        {
            state.store(AssetState::Missing, std::memory_order_release);
        }

        void addCallback(std::function<void(T&)> cb)
        {
            std::lock_guard lock(callbackMutex);
            callbacks.push_back(std::move(cb));
        }

        void drainCallbacks()
        {
            std::vector<std::function<void(T&)>> toCall;
            {
                std::lock_guard lock(callbackMutex);
                toCall.swap(callbacks);
            }
            if (data)
                for (auto& cb : toCall)
                    cb(*data);
        }

        AssetEntry() = default;
        ~AssetEntry() = default;
        AssetEntry(const AssetEntry&) = delete;
        AssetEntry& operator=(const AssetEntry&) = delete;
    };
} // namespace Radis