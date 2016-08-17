#ifndef TILE_HPP
#define TILE_HPP

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "color.hpp"
#include "palette.hpp"

class Image16Bpp;
class Image8Bpp;

#define TOTAL_TILE_MEMORY_BYTES 65536
#define TILE_SIZE 64
#define PALETTE_SIZE 16
#define TILE_SIZE_BYTES_8BPP 64
#define TILE_SIZE_BYTES_4BPP 32
#define TILE_SIZE_SHORTS_8BPP 32
#define TILE_SIZE_SHORTS_4BPP 16
#define SIZE_CBB_BYTES (8192 * 2)
#define SIZE_SBB_BYTES (1024 * 2)
#define SIZE_SBB_SHORTS 1024

/** This class represents a one to one mapping between an Image and Tile. */
class ImageTile
{
    public:
        ImageTile(const Image16Bpp& image, int tilex, int tiley, int border = 0);
        bool IsEqual(const ImageTile& other) const;
        bool IsSameAs(const ImageTile& other) const;
        bool operator<(const ImageTile& other) const;
        bool operator==(const ImageTile& other) const;
        static const ImageTile& GetNullTile();
        int id;
        std::vector<Color16> pixels;
    private:
        ImageTile(const Color16& color = Color16()) : id(0), pixels(TILE_SIZE, color) {}
};

/* */
class Tile
{
    public:
        Tile(const Image16Bpp& image, int tilex, int tiley, int border = 0, int bpp = 8);
        Tile(const ImageTile& imageTile, int bpp);
        Tile(const Image8Bpp& image, int tilex, int tiley, int border = 0, int bpp = 8);
        bool IsEqual(const Tile& other) const;
        bool IsSameAs(const Tile& other) const;
        bool operator<(const Tile& other) const;
        bool operator==(const Tile& other) const;
        /* Set to use palette bank only for 4bpp tiles */
        void UsePalette(const PaletteBank& bank);
        static const Tile& GetNullTile8();
        static const Tile& GetNullTile4();
        int id;
        std::vector<unsigned char> pixels;
        int bpp;
        int palette_bank;
        Palette palette;
        /* Shared because copies of tiles exist */
        std::shared_ptr<ImageTile> sourceTile;

    friend std::ostream& operator<<(std::ostream& file, const Tile& tile);
    private:
        Tile(int _bpp) : id(0), pixels(TILE_SIZE), bpp(_bpp), palette_bank(0) {}
};

bool TilesPaletteSizeComp(const Tile& i, const Tile& j);

#endif
