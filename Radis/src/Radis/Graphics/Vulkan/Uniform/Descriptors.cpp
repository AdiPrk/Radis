#include <PCH/pch.h>

#include "Descriptors.h"
#include "../Core/Device.h"
#include "ECS/Resources/RenderingResource.h"

namespace Dog
{
    //-----Descriptor Set Layout Builder--------------------------------------------------------------------------------------------------------------------------------------------------

    DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::AddBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t count)
    {
        //Make sure binding is unique in map of bindings
        if (mBindings.count(binding) != 0)
        {
            DOG_CRITICAL("Binding already in use");
        }

        //Create binding layout
        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = binding;               //Set binding index
        layoutBinding.descriptorType = descriptorType; //Set type of descriptor to be bound
        layoutBinding.descriptorCount = count;         //Set the # of descriptors to be bound
        layoutBinding.stageFlags = stageFlags;         //Set what stages to be bound too

        //Add to map of bindings
        mBindings[binding] = layoutBinding;

        //Return self
        return *this;
    }

    DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::AddBinding(VkDescriptorSetLayoutBinding binding)
    {
        //Make sure binding is unique in map of bindings
        if (mBindings.count(binding.binding) != 0)
        {
            DOG_CRITICAL("Binding already in use");
        }

        //Add to map of bindings
        mBindings[binding.binding] = binding;

        //Return self
        return *this;
    }

    std::unique_ptr<DescriptorSetLayout> DescriptorSetLayout::Builder::Build() const
    {
        //Make a descriptor set layout
        return std::make_unique<DescriptorSetLayout>(mDevice, mBindings);
    }

    //-----Descriptor Set Layout----------------------------------------------------------------------------------------------------------------------------------------------------------

    DescriptorSetLayout::DescriptorSetLayout(Device& device, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings)
        : mDevice{ device }, mBindings{ bindings }
    {
        //Put all bindings from passed map into a vector
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
        for (std::pair<uint32_t, VkDescriptorSetLayoutBinding> bind : bindings)
        {
            setLayoutBindings.push_back(bind.second);
        }

        //Make create info for this Descriptor set
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
        descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;    //Define the type of this struct to be a set layout create info
        descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size()); //Set the number of binding to be in this set
        descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();                           //Array of VkDescriptorSetLayoutBindings to make the set out of

        //Attempt to create the descriptor set layout
        if (vkCreateDescriptorSetLayout(mDevice.GetDevice(), &descriptorSetLayoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS)
        {
            //If failed throw error
            DOG_CRITICAL("Failed to create descriptor set layout");
        }
    }

    DescriptorSetLayout::~DescriptorSetLayout()
    {
        //Destroy the set layout
        vkDestroyDescriptorSetLayout(mDevice.GetDevice(), mDescriptorSetLayout, nullptr);
    }

    //-----Descriptor Pool Builder--------------------------------------------------------------------------------------------------------------------------------------------------------

    DescriptorPool::Builder& DescriptorPool::Builder::AddPoolSize(VkDescriptorType descriptorType, uint32_t count)
    {
        //Add a pool size to the vector (each pool size defines a number of discriptors of a type that can be allocated by pool to create later)
        mPoolSizes.push_back({ descriptorType, count });
        return *this;
    }

    DescriptorPool::Builder& DescriptorPool::Builder::SetPoolFlags(VkDescriptorPoolCreateFlags flags)
    {
        //Set flags to use when building
        mPoolFlags = flags;
        return *this;
    }
    DescriptorPool::Builder& DescriptorPool::Builder::SetMaxSets(uint32_t count)
    {
        //Set max number of sets that wanted pool can make
        mMaxSets = count;
        return *this;
    }

    std::unique_ptr<DescriptorPool> DescriptorPool::Builder::Build() const
    {
        //Create a descriptor pool
        return std::make_unique<DescriptorPool>(mDevice, mMaxSets, mPoolFlags, mPoolSizes);
    }

    //-----Descriptor Pool----------------------------------------------------------------------------------------------------------------------------------------------------------------

    DescriptorPool::DescriptorPool(Device& device, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags, const std::vector<VkDescriptorPoolSize>& poolSizes)
        : mDevice{ device }
    {
        //Create info for the descriptor poo;
        VkDescriptorPoolCreateInfo descriptorPoolInfo{};
        descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;   //Define this struct as a Descriptor pool create info
        descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size()); //Set # of pool sizes to use, which each define the number of allocateable discriptors of the type held in that struct
        descriptorPoolInfo.pPoolSizes = poolSizes.data();                           //Give all pool sizes using
        descriptorPoolInfo.maxSets = maxSets;                                       //Max number of sets allocatable
        descriptorPoolInfo.flags = poolFlags;                                       //Set flags to customize the pool

        //Attempt to create the descriptor pool
        if (vkCreateDescriptorPool(mDevice.GetDevice(), &descriptorPoolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS)
        {
            //If failed throw error
            DOG_CRITICAL("Failed to create descriptor pool");
        }
    }

    DescriptorPool::~DescriptorPool()
    {
        //Destroy the descriptor pool
        vkDestroyDescriptorPool(mDevice.GetDevice(), mDescriptorPool, nullptr);
    }

    bool DescriptorPool::AllocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const
    {
        //Allocatation info for a VkDescriptor set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; //Define this structor to be a descriptor set
        allocInfo.descriptorPool = mDescriptorPool;                        //Set pool to allocate from
        allocInfo.pSetLayouts = &descriptorSetLayout;                     //Set layout of set to create (what type of data to store)
        allocInfo.descriptorSetCount = 1;                                 //Number of descriptors to allocate of this layout

        //Attempt to allocate the descriptor set
        if (vkAllocateDescriptorSets(mDevice.GetDevice(), &allocInfo, &descriptor) != VK_SUCCESS)
        {
            //Most likely error if got here is that the pool is empty (no more memory)
            //Might want to create a "DescriptorPoolManager" class that handles this case, and builds
            //a new pool whenever an old pool fills up. But this is beyond our current scope
            return false;
        }

        return true;
    }

    void DescriptorPool::FreeDescriptors(std::vector<VkDescriptorSet>& descriptors) const
    {
        //Free passed descriptors (returned to pool)
        vkFreeDescriptorSets(mDevice.GetDevice(), mDescriptorPool, static_cast<uint32_t>(descriptors.size()), descriptors.data());
    }

    void DescriptorPool::ResetPool()
    {
        //Free all descriptors allocated from this pool (all returned to pool)
        vkResetDescriptorPool(mDevice.GetDevice(), mDescriptorPool, 0);
    }

    //-----Descriptor Writer------------------------------------------------------------------------------------------------------------------------------------------------------------------

    DescriptorWriter::DescriptorWriter(DescriptorSetLayout& setLayout, DescriptorPool& pool)
        : mSetLayout{ setLayout }, mPool{ pool } {
    }

    DescriptorWriter& DescriptorWriter::WriteBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, uint32_t count)
    {
        //Make sure this binding index is within map of bindings
        if (mSetLayout.mBindings.count(binding) != 1)
        {
            DOG_CRITICAL("Layout does not contain specified binding");
        }

        //Get the binding data of the descriptor set at binding index
        VkDescriptorSetLayoutBinding& bindingDescription = mSetLayout.mBindings[binding];

        //Make sure that binding is only wants one descriptor set
        if (bindingDescription.descriptorCount != count)
        {
            DOG_CRITICAL("Binding single descriptor info, but binding expects multiple");
        }

        //Struct for write all info
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;     //Define this structure as a write descriptor set
        write.descriptorType = bindingDescription.descriptorType; //Set the type of descriptor is being written
        write.dstBinding = binding;                               //Set the destination binding index of this write
        write.pBufferInfo = bufferInfo;                           //Pointer to the info to write
        write.descriptorCount = count;                            //# of descriptors being written

        //Add this write call to the write to preform vector to be preformed when build is called
        mWritesToPreform.push_back(write);

        //Return self
        return *this;
    }

    DescriptorWriter& DescriptorWriter::WriteImage(uint32_t binding, VkDescriptorImageInfo* imageInfo, uint32_t imageCount)
    {
        //Make sure this binding index is within map of bindings
        if (mSetLayout.mBindings.count(binding) != 1)
        {
            DOG_CRITICAL("Layout does not contain specified binding");
        }

        //Get the binding data of the descriptor set at binding index
        VkDescriptorSetLayoutBinding& bindingDescription = mSetLayout.mBindings[binding];

        //Make sure that binding is only wants one descriptor set
        if (bindingDescription.descriptorCount != imageCount)
        {
            DOG_CRITICAL("Binding single descriptor info, but binding expects multiple");
        }

        //Struct for write all info
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;     //Define this structure as a write descriptor set
        write.descriptorType = bindingDescription.descriptorType; //Set the type of descriptor is being written
        write.dstBinding = binding;                               //Set the destination binding index of this write
        write.pImageInfo = imageInfo;                             //Pointer to the info to write
        write.descriptorCount = imageCount;                       //# of descriptors being written

        //Add this write call to the write to preform vector to be preformed when build is called
        mWritesToPreform.push_back(write);

        //Return self
        return *this;
    }

    DescriptorWriter& DescriptorWriter::WriteAccelerationStructure(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR* asInfo)
    {
        // Make sure this binding index exists
        if (mSetLayout.mBindings.count(binding) != 1)
        {
            DOG_CRITICAL("Layout does not contain specified binding");
        }

        VkDescriptorSetLayoutBinding& bindingDescription = mSetLayout.mBindings[binding];

        // Make sure descriptor type matches
        if (bindingDescription.descriptorType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
        {
            DOG_CRITICAL("Binding is not an acceleration structure type");
        }

        // Fill a VkWriteDescriptorSet that points to the AS struct
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = asInfo; // Pointer to the acceleration structure info
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

        mWritesToPreform.push_back(write);

        return *this;
    }


    bool DescriptorWriter::Build(VkDescriptorSet& set)
    {
        //Allocate a descritor set
        bool success = mPool.AllocateDescriptor(mSetLayout.GetDescriptorSetLayout(), set);

        //If failed
        if (!success)
        {
            return false;
        }

        //Overwrite data on gpu (preform writes)
        Overwrite(set);

        return true;
    }

    void DescriptorWriter::Overwrite(VkDescriptorSet& set)
    {
        //Go through all writes to preform
        for (auto& write : mWritesToPreform)
        {
            //Set their write destination to the passed descriptor set
            write.dstSet = set;
        }

        //Update this descriptor set on the gpu
        vkUpdateDescriptorSets(mPool.mDevice.GetDevice(),
            static_cast<uint32_t>(mWritesToPreform.size()),
            mWritesToPreform.data(),
            0,
            nullptr);
    }
}