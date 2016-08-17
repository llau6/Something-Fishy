#include "sprite.hpp"

#include "logger.hpp"
#include "fileutils.hpp"
#include "image16.hpp"
#include "mediancut.hpp"
#include "shared.hpp"

const int sprite_shapes[16] =
{
//h = 1,  2, 4,  8
      0,  2, 2, -1, // width = 1
      1,  0, 2, -1, // width = 2
      1,  1, 0,  2, // width = 4
     -1, -1, 1,  0  // width = 8
};

const int sprite_sizes[16] =
{
//h = 1,  2, 4,  8
      0,  0, 1, -1, // width = 1
      0,  1, 2, -1, // width = 2
      1,  2, 2,  3, // width = 4
     -1, -1, 3,  3  // width = 8
};

Sprite::Sprite(const Image16Bpp& image, std::shared_ptr<Palette>& global_palette) :
    Image(image.width / 8, image.height / 8, image.name, image.filename, image.frame, image.animated), palette(global_palette),
    palette_bank(-1), size(0), shape(0), offset(0), bpp(8)
{
    unsigned int key = (log2(width) << 2) | log2(height);
    if (key > 15 || sprite_sizes[key] == -1 || image.width & 7 || image.height & 7)
        FatalLog("Invalid sprite size for image %s, (%d %d) Please fix", image.name.c_str(), image.width, image.height);
    shape = sprite_shapes[key];
    size = sprite_sizes[key];

    // Is actually an 8 or 4bpp image
    Image8Bpp image8(image, palette);

    data.reserve(width * height);
    for (unsigned int i = 0; i < width * height; i++)
        data.emplace_back(image8, i % width, i / width, 0, 8);
}

Sprite::Sprite(const Image16Bpp& image, int _bpp) : Image(image.width / 8, image.height / 8, image.name, image.filename, image.frame, image.animated),
    palette(new Palette()), palette_bank(-1), size(0), shape(0), offset(0), bpp(_bpp)
{
    unsigned int key = (log2(width) << 2) | log2(height);
    if (key > 15 || sprite_sizes[key] == -1 || image.width & 7 || image.height & 7)
        FatalLog("Invalid sprite size for image %s, (%d %d) Please fix", image.name.c_str(), image.width, image.height);
    shape = sprite_shapes[key];
    size = sprite_sizes[key];

    GetPalette(image.pixels, 1 << bpp, params.transparent_color, 0, *palette);
    // Is actually an 8 or 4bpp image
    Image8Bpp image8(image, palette);

    data.reserve(width * height);
    for (unsigned int i = 0; i < width * height; i++)
        data.emplace_back(image8, i % width, i / width, 0, bpp);
}

void Sprite::UsePalette(const PaletteBank& bank)
{
    for (auto& tile : data)
        tile.UsePalette(bank);
    palette->Set(bank.GetColors());
}

void Sprite::WriteTile(unsigned char* arr, int x, int y) const
{
    int index = y * width + x;
    const Tile& tile = data[index];
    for (unsigned int i = 0; i < TILE_SIZE; i++)
    {
        arr[i] = tile.pixels[i];
    }
}

void Sprite::WriteCommonExport(std::ostream& file) const
{
    WriteDefine(file, name, "_SPRITE_SHAPE", shape, 14);
    WriteDefine(file, name, "_SPRITE_SIZE", size, 14);
}

void Sprite::WriteExport(std::ostream& file) const
{
    if (params.bpp == 4)
        WriteDefine(file, export_name, "_PALETTE", palette_bank, 12);
    if (!animated)
    {
        WriteDefine(file, export_name, "_SPRITE_SHAPE", shape, 14);
        WriteDefine(file, export_name, "_SPRITE_SIZE", size, 14);
    }
    WriteDefine(file, export_name, "_ID", offset | (params.for_bitmap ? 512 : 0));
    WriteNewLine(file);
}

std::string Sprite::GetExportName() const
{
    return ToUpper(export_name) + "_ID";
}

std::ostream& operator<<(std::ostream& file, const Sprite& sprite)
{
    for (unsigned int i = 0; i < sprite.data.size(); i++)
    {
        file << sprite.data[i];
        if (i != sprite.data.size() - 1)
            file << ",\n\t";
    }
    return file;
}

bool BlockSize::operator==(const BlockSize& rhs) const
{
    return width == rhs.width && height == rhs.height;
}

