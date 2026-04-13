/*****************************************************************//**
 * \file   AssetSystem.h
 * \brief  Central async asset loading system.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "AssetTypes.h"
#include "AssetLoader.h"
#include "AssetEntry.h"
#include "ThreadPool.h"

#include "../IResource.h"

namespace Radis
{
    struct AssetSystemStats
    {
        uint32_t total = 0;
        uint32_t workerCount = 0;
        uint32_t activeWorkers = 0;
        uint32_t queueDepth = 0;

        bool allSettled() const { return queueDepth == 0 && activeWorkers == 0; }
    };

    class AssetResource : public IResource
    {
    public:
        AssetResource(uint32_t workerThreads = 0);
        ~AssetResource();

        AssetResource(const AssetResource&) = delete;
        AssetResource& operator=(const AssetResource&) = delete;

        // --- Setup ----------------------------------------------------------

        template<typename T>
        void registerLoader(std::unique_ptr<AssetLoader<T>> loader);

        template<typename T>
        void registerPlaceholder(std::shared_ptr<T> placeholder);

        // --- Load -----------------------------------------------------------
        template<typename T>
        AssetID load(const std::string& path, AssetPriority priority = AssetPriority::Normal);

        template<typename T>
        AssetID load(AssetID id, AssetPriority priority = AssetPriority::Normal);

        template<typename T>
        AssetID loadFromMemory(const std::string& virtualName, std::vector<unsigned char> data, AssetPriority priority = AssetPriority::Normal);

        template<typename T>
        AssetID find(const std::string& path);

        void waitAll();

        // --- Per-frame tick (main thread) ------------------------------------

        void dispatchCallbacks();

        // --- Serialization --------------------------------------------------

        std::string pathForID(AssetID id) const;

        // --- Debug ----------------------------------------------------------

        AssetSystemStats stats();
        void             logStatus();

    private:
        template<typename T>
        struct TypedStore
        {
            std::unique_ptr<AssetLoader<T>>                  loader;
            std::shared_ptr<T>                               placeholder;
            std::deque<AssetEntry<T>>                        entries;
            std::unordered_map<std::string, AssetEntry<T>*>  pathMap;
            std::unordered_map<AssetID, AssetEntry<T>*>      idMap;
            mutable std::shared_mutex                        mutex;
        };

        struct FinalizeItem
        {
            std::function<void()> run;
        };

        template<typename T>
        AssetID loadInternal(const std::string& path, AssetPriority priority);

        template<typename T>
        TypedStore<T>& getStore();

        template<typename T>
        TypedStore<T>* findStore();

        mutable std::mutex       mIDMutex;
        std::atomic<uint32_t>    mNextID{ 0 };
        std::vector<std::string> mIDToPath;

        std::unordered_map<std::type_index, std::unique_ptr<void, std::function<void(void*)>>> mStores;
        std::mutex mStoresMutex;

        std::queue<FinalizeItem> mFinalizeQueue;
        std::mutex               mFinalizeQueueMutex;

        ThreadPool mThreadPool;
    };


    // =========================================================================
    // Template implementations
    // =========================================================================

    template<typename T>
    void AssetResource::registerLoader(std::unique_ptr<AssetLoader<T>> loader)
    {
        getStore<T>().loader = std::move(loader);
    }

    template<typename T>
    void AssetResource::registerPlaceholder(std::shared_ptr<T> placeholder)
    {
        getStore<T>().placeholder = std::move(placeholder);
    }

    template<typename T>
    AssetID AssetResource::load(const std::string& path, AssetPriority priority)
    {
        return loadInternal<T>(path, priority);
    }

    template<typename T>
    AssetID AssetResource::load(AssetID id, AssetPriority priority)
    {
        std::string path;
        {
            std::lock_guard lock(mIDMutex);
            if (id >= static_cast<uint32_t>(mIDToPath.size()) || mIDToPath[id].empty())
            {
                RADIS_WARN("AssetSystem::load — unknown AssetID {}", id);
                return INVALID_ASSET_ID;
            }
            path = mIDToPath[id];
        }
        return loadInternal<T>(path, priority);
    }

    template<typename T>
    AssetID AssetResource::loadFromMemory(const std::string& virtualName, std::vector<unsigned char> data, AssetPriority priority)
    {
        auto& store = getStore<T>();

        // Fast path
        {
            std::shared_lock readLock(store.mutex);
            auto it = store.pathMap.find(virtualName);
            if (it != store.pathMap.end()) return it->second->id;
        }

        // Slow path; new entry
        AssetEntry<T>* entry = nullptr;
        {
            std::unique_lock writeLock(store.mutex);

            auto it = store.pathMap.find(virtualName);
            if (it != store.pathMap.end())
                return it->second->id;

            // Assign global AssetID.
            AssetID id;
            {
                std::lock_guard idLock(mIDMutex);
                id = mNextID.fetch_add(1, std::memory_order_relaxed);
                if (id >= static_cast<uint32_t>(mIDToPath.size()))
                    mIDToPath.resize(id + 1);
                mIDToPath[id] = virtualName;
            }

            store.entries.emplace_back();
            entry = &store.entries.back();
            entry->id = id;
            entry->path = virtualName;
            entry->placeholder = store.placeholder;
            entry->priority.store(priority, std::memory_order_relaxed);
            
            store.pathMap[virtualName] = entry;
            store.idMap[id] = entry;

            entry->state.store(AssetState::Queued, std::memory_order_release);
        }

        TypedStore<T>* storePtr = &store;

        // Submit to thread pool. Notice we capture `data` by move into `payload`.
        // The lambda must be marked `mutable` so we can move `payload` out of it later.
        mThreadPool.submit([this, entry, virtualName, storePtr, payload = std::move(data)]() mutable
        {
            entry->state.store(AssetState::Loading, std::memory_order_release);

            // Move the payload into the context so the loader takes ownership
            LoadContext ctx{ *this, entry->id, virtualName, entry->priority.load(std::memory_order_relaxed), std::move(payload) };

            std::string error;
            std::shared_ptr<T> assetData = storePtr->loader->Load(ctx, error);

            if (!assetData)
            {
                entry->markFailed(std::move(error));
                return;
            }

            entry->state.store(AssetState::Finalizing, std::memory_order_release);

            std::lock_guard queueLock(mFinalizeQueueMutex);
            mFinalizeQueue.push({
                [this, entry, assetData = std::move(assetData), storePtr]() mutable
                {
                    storePtr->loader->Finalize(ecs, entry->id, *assetData);
                    entry->markLoaded(std::move(assetData));
                    entry->drainCallbacks();
                }
            });
        }, priority);

        return entry->id;
    }

    template<typename T>
    AssetID AssetResource::find(const std::string& path)
    {
        TypedStore<T>* store = findStore<T>();
        if (!store) return INVALID_ASSET_ID;
        std::shared_lock lock(store->mutex);
        auto it = store->pathMap.find(path);
        if (it == store->pathMap.end()) return INVALID_ASSET_ID;
        return it->second->id;
    }

    template<typename T>
    AssetID AssetResource::loadInternal(const std::string& path, AssetPriority priority)
    {
        auto& store = getStore<T>();

        // Fast path — already known.
        {
            std::shared_lock readLock(store.mutex);
            auto it = store.pathMap.find(path);
            if (it != store.pathMap.end())
                return it->second->id;
        }

        // Slow path — new entry. Write lock closes the dedup race.
        AssetEntry<T>* entry = nullptr;
        {
            std::unique_lock writeLock(store.mutex);

            auto it = store.pathMap.find(path);
            if (it != store.pathMap.end())
                return it->second->id;

            // Assign AssetID.
            AssetID id;
            {
                std::lock_guard idLock(mIDMutex);
                id = mNextID.fetch_add(1, std::memory_order_relaxed);
                if (id >= static_cast<uint32_t>(mIDToPath.size()))
                    mIDToPath.resize(id + 1);
                mIDToPath[id] = path;
            }

            store.entries.emplace_back();
            entry = &store.entries.back();
            entry->id = id;
            entry->path = path;
            entry->placeholder = store.placeholder;
            entry->priority.store(priority, std::memory_order_relaxed);

            store.pathMap[path] = entry;
            store.idMap[id] = entry;

            if (!std::filesystem::exists(path))
            {
                entry->markMissing();
                return entry->id;
            }

            entry->state.store(AssetState::Queued, std::memory_order_release);
        }

        TypedStore<T>* storePtr = &store;

        mThreadPool.submit([this, entry, path, storePtr]()
        {
            entry->state.store(AssetState::Loading, std::memory_order_release);

            LoadContext ctx{ *this, entry->id, path, entry->priority.load(std::memory_order_relaxed) };
            std::string error;
            std::shared_ptr<T> data = storePtr->loader->Load(ctx, error);

            if (!data)
            {
                entry->markFailed(std::move(error));
                return;
            }

            entry->state.store(AssetState::Finalizing, std::memory_order_release);

            std::lock_guard queueLock(mFinalizeQueueMutex);
            mFinalizeQueue.push({
                [this, entry, data = std::move(data), storePtr]() mutable
                {
                    storePtr->loader->Finalize(ecs, entry->id, *data);
                    entry->markLoaded(std::move(data));
                    entry->drainCallbacks();
                }
            });
        }, priority);

        return entry->id;
    }

    template<typename T>
    AssetResource::TypedStore<T>& AssetResource::getStore()
    {
        std::type_index key(typeid(T));
        std::lock_guard lock(mStoresMutex);
        auto it = mStores.find(key);
        if (it != mStores.end())
            return *static_cast<TypedStore<T>*>(it->second.get());
        auto* raw = new TypedStore<T>();
        mStores.emplace(key, std::unique_ptr<void, std::function<void(void*)>>(
            raw, [](void* p) { delete static_cast<TypedStore<T>*>(p); }
        ));
        return *raw;
    }

    template<typename T>
    AssetResource::TypedStore<T>* AssetResource::findStore()
    {
        std::type_index key(typeid(T));
        std::lock_guard lock(mStoresMutex);
        auto it = mStores.find(key);
        if (it == mStores.end()) return nullptr;
        return static_cast<TypedStore<T>*>(it->second.get());
    }

} // namespace Radis