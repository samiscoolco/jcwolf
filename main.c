#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define TEXTURE_WIDTH 64
#define TEXTURE_HEIGHT 64
#define TEXTURE_SIZE (TEXTURE_WIDTH * TEXTURE_HEIGHT)
#define SCREEN_SCALE 8
#define MAX_CHUNKS 2048
#define FOV 60.0f
#define NEARTAG 0xA7
#define FARTAG 0xA8
#define NUMMAPS 60
#define MAPPLANES 3
#define PLANESIZE (64 * 64)
#define AREATILE 90
#define SPRITE_WIDTH 64
#define SPRITE_HEIGHT 64
#define SPRITE_DIM 64

typedef unsigned char byte;
typedef unsigned short word;
int doorOpenTest = 0;

typedef struct
{
    word RLEWtag;
    long headerOffsets[NUMMAPS];
    // tileinfo comes after this but we don't need it
} MapHead;

typedef struct
{
    long planestart[3];
    word planelength[3];
    word width, height;
    char name[16];
} MapHeader;

const char *VSWAP_FILE = "VSWAP.WL6";
const char *PAL_FILE = "wolf.pal";

unsigned int chunk_offsets[MAX_CHUNKS];
int sprite_start = 0;
int nextSprite = 0;

Color palette[256];

const float TPI = 2 * PI;
const float P2 = PI / 2;
const float P3 = 3 * PI / 2;

int screenWidth = 320;
int screenHeight = 200;

// native
int renderWidth = 320;
int renderHeight = 200;

// foolish
float wallDepth[2000];

RenderTexture2D renderTex;

float px, py, pa, pdx, pdy;
float speed;
float maxspeed;
float turnspeed;
float dt;
bool mode;
int vdep;

Texture2D doorTexture2d;
float shootFrame;
bool shooting;

int mapS = 64;
int mapX = 64, mapY = 64;
int *map;

Texture2D spTex[125];
Color *walltextures[125];

typedef struct
{
    int type;      // static, key, enemy
    int state;     // on off
    int map;       // texture to show
    float x, y, z; // position
    int dist;
} sprite;

sprite sp[255];
sprite spStatic[255];

Image stxloadVSWAP_Sprite(const char *filename, int desiredSpr)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        fprintf(stderr, "Error opening %s\n", filename);
        exit(1);
    }

    uint16_t totalChunks, spriteStart, soundStart;
    fread(&totalChunks, sizeof(uint16_t), 1, f);
    fread(&spriteStart, sizeof(uint16_t), 1, f);
    fread(&soundStart, sizeof(uint16_t), 1, f);

    uint32_t *offsets = malloc(totalChunks * sizeof(uint32_t));
    fread(offsets, sizeof(uint32_t), totalChunks, f);
    uint16_t *sizes = malloc(totalChunks * sizeof(uint16_t));
    fread(sizes, sizeof(uint16_t), totalChunks, f);

    int sprNum = spriteStart + desiredSpr;
    fseek(f, offsets[sprNum], SEEK_SET);
    uint8_t *spr = malloc(sizes[sprNum]);
    fread(spr, 1, sizes[sprNum], f);
    fclose(f);

    int16_t leftpix = *(int16_t *)&spr[0];
    int16_t rightpix = *(int16_t *)&spr[2];
    int colcount = rightpix - leftpix + 1;

    uint16_t *cmd_offsets = (uint16_t *)(spr + 4);
    uint8_t tmp[SPRITE_DIM * SPRITE_DIM] = {0};
    bool written[SPRITE_DIM * SPRITE_DIM] = {false};

    for (int col = 0; col < colcount; col++)
    {
        int x = leftpix + col;
        int ptr = cmd_offsets[col];

        while (1)
        {
            if (ptr + 6 > sizes[sprNum])
                break;

            int16_t endY2 = spr[ptr] | (spr[ptr + 1] << 8);
            int16_t dataOff = spr[ptr + 2] | (spr[ptr + 3] << 8);
            int16_t startY2 = spr[ptr + 4] | (spr[ptr + 5] << 8);

            if (endY2 == 0)
                break;

            int top = startY2 / 2;
            int bottom = endY2 / 2;
            int n = top + dataOff;

            for (int y = top; y < bottom; y++)
            {
                int index = y * SPRITE_DIM + x;
                if (x >= 0 && x < SPRITE_DIM && y >= 0 && y < SPRITE_DIM && n < sizes[sprNum])
                {
                    tmp[index] = spr[n];
                    written[index] = true;
                }
                n++;
            }

            ptr += 6;
        }
    }

    Color *pixels = malloc(sizeof(Color) * SPRITE_DIM * SPRITE_DIM);
    for (int y = 0; y < SPRITE_DIM; y++)
    {
        for (int x = 0; x < SPRITE_DIM; x++)
        {
            int index = y * SPRITE_DIM + x;
            uint8_t idx = tmp[index];
            if (written[index])
            {
                pixels[index] = palette[idx]; // use full color including black
            }
            else
            {
                pixels[index] = (Color){0, 0, 0, 0}; // fully transparent
            }
        }
    }

    free(offsets);
    free(sizes);
    free(spr);

    Image img = {
        .data = pixels,
        .width = SPRITE_DIM,
        .height = SPRITE_DIM,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    return img;
}

