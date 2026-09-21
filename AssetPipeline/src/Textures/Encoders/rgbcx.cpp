#include <pch.h>
#include "../TextureEncoding.h"

EncodeResult EncodeWithRgbcx(const ImageView&, const EncodeParams& params, std::span<std::byte>)
{
    return EncodeResult(std::unexpect, std::format("{}: rgbcx encoder not implemented yet", GetFormatInfo(params.format).name));
}