bool BlockSize::operator<(const BlockSize& rhs) const
{
    if (width != rhs.width)
        return width < rhs.width;
    else
        return height < rhs.height;
}

std::vector<BlockSize> BlockSize::BiggerSizes(const BlockSize& b)
{
    switch(b.Size())
    {
    case 1:
        return {BlockSize(2, 1), BlockSize(1, 2)};
    case 2:
        if (b.width == 2)
            return {BlockSize(4, 1), BlockSize(2, 2)};
        else
            return {BlockSize(1, 4), BlockSize(2, 2)};
    case 4:
        if (b.width == 4)
            return {BlockSize(4, 2)};
        else if (b.height == 4)
            return {BlockSize(2, 4)};
        else
            return {BlockSize(4, 2), BlockSize(2, 4)};
    case 8:
        return {BlockSize(4, 4)};
    case 16:
        return {BlockSize(8, 4), BlockSize(4, 8)};
    case 32:
        return {BlockSize(8, 8)};
    default:
        return {};
    }
}

Block Block::VSplit()
{
    size.height /= 2;
    return Block(x, y + size.height, size);
}

Block Block::HSplit()
{
    size.width /= 2;
    return Block(x + size.width, y, size);
}

Block Block::Split(const BlockSize& to_this_size)
{
    if (size.width == to_this_size.width)
        return VSplit();
    else if (size.height == to_this_size.height)
        return HSplit();
    else
        FatalLog("Error Block::Split this shouldn't happen");

    return Block();
}

bool SpriteCompare(const Image* l, const Image* r)
{
    const Sprite* lhs = dynamic_cast<const Sprite*>(l);
    const Sprite* rhs = dynamic_cast<const Sprite*>(r);
    if (lhs->Size() != rhs->Size())
        return lhs->Size() > rhs->Size();
    else
        // Special case 2x2 should be of lesser priority than 4x1/1x4
        // This is because 2x2's don't care how a 4x4 block is split they are happy with a 4x2 or 2x4
        // Whereas 4x1 must get 4x2 and 1x4 must get 2x4.
        return lhs->width + lhs->height > rhs->width + rhs->height;
}

SpriteSheet::SpriteSheet(const std::vector<Sprite*>& _sprites, const std::string& _name, int _bpp) :
    name(_name), bpp(_bpp), sprites(_sprites)
{
    width = bpp == 4 ? 32 : 16;
    height = !params.for_bitmap ? 32 : 16;
    data.resize(width * 8 * height * 8);
}

void SpriteSheet::Compile()
{
    PlaceSprites();
    for (const auto& block : placedBlocks)
    {
        for (unsigned int i = 0; i < block.size.height; i++)
        {
            for (unsigned int j = 0; j < block.size.width; j++)
            {
                int x = block.x + j;
                int y = block.y + i;
                int index = y * width + x;
                const Sprite* sprite = sprites[block.sprite_id];
                sprite->WriteTile(data.data() + index * 64, j, i);
            }
        }
    }
}

void SpriteSheet::WriteData(std::ostream& file) const
{
    if (bpp == 8)
        WriteShortArray(file, name, "", data, 8);
    else
        WriteShortArray4Bit(file, name, "", data, 8);
    WriteNewLine(file);
}

void SpriteSheet::WriteExport(std::ostream& file) const
{
    unsigned int size = data.size() / (bpp == 4 ? 4 : 2);
    WriteExtern(file, "const unsigned short", name, "", size);
    WriteDefine(file, name, "_SIZE", size);
    WriteNewLine(file);

    for (const auto& sprite : sprites)
        sprite->WriteExport(file);
}

