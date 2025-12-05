#pragma once

namespace Dog
{
    // this is simply unused :((((((((((
    // Hopefully i get back to this some day
    // @todo better RHI Abstraction

    // Main dispatch struct
    struct RHI
    {
        // lifecycle
        static bool (*Init)();
        static void (*Shutdown)();

        // mesh
        static void (*CreateVertexBuffers)();
        static void (*CreateIndexBuffers)();

        static bool RHI_Init(GraphicsAPI backend);
    };


}
