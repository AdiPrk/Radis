/*****************************************************************//**
 * \file   RenderMode.h
 * \brief  Rendering pipeline mode enumeration
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/
#pragma once

namespace Radis
{
    /**
     * \brief Available rendering pipeline modes.
     */
    enum class RenderMode
    {
        Forward,    ///< Traditional forward rendering
        Deferred,   ///< Deferred shading with G-Buffer
        Raytracing  ///< Hardware-accelerated raytracing
    };

} // namespace Radis