void drawDoors()
{
    for (int y = 0; y < mapY; y++)
    {
        for (int x = 0; x < mapX; x++)
        {
            int i = y * mapX + x;
            int tile = map[i];
            if (tile != 90 && tile != 91)
                continue;

            bool vertical = (tile == 90);
            float doorX = x * 64 + 32;
            float doorY = y * 64 + 32;
            
            //vert/hor doors
            //doorX += doorOpenTest;
            doorY += doorOpenTest;

            // Angle to center of door
            float dx = doorX - px;
            float dy = doorY - py;
            float distToDoor = sqrtf(dx * dx + dy * dy);
            float angleToDoor = atan2f(dy, dx) - pa;

            while (angleToDoor < -PI)
                angleToDoor += TPI;
            while (angleToDoor > PI)
                angleToDoor -= TPI;

            // Approx screen X of door center
            float centerX = (angleToDoor / (DEG2RAD * FOV)) * renderWidth + (renderWidth / 2);

            // Door angle width (depends on orientation)
            float halfWidth = atan2f(32, distToDoor);
            int screenX1 = (int)((angleToDoor - halfWidth) / (DEG2RAD * FOV) * renderWidth + renderWidth / 2);
            int screenX2 = (int)((angleToDoor + halfWidth) / (DEG2RAD * FOV) * renderWidth + renderWidth / 2);

            if (screenX1 < 0)
                screenX1 = 0;
            if (screenX2 >= renderWidth)
                screenX2 = renderWidth - 1;

            for (int sx = screenX1; sx <= screenX2; sx++)
            {
                float rayAngle = ((float)sx - renderWidth / 2.0f) / renderWidth * (DEG2RAD * FOV);
                float actualAngle = pa + rayAngle;

                float rayDirX = cosf(actualAngle);
                float rayDirY = sinf(actualAngle);

                // Ray intersection with door plane
                float denom = vertical ? rayDirX : rayDirY;
                if (fabs(denom) < 0.0001f)
                    continue;

                float planeOffset = vertical ? (doorX - px) / rayDirX : (doorY - py) / rayDirY;
                if (planeOffset < 0.1f)
                    continue;

                float hitX = px + rayDirX * planeOffset;
                float hitY = py + rayDirY * planeOffset;

                float tx = vertical ? (hitY - (y * 64)) : (hitX - (x * 64));
                int texX = (int)(tx / 64.0f * 64.0f);
                texX -= doorOpenTest;
                if (texX < 0)
                {
                    continue;
                }
                if (texX > 63)
                {
                    continue;
                }

                float correctedDist = planeOffset * cosf(rayAngle);
                float lineH = (64 * renderHeight) / correctedDist;
                float lineY = (renderHeight / 2) - (lineH / 2);

                // Occlusion
                if (sx >= 0 && sx < renderWidth && correctedDist < wallDepth[sx])
                {

                    Rectangle src = {(float)texX, 0, 1, 64};
                    Rectangle dst = {(float)sx, lineY, 1, lineH};
                    DrawTexturePro(doorTexture2d, src, dst, (Vector2){0, 0}, 0.0f, WHITE);

                    wallDepth[sx] = correctedDist;
                }
            }
        }
    }
}

