/*****************************************************************//**
 * \file   AssetLoader.h
 * \brief  Abstract base for per-type asset loaders.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "AssetTypes.h"

namespace Radis
{
    class AssetResource;
    
    struct LoadContext
    {
        AssetResource& assets;
        AssetID                     id;
        std::string                 path;
        AssetPriority               priority = AssetPriority::Normal;
        std::vector<unsigned char>  payload;
    };

    template<typename T>
    class AssetLoader
    {
    public:
        virtual ~AssetLoader() = default;

        // Runs on worker thread
        virtual std::shared_ptr<T> Load(LoadContext& ctx, std::string& errorOut) = 0;

        // Runs on main thread
        virtual void Finalize(class ECS* ecs, AssetID id, T& asset) {}
    };

} // namespace Radis