void SpriteSheet::PlaceSprites()
{
    // Gimme some blocks.
    BlockSize size(8, 8);
    for (unsigned int y = 0; y < height; y += 8)
    {
        for (unsigned int x = 0; x < width; x += 8)
        {
            freeBlocks[size].push_back(Block(x, y, size));
        }
    }

    // Sort by request size
    std::sort(sprites.begin(), sprites.end(), SpriteCompare);

    for (unsigned int i = 0; i < sprites.size(); i++)
    {
        Sprite& sprite = *sprites[i];
        BlockSize size(sprite.width, sprite.height);
        std::list<BlockSize> slice;

        // Mother may I have block of this size?
        if (AssignBlockIfAvailable(size, sprite, i))
            continue;

        if (size.IsBiggestSize())
            FatalLog("Out of sprite memory could not allocate sprite %s size (%d,%d)", sprite.name.c_str(), sprite.width, sprite.height);

        slice.push_front(size);
        while (!HasAvailableBlock(size))
        {
            std::vector<BlockSize> sizes = BlockSize::BiggerSizes(size);
            if (sizes.empty())
                FatalLog("Out of sprite memory could not allocate sprite %s size (%d,%d)", sprite.name.c_str(), sprite.width, sprite.height);

            // Default next search size will be last.
            size = sizes.back();
            for (auto& new_size : sizes)
            {
                if (HasAvailableBlock(new_size))
                {
                    size = new_size;
                    break;
                }
            }
            if (!HasAvailableBlock(size))
                slice.push_front(size);
        }

        if (!HasAvailableBlock(size))
            FatalLog("Out of sprite memory could not allocate sprite %s size (%d,%d)", sprite.name.c_str(), sprite.width, sprite.height);

        SliceBlock(size, slice);

        size = BlockSize(sprite.width, sprite.height);
        // Mother may I have block of this size?
        if (AssignBlockIfAvailable(size, sprite, i))
            continue;
        else
            FatalLog("Out of sprite memory could not allocate sprite %s size (%d,%d)", sprite.name.c_str(), sprite.width, sprite.height);
    }
}

bool SpriteSheet::AssignBlockIfAvailable(BlockSize& size, Sprite& sprite, unsigned int i)
{
    if (HasAvailableBlock(size))
    {
        // Yes you may deary.
        Block allocd = freeBlocks[size].front();
        freeBlocks[size].pop_front();
        allocd.sprite_id = i;
        sprite.offset = (allocd.y * width + allocd.x) * (params.bpp == 4 ? 1 : 2);
        placedBlocks.push_back(allocd);
        return true;
    }

    return false;
}

bool SpriteSheet::HasAvailableBlock(const BlockSize& size)
{
    return !freeBlocks[size].empty();
}

void SpriteSheet::SliceBlock(const BlockSize& size, const std::list<BlockSize>& slice)
{
    Block toSplit = freeBlocks[size].front();
    freeBlocks[size].pop_front();

    for (const auto& split_size : slice)
    {
        Block other = toSplit.Split(split_size);
        freeBlocks[split_size].push_back(other);
    }

    freeBlocks[toSplit.size].push_front(toSplit);
}

SpriteScene::SpriteScene(const std::vector<Image16Bpp>& images, const std::string& _name, bool _is2d, int _bpp) : Scene(_name), bpp(_bpp), paletteBanks(name),
    is2d(_is2d)
{
    switch(bpp)
    {
        case 4:
            Init4bpp(images);
            break;
        default:
            Init8bpp(images);
            break;
    }
}

SpriteScene::SpriteScene(const std::vector<Image16Bpp>& images, const std::string& _name, bool _is2d, std::shared_ptr<Palette>& _palette) :
    Scene(_name), bpp(8), palette(_palette), paletteBanks(name), is2d(_is2d)
{
    Init8bpp(images);
}

SpriteScene::SpriteScene(const std::vector<Image16Bpp>& images, const std::string& _name, bool _is2d, const std::vector<PaletteBank>& _paletteBanks) :
    Scene(_name), bpp(4), paletteBanks(name, _paletteBanks), is2d(_is2d)
{
    Init4bpp(images);
}

void SpriteScene::Build()
{
    if (is2d)
    {
        std::vector<Sprite*> sprites;
        for (const auto& image : images)
        {
            Sprite* sprite = dynamic_cast<Sprite*>(image.get());
            if (!sprite) FatalLog("Could not cast Image to Sprite. This shouldn't happen");
            sprites.push_back(sprite);
        }
        spriteSheet.reset(new SpriteSheet(sprites, name, bpp));
        spriteSheet->Compile();
    }
    else
    {
        unsigned int offset = 0;
        for (unsigned int k = 0; k < images.size(); k++)
        {
            Sprite* sprite = dynamic_cast<Sprite*>(images[k].get());
            if (!sprite) FatalLog("Could not cast Image to Sprite. This shouldn't happen");
            sprite->offset = offset;
            offset += sprite->width * sprite->height * (bpp == 8 ? 2 : 1);
        }
    }
}

const Sprite& SpriteScene::GetSprite(int index) const
{
    const Sprite* sprite = dynamic_cast<const Sprite*>(images[index].get());
    if (!sprite) FatalLog("Could not cast Image to Sprite. This shouldn't happen");
    return *sprite;
}

