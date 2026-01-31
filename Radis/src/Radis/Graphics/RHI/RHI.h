/*****************************************************************//**
 * \file   RHI.h
 * \brief  Render Hardware Interface abstraction layer
 * 
 * \author Adi
 * \date   2026
 * 
 * \note   This abstraction is currently unused. Future work will
 *         expand this to provide a complete graphics API abstraction.
 *********************************************************************/
#pragma once

namespace Radis
{
    /**
     * \brief Function pointer dispatch table for RHI operations.
     * 
     * \todo Expand this to provide complete graphics API abstraction.
     */
    struct RHI
    {
        // Lifecycle
        static bool (*Init)();
        static void (*Shutdown)();

        // Mesh operations
        static void (*CreateVertexBuffers)();
        static void (*CreateIndexBuffers)();

        /** Initialize the RHI for a specific graphics backend. */
        static bool Initialize(GraphicsAPI backend);
    };

} // namespace Radis