void drawSprites()
{
    for (int s = 0; s < nextSprite; s++)
    {
        float sx = sp[s].x - px;
        float sy = sp[s].y - py;
        float sz = sp[s].z;

        float CS = cos(-pa), SN = sin(-pa);
        float a = sy * CS + sx * SN;
        float b = sx * CS - sy * SN;
        sx = a;
        sy = b;

        if (b <= 0.1f)
            continue; // sprite is behind player

        sx = (sx * renderWidth / sy) + (renderWidth / 2);
        sy = (sz * renderHeight / sy) + (renderHeight / 2);

        float scale = (renderHeight / 2.0f) / b;
        if (scale < 0)
            scale = 0;

        scale = scale * 30;

        float sprLeft = sx - (scale * 2.5f);
        float sprRight = sx + (scale * 2.5f);

        // === SPRITE OCCLUSION CHECK ===
        // Skip drawing if behind wall in center column
        int centerColumn = (int)sx;
        if (centerColumn >= 0 && centerColumn < renderWidth)
        {
            if (b > wallDepth[centerColumn])
                continue; // sprite is behind wall
        }

        // Draw the sprite
        DrawTexturePro(
            spTex[sp[s].map],
            (Rectangle){0, 0, 64, 64},
            (Rectangle){sx - (scale * 2.5), sy - (scale * 4), scale * 5, scale * 5},
            (Vector2){0, 0},
            0.0f,
            WHITE);
    }
}

void sortSprites()
{
    for (int i = 0; i < nextSprite; i++)
    {
        float dx = sp[i].x - px;
        float dy = sp[i].y - py;
        sp[i].dist = dx * dx + dy * dy; // squared distance is fine
    }

    // Simple bubble sort (fine for small sprite counts)
    for (int i = 0; i < nextSprite - 1; i++)
    {
        for (int j = i + 1; j < nextSprite; j++)
        {
            if (sp[i].dist < sp[j].dist)
            {
                sprite temp = sp[i];
                sp[i] = sp[j];
                sp[j] = temp;
            }
        }
    }
}

void CAL_CarmackExpand(unsigned short *source, unsigned short *dest, unsigned length)
{
    unsigned ch, chhigh, count, offset;
    unsigned short *copyptr, *inptr, *outptr;

    length /= 2;

    inptr = source;
    outptr = dest;

    while (length)
    {
        ch = *inptr++;
        chhigh = ch >> 8;
        if (chhigh == NEARTAG)
        {
            count = ch & 0xff;
            if (!count)
            { // have to insert a word containing the tag byte
                unsigned char *byteptr = (unsigned char *)inptr;
                ch |= *byteptr++;
                inptr = (word *)byteptr;
                *outptr++ = ch;
                length--;
            }
            else
            {
                unsigned char *byteptr = (unsigned char *)inptr;
                offset = *byteptr++;
                inptr = (word *)byteptr;
                copyptr = outptr - offset;
                length -= count;
                while (count--)
                    *outptr++ = *copyptr++;
            }
        }
        else if (chhigh == FARTAG)
        {
            count = ch & 0xff;
            if (!count)
            { // have to insert a word containing the tag byte
                unsigned char *byteptr = (unsigned char *)inptr;
                ch |= *byteptr++;
                inptr = (word *)byteptr;
                *outptr++ = ch;
                length--;
            }
            else
            {
                offset = *inptr++;
                copyptr = dest + offset;
                length -= count;
                while (count--)
                    *outptr++ = *copyptr++;
            }
        }
        else
        {
            *outptr++ = ch;
            length--;
        }
    }
}

void CA_RLEWexpand(word *source, word *dest, long length, word rlewtag)
{
    word value, count;
    word *end = dest + (length / 2);

    while (dest < end)
    {
        value = *source++;
        if (value != rlewtag)
        {
            *dest++ = value;
        }
        else
        {
            count = *source++;
            value = *source++;
            for (int i = 0; i < count; i++)
            {
                *dest++ = value;
            }
        }
    }
}

