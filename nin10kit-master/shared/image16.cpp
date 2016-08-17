#include <cstdio>

#include "logger.hpp"
#include "fileutils.hpp"
#include "image16.hpp"
#include "image32.hpp"

Image16Bpp::Image16Bpp(const Image32Bpp& image) : Image(image), pixels(width * height)
{
    unsigned int num_pixels = width * height;
    const std::vector<Color>& pixels32 = image.pixels;

    for (unsigned int i = 0; i < num_pixels; i++)
        pixels[i] = Color16(pixels32[i]);
}

void Image16Bpp::WriteData(std::ostream& file) const
{
    char buffer[7];
    WriteBeginArray(file, "const unsigned short", export_name, "", pixels.size());
    for (unsigned int i = 0; i < pixels.size(); i++)
    {
        snprintf(buffer, 7, "0x%04x", pixels[i].ToGBAShort());
        WriteElement(file, buffer, pixels.size(), i, 16);
    }
    WriteEndArray(file);
    WriteNewLine(file);
}

void Image16Bpp::WriteCommonExport(std::ostream& file) const
{
    WriteDefine(file, name, "_SIZE", pixels.size());
    WriteDefine(file, name, "_WIDTH", width);
    WriteDefine(file, name, "_HEIGHT", height);
}

void Image16Bpp::WriteExport(std::ostream& file) const
{
    WriteExtern(file, "const unsigned short", export_name, "", pixels.size());
    if (!animated)
    {
        WriteDefine(file, export_name, "_SIZE", pixels.size());
        WriteDefine(file, export_name, "_WIDTH", width);
        WriteDefine(file, export_name, "_HEIGHT", height);
    }
    WriteNewLine(file);
}

Magick::Image Image16Bpp::ToMagick() const
{
    Magick::Image ret(Magick::Geometry(width, height), Magick::Color(0, 0, 0));
    Magick::PixelPacket* packet = ret.getPixels(0, 0, width, height);
    for (unsigned int i = 0; i < height; i++)
    {
        for (unsigned int j = 0; j < width; j++)
        {
            const Color color = pixels[i * width + j].ToColor();
            packet[i * width + j] = Magick::Color(color.r << 8, color.g << 8, color.b << 8);
        }
    }
    ret.syncPixels();
    ret.write("image16.png");
    return ret;
}