unsigned int SpriteScene::Size() const
{
    unsigned int total = 0;
    for (const auto& image : images)
        total += image->width * image->height;

    return total * (bpp == 4 ? TILE_SIZE_SHORTS_4BPP : TILE_SIZE_SHORTS_8BPP);
}

void SpriteScene::WriteData(std::ostream& file) const
{
    if (bpp == 4)
        paletteBanks.WriteData(file);
    else
        palette->WriteData(file);

    if (is2d)
    {
        spriteSheet->WriteData(file);
    }
    else
    {
        WriteBeginArray(file, "const unsigned short", name, "", Size());
        for (unsigned int i = 0; i < images.size(); i++)
        {
            file << GetSprite(i);
            if (i != images.size() - 1)
                file << ",\n\t";
        }
        WriteEndArray(file);
        WriteNewLine(file);
    }
}

void SpriteScene::WriteExport(std::ostream& file) const
{
    WriteDefine(file, name, "_PALETTE_TYPE", bpp == 8, 13);
    WriteDefine(file, name, "_DIMENSION_TYPE", !is2d, 6);
    WriteNewLine(file);

    if (bpp == 4)
        paletteBanks.WriteExport(file);
    else
        palette->WriteExport(file);


    if (is2d)
    {
        spriteSheet->WriteExport(file);
    }
    else
    {
        WriteExtern(file, "const unsigned short", name, "", Size());
        WriteDefine(file, name, "_SIZE", Size());
        WriteNewLine(file);

        for (const auto& sprite : images)
            sprite->WriteExport(file);
    }
}

bool SpritePaletteSizeComp(const Sprite* i, const Sprite* j)
{
    return i->palette->Size() > j->palette->Size();
}

void SpriteScene::Init4bpp(const std::vector<Image16Bpp>& images16)
{
    // Form sprites
    std::vector<Sprite*> sprites;
    for (const auto& image : images16)
        sprites.push_back(new Sprite(image, bpp));

    // Palette bank selection time
    // Ensure image contains < 256 colors
    std::set<Color16> bigPalette;
    for (const auto& sprite : sprites)
    {
        const std::vector<Color16>& sprite_palette = sprite->palette->GetColors();
        bigPalette.insert(sprite_palette.begin(), sprite_palette.end());
    }

    if (bigPalette.size() > 256 && !params.force)
        FatalLog("Image after reducing sprites to 4bpp still contains more than 256 distinct colors.  Found %d colors. Please fix.", bigPalette.size());

    // Greedy approach deal with tiles with largest palettes first.
    std::sort(sprites.begin(), sprites.end(), SpritePaletteSizeComp);

    // But deal with transparent color
    for (unsigned int i = 0; i < paletteBanks.Size(); i++)
        paletteBanks[i].Add(Color16(params.transparent_color));

    // Construct palette banks, assign bank id to tile, remap sprite to palette bank given, assign tile ids
    for (auto& sprite : sprites)
    {
        int pbank = -1;
        // Fully contains checks
        for (unsigned int i = 0; i < paletteBanks.Size(); i++)
        {
            PaletteBank& bank = paletteBanks[i];
            if (bank.Contains(*sprite->palette))
                pbank = i;
        }

        // Ok then find least affected bank
        if (pbank == -1)
        {
            int min_delta = 0x7FFFFFFF;
            for (unsigned int i = 0; i < paletteBanks.Size(); i++)
            {
                PaletteBank& bank = paletteBanks[i];
                int colors_left;
                int delta;
                bank.CanMerge(*sprite->palette, colors_left, delta);
                if (colors_left >= 0 && delta < min_delta)
                {
                    min_delta = delta;
                    pbank = i;
                }
            }
        }

        // Cry and die for now. Unless you tell me to keep going.
        if (pbank == -1 && !params.force)
            FatalLog("More than 16 distinct palettes found, please use 8bpp mode.");

        // Merge step and assign palette bank
        paletteBanks[pbank].Merge(*sprite->palette);
        sprite->palette_bank = pbank;
        sprite->UsePalette(paletteBanks[pbank]);

        // Add to images list
        images.emplace_back(sprite);
    }
}

void SpriteScene::Init8bpp(const std::vector<Image16Bpp>& images16)
{
    if (!palette)
    {
        palette.reset(new Palette(name));
        GetPalette(images16, params.palette, params.transparent_color, params.offset, *palette);
    }

    for (const auto& image : images16)
        images.emplace_back(new Sprite(image, palette));
}