int *load_map_plane0(const char *maphead_path, const char *gamemaps_path, int map_number)
{
    FILE *fhead = fopen(maphead_path, "rb");
    FILE *fmap = fopen(gamemaps_path, "rb");
    if (!fhead || !fmap)
    {
        printf("Failed to open files.\n");
        return NULL;
    }

    // Load MAPHEAD
    MapHead maphead;
    fread(&maphead.RLEWtag, sizeof(word), 1, fhead);
    fread(maphead.headerOffsets, sizeof(long), NUMMAPS, fhead);

    long offset = maphead.headerOffsets[map_number];
    if (offset <= 0)
    {
        printf("Invalid map offset.\n");
        return NULL;
    }

    // Read map header
    fseek(fmap, offset, SEEK_SET);
    MapHeader header;
    fread(&header, sizeof(MapHeader), 1, fmap);

    long planeOffset = header.planestart[0];
    word planeLength = header.planelength[0];

    fseek(fmap, planeOffset, SEEK_SET);

    byte *compressed = malloc(planeLength);
    fread(compressed, 1, planeLength, fmap);

    word expandedLength = *(word *)compressed;
    word *carmack_source = (word *)(compressed + 2);
    word *carmack_output = malloc(expandedLength);

    CAL_CarmackExpand(carmack_source, carmack_output, expandedLength);

    word *rlew_source = carmack_output + 1;
    word *rlew_output = malloc(PLANESIZE * sizeof(word));

    CA_RLEWexpand(rlew_source, rlew_output, PLANESIZE * 2, maphead.RLEWtag);

    // Copy to int[]
    int *final_map = malloc(PLANESIZE * sizeof(int));
    for (int i = 0; i < PLANESIZE; i++)
    {
        if (rlew_output[i] <= 92)
        {
            final_map[i] = rlew_output[i];
        }
        else
        {
            final_map[i] = 0;
        }
    }

    free(compressed);
    free(carmack_output);
    free(rlew_output);
    fclose(fhead);
    fclose(fmap);

    return final_map;
}

int *load_map_plane1(const char *maphead_path, const char *gamemaps_path, int map_number, float *opx, float *opy, float *opa)
{
    FILE *fhead = fopen(maphead_path, "rb");
    FILE *fmap = fopen(gamemaps_path, "rb");
    if (!fhead || !fmap)
    {
        printf("Failed to open files.\n");
        return NULL;
    }

    // Load MAPHEAD
    MapHead maphead;
    fread(&maphead.RLEWtag, sizeof(word), 1, fhead);
    fread(maphead.headerOffsets, sizeof(long), NUMMAPS, fhead);

    long offset = maphead.headerOffsets[map_number];
    if (offset <= 0)
    {
        printf("Invalid map offset.\n");
        return NULL;
    }

    // Read map header
    fseek(fmap, offset, SEEK_SET);
    MapHeader header;
    fread(&header, sizeof(MapHeader), 1, fmap);

    long planeOffset = header.planestart[1];
    word planeLength = header.planelength[1];

    fseek(fmap, planeOffset, SEEK_SET);

    byte *compressed = malloc(planeLength);
    fread(compressed, 1, planeLength, fmap);

    word expandedLength = *(word *)compressed;
    word *carmack_source = (word *)(compressed + 2);
    word *carmack_output = malloc(expandedLength);

    CAL_CarmackExpand(carmack_source, carmack_output, expandedLength);

    word *rlew_source = carmack_output + 1;
    word *rlew_output = malloc(PLANESIZE * sizeof(word));

    CA_RLEWexpand(rlew_source, rlew_output, PLANESIZE * 2, maphead.RLEWtag);

    // find starting pos/angle
    int face;
    int curP = 0;
    int startP = 0;
    int *final_map = malloc(PLANESIZE * sizeof(int));
    for (int i = 0; i < PLANESIZE; i++)
    {
        uint8_t tile = rlew_output[i];
        if (tile >= 19 && tile <= 22)
        {
            face = tile;
            startP = curP;
        }

        // not sure the best way to do this it will get nasty
        // 180 - 183	Standing Guard	Hard
        // 144 - 147	Standing Guard	Medium
        // 108 - 111

        // guards
        if ((tile >= 180 && tile <= 183) || (tile >= 144 && tile <= 147) || (tile >= 108 && tile <= 111))
        {
            sp[nextSprite].type = 1;
            sp[nextSprite].state = 1;
            sp[nextSprite].map = 50;
            sp[nextSprite].x = (curP % 64) * 64 + 32;
            sp[nextSprite].y = (curP / 64) * 64 + 32;
            sp[nextSprite].z = 20;
            nextSprite++;
        }

        // its a static

        if (tile >= 23 && tile <= 73)
        {
            int statTile = tile - 23;
            int staticSprIndex = 2 + statTile;
            sp[nextSprite].type = 1;
            sp[nextSprite].state = 1;
            sp[nextSprite].map = staticSprIndex;
            sp[nextSprite].x = (curP % 64) * 64 + 32;
            sp[nextSprite].y = (curP / 64) * 64 + 32;
            sp[nextSprite].z = 20;
            nextSprite++;
        }
        curP++;
    }

    int startX = startP % 64;
    int startY = startP / 64;

    free(compressed);
    free(carmack_output);
    free(rlew_output);
    fclose(fhead);
    fclose(fmap);
    *opx = (float)startX * 64;
    *opy = (float)startY * 64;
    *opa = (float)(face - 19) * (PI / 2);

    // adjust for this engines quirks
    *opa = *opa - (PI / 2);
    return 0;
}

