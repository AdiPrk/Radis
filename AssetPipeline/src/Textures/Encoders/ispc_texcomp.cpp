// ispc_texcomp.cpp
#include <pch.h>
#include "../TextureEncoding.h"

EncodeResult EncodeWithIspcTexcomp(const ImageView&, const EncodeParams& params, std::span<std::byte>)
{
    return EncodeResult(std::unexpect, std::format("{}: ispc_texcomp encoder not implemented yet", GetFormatInfo(params.format).name));
}