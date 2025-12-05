#pragma once

namespace Dog
{
    class Device;
    struct RenderingResource;

    class DescriptorSetLayout
    {
    public:

        //Class for building Descriptor set layouts
        class Builder
        {
        public:
            Builder(Device& device)
                : mDevice{ device }
            {
            }

            Builder& AddBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t count = 1);
            Builder& AddBinding(VkDescriptorSetLayoutBinding binding);

            std::unique_ptr<DescriptorSetLayout> Build() const;

        private:
            
            Device& mDevice; //Device descriptors are being made for
            std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> mBindings{}; //Map of building used to build a descriptorSetLayout
        };

        //Delete copy operators
        DescriptorSetLayout(const DescriptorSetLayout&) = delete;
        DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

        DescriptorSetLayout(Device& device, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);
        ~DescriptorSetLayout();

        VkDescriptorSetLayout GetDescriptorSetLayout() const { return mDescriptorSetLayout; }
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& GetBindings() { return mBindings; };

    private:

        Device& mDevice;                            //Device descriptors are for
        VkDescriptorSetLayout mDescriptorSetLayout; //Vulkan refence of the descriptor set layout itself
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> mBindings; //Map of all bindings in this set layout

        friend class DescriptorWriter;
    };

    //Class wrapping VkDescriptorPool's
    class DescriptorPool
    {
    public:

        //Class for building Descriptor pools
        class Builder
        {
        public:
            Builder(Device& device)
                : mDevice{ device }
            {
            }

            Builder& AddPoolSize(VkDescriptorType descriptorType, uint32_t count);
            Builder& SetPoolFlags(VkDescriptorPoolCreateFlags flags);
            Builder& SetMaxSets(uint32_t count);
            std::unique_ptr<DescriptorPool> Build() const;

        private:

            Device& mDevice;                               //Device descriptors are being made for
            std::vector<VkDescriptorPoolSize> mPoolSizes{}; //Sizes of descriptor to be allocated from this pool
            uint32_t mMaxSets = 1000;                       //Max number of sets that can be allocated from this pool
            VkDescriptorPoolCreateFlags mPoolFlags = 0;     //Flag settings for this pool
        };

        //Delete copy operators
        DescriptorPool(const DescriptorPool&) = delete;
        DescriptorPool& operator=(const DescriptorPool&) = delete;

        DescriptorPool(Device& device,
            uint32_t maxSets,
            VkDescriptorPoolCreateFlags poolFlags,
            const std::vector<VkDescriptorPoolSize>& poolSizes);

        ~DescriptorPool();

        const VkDescriptorPool& GetDescriptorPool() const { return mDescriptorPool; }

        bool AllocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const;
        void FreeDescriptors(std::vector<VkDescriptorSet>& descriptors) const;

        void ResetPool();

    private:

        Device& mDevice;                  //Device descriptors are for
        VkDescriptorPool mDescriptorPool; //Vulkan DescriptorPool object that is being wrapped by this class

        friend class DescriptorWriter;
    };

    class DescriptorWriter
    {
    public:
        DescriptorWriter(DescriptorSetLayout& setLayout, DescriptorPool& pool);

        DescriptorWriter& WriteBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, uint32_t count = 1);
        DescriptorWriter& WriteImage(uint32_t binding, VkDescriptorImageInfo* imageInfo, uint32_t imageCount = 1);
        DescriptorWriter& WriteAccelerationStructure(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR* asInfo);

        bool Build(VkDescriptorSet& set);
        void Overwrite(VkDescriptorSet& set);

    private:

        DescriptorSetLayout& mSetLayout;                    //Layout of the sets being written
        DescriptorPool& mPool;                              //Pool to allocate sets from
        std::vector<VkWriteDescriptorSet> mWritesToPreform; //Writes to preform on build
    };
}
