#include "render.h"

#include "SDL.h"
#include <cel/celfile.h>
#include <cel/celframe.h>
#include <level/level.h>

#ifdef SDL_HWSURFACE
    #define SURFACE_FLAGS SDL_HWSURFACE
#else
    #define SURFACE_FLAGS 0
#endif

namespace Render
{
	extern int32_t WIDTH;
	extern int32_t HEIGHT;
    #define BPP 4
    #define DEPTH 32

    void clearTransparentSurface(SDL_Surface* s)
    {
        SDL_FillRect(s, NULL, SDL_MapRGBA(s->format, 0, 0, 0, 0)); 
    }

    SDL_Surface* createTransparentSurface(size_t width, size_t height)
    {
         SDL_Surface* s; 
        
        // SDL y u do dis
        #if SDL_BYTEORDER == SDL_BIG_ENDIAN
            s = SDL_CreateRGBSurface(SURFACE_FLAGS, width, height, DEPTH, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
        #else
            s = SDL_CreateRGBSurface(SURFACE_FLAGS, width, height, DEPTH, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
        #endif

        clearTransparentSurface(s);

        return s;
    }

    void setpixel(SDL_Surface *s, int x, int y, Cel::Colour c)
    {
        y = y*s->pitch/BPP;

        Uint32 *pixmem32;
        Uint32 colour;  
     
        colour = SDL_MapRGB( s->format, c.r, c.g, c.b );
      
        pixmem32 = (Uint32*) s->pixels  + y + x;
        *pixmem32 = colour;
    }

    void drawFrame(SDL_Surface* s, int start_x, int start_y, const Cel::CelFrame& frame)
    {
        for(size_t x = 0; x < frame.mWidth; x++)
        {
            for(size_t y = 0; y < frame.mHeight; y++)
            {
                if(frame[x][y].visible)
                    setpixel(s, start_x+x, start_y+y, frame[x][y]);
            }
        }
    }

    void drawMinTile(SDL_Surface* s, Cel::CelFile& f, int x, int y, int16_t l, int16_t r)
    {
        if(l != -1)
            drawFrame(s, x, y, f[l]);

        if(r != -1)
            drawFrame(s, x+32, y, f[r]);
    }

    void drawMinPillar(SDL_Surface* s, int x, int y, const Level::MinPillar& pillar, Cel::CelFile& tileset, bool top)
    {
        // compensate for maps using 5-row min files
        if(pillar.size() == 10)
            y += 3*32;

        size_t i, lim;

        if(top)
        {
            i = 0;
            lim = pillar.size() - 2;
        }
        else
        {
            i = pillar.size() - 2;
            lim = pillar.size();
            y += i*16;
        }

        // Each iteration draw one row of the min
        for(; i < lim; i+=2)
        {
            int16_t l = (pillar[i]&0x0FFF)-1;
            int16_t r = (pillar[i+1]&0x0FFF)-1;
            
            drawMinTile(s, tileset, x, y, l, r);
        
            y += 32; // down 32 each row
        }
    }

    void drawMinPillarTop(SDL_Surface* s, int x, int y, const Level::MinPillar& pillar, Cel::CelFile& tileset)
    {
        drawMinPillar(s, x, y, pillar, tileset, true);
    }

    void drawMinPillarBase(SDL_Surface* s, int x, int y, const Level::MinPillar& pillar, Cel::CelFile& tileset)
    {
        drawMinPillar(s, x, y, pillar, tileset, false);
    }

    void drawAt(RenderLevel* level, const Sprite& sprite, int32_t x1, int32_t y1, int32_t x2, int32_t y2, size_t dist)
    {
        size_t w, h;
        spriteSize(sprite, w, h);

        int32_t xPx1 = ((y1*(-32)) + 32*x1 + level->levelWidth*32) + level->levelX - w/2;
        int32_t yPx1 = ((y1*16) + (16*x1) +160) + level->levelY;

        int32_t xPx2 = ((y2*(-32)) + 32*x2 + level->levelWidth*32) + level->levelX - w/2;
        int32_t yPx2 = ((y2*16) + (16*x2) +160) + level->levelY;

        int32_t x = xPx1 + ((((float)(xPx2-xPx1))/100.0)*(float)dist);
        int32_t y = yPx1 + ((((float)(yPx2-yPx1))/100.0)*(float)dist);

        drawAt(sprite, x, y);
    }

    std::pair<size_t, size_t> getClickedTile(RenderLevel* level, size_t x, size_t y)
    {
        // Position on the map in pixels
        int32_t flatX = x - level->levelX;
        int32_t flatY = y - level->levelY;

        // position on the map divided into 32x16 flat blocks
        // every second one of these blocks is centred on an isometric
        // block centre, the others are centred on isometric block corners
        int32_t flatGridX = (flatX+16) / 32;
        int32_t flatGridY = (flatY+8) / 16;
        
        // origin position (in flat grid coords) for the first line (isometric y = 0)
        int32_t flatOriginPosX = level->levelHeight;
        int32_t flatOriginPosY = 15;

        // when a flat grid box is clicked that does not centre on an isometric block, work out which
        // isometric quadrant of that box was clicked, then adjust flatGridPos accordingly
        if((flatGridX % 2 == 1 && flatGridY % 2 == 1) || (flatGridX % 2 == 0 && flatGridY % 2 == 0))
        {
            
            // origin of current flat grid box
            int32_t baseX = 32*flatGridX - 16;
            int32_t baseY = 16*flatGridY - 8;
            
            // position within grid box
            int32_t blockPosX = flatX - baseX;
            int32_t blockPosY = flatY - baseY;

            if(blockPosY*2 > blockPosX)
            {
                if(blockPosX < (15-blockPosY)*2)
                    flatGridX--;
                else
                    flatGridY++;
            }
            else
            {
                if(blockPosX < (15-blockPosY)*2)
                    flatGridY--;
                else
                    flatGridX++;
            }
        }
        
        // flatOrigin adjusted for the current y value
        int32_t lineOriginPosX = flatOriginPosX + ((flatGridX - flatOriginPosX) - (flatGridY - flatOriginPosY))/2;
        int32_t lineOriginPosY = flatOriginPosY - (-(flatGridX - flatOriginPosX) -( flatGridY - flatOriginPosY))/2;

        int32_t isoPosX = flatGridX - lineOriginPosX;
        int32_t isoPosY = flatGridY - lineOriginPosY;

        return std::make_pair(isoPosX, isoPosY);
    }

    void drawLevelHelper(RenderLevel* level, std::map<int32_t, Sprite>& minMap, int32_t x, int32_t y)
    {
        if(x < level->level->width() && y < level->level->height())
        {
            size_t index = level->level->operator[](x)[y].index();
            int32_t xCoord = (y*(-32)) + 32*x + level->level->height()*32-32 +level->levelX;
            int32_t yCoord = (y*16) + 16*x + level->levelY;

            if(xCoord >= -64 && xCoord <=  WIDTH  && yCoord >= -256 && yCoord <= HEIGHT && minMap.find(index) != minMap.end())
                drawAt(minMap[index], xCoord, yCoord);
        }
    }

    void drawLevel(RenderLevel* level, LevelObjects& objs, int32_t x1, int32_t y1, int32_t x2, int32_t y2, size_t dist)
    {
        int16_t xPx1 = -((y1*(-32)) + 32*x1 + level->levelWidth*32) +WIDTH/2;
        int16_t yPx1 = -((y1*16) + (16*x1) +160) + HEIGHT/2;

        int16_t xPx2 = -((y2*(-32)) + 32*x2 + level->levelWidth*32) +WIDTH/2;
        int16_t yPx2 = -((y2*16) + (16*x2) +160) + HEIGHT/2;

        level->levelX = xPx1 + ((((float)(xPx2-xPx1))/100.0)*(float)dist);
        level->levelY = yPx1 + ((((float)(yPx2-yPx1))/100.0)*(float)dist);

        //TODO clean up the magic numbers here, and elsewhere in this file
        
        for(size_t x = 0; x < level->level->width(); x++)
        {
            for(size_t y = 0; y < level->level->height(); y++)
            {
                drawLevelHelper(level, level->minBottoms, x+1, y+1);
            }
        }

        for(size_t x = 0; x < level->level->width(); x++)
        {
            for(size_t y = 0; y < level->level->height(); y++)
            {
                if(objs[x][y].sprite)
                    drawAt(level, objs[x][y].sprite, x, y, objs[x][y].x2, objs[x][y].y2, objs[x][y].dist);

                drawLevelHelper(level, level->minTops, x, y);
            }
        }
    }
}