void LoadPalette(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f)
    {
        fprintf(stderr, "[ERROR] Couldn't open palette file %s\n", filename);
        exit(1);
    }
    char header[16];
    int num;
    fgets(header, sizeof(header), f); // JASC-PAL
    fgets(header, sizeof(header), f); // 0100
    fscanf(f, "%d", &num);            // 256
    for (int i = 0; i < num; i++)
    {
        int r, g, b;
        fscanf(f, "%d %d %d", &r, &g, &b);
        palette[i] = (Color){r, g, b, 255};
    }
    fclose(f);
}

int ReadVSwapHeader(FILE *f)
{
    uint16_t num_chunks;
    fread(&num_chunks, 2, 1, f);
    fread(&sprite_start, 2, 1, f);
    fseek(f, 2, SEEK_CUR); // skip soundStart

    fread(chunk_offsets, sizeof(uint32_t), num_chunks, f);
    fseek(f, 2 * num_chunks, SEEK_CUR); // skip chunk sizes
    return num_chunks;
}

unsigned char *ReadChunk(FILE *f, int offset)
{
    fseek(f, offset, SEEK_SET);
    unsigned char *data = malloc(TEXTURE_SIZE);
    fread(data, 1, TEXTURE_SIZE, f);
    return data;
}

Image DecodeWallTexture(unsigned char *data)
{
    Image img;
    img.width = TEXTURE_WIDTH;
    img.height = TEXTURE_HEIGHT;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    img.mipmaps = 1;
    Color *pixels = malloc(sizeof(Color) * TEXTURE_SIZE);

    for (int y = 0; y < TEXTURE_HEIGHT; y++)
    {
        for (int x = 0; x < TEXTURE_WIDTH; x++)
        {
            int idx = x * TEXTURE_WIDTH + y; // rotated in
            unsigned char colorIndex = data[idx];
            pixels[y * TEXTURE_WIDTH + x] = palette[colorIndex];
        }
    }
    img.data = pixels;
    return img;
}

float dist(float ax, float ay, float bx, float by, float ang)
{
    return (sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay)));
}

void drawMap2D()
{
    int x, y;
    float newmaps = renderWidth / mapX;
    float scalefactor = newmaps / mapS;

    float newmapsy = renderHeight / mapY;
    float scalefactory = newmapsy / mapS;
    int mapitem;
    Color sqColor;
    Texture2D sqTex;

    char numStr[4]; // enough for 3-digit tile numbers

    for (y = 0; y < mapY; y++)
    {
        for (x = 0; x < mapX; x++)
        {
            mapitem = map[y * mapX + x];
            float tileX = x * newmaps;
            float tileY = y * newmapsy;

            // Draw tile background
            // DrawRectangle(tileX, tileY, newmaps, newmapsy, BLACK);
            if (mapitem >= 1)
            {
                DrawRectangle(tileX, tileY, newmaps, newmapsy, BLUE);
            }
            else
            {
                DrawRectangle(tileX, tileY, newmaps, newmapsy, LIGHTGRAY);
            }

            // Draw tile number
            // snprintf(numStr, sizeof(numStr), "%d", mapitem);
            // DrawText(numStr, tileX + 3, tileY + 3, 10, RAYWHITE);
        }
    }

    DrawCircle(px * scalefactor, py * scalefactory, 5, RED);
    DrawLineEx((Vector2){px * scalefactor, py * scalefactory},
               (Vector2){px * scalefactor + (pdx * 10), py * scalefactory + (pdy * 10)},
               3.0f, MAROON);
}

