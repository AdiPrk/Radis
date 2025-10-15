#include <PCH/pch.h>

#include "../Core/Device.h"
#include "Buffer.h"
#include "../Core/Allocator.h"

namespace Dog
{
    Buffer::Buffer(Device& device, VkDeviceSize instanceSize, uint32_t instanceCount, VkBufferUsageFlags usageFlags, VmaMemoryUsage memoryUsage, VkDeviceSize minOffsetAlignment)
        : mDevice{ device }, mInstanceSize{ instanceSize }, mInstanceCount{ instanceCount }, mUsageFlags{ usageFlags }, mMemoryUsage{ memoryUsage }
    {
        //Get the size needed to align to offset alignment
        mAlignmentSize = GetAlignment(instanceSize, minOffsetAlignment);

        //Get size needed for buffer when using alignment size
        mBufferSize = mAlignmentSize * instanceCount;

        //Create the buffer based on passed data and calculated size
        
        // @TODO: COME BACK TO THIS FOR VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        mDevice.GetAllocator()->CreateBuffer(mBufferSize, usageFlags, memoryUsage, mBuffer, mBufferAllocation);
    }

    Buffer::~Buffer()
    {
        //Remove any mapped memorey
        Unmap();

        mDevice.GetAllocator()->DestroyBuffer(mBuffer, mBufferAllocation);
    }

    void Buffer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
    {
        //Start a single time command
        VkCommandBuffer commandBuffer = mDevice.BeginSingleTimeCommands();

        //Create a buffer copy structs which hold infomation on which regions to copy in a buffer copy opertation
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;  //No offset on source buffer
        copyRegion.dstOffset = 0;  //No offset on destintation buffer
        copyRegion.size = size;    //Set the size of the area to copy to the passed size

        //Use copy buffer to get commands for copying 
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        //Stop recording to this buffer and subtmit it
        mDevice.EndSingleTimeCommands(commandBuffer);
    }

    VkResult Buffer::Map(VkDeviceSize size, VkDeviceSize offset)
    {
        //Make sure buffer exists
        if (!mBuffer && !mBufferAllocation)
        {
            DOG_CRITICAL("Called map on buffer before create");
        }

        VmaAllocator allocator = mDevice.GetAllocator()->GetAllocator();
        return vmaMapMemory(allocator, mBufferAllocation, &mMapped);
    }

    void Buffer::Unmap()
    {
        //If the memory is mapped
        if (mMapped)
        {
            VmaAllocator allocator = mDevice.GetAllocator()->GetAllocator();
            vmaUnmapMemory(allocator, mBufferAllocation);
            mMapped = nullptr;
        }
    }

    void Buffer::WriteToBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        //Make sure the buffer is mapped
        if (!mMapped)
        {
            DOG_CRITICAL("Cannot copy to unmapped buffer");
            return;
        }

        //If whole size was request
        if (size == VK_WHOLE_SIZE)
        {
            //Write to all of the buffer
            memcpy(mMapped, data, mBufferSize);
        }
        else
        {
            //Write data to buffer after offset
            char* memOffset = (char*)mMapped;
            memOffset += offset;
            memcpy(memOffset, data, size);
        }
    }

    VkResult Buffer::Flush(VkDeviceSize size, VkDeviceSize offset)
    {
        VmaAllocator allocator = mDevice.GetAllocator()->GetAllocator();
        return vmaFlushAllocation(allocator, mBufferAllocation, offset, size);
    }

    VkResult Buffer::Invalidate(VkDeviceSize size, VkDeviceSize offset)
    {
        VmaAllocator allocator = mDevice.GetAllocator()->GetAllocator();
        return vmaInvalidateAllocation(allocator, mBufferAllocation, offset, size);
    }

    VkDescriptorBufferInfo Buffer::DescriptorInfo(VkDeviceSize size, VkDeviceSize offset)
    {
        //Create buffer descriptor using buffer data and passed data
        return VkDescriptorBufferInfo{ mBuffer, offset, size };
    }

    void Buffer::WriteToIndex(void* data, int index)
    {
        //Write to buffer at specified index
        WriteToBuffer(data, mInstanceSize, index * mAlignmentSize);
    }

    VkResult Buffer::FlushIndex(int index)
    {
        //Flush data at specified index
        return Flush(mAlignmentSize, index * mAlignmentSize);
    }

    VkDescriptorBufferInfo Buffer::DescriptorInfoForIndex(int index)
    {
        //Make descriptor info for data at index
        return DescriptorInfo(mAlignmentSize, index * mAlignmentSize);
    }

    VkResult Buffer::InvalidateIndex(int index)
    {
        //Invalidate data at index
        return Invalidate(mAlignmentSize, index * mAlignmentSize);
    }

    VkDeviceAddress Buffer::GetDeviceAddress()
    {
        if (mDeviceAddress != 0) return mDeviceAddress;

        // @TODO: Re-add this check
        //if ((mUsageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) == 0)
        //{
        //    DOG_CRITICAL("GetDeviceAddress called on a buffer not created with VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT.");
        //    return 0;
        //}

        if (mBuffer == VK_NULL_HANDLE)
        {
            DOG_CRITICAL("GetDeviceAddress called before buffer creation.");
            return 0;
        }

        VkBufferDeviceAddressInfo addressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        addressInfo.buffer = mBuffer;

        VkDevice deviceHandle = mDevice.GetDevice();

        mDeviceAddress = vkGetBufferDeviceAddress(deviceHandle, &addressInfo);
        return mDeviceAddress;
    }

    VkDeviceSize Buffer::GetAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment)
    {
        //If min offset exists
        if (minOffsetAlignment > 0)
        {
            //Calculate the minum size needed to satsify that offset in the buffer (bit alignment)
            return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
        }

        //Return this instance size since none is needed
        return instanceSize;
    }
}
