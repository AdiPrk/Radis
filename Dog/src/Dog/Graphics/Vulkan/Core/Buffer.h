#pragma once

namespace Dog
{
    // Forward declerations
    class Device;

    class Buffer
    {
    public:
        //Delete copy operations
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;

        Buffer(
            Device& device,
            VkDeviceSize instanceSize,
            uint32_t instanceCount,
            VkBufferUsageFlags usageFlags,
            VmaMemoryUsage memoryUsage,
            VkDeviceSize minOffsetAlignment = 1);

        ~Buffer();

        void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

        VkResult Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

        void Unmap();

        void WriteToBuffer(const void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        
        VkResult Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        
        VkResult Invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        
        VkDescriptorBufferInfo DescriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

        /*********************************************************************
         * param:  data: Pointer to the data to copy
         * param:  index: Used in offset calculation
         *
         * brief:  Copies "instanceSize" bytes of data to the mapped buffer at
         *         an offset of index * alignmentSize
         *********************************************************************/
        void WriteToIndex(void* data, int index);
        /*********************************************************************
         * param:  index: index Used in offset calculation
         * return: VkResult of the flushing call
         *
         * brief:  Flush the memory range at index * alignmentSize of the
         *         buffer to make it visible to the device
         *********************************************************************/
        VkResult FlushIndex(int index);
        /*********************************************************************
         * param:  index: pecifies the region given by index * alignmentSize
         * return: VkDescriptorBufferInfo for instance at index
         *
         * brief:  Create a buffer info descriptor
         *********************************************************************/
        VkDescriptorBufferInfo DescriptorInfoForIndex(int index);
        /*********************************************************************
         * param:  index: Specifies the region to invalidate:
         *                index * alignmentSize
         * return: VkResult of the invalidate call
         *
         * brief:  Invalidate a memory range of the buffer to make it visible
         *         to the host, Note: Only required for non-coherent memory
         *********************************************************************/
        VkResult InvalidateIndex(int index);

        ////////////////////////////////////////////////////////////////////////////////////////////
        /// Getters ////////////////////////////////////////////////////////////////////////////////

        /*********************************************************************
         * return: vKBuffer object being wrapped
         *
         * brief:  Returns the vKBuffer object being wrapped
         *********************************************************************/
        VkBuffer GetBuffer() const { return mBuffer; }
        const VkBuffer* GetBufferPtr() const { return &mBuffer; }
        /*********************************************************************
         * return: Pointer to memory mapped to gpu
         *
         * brief:  Returns the pointer to memory mapped to gpu
         *********************************************************************/
        void* GetMappedMemory() const { return mMapped; }
        /*********************************************************************
         * return: Number of instances
         *
         * brief:  Returns the number of instances
         *********************************************************************/
        uint32_t GetInstanceCount() const { return mInstanceCount; }
        /*********************************************************************
         * return: Size of an instance
         *
         * brief:  Returns the size of an instance
         *********************************************************************/
        VkDeviceSize GetInstanceSize() const { return mInstanceSize; }
        /*********************************************************************
         * return: Currently returns instance size
         *
         * brief:  Returns the instance size
         *********************************************************************/
        VkDeviceSize GetAlignmentSize() const { return mInstanceSize; }
        /*********************************************************************
         * return: Flags defining how this buffer can be used
         *
         * brief:  Returns the flags defining how this buffer can be used
         *********************************************************************/
        VkBufferUsageFlags GetUsageFlags() const { return mUsageFlags; }
        /*********************************************************************
         * return: Flags defining how this buffers memory can be accessed/stored
         *
         * brief:  Returns the flags defining how this buffers memory can be
         *         accessed/stored
         *********************************************************************/
        VmaMemoryUsage GetMemoryUsageFlags() const { return mMemoryUsage; }
        /*********************************************************************
         * return: Size of this buffer
         *
         * brief:  Returns the size of this buffer
         *********************************************************************/
        VkDeviceSize GetBufferSize() const { return mBufferSize; }

        VkDeviceAddress GetDeviceAddress();

    private:
        /*********************************************************************
         * param:  instanceSize: The size of an instance
         * param:  minOffsetAlignment: The minimum required alignment, in
         *                             bytes, for the offset member
         *                             (eg minUniformBufferOffsetAlignment)
         * return: VkResult of the buffer mapping call
         *
         * brief: Returns the minimum instance size required to be compatible
         *        with devices minOffsetAlignment
         *********************************************************************/
        static VkDeviceSize GetAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

        ////////////////////////////////////////////////////////////////////////////////////////////
       /// Varibles ///////////////////////////////////////////////////////////////////////////////

        Device& mDevice;                            //Device this buffer will be interacting with
        void* mMapped = nullptr;                    //Memory location to memory mapped to the gpu (Null when not active)

        VkBuffer mBuffer = VK_NULL_HANDLE; 				  //Buffer object
        VmaAllocation mBufferAllocation; 						//Memory allocation for this buffer

        VkDeviceSize mBufferSize;                   //Size of this buffer
        uint32_t mInstanceCount;                    //Number of instances of data in this buffer
        VkDeviceSize mInstanceSize;                 //Size of an instance in this buffer
        VkDeviceSize mAlignmentSize;                //Size of an instance when aligning to needed bit alignment (same as instance size if none was given)
        VkBufferUsageFlags mUsageFlags;             //Flags defining how this buffer can be used
        VmaMemoryUsage mMemoryUsage;                //Flags defining how this buffers memory can be accessed/stored
        VkDeviceAddress mDeviceAddress{ 0 };
    };
}