void drawGame()
{
    int r, mx, my, mp, dof;
    float rx, ry, ra, xo, yo;

    int walltex = -1;
    int walltexv = -1;
    int walltexh = -1;
    int wallAdjacenth = 0;
    int wallAdjacentv = 0;
    int hitph = 0;
    int hitpv = 0;
    int hitp = 0;

    int num_rays = renderWidth;

    ra = pa - DEG2RAD * (FOV / 2); // Start left edge of FOV

    if (ra < 0)
    {
        ra += TPI;
    }
    if (ra > TPI)
    {
        ra -= TPI;
    }
    if (!mode)
    {
        drawMap2D();
    }
    for (int r = 0; r < num_rays; r++)
    {
        // check hor lines
        float disH = 1000000, hx = px, hy = py;
        // depth of field
        dof = 0;
        float aTan = -1 / tan(ra);
        if (ra > PI)
        {
            // looking up
            ry = floor(py / 64.0) * 64.0 - 0.01;
            rx = (py - ry) * aTan + px;
            yo = -64;
            xo = -yo * aTan;
        }
        if (ra < PI)
        {
            // looking down
            ry = floor(py / 64.0) * 64.0 + 64;
            rx = (py - ry) * aTan + px;
            yo = 64;
            xo = -yo * aTan;
        }
        if (ra == 0 || ra == PI)
        {
            // directly left or right
            rx = px;
            ry = py;
            dof = vdep;
        }
        while (dof < vdep)
        {
            mx = (int)floor(rx / 64.0);
            my = (int)floor(ry / 64.0);

            mp = my * mapX + mx;
            if (mp > 0 && mp < mapX * mapY && map[mp] >= 1 && map[mp] < AREATILE)
            {
                // hit wall
                hx = rx;
                hy = ry;
                disH = dist(px, py, hx, hy, ra);
                walltexh = map[mp];
                hitph = mp;
                dof = vdep;
            }
            else
            {
                rx += xo;
                ry += yo;
                dof += 1;
            }
        }

        // check vert lines
        float disV = 1000000, vx = px, vy = py;
        dof = 0; // depth of field
        float nTan = -tan(ra);
        if (ra > P2 && ra < P3)
        { // looking left

            rx = floor(px / 64.0) * 64.0 - 0.01;
            ry = (px - rx) * nTan + py;
            xo = -64;
            yo = -xo * nTan;
        }
        if (ra < P2 || ra > P3)
        { // looking right
            rx = floor(px / 64.0) * 64.0 + 64;
            ry = (px - rx) * nTan + py;
            xo = 64;
            yo = -xo * nTan;
        }
        if (ra == 0 || ra == PI)
        { // directly straight up or down
            rx = px;
            ry = py;
            dof = vdep;
        }
        while (dof < vdep)
        {
            mx = (int)floor(rx / 64.0);
            my = (int)floor(ry / 64.0);

            mp = my * mapX + mx;

            if (mp > 0 && mp < mapX * mapY && map[mp] >= 1 && map[mp] < AREATILE)
            {
                vx = rx;
                vy = ry;
                disV = dist(px, py, vx, vy, ra);
                dof = vdep; // hit wall
                walltexv = map[mp];
                hitpv = mp;
            }
            else
            {
                rx += xo;
                ry += yo;
                dof += 1;
            }
        }

        int hitdist;
        int wallSide;
        if (disV < disH)
        {
            rx = vx;
            ry = vy;
            hitdist = disV;
            wallSide = 0;
            walltex = walltexv;
            hitp = hitpv;
        }
        if (disH < disV)
        {
            rx = hx;
            ry = hy;
            hitdist = disH;
            wallSide = 1;
            walltex = walltexh;
            hitp = hitph;
        }

        if (mode)
        {
            wallDepth[r] = hitdist;
            hitdist *= cos(pa - ra); // Remove fisheye effect
            if (hitdist <= 0)
            {
                hitdist = 1;
            }

            float lineH = (mapS * renderHeight) / hitdist;
            float ty_step = 64 / (float)lineH;
            float ty_off = 0;

            if (lineH > renderHeight)
            {
                ty_off = (lineH - renderHeight) / 2.0;
                lineH = renderHeight;
            }
            float lineO = (renderHeight / 2) - lineH / 2;

            int pxy;
            float ty = ty_off * ty_step;
            float tx;
            if (wallSide == 1)
            {
                tx = (int)(rx) % 64;
            }
            else
            {
                tx = (int)(ry) % 64;
            }

            for (pxy = 0; pxy < lineH; pxy++)
            {
                if ((wallSide == 0 && (map[hitp + 1] >= 90 || map[hitp - 1] >= 90)) || (wallSide == 1 && (map[hitp + 64] >= 90 || map[hitp - 64] >= 90)))
                {
                    walltex = 51;
                }

                // Color c = walltextures[51][((int)(ty) * 64) + (int)tx];
                //  shading
                Color c = walltextures[(walltex - 1) * 2][((int)(ty) * 64) + (int)tx];
                if (wallSide == 1)
                {
                    c.r = c.r * 0.5;
                    c.g = c.g * 0.5;
                    c.b = c.b * 0.5;
                }

                DrawRectangle(r, pxy + lineO, 1, 1, c);
                ty += ty_step;
            }

            // DrawLineEx((Vector2){(r * 8.6), lineO}, (Vector2){(r * 8.6), lineH + lineO}, 8.6f, wallCol);
        }

        ra += DEG2RAD * (FOV / num_rays);
        if (ra < 0)
        {
            ra += TPI;
        }
        if (ra > TPI)
        {
            ra -= TPI;
        }
    }
}

void init()
{
    printf("initializing...");
    nextSprite = 0;
    mode = true;
    px = 0;
    py = 0;
    pa = 0;
    speed = 0;
    vdep = 300;
    shootFrame = 0;
    maxspeed = 300;   // max running speed
    turnspeed = 2.5f; // turning

    printf("loading textures...  %d", sprite_start);

    LoadPalette(PAL_FILE);
    FILE *f = fopen(VSWAP_FILE, "rb");
    int num_chunks = ReadVSwapHeader(f);

    for (int texnum = 0; texnum < 125; texnum++)
    {
        unsigned char *data = ReadChunk(f, chunk_offsets[texnum]);
        Image img = DecodeWallTexture(data);
        walltextures[texnum] = LoadImageColors(img);
        UnloadImage(img);
    }

    int texnum = 98;
    unsigned char *data = ReadChunk(f, chunk_offsets[texnum]);
    Image img = DecodeWallTexture(data);
    doorTexture2d = LoadTextureFromImage(img);
    UnloadImage(img);

    // what level
    int clevel = 0;
    printf("loading map data and textures for level %d", clevel + 1);
    map = load_map_plane0("MAPHEAD.WL6", "GAMEMAPS.WL6", clevel);
    if (!map)
    {
        fprintf(stderr, "Failed to load map from files.\n");
    }

    load_map_plane1("MAPHEAD.WL6", "GAMEMAPS.WL6", clevel, &px, &py, &pa);

    // ammo 28
    // guard 50
    // blueguard 138
    printf("loading sprites...");
    for (int sprnum = 0; sprnum < 100; sprnum++)
    {
        Image img = stxloadVSWAP_Sprite("VSWAP.WL6", sprnum);
        spTex[sprnum] = LoadTextureFromImage(img);
        UnloadImage(img);
    }

    printf("starting game...");
}

