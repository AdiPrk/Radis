/*****************************************************************//**
 * \file   AssetSystem.cpp
 * \brief  Non-template implementation of AssetSystem.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "AssetResource.h"
#include "Graphics/RHI/ITexture.h"
#include "Graphics/Common/TextureLoader.h"

namespace Radis
{
    AssetResource::AssetResource(uint32_t workerThreads)
        : mThreadPool(workerThreads)
    {
        registerLoader<TextureData>(std::make_unique<TextureLoader>());

        // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "ErrorTexture.png");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "circle.png");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "dog.png");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "circleOutline2.png");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "dogmodel.png");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "error.png");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "square.png");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "glslIcon.png");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "playButton.png");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "stopButton.png");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "texture.jpg");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "folderIcon.png");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "unknownFileIcon.png");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "shikaout.ktx2");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "autumn_field_puresky_4k.hdr");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "Alexs_Apt_2k.hdr");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "Newport_Loft_Ref.hdr");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "autumn_field_puresky_4k.IRMAP.hdr");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "Alexs_Apt_2k.IRMAP.hdr");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "Newport_Loft_Ref.IRMAP.hdr");

        load<TextureData>(Assets::ImagesPath + "ErrorTexture.png");
    }

    AssetResource::~AssetResource()
    {
        waitAll();
    }

    void AssetResource::dispatchCallbacks()
    {
        std::queue<FinalizeItem> local;
        {
            std::lock_guard lock(mFinalizeQueueMutex);
            local.swap(mFinalizeQueue);
        }
        while (!local.empty())
        {
            local.front().run();
            local.pop();
        }
    }

    void AssetResource::waitAll()
    {
        mThreadPool.waitAll();
        dispatchCallbacks();
    }

    std::string AssetResource::pathForID(AssetID id) const
    {
        std::lock_guard lock(mIDMutex);
        if (id >= static_cast<uint32_t>(mIDToPath.size())) return {};
        return mIDToPath[id];
    }

    AssetSystemStats AssetResource::stats()
    {
        AssetSystemStats s;
        s.workerCount = mThreadPool.threadCount();
        s.activeWorkers = mThreadPool.activeCount();
        s.queueDepth = mThreadPool.queueDepth();
        {
            std::lock_guard lock(mIDMutex);
            s.total = static_cast<uint32_t>(mIDToPath.size());
        }
        return s;
    }

    void AssetResource::logStatus()
    {
        const auto s = stats(); (void)s;
        RADIS_INFO("{}/{} workers active | {} jobs queued | {} assets registered", s.activeWorkers, s.workerCount, s.queueDepth, s.total);
    }

} // namespace Radis