void buttons()
{
    // Handle turning
    if (IsKeyDown(KEY_A))
    {
        pa -= turnspeed * dt;
        if (pa < 0)
            pa += TPI;
    }
    if (IsKeyDown(KEY_D))
    {
        pa += turnspeed * dt;
        if (pa > TPI)
            pa -= TPI;
    }

    // Direction vector
    pdx = cos(pa);
    pdy = sin(pa);

    // Base movement speed
    speed = 0;

    // Forward/Backward
    if (IsKeyDown(KEY_W))
    {
        speed = maxspeed;
    }
    else if (IsKeyDown(KEY_S))
    {
        speed = -maxspeed;
    }

    // Sprinting
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
    {
        speed *= 3.0f;
    }

    // Strafing with comma (left) and period (right)
    if (IsKeyDown(KEY_PERIOD))
    {
        // perpendicular left
        px -= pdy * maxspeed * dt;
        py += pdx * maxspeed * dt;
    }
    if (IsKeyDown(KEY_COMMA))
    {
        // perpendicular right
        px += pdy * maxspeed * dt;
        py -= pdx * maxspeed * dt;
    }

    // Toggle map view
    if (IsKeyPressed(KEY_TAB))
    {
        mode = !mode;
    }

    if (IsKeyPressed(KEY_H))
    {
        doorOpenTest += 2;
    }

    if (IsKeyPressed(KEY_J))
    {
        doorOpenTest -= 2;
    }

    // Resolution down
    if (IsKeyPressed(KEY_M))
    {
        renderHeight -= 50;
        renderWidth -= 80;
        renderTex = LoadRenderTexture(renderWidth, renderHeight);
        printf("New Reso %dx%d\n", renderWidth, renderHeight);
    }

    // Resolution up
    if (IsKeyPressed(KEY_N))
    {
        renderHeight += 50;
        renderWidth += 80;
        renderTex = LoadRenderTexture(renderWidth, renderHeight);
        printf("New Reso %dx%d\n", renderWidth, renderHeight);
    }
}

void playerMovement()
{
    float radius = 12.0f;
    float next_px = px + pdx * speed * dt;
    float next_py = py + pdy * speed * dt;

    // Check X movement with buffer
    int mx1 = (int)(next_px + radius) / 64;
    int mx2 = (int)(next_px - radius) / 64;
    int my = (int)(py) / 64;
    if (map[my * mapX + mx1] < 1 && map[my * mapX + mx2] < 1)
    {
        px = next_px;
    }

    // Check Y movement with buffer
    int my1 = (int)(next_py + radius) / 64;
    int my2 = (int)(next_py - radius) / 64;
    int mx = (int)(px) / 64;
    if (map[my1 * mapX + mx] < 1 && map[my2 * mapX + mx] < 1)
    {
        py = next_py;
    }
}

int main(void)
{

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "JCWolf");
    init();
    renderTex = LoadRenderTexture(renderWidth, renderHeight);

    SetTargetFPS(60);
    char myString[50];

    while (!WindowShouldClose())
    {

        dt = GetFrameTime();

        buttons();
        playerMovement();

        BeginTextureMode(renderTex);
        ClearBackground(DARKGRAY);
        DrawRectangle(0, renderHeight / 2, renderWidth, renderHeight / 2, GRAY);
        drawGame();
        drawDoors();

        sortSprites();
        drawSprites();

        if (shootFrame > 5)
        {
            
            doorOpenTest+=1;
            shootFrame = 0;
            if(doorOpenTest>70){
                doorOpenTest=0;
            }
        }
        else
        {
            shootFrame += 100 * dt;
        }

        // if (mode)
        //{

        // DrawTextureRec(testSpriteTex, (Rectangle){0, 0, 64, 64}, (Vector2){32, 32}, WHITE);
        // DrawTexturePro(gun_tex, (Rectangle){(int)shootFrame * 64, 0, 64, 64}, (Rectangle){130, 300, 256, 256}, (Vector2){0, 0}, 0, WHITE);
        //  DrawTextureEx(gun_tex, (Vector2){drawX, drawY}, 0.0f, scale, WHITE);
        //}

        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);

        screenWidth = GetScreenWidth();
        screenHeight = GetScreenHeight();

        // scale to full screen
        DrawTexturePro(
            renderTex.texture,
            (Rectangle){0, 0, renderWidth, -renderHeight}, // flip Y axis for RenderTexture
            (Rectangle){0, 0, screenWidth, screenHeight},  // stretch to window size
            (Vector2){0, 0},
            0.0f,
            WHITE);

        // optionally draw HUD here if you want it full-res (e.g. overlays)
        snprintf(myString, 50, "JCWOLF x%f y%f fps: %d", px, py, GetFPS());
        DrawText(myString, 10, 10, 22, BLACK);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
