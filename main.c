// current todo
//  enemies walk through wall
//  tweak enemy view settings and get proper ammo/health values
//  enemies cant see through open doors because they only look at the tile
//  SOUND

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#define TEXTURE_WIDTH 64
#define TEXTURE_HEIGHT 64
#define TEXTURE_SIZE (TEXTURE_WIDTH * TEXTURE_HEIGHT)
#define MAX_CHUNKS 2048
#define NEARTAG 0xA7
#define FARTAG 0xA8
#define NUMMAPS 60
#define PLANESIZE (64 * 64)
#define AREATILE 90
#define SPRITE_WIDTH 64
#define SPRITE_HEIGHT 64
#define SPRITE_DIM 64
#define GROUND_Z 20
#define MELEEDIST 4800
#define ENEMY_VIEWDIST 300000
#define ENEMY_SHOOTDIST 200000
#define GUN_TEXTURES_START 421
#define DEBUGTARGETCOLOR WHITE

// are you using shareware or no?
#define PALFILE "WOLF.PAL"

// remove this if you have the WL6 files
#define SHAREWARE

#ifdef SHAREWARE
#define VSWAPFILE "VSWAP.WL1"
#define GAMEMAPSFILE "GAMEMAPS.WL1"
#define MAPHEADFILE "MAPHEAD.WL1"
#endif

#ifndef SHAREWARE
#define VSWAPFILE "VSWAP.WL6"
#define GAMEMAPSFILE "GAMEMAPS.WL6"
#define MAPHEADFILE "MAPHEAD.WL6"

#endif

// states
enum {
    STATE_COLLECTED,
    STATE_DEAD,
    STATE_DYING,
    STATE_HURT,
    STATE_ALERTED,
    STATE_SHOOT,
    STATE_STAND,
    STATE_PATROL,
    STATE_CHASE

};

typedef unsigned char byte;
typedef unsigned short word;
float ANIM_TIMER;

float FOV = 72.0f;

uint8_t logTimer = 0;
uint8_t maxLogTimer = 30;

uint8_t fxTimer = 0;
uint8_t maxFxTimer = 3;
Color feedbackColor;

Image framebuffer;
Color *fbPixels;
Texture2D frameTexture;

Font font;

char gameLog[100];

typedef struct
{
    word RLEWtag;
    long headerOffsets[NUMMAPS];
} MapHead;

typedef struct
{
    long planestart[3];
    word planelength[3];
    word width, height;
    char name[16];
} MapHeader;

unsigned int chunk_offsets[MAX_CHUNKS];
int nextSprite = 0;
uint8_t selectedGun = 1;
short clevel;
Color palette[256];

// floor and ceiling
Color ceilingColor;
Color floorColor;
Color borderColor;

const float TPI = 2 * PI;
const float P2 = PI / 2;
const float P3 = 3 * PI / 2;

bool hasKey1;
bool hasKey2;
bool hasKey3;

bool hasPistol = true;
bool hasMP40 = false;
bool hasChaingun = false;
uint8_t ammo;
uint8_t phealth;
int score;

int screenWidth = 640;
int screenHeight = 480;

// native
int renderWidth = 320;
int renderHeight = 200;

RenderTexture2D renderTex;

float px, py, pa, pdx, pdy;
float speed;
float strafespeed;
float maxspeed;
float turnspeed;
float dt;
bool mode;
int vdep;

Texture2D doorTexture2d;
Texture2D elevatorTexture2d;
Texture2D keydoorTexture2d;

float shootFrame;
bool shooting;

int mapS = 64;
int mapX = 64, mapY = 64;
int *map;
int *map_obj;

Texture2D spTex[500];
Color *walltextures[125];
typedef struct
{
    short type;
    short state;
    short state2;
    short health;
    short map;
    float x, y;
    int mappos;
    int dist;
    float angle;
    float speed;
    short map_start;
    short anim_start;
    short anim_len;
    short anim_currframe;
    bool targeted;
    bool anim_done;
    short anim_walk_start;
    short anim_walk_len;
    short anim_stand;
    short anim_pain;
    short anim_death_start;
    short anim_death_len;
    short anim_dead;
    short anim_shoot_start;
    short anim_shoot_len;
    bool anim_dontloop;
    bool cqc;

} sprite;

typedef struct
{
    short type;
    short state;
    short state2;
    short health;
    short map;
    float x, y;
    int mappos;
    int dist;
    float angle;
    float speed;
    short map_start;
    short anim_start;
    short anim_len;
    short anim_currframe;
    bool targeted;
    bool anim_done;
    short anim_walk_start;
    short anim_walk_len;
    short anim_stand;
    short anim_pain;
    short anim_death_start;
    short anim_death_len;
    short anim_dead;
    short anim_shoot_start;
    short anim_shoot_len;
    bool anim_dontloop;
    bool cqc;
} entity;

typedef struct
{
    uint8_t type;
    uint8_t state;
    float state2;
    float state3;
    float state4;
    float state5;
    float state6;
    int mappos;
    bool solid;
} interactable;

interactable *interactables;

// hopefully you dont need more than this, otherwise i will need intelligent management (i.e using old slots when a sprite is destoryed)
sprite sp[3000];
entity ents[3000];
float wallDepth[3000];

// ovride is if the visual spr needs to be seperate from the tile data
void addStatic(int tile, int mapxy, bool mapLoaded, int ovride) {
    int statTile = tile - 23;
    int staticSprIndex = 2 + statTile;
    sp[nextSprite].type = 0;
    sp[nextSprite].state = 1;
    sp[nextSprite].map = staticSprIndex;
    sp[nextSprite].x = (mapxy % 64) * 64 + 32;
    sp[nextSprite].y = (mapxy / 64) * 64 + 32;
    sp[nextSprite].mappos = mapxy;
    if (mapLoaded) {
        if (ovride > -1) {
            map_obj[mapxy] = ovride;
        } else {
            map_obj[mapxy] = tile;
        }
    }

    nextSprite++;
}

Image stxloadVSWAP_Sprite(FILE *f, int desiredSpr) {
    fseek(f, 0, SEEK_SET);
    if (!f) {
        fprintf(stderr, "Error opening vswap\n");
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
    if (sizes[sprNum] < 1) {
        return (Image){0};
    }
    fread(spr, 1, sizes[sprNum], f);

    int16_t leftpix = *(int16_t *)&spr[0];
    int16_t rightpix = *(int16_t *)&spr[2];
    int colcount = rightpix - leftpix + 1;

    uint16_t *cmd_offsets = (uint16_t *)(spr + 4);
    uint8_t tmp[SPRITE_DIM * SPRITE_DIM] = {0};
    bool written[SPRITE_DIM * SPRITE_DIM] = {false};

    for (int col = 0; col < colcount; col++) {
        int x = leftpix + col;
        int ptr = cmd_offsets[col];

        while (1) {
            if (ptr + 6 > sizes[sprNum]) {
                break;
            }

            int16_t endY2 = spr[ptr] | (spr[ptr + 1] << 8);
            int16_t dataOff = spr[ptr + 2] | (spr[ptr + 3] << 8);
            int16_t startY2 = spr[ptr + 4] | (spr[ptr + 5] << 8);

            if (endY2 == 0) {
                break;
            }

            int top = startY2 / 2;
            int bottom = endY2 / 2;
            int n = top + dataOff;

            for (int y = top; y < bottom; y++) {
                int index = y * SPRITE_DIM + x;
                if (x >= 0 && x < SPRITE_DIM && y >= 0 && y < SPRITE_DIM &&
                    n < sizes[sprNum]) {
                    tmp[index] = spr[n];
                    written[index] = true;
                }
                n++;
            }

            ptr += 6;
        }
    }

    Color *pixels = malloc(sizeof(Color) * SPRITE_DIM * SPRITE_DIM);
    for (int y = 0; y < SPRITE_DIM; y++) {
        for (int x = 0; x < SPRITE_DIM; x++) {
            int index = y * SPRITE_DIM + x;
            uint8_t idx = tmp[index];
            if (written[index]) {
                pixels[index] = palette[idx];  // use full color including black
            } else {
                pixels[index] = (Color){0, 0, 0, 0};  // fully transparent
            }
        }
    }

    free(offsets);
    free(sizes);
    free(spr);

    Image img = {.data = pixels,
                 .width = SPRITE_DIM,
                 .height = SPRITE_DIM,
                 .mipmaps = 1,
                 .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    return img;
}

void changeEntityState(sprite *thisEnt, int state) {
    thisEnt->state = state;
    thisEnt->anim_currframe = 0;
    thisEnt->anim_done = false;
    thisEnt->anim_dontloop = false;
    switch (state) {
        case STATE_ALERTED:
            // he saw us
            thisEnt->health -= 1;
            thisEnt->map = thisEnt->anim_stand;
            thisEnt->anim_start = -1;
            thisEnt->anim_len = 0;
            thisEnt->anim_dontloop = true;
            thisEnt->state2 = 3;
            break;
        case STATE_SHOOT:
            thisEnt->anim_start = thisEnt->anim_shoot_start;
            thisEnt->anim_len = thisEnt->anim_shoot_len;
            break;
        case STATE_HURT:
            // weapon damaged entity logic may change later
            thisEnt->health -= 1;
            thisEnt->map = thisEnt->anim_pain;
            thisEnt->anim_start = -1;
            thisEnt->anim_len = 0;
            thisEnt->anim_dontloop = true;
            thisEnt->state2 = 2;
            break;
        case STATE_PATROL:
        case STATE_CHASE:
            thisEnt->anim_start = thisEnt->anim_walk_start;
            thisEnt->anim_len = thisEnt->anim_walk_len;
            break;
        case STATE_DEAD:
            thisEnt->anim_start = -1;
            thisEnt->anim_len = 0;
            thisEnt->map = thisEnt->anim_dead;
            thisEnt->type = 0;
            int tileX = (int)thisEnt->x / 64;
            int tileY = (int)thisEnt->y / 64;
            int mapxy = tileY * mapX + tileX;
            // ammo pickup 49=ammo sprite, 75-23=boclip2tile
            addStatic(49, mapxy, true, 75);
            break;
        case STATE_DYING:
            thisEnt->anim_start = thisEnt->anim_death_start;
            thisEnt->anim_len = thisEnt->anim_death_len;
            thisEnt->anim_dontloop = true;
            break;
        default:
            // stand there and look pretty
            break;
    }
}

void drawDoors() {
    for (int y = 0; y < mapY; y++) {
        for (int x = 0; x < mapX; x++) {
            int i = y * mapX + x;
            // change to pointer
            interactable tile = interactables[i];
            if (tile.type != 90 && tile.type != 91 && tile.type != 100 && tile.type != 92 && tile.type != 93 && tile.type != 94 && tile.type != 95) {
                continue;
            }

            Texture2D *dtex;

            if (tile.type == 90 || tile.type == 91) {
                dtex = &doorTexture2d;
            } else if (tile.type >= 92 && tile.type <= 95) {
                dtex = &keydoorTexture2d;
            } else {
                dtex = &elevatorTexture2d;
            }

            bool vertical = (tile.type == 90 || tile.type == 92 || tile.type == 100 || tile.type == 94);
            float doorX = x * 64 + 32;
            float doorY = y * 64 + 32;

            // Angle to center of door
            float dx = doorX - px;
            float dy = doorY - py;
            float distToDoor = sqrtf(dx * dx + dy * dy);
            float angleToDoor = atan2f(dy, dx) - pa;

            while (angleToDoor < -PI) {
                angleToDoor += TPI;
            }

            while (angleToDoor > PI) {
                angleToDoor -= TPI;
            }

            // Approx screen X of door center
            float centerX = (angleToDoor / (DEG2RAD * FOV)) * renderWidth +
                            (renderWidth / 2);

            // Door angle width (depends on orientation)
            float halfWidth = atan2f(32, distToDoor);
            int screenX1 = (int)((angleToDoor - halfWidth) / (DEG2RAD * FOV) *
                                     renderWidth +
                                 renderWidth / 2);
            int screenX2 = (int)((angleToDoor + halfWidth) / (DEG2RAD * FOV) *
                                     renderWidth +
                                 renderWidth / 2);

            if (screenX1 < 0) {
                screenX1 = 0;
            }

            if (screenX2 >= renderWidth) {
                screenX2 = renderWidth - 1;
            }

            for (int sx = screenX1; sx <= screenX2; sx++) {
                float rayAngle = ((float)sx - renderWidth / 2.0f) /
                                 renderWidth * (DEG2RAD * FOV);
                float actualAngle = pa + rayAngle;

                float rayDirX = cosf(actualAngle);
                float rayDirY = sinf(actualAngle);

                // Ray intersection with door plane
                float denom = vertical ? rayDirX : rayDirY;
                if (fabs(denom) < 0.0001f) {
                    continue;
                }

                float planeOffset =
                    vertical ? (doorX - px) / rayDirX : (doorY - py) / rayDirY;

                if (planeOffset < 0.1f) {
                    continue;
                }

                float hitX = px + rayDirX * planeOffset;
                float hitY = py + rayDirY * planeOffset;

                float tx = vertical ? (hitY - (y * 64)) : (hitX - (x * 64));
                int texX = (int)(tx / 64.0f * 64.0f);
                texX -= tile.state2;
                if (texX < 0) {
                    continue;
                }
                if (texX > 63) {
                    continue;
                }

                float correctedDist = planeOffset * cosf(rayAngle);
                float lineH = (64 * renderHeight) / correctedDist;
                float lineY = (renderHeight / 2) - (lineH / 2);

                // Occlusion
                if (sx >= 0 && sx < renderWidth &&
                    correctedDist < wallDepth[sx]) {
                    Rectangle src = {(float)texX, 0, 1, 64};
                    Rectangle dst = {(float)sx, lineY, 1, lineH};
                    DrawTexturePro(*dtex, src, dst, (Vector2){0, 0},
                                   0.0f, WHITE);

                    wallDepth[sx] = correctedDist;
                }
            }
        }
    }
}

void drawSprites() {
    // Calculate view plane distance based on FOV
    float halfFOVrad = DEG2RAD * (FOV / 2.0f);
    float projectionPlaneDist = (renderWidth / 2.0f) / tanf(halfFOVrad);

    for (int s = 0; s < nextSprite; s++) {
        sp[s].targeted = false;
        // collected
        if (sp[s].state == STATE_COLLECTED) {
            continue;
        }

        float sx = sp[s].x - px;
        float sy = sp[s].y - py;

        // Rotate around player
        float CS = cosf(-pa), SN = sinf(-pa);

        float a = sy * CS + sx * SN;
        float b = sx * CS - sy * SN;
        sx = a;
        sy = b;

        // sprite is behind player
        if (b <= 0.1f) {
            continue;
        }

        // Perspective projection
        float screenX = (sx * projectionPlaneDist / b) + (renderWidth / 2.0f);
        float screenY = (GROUND_Z * renderHeight / b) + (renderHeight / 2.0f);

        // Scaling sprite based on distance
        float scale = (renderHeight / 2.0f) / b;
        scale = scale * 30.0f;

        float drawX = screenX - (scale * 2.5f);
        float drawY = screenY - (scale * 4.0f);
        float drawW = scale * 5.0f;
        float drawH = scale * 5.0f;

        if (screenX >= 0 && screenX < renderWidth) {
            // sprite is behind wall
            if (b > wallDepth[(int)screenX]) {
                continue;
            }
        } else {
            // these boys aint even on the screen son
            continue;
        }
        int angleIndex = 0;
        if (sp[s].type == 1 && sp[s].state > STATE_SHOOT) {
            float dx = px - sp[s].x;
            float dy = py - sp[s].y;
            float angleToPlayer = atan2f(dy, dx);  // world-space angle
            float spriteFacing = sp[s].angle;
            if (sp[s].state == 10) {
                spriteFacing = angleToPlayer;
            }
            float relativeAngle = angleToPlayer - spriteFacing;

            while (relativeAngle < 0.0f) {
                relativeAngle += TPI;
            }
            while (relativeAngle >= TPI) {
                relativeAngle -= TPI;
            }

            relativeAngle = TPI - relativeAngle;
            if (relativeAngle >= TPI) {
                relativeAngle -= TPI;
            }

            // Convert to 8-angle index
            angleIndex = (int)(relativeAngle / (TPI / 8.0f));  // 0–7
        }

        // if you made it this far the sprite is visible to the player.. now is it shootable?
        int centerRay = renderWidth / 2;
        float hitDepth = wallDepth[centerRay];

        float screenCenter = renderWidth / 2.0f;
        float centerThreshold = renderWidth * 0.08f;  // shot hitbox maybe tweak based on gun

        bool nearCenter = fabs(screenX - screenCenter) < centerThreshold;

        if (nearCenter && sp[s].type == 1) {
            sp[s].targeted = true;
            DrawTexturePro(
                spTex[sp[s].map + angleIndex],
                (Rectangle){0, 0, 64, 64},
                (Rectangle){drawX, drawY, drawW, drawH},
                (Vector2){0, 0}, 0.0f, DEBUGTARGETCOLOR);
        } else {
            DrawTexturePro(
                spTex[sp[s].map + angleIndex],
                (Rectangle){0, 0, 64, 64},
                (Rectangle){drawX, drawY, drawW, drawH},
                (Vector2){0, 0}, 0.0f, WHITE);
        }
    }
}

void sortSprites() {
    for (int i = 0; i < nextSprite; i++) {
        float dx = sp[i].x - px;
        float dy = sp[i].y - py;
        sp[i].dist = dx * dx + dy * dy;  // squared distance is fine
    }

    // Simple bubble sort (fine for small sprite counts)
    for (int i = 0; i < nextSprite - 1; i++) {
        for (int j = i + 1; j < nextSprite; j++) {
            if (sp[i].dist < sp[j].dist) {
                sprite temp = sp[i];
                sp[i] = sp[j];
                sp[j] = temp;
            }
        }
    }
}

void CAL_CarmackExpand(unsigned short *source, unsigned short *dest, unsigned length) {
    unsigned ch, chhigh, count, offset;
    unsigned short *copyptr, *inptr, *outptr;

    length /= 2;

    inptr = source;
    outptr = dest;

    while (length) {
        ch = *inptr++;
        chhigh = ch >> 8;
        if (chhigh == NEARTAG) {
            count = ch & 0xff;
            if (!count) {  // have to insert a word containing the tag byte
                unsigned char *byteptr = (unsigned char *)inptr;
                ch |= *byteptr++;
                inptr = (word *)byteptr;
                *outptr++ = ch;
                length--;
            } else {
                unsigned char *byteptr = (unsigned char *)inptr;
                offset = *byteptr++;
                inptr = (word *)byteptr;
                copyptr = outptr - offset;
                length -= count;
                while (count--)
                    *outptr++ = *copyptr++;
            }
        } else if (chhigh == FARTAG) {
            count = ch & 0xff;
            if (!count) {  // have to insert a word containing the tag byte
                unsigned char *byteptr = (unsigned char *)inptr;
                ch |= *byteptr++;
                inptr = (word *)byteptr;
                *outptr++ = ch;
                length--;
            } else {
                offset = *inptr++;
                copyptr = dest + offset;
                length -= count;
                while (count--)
                    *outptr++ = *copyptr++;
            }
        } else {
            *outptr++ = ch;
            length--;
        }
    }
}

void CA_RLEWexpand(word *source, word *dest, long length, word rlewtag) {
    word value, count;
    word *end = dest + (length / 2);

    while (dest < end) {
        value = *source++;
        if (value != rlewtag) {
            *dest++ = value;
        } else {
            count = *source++;
            value = *source++;
            for (int i = 0; i < count; i++) {
                *dest++ = value;
            }
        }
    }
}

void *load_map_plane0(const char *maphead_path, const char *gamemaps_path, int map_number) {
    FILE *fhead = fopen(maphead_path, "rb");
    FILE *fmap = fopen(gamemaps_path, "rb");
    if (!fhead || !fmap) {
        printf("Failed to open files.\n");
        return NULL;
    }

    // Load MAPHEAD
    MapHead maphead;
    fread(&maphead.RLEWtag, sizeof(word), 1, fhead);
    fread(maphead.headerOffsets, sizeof(long), NUMMAPS, fhead);

    long offset = maphead.headerOffsets[map_number];

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
    map = malloc(PLANESIZE * sizeof(int));
    interactables = malloc(PLANESIZE * sizeof(interactable));
    for (int i = 0; i < PLANESIZE; i++) {
        if (rlew_output[i] <= 100) {
            // all doors
            map[i] = rlew_output[i];
            if (rlew_output[i] == 90 || rlew_output[i] == 91 || rlew_output[i] == 100 || rlew_output[i] == 92 || rlew_output[i] == 93 || rlew_output[i] == 94 || rlew_output[i] == 95) {
                interactables[i].type = rlew_output[i];
                interactables[i].state = 0;
                interactables[i].state2 = 0;
                interactables[i].state3 = 0;
                interactables[i].mappos = i;
                interactables[i].solid = true;
            }
        } else {
            map[i] = 0;
            interactables[i].type = 0;
        }
    }

    free(compressed);
    free(carmack_output);
    free(rlew_output);
    fclose(fhead);
    fclose(fmap);
}

int *load_map_plane1(const char *maphead_path, const char *gamemaps_path, int map_number, float *opx, float *opy, float *opa) {
    FILE *fhead = fopen(maphead_path, "rb");
    FILE *fmap = fopen(gamemaps_path, "rb");
    if (!fhead || !fmap) {
        printf("Failed to open files.\n");
        return NULL;
    }

    // Load MAPHEAD
    MapHead maphead;
    fread(&maphead.RLEWtag, sizeof(word), 1, fhead);
    fread(maphead.headerOffsets, sizeof(long), NUMMAPS, fhead);

    long offset = maphead.headerOffsets[map_number];

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

    for (int i = 0; i < PLANESIZE; i++) {
        uint8_t tile = rlew_output[i];
        final_map[i] = rlew_output[i];

        if (rlew_output[i] == 98) {
            interactables[i].type = rlew_output[i];
            interactables[i].state = 0;
            interactables[i].state2 = 0;
            interactables[i].state3 = 0;
            interactables[i].mappos = i;
            interactables[i].solid = true;
        }

        if (tile >= 19 && tile <= 22) {
            face = tile;
            startP = curP;
        }

        // not sure the best way to do this it will get nasty
        // guards
        if ((tile >= 180 && tile <= 183) || (tile >= 144 && tile <= 147) ||
            (tile >= 108 && tile <= 111) || (tile >= 112 && tile <= 115) || (tile >= 148 && tile <= 151) || (tile >= 184 && tile <= 187)) {
            sp[nextSprite].type = 1;

            sp[nextSprite].map = 50;
            sp[nextSprite].x = (i % 64) * 64 + 32;
            sp[nextSprite].y = (i / 64) * 64 + 32;
            sp[nextSprite].angle = TPI / 2;

            sp[nextSprite].map_start = sp[nextSprite].map;
            sp[nextSprite].anim_stand = 50;
            sp[nextSprite].anim_pain = 94;
            sp[nextSprite].anim_walk_start = 58;
            sp[nextSprite].anim_walk_len = 4;
            sp[nextSprite].anim_shoot_start = 96;
            sp[nextSprite].anim_shoot_len = 3;
            sp[nextSprite].anim_death_start = 90;
            // death len including stray pain sprite
            sp[nextSprite].anim_death_len = 6;
            sp[nextSprite].anim_dead = 95;
            sp[nextSprite].health = 3;
            sp[nextSprite].cqc = false;
            changeEntityState(&sp[nextSprite], STATE_PATROL);
            nextSprite++;
        }

        // SS
        if ((tile >= 198 && tile <= 201) || (tile >= 162 && tile <= 165) ||
            (tile >= 126 && tile <= 129) || (tile >= 202 && tile <= 205) || (tile >= 166 && tile <= 169) || (tile >= 130 && tile <= 133)) {
            sp[nextSprite].type = 1;
            sp[nextSprite].map = 138;
            sp[nextSprite].x = (i % 64) * 64 + 32;
            sp[nextSprite].y = (i / 64) * 64 + 32;
            sp[nextSprite].mappos = i;
            sp[nextSprite].angle = 0;

            sp[nextSprite].map_start = sp[nextSprite].map;
            sp[nextSprite].anim_stand = 138;
            sp[nextSprite].anim_pain = 182;
            sp[nextSprite].anim_walk_start = 146;
            sp[nextSprite].anim_walk_len = 4;
            sp[nextSprite].anim_shoot_start = 184;
            sp[nextSprite].anim_shoot_len = 3;
            sp[nextSprite].anim_death_start = 178;
            // death len including stray pain sprite
            sp[nextSprite].anim_death_len = 6;
            sp[nextSprite].anim_dead = 183;
            sp[nextSprite].health = 6;
            sp[nextSprite].cqc = false;
            changeEntityState(&sp[nextSprite], STATE_PATROL);
            nextSprite++;
        }

        // OFFICER
        if ((tile >= 188 && tile <= 191) || (tile >= 152 && tile <= 155) ||
            (tile >= 116 && tile <= 119) || (tile >= 192 && tile <= 195) || (tile >= 156 && tile <= 159) || (tile >= 120 && tile <= 123)) {
            sp[nextSprite].type = 1;

            sp[nextSprite].map = 238;
            sp[nextSprite].x = (i % 64) * 64 + 32;
            sp[nextSprite].y = (i / 64) * 64 + 32;
            sp[nextSprite].mappos = i;
            sp[nextSprite].angle = 0;
            sp[nextSprite].map_start = sp[nextSprite].map;
            sp[nextSprite].anim_stand = 238;
            sp[nextSprite].anim_pain = 282;
            sp[nextSprite].anim_walk_start = 246;
            sp[nextSprite].anim_walk_len = 4;
            sp[nextSprite].anim_shoot_start = 285;
            sp[nextSprite].anim_shoot_len = 3;
            sp[nextSprite].anim_death_start = 278;
            // death len including stray pain sprite
            sp[nextSprite].anim_death_len = 6;
            sp[nextSprite].anim_dead = 284;
            sp[nextSprite].health = 10;
            sp[nextSprite].cqc = false;
            changeEntityState(&sp[nextSprite], STATE_PATROL);
            nextSprite++;
        }

        // ZOMBIE
        if ((tile >= 252 && tile <= 255) || (tile >= 234 && tile <= 237) ||
            (tile >= 216 && tile <= 219) || (tile >= 256 && tile <= 259) || (tile >= 238 && tile <= 241) || (tile >= 220 && tile <= 223)) {
            sp[nextSprite].type = 1;

            sp[nextSprite].map = 187;
            sp[nextSprite].x = (i % 64) * 64 + 32;
            sp[nextSprite].y = (i / 64) * 64 + 32;
            sp[nextSprite].mappos = i;
            sp[nextSprite].angle = 0;
            sp[nextSprite].map_start = sp[nextSprite].map;
            sp[nextSprite].anim_stand = 187;
            sp[nextSprite].anim_pain = 231;
            sp[nextSprite].anim_walk_start = 195;
            sp[nextSprite].anim_walk_len = 4;
            sp[nextSprite].anim_shoot_start = 234;
            sp[nextSprite].anim_shoot_len = 4;
            sp[nextSprite].anim_death_start = 227;
            // death len including stray pain sprite
            sp[nextSprite].anim_death_len = 7;
            sp[nextSprite].anim_dead = 233;
            sp[nextSprite].health = 7;
            sp[nextSprite].cqc = false;
            changeEntityState(&sp[nextSprite], STATE_PATROL);
            nextSprite++;
        }

        // EVIL DOG
        if ((tile >= 206 && tile <= 209) || (tile >= 170 && tile <= 173) ||
            (tile >= 134 && tile <= 137) || (tile >= 210 && tile <= 213) || (tile >= 174 && tile <= 177) || (tile >= 138 && tile <= 141)) {
            sp[nextSprite].type = 1;

            sp[nextSprite].map = 99;
            sp[nextSprite].x = (i % 64) * 64 + 32;
            sp[nextSprite].y = (i / 64) * 64 + 32;
            sp[nextSprite].mappos = i;
            sp[nextSprite].angle = 0;
            sp[nextSprite].map_start = sp[nextSprite].map;
            sp[nextSprite].anim_stand = 99;
            sp[nextSprite].anim_pain = 131;
            sp[nextSprite].anim_walk_start = 99;
            sp[nextSprite].anim_walk_len = 4;
            sp[nextSprite].anim_shoot_start = 135;
            sp[nextSprite].anim_shoot_len = 3;
            sp[nextSprite].anim_death_start = 131;
            // death len including stray pain sprite
            sp[nextSprite].anim_death_len = 4;
            sp[nextSprite].anim_dead = 134;
            sp[nextSprite].health = 2;
            sp[nextSprite].cqc = true;
            changeEntityState(&sp[nextSprite], STATE_PATROL);
            nextSprite++;
        }

        // its a static

        if (tile >= 23 && tile <= 73) {
            addStatic(tile, i, false, -1);
        }
        curP++;
    }

    free(compressed);
    free(carmack_output);
    free(rlew_output);
    fclose(fhead);
    fclose(fmap);

    int startX = startP % 64;
    int startY = startP / 64;

    *opx = (float)startX * 64 + 32;
    *opy = (float)startY * 64 + 32;
    *opa = (float)(face - 19) * (PI / 2);

    // adjust for this engines quirks
    *opa = *opa - (PI / 2);
    return final_map;
}

void LoadPalette() {
    printf("\nloading palette..\n");
    FILE *f = fopen(PALFILE, "r");
    char header[16];
    int num;
    fgets(header, sizeof(header), f);  // JASC-PAL
    fgets(header, sizeof(header), f);  // 0100
    fscanf(f, "%d", &num);             // 256
    for (int i = 0; i < num; i++) {
        int r, g, b;
        fscanf(f, "%d %d %d", &r, &g, &b);
        palette[i] = (Color){r, g, b, 255};
    }
    fclose(f);
}

unsigned char *ReadChunk(FILE *f, int offset) {
    fseek(f, offset, SEEK_SET);
    unsigned char *data = malloc(TEXTURE_SIZE);
    fread(data, 1, TEXTURE_SIZE, f);
    return data;
}

Image DecodeWallTexture(unsigned char *data) {
    Image img;
    img.width = TEXTURE_WIDTH;
    img.height = TEXTURE_HEIGHT;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    img.mipmaps = 1;
    Color *pixels = malloc(sizeof(Color) * TEXTURE_SIZE);

    for (int y = 0; y < TEXTURE_HEIGHT; y++) {
        for (int x = 0; x < TEXTURE_WIDTH; x++) {
            int idx = x * TEXTURE_WIDTH + y;  // rotated in
            unsigned char colorIndex = data[idx];
            pixels[y * TEXTURE_WIDTH + x] = palette[colorIndex];
        }
    }
    img.data = pixels;
    return img;
}

float dist(float ax, float ay, float bx, float by, float ang) {
    return (sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay)));
}

void LoadTextures() {
    printf("loading textures... ");
    uint16_t totalChunks, spriteStart, soundStart;

    FILE *f = fopen(VSWAPFILE, "rb");
    fread(&totalChunks, 2, 1, f);
    fread(&spriteStart, 2, 1, f);
    fseek(f, 2, SEEK_CUR);  // skip soundStart

    fread(chunk_offsets, sizeof(uint32_t), totalChunks, f);
    fseek(f, 2 * totalChunks, SEEK_CUR);  // skip chunk sizes

    for (int texnum = 0; texnum < 125; texnum++) {
        unsigned char *data = ReadChunk(f, chunk_offsets[texnum]);
        Image img = DecodeWallTexture(data);
        walltextures[texnum] = LoadImageColors(img);
        UnloadImage(img);
    }

    unsigned char *data = ReadChunk(f, chunk_offsets[98]);
    Image img = DecodeWallTexture(data);
    doorTexture2d = LoadTextureFromImage(img);
    UnloadImage(img);

    data = ReadChunk(f, chunk_offsets[24]);
    img = DecodeWallTexture(data);
    elevatorTexture2d = LoadTextureFromImage(img);
    UnloadImage(img);

    data = ReadChunk(f, chunk_offsets[104]);
    img = DecodeWallTexture(data);
    keydoorTexture2d = LoadTextureFromImage(img);
    UnloadImage(img);
}

void drawMap2D() {
    int x, y;
    float newmaps = screenWidth / mapX;
    float scalefactor = newmaps / mapS;

    float newmapsy = screenHeight / mapY;
    float scalefactory = newmapsy / mapS;
    int mapitem;
    Color sqColor;
    Texture2D sqTex;

    for (y = 0; y < mapY; y++) {
        for (x = 0; x < mapX; x++) {
            mapitem = map[y * mapX + x];
            float tileX = x * newmaps;
            float tileY = y * newmapsy;

            if (mapitem >= 1) {
                DrawRectangle(tileX, tileY, newmaps, newmapsy, BLUE);
            } else {
                DrawRectangle(tileX, tileY, newmaps, newmapsy, LIGHTGRAY);
            }
        }
    }

    DrawCircle(px * scalefactor, py * scalefactory, 10 * scalefactor * 2, RED);
    DrawLineEx((Vector2){px * scalefactor, py * scalefactory},
               (Vector2){px * scalefactor + (pdx * 10),
                         py * scalefactory + (pdy * 10)},
               3.0f, MAROON);
}

void drawGame() {
    for (int i = 0; i < renderWidth * renderHeight; i++) {
        if (i > (renderWidth * renderHeight) / 2) {
            fbPixels[i] = floorColor;
        } else {
            fbPixels[i] = ceilingColor;
        }
    }

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

    ra = pa - DEG2RAD * (FOV / 2);  // Start left edge of FOV

    if (ra < 0) {
        ra += TPI;
    }
    if (ra > TPI) {
        ra -= TPI;
    }

    for (int r = 0; r < num_rays; r++) {
        // check hor lines
        float disH = 1000000, hx = px, hy = py;
        // depth of field
        dof = 0;
        float aTan = -1 / tan(ra);
        if (ra > PI) {
            // looking up
            ry = floor(py / 64.0) * 64.0 - 0.01;
            rx = (py - ry) * aTan + px;
            yo = -64;
            xo = -yo * aTan;
        }
        if (ra < PI) {
            // looking down
            ry = floor(py / 64.0) * 64.0 + 64;
            rx = (py - ry) * aTan + px;
            yo = 64;
            xo = -yo * aTan;
        }
        if (ra == 0 || ra == PI) {
            // directly left or right
            rx = px;
            ry = py;
            dof = vdep;
        }
        while (dof < vdep) {
            mx = (int)floor(rx / 64.0);
            my = (int)floor(ry / 64.0);

            mp = my * mapX + mx;
            if (mp > 0 && mp < mapX * mapY && map[mp] >= 1 &&
                map[mp] < AREATILE) {
                // hit wall
                hx = rx;
                hy = ry;
                disH = dist(px, py, hx, hy, ra);
                walltexh = map[mp];
                hitph = mp;
                dof = vdep;
            } else {
                rx += xo;
                ry += yo;
                dof += 1;
            }
        }

        // check vert lines
        float disV = 1000000, vx = px, vy = py;
        dof = 0;  // depth of field
        float nTan = -tan(ra);
        if (ra > P2 && ra < P3) {  // looking left

            rx = floor(px / 64.0) * 64.0 - 0.01;
            ry = (px - rx) * nTan + py;
            xo = -64;
            yo = -xo * nTan;
        }
        if (ra < P2 || ra > P3) {  // looking right
            rx = floor(px / 64.0) * 64.0 + 64;
            ry = (px - rx) * nTan + py;
            xo = 64;
            yo = -xo * nTan;
        }
        if (ra == 0 || ra == PI) {  // directly straight up or down
            rx = px;
            ry = py;
            dof = vdep;
        }
        while (dof < vdep) {
            mx = (int)floor(rx / 64.0);
            my = (int)floor(ry / 64.0);

            mp = my * mapX + mx;

            if (mp > 0 && mp < mapX * mapY && map[mp] >= 1 &&
                map[mp] < AREATILE) {
                vx = rx;
                vy = ry;
                disV = dist(px, py, vx, vy, ra);
                dof = vdep;  // hit wall
                walltexv = map[mp];
                hitpv = mp;
            } else {
                rx += xo;
                ry += yo;
                dof += 1;
            }
        }

        int hitdist;
        int wallSide;
        if (disV < disH) {
            rx = vx;
            ry = vy;
            hitdist = disV;
            wallSide = 0;
            walltex = walltexv;
            hitp = hitpv;
        }
        if (disH < disV) {
            rx = hx;
            ry = hy;
            hitdist = disH;
            wallSide = 1;
            walltex = walltexh;
            hitp = hitph;
        }

        if (mode) {
            wallDepth[r] = hitdist;
            hitdist *= cos(pa - ra);  // Remove fisheye effect
            if (hitdist <= 0) {
                hitdist = 1;
            }

            float lineH = (mapS * renderHeight) / hitdist;
            float ty_step = 64 / (float)lineH;
            float ty_off = 0;

            if (lineH > renderHeight) {
                ty_off = (lineH - renderHeight) / 2.0;
                lineH = renderHeight;
            }
            float lineO = (renderHeight / 2) - lineH / 2;

            int pxy;
            float ty = ty_off * ty_step;
            float tx;
            if (wallSide == 1) {
                tx = (int)(rx) % 64;
            } else {
                tx = (int)(ry) % 64;
            }

            for (pxy = 0; pxy < lineH; pxy++) {
                if ((wallSide == 0 &&
                     (map[hitp + 1] >= 90 || map[hitp - 1] >= 90)) ||
                    (wallSide == 1 &&
                     (map[hitp + 64] >= 90 || map[hitp - 64] >= 90))) {
                    walltex = 51;
                }

                // Color c = walltextures[51][((int)(ty) * 64) + (int)tx];
                //  shading

                Color c = walltextures[(walltex - 1) * 2][((int)(ty) * 64) + (int)tx];
                if (wallSide == 0) {
                    c.r = c.r * 0.5;
                    c.g = c.g * 0.5;
                    c.b = c.b * 0.5;
                }
                int drawX = r;
                int drawY = (int)(lineO + pxy);  // final Y location
                if (drawX >= 0 && drawX < renderWidth && drawY >= 0 && drawY < renderHeight) {
                    fbPixels[drawY * renderWidth + drawX] = c;  // 'c' is the wall Color
                }

                // DrawRectangle(r, pxy + lineO, 1, 1, c);
                ty += ty_step;
            }

            // DrawLineEx((Vector2){(r * 8.6), lineO}, (Vector2){(r * 8.6),
            // lineH + lineO}, 8.6f, wallCol);
        }

        ra += DEG2RAD * (FOV / num_rays);
        if (ra < 0) {
            ra += TPI;
        }
        if (ra > TPI) {
            ra -= TPI;
        }
    }
    if (mode) {
        UpdateTexture(frameTexture, fbPixels);
        DrawTexture(frameTexture, 0, 0, WHITE);
    }
}

void LoadSprites() {
    printf("\nloading sprites...\n");
    FILE *f = fopen(VSWAPFILE, "rb");
    for (int sprnum = 0; sprnum < 440; sprnum++) {
        Image img = stxloadVSWAP_Sprite(f, sprnum);
        spTex[sprnum] = LoadTextureFromImage(img);
        UnloadImage(img);
    }
    fclose(f);
}

void LoadMapPlanes(uint8_t levelnum) {
    printf("\nloading map data for level %d\n", levelnum + 1);
    load_map_plane0(MAPHEADFILE, GAMEMAPSFILE, levelnum);
    map_obj = load_map_plane1(MAPHEADFILE, GAMEMAPSFILE, levelnum, &px, &py, &pa);
}

void init() {
    SetTraceLogLevel(LOG_NONE);
    // SetTraceLogLevel(LOG_ALL);

    printf("\ninitializing...\n");
    font = GetFontDefault();
    nextSprite = 0;
    mode = true;
    px = 0;
    py = 0;
    pa = 0;
    speed = 0;
    vdep = 100;
    shootFrame = 0;
    maxspeed = 325;    // max running speed
    turnspeed = 2.0f;  // turning
    hasKey1 = false;
    hasKey2 = false;
    hasKey3 = false;

    hasPistol = true;
    hasMP40 = false;
    hasChaingun = false;
    ammo = 10;
    // starting ammo debug
    // ammo = 255;

    phealth = 100;
    score = 0;
    ANIM_TIMER = 0.0f;

    LoadPalette();
    LoadTextures();
    LoadSprites();
    LoadMapPlanes(clevel);

    ceilingColor = palette[29];
    floorColor = palette[24];
    borderColor = palette[127];

    framebuffer = GenImageColor(renderWidth, renderHeight, DARKGRAY);
    fbPixels = LoadImageColors(framebuffer);
    frameTexture = LoadTextureFromImage(framebuffer);
    printf("\nstarting game...\n");
}

void flashScreen(Color dcolor) {
    fxTimer = maxFxTimer;
    feedbackColor = dcolor;
}

void logText(char *text) {
    snprintf(gameLog, 100, text);
    logTimer = maxLogTimer;
}

void buttons() {
    // Handle turning
    if (IsKeyDown(KEY_A)) {
        pa -= turnspeed * dt;
        if (pa < 0) {
            pa += TPI;
        }
    }
    if (IsKeyDown(KEY_D)) {
        pa += turnspeed * dt;
        if (pa > TPI) {
            pa -= TPI;
        }
    }

    // Direction vector
    pdx = cos(pa);
    pdy = sin(pa);

    // Base movement speed
    strafespeed = 0;
    speed = 0;

    // Forward/Backward
    if (IsKeyDown(KEY_W)) {
        speed = maxspeed;
    } else if (IsKeyDown(KEY_S)) {
        speed = -maxspeed;
    }

    // Strafing with comma (left) and period (right)
    if (IsKeyDown(KEY_PERIOD)) {
        strafespeed = -maxspeed;
    }
    if (IsKeyDown(KEY_COMMA)) {
        strafespeed = maxspeed;
    }

    // Sprinting
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
        speed *= 3.0f;
        strafespeed *= 2.5f;
    }

    // Toggle map view
    if (IsKeyPressed(KEY_TAB)) {
        mode = !mode;
    }

    // SHOT
    if (IsKeyDown(KEY_LEFT_CONTROL)) {
        if (!shooting) {
            // knife dont need ammo
            if (selectedGun != 0 && ammo > 0) {
                ammo--;
                shooting = true;
                // flashScreen(YELLOW);
            }

            else if (selectedGun == 0) {
                shooting = true;
            }
        }
    }

    if (!shooting) {
        if (IsKeyPressed(KEY_ONE)) {
            selectedGun = 0;
        }
        if (IsKeyPressed(KEY_TWO) && hasPistol) {
            selectedGun = 1;
        }
        if (IsKeyPressed(KEY_THREE) && hasMP40) {
            selectedGun = 2;
        }
        if (IsKeyPressed(KEY_FOUR) && hasChaingun) {
            selectedGun = 3;
        }
    }

    // debug stuff
    if (IsKeyPressed(KEY_N)) {
        // testpal -= 1;
        // borderColor = palette[testpal];
        // printf("%d pal", testpal);

        // renderHeight -= 50;
        // renderWidth -= 80;
        // renderTex = LoadRenderTexture(renderWidth, renderHeight);
        // printf("New Reso %dx%d\n", renderWidth, renderHeight);
        // framebuffer = GenImageColor(renderWidth, renderHeight, DARKGRAY);
        // fbPixels = LoadImageColors(framebuffer);
        // frameTexture = LoadTextureFromImage(framebuffer);
    }

    // debug stuff
    if (IsKeyPressed(KEY_M)) {
        // testpal += 1;
        // borderColor = palette[testpal];
        // printf("%d pal", testpal);

        // renderHeight += 50;
        // renderWidth += 80;
        // renderTex = LoadRenderTexture(renderWidth, renderHeight);
        // printf("\nNew Reso %dx%d\n", renderWidth, renderHeight);
        // framebuffer = GenImageColor(renderWidth, renderHeight, DARKGRAY);
        // fbPixels = LoadImageColors(framebuffer);
        // frameTexture = LoadTextureFromImage(framebuffer);
    }
}

void checkInteract(float x, float y) {
    int tileX = ((int)px + pdx * 64) / 64;
    int tileY = ((int)py + pdy * 64) / 64;

    int mapxy = tileY * mapX + tileX;
    int tile = map[mapxy];

    if (IsKeyDown(KEY_SPACE)) {
        printf("\n%d\n", tile);

        if (interactables[mapxy].state != 1) {
            interactables[mapxy].state = 1;
        }
    }
}

void removeStaticSprite(int mapxy) {
    for (int i = 0; i < nextSprite; i++) {
        if (sp[i].mappos == mapxy) {
            sp[i].state = STATE_COLLECTED;
            map_obj[mapxy] = 0;
        }
    }
}

void hurtSelf(short amt) {
    phealth = phealth - amt;
    if (phealth > 100) {
        phealth = 0;
    }
    if (phealth == 0) {
        // logs text kicking off the very primitive death screen timer that shares logTimer;
        logText("YOURE DEAD");
    }
}

bool healSelf(short amt) {
    if (phealth < 100) {
        phealth += amt;

        if (phealth > 100) {
            phealth = 100;
        }
        return true;
    }
    return false;
}

bool giveAmmo(short amt) {
    if (ammo < 99) {
        ammo += amt;

        if (ammo > 99) {
            ammo = 99;
        }
        return true;
    }
    return false;
}

void givePoints(short amt) {
    score += amt;
}

// handle interactions when bumping statics
bool checkStaticInteraction(int stile, int mapxy, bool fromPlayer) {
    stile = stile - 23;
    bool removeme = false;
    if (fromPlayer) {
        switch (stile) {
            case 6:  // Bad food (bo_alpo)
                removeme = healSelf(4);
                if (removeme) {
                    logText("Tasty kibble..");
                }
                break;
            case 20:  // Key 1 (bo_key1)
                hasKey1 = true;
                removeme = true;
                logText("Found the key!");
                break;
            case 21:  // Key 2 (bo_key2)
                hasKey2 = true;
                removeme = true;
                logText("Found the key 2!");

                break;
            case 24:  // Good food (bo_food)
                removeme = healSelf(10);
                if (removeme) {
                    logText("Found yummy food!");
                }
                break;
            case 25:  // First aid (bo_firstaid)
                removeme = healSelf(25);
                if (removeme) {
                    logText("Found med kit!");
                }
                break;
            case 52:  // Extra clip (bo_clip2)
                removeme = giveAmmo(4);
                if (removeme) {
                    logText("Found spare ammo!");
                }
                break;
            case 26:  // Clip (bo_clip)
                removeme = giveAmmo(8);
                if (removeme) {
                    logText("Found ammo!");
                }

                break;
            case 27:  // Machine gun (bo_machinegun)
                hasMP40 = true;
                giveAmmo(6);
                logText("Yeah brother thats an mp40!");
                removeme = true;
                break;
            case 28:  // Gatling gun (bo_chaingun)
                hasChaingun = true;
                giveAmmo(6);
                logText("Yeah brother thats a chaingun!");
                removeme = true;
                break;
            case 29:  // Cross (bo_cross)
                givePoints(100);
                removeme = true;
                logText("Found Cross!");

                break;
            case 30:  // Chalice (bo_chalice)
                givePoints(500);
                removeme = true;
                logText("Found Chalice!");

                break;
            case 31:  // Bible (bo_bible)
                givePoints(1000);
                removeme = true;
                logText("Found Gold Chest!");

                break;
            case 32:  // Crown (bo_crown)
                givePoints(5000);
                removeme = true;
                logText("Found Crown!");

                break;
            case 33:  // One up (bo_fullheal)
            case 34:  // Gibs (bo_gibs)
            case 38:  // Gibs 2 (bo_gibs)
        }
        if (removeme) {
            flashScreen(WHITE);
            removeStaticSprite(mapxy);
        }
    }

    // collisions
    switch (stile) {
        case 1:   // Green Barrel
        case 2:   // Table/chairs
        case 3:   // Floor lamp
        case 5:   // Hanged man
        case 7:   // Red pillar
        case 8:   // Tree
        case 10:  // Sink
        case 11:  // Potted plant
        case 12:  // Urn
        case 13:  // Bare table
        case 16:  // Suit of armor
        case 17:  // Hanging cage
        case 18:  // Skeleton in Cage
        case 22:  // Stuff (SOD gib)
        case 35:  // Barrel
        case 36:  // Well
        case 37:  // Empty well
        case 39:  // Flag
        case 40:  // Call Apogee
        case 45:  // Stove
        case 46:  // Spears
            return true;
        default:
            return false;
    }
}

bool checkCollision(float x, float y, bool fromPlayer) {
    int tileX = (int)x / 64;
    int tileY = (int)y / 64;

    int mapxy = tileY * mapX + tileX;
    int tile = map[mapxy];
    int stattile = map_obj[mapxy];

    // printf("%d ", stattile);

    if (tile >= 1 && tile < AREATILE) {
        return true;  // wall
    }

    if (tile == 90 || tile == 91 || tile == 100 || tile == 92 || tile == 93 || tile == 94 || tile == 95) {
        return interactables[mapxy].solid;
    }

    return checkStaticInteraction(stattile, mapxy, fromPlayer);
}
bool checkLine(float x0, float y0, float x1, float y1) {
    // Convert world coords to tile coords
    int tileX0 = (int)(x0 / 64.0f);
    int tileY0 = (int)(y0 / 64.0f);
    int tileX1 = (int)(x1 / 64.0f);
    int tileY1 = (int)(y1 / 64.0f);

    int dx = abs(tileX1 - tileX0);
    int dy = abs(tileY1 - tileY0);
    int sx = (tileX0 < tileX1) ? 1 : -1;
    int sy = (tileY0 < tileY1) ? 1 : -1;
    int err = dx - dy;

    while (tileX0 != tileX1 || tileY0 != tileY1) {
        // Check if this tile is a wall
        int index = tileY0 * mapX + tileX0;
        // area tile patch fix soon!
        if (map[index] >= 1 && map[index] < 100) {
            // is the door open?
            if (map[index] > 90) {
                if (interactables[index].solid) {
                    return false;  // wall blocks view
                }
            } else {
                return false;
            }
        }

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            tileX0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            tileY0 += sy;
        }
    }

    // printf("sfx Wha ha!");
    return true;  // clear path
}

bool canSeePlayer(sprite *enemy) {
    // Angle check
    float dx = px - enemy->x;
    float dy = py - enemy->y;
    float angleToPlayer = atan2f(dy, dx);
    float angleDiff = angleToPlayer - enemy->angle;
    while (angleDiff < -PI) {
        angleDiff += TPI;
    }

    while (angleDiff > PI) {
        angleDiff -= TPI;
    }

    if (fabsf(angleDiff) > 45.0f * DEG2RAD) {
        return false;
    }
    return checkLine(enemy->x, enemy->y, px, py);
}

void updateSprites() {
    for (int i = 0; i < nextSprite; i++) {
        float moveX = 0;
        float moveY = 0;
        float nextX = 0;
        float nextY = 0;
        float sprMoveSpeed = 50.0f * dt;

        if (sp[i].type == 1) {
            // getting shot is probably top level state
            if (sp[i].state != STATE_DEAD && sp[i].state != STATE_DYING && (int)shootFrame == 3 && sp[i].targeted && sp[i].state2 <= 0) {
                // if its melee weapon check that were close
                if (selectedGun == 0) {
                    if (sp[i].dist < MELEEDIST) {
                        changeEntityState(&sp[i], STATE_HURT);
                    }
                } else {
                    changeEntityState(&sp[i], STATE_HURT);
                }

                // dead
                if (sp[i].health <= 0) {
                    changeEntityState(&sp[i], STATE_DYING);
                }
            }

            else if (sp[i].state == STATE_HURT && ANIM_TIMER == 0) {
                sp[i].state2 -= 1;
                if (sp[i].state2 <= 0) {
                    changeEntityState(&sp[i], STATE_CHASE);
                }
            } else if (sp[i].state == STATE_ALERTED && ANIM_TIMER == 0) {
                sp[i].state2 -= 1;
                if (sp[i].state2 <= 0) {
                    changeEntityState(&sp[i], STATE_CHASE);
                }
            } else if (sp[i].state == STATE_DYING) {
                // anim is over
                if (sp[i].anim_done) {
                    // laid to rest
                    changeEntityState(&sp[i], STATE_DEAD);
                }
            }
            // behavior
            else if (sp[i].state == STATE_PATROL) {
                // Check if sprite sees player
                if (sp[i].dist < ENEMY_VIEWDIST && canSeePlayer(&sp[i])) {
                    changeEntityState(&sp[i], STATE_ALERTED);

                } else {
                    // Regular patrol movement
                    moveX = cosf(sp[i].angle) * sprMoveSpeed;
                    moveY = sinf(sp[i].angle) * sprMoveSpeed;
                }
            } else if (sp[i].state == STATE_CHASE) {
                // Distance to player
                float dx = px - sp[i].x;
                float dy = py - sp[i].y;

                // If close enough and can see, shoot
                int checkdist = ENEMY_SHOOTDIST;
                if (sp[i].cqc) {
                    checkdist = MELEEDIST;
                }
                if (sp[i].dist < checkdist && canSeePlayer(&sp[i])) {
                    changeEntityState(&sp[i], STATE_SHOOT);
                    // maybe face the player
                    sp[i].angle = atan2f(dy, dx);
                } else {
                    // Move toward player
                    float sprMoveSpeed = 60.0f * dt;
                    float angle = atan2f(dy, dx);
                    moveX = cosf(angle) * sprMoveSpeed;
                    moveY = sinf(angle) * sprMoveSpeed;
                    sp[i].angle = angle;
                }
            } else if (sp[i].state == STATE_SHOOT) {
                if (ANIM_TIMER == 0 && sp[i].anim_currframe == 2) {
                    // check for hit
                    flashScreen(RED);
                    hurtSelf(5);
                }
                if (!canSeePlayer(&sp[i])) {
                    changeEntityState(&sp[i], STATE_CHASE);
                }
                // printf("wallykazam");
            }

            // if movement is needed!
            if (moveX != 0 || moveY != 0) {
                // checkCollision
                nextX = sp[i].x + moveX;
                nextY = sp[i].y + moveY;
                int tileX = (int)(nextX / 64.0f);
                int tileY = (int)(nextY / 64.0f);
                float radius = 19.0f;

                // Try X axis movement separately
                if (!checkCollision(nextX + radius, sp[i].y, false) &&
                    !checkCollision(nextX - radius, sp[i].y, false) &&
                    !checkCollision(nextX, sp[i].y + radius, false) &&
                    !checkCollision(nextX, sp[i].y - radius, false)) {
                    sp[i].x = nextX;
                } else {
                    if (sp[i].state == STATE_PATROL) {
                        float newAngle = ((rand() % 360) * DEG2RAD);
                        sp[i].angle = newAngle;
                    }
                }

                // Try Y axis movement separately
                if (!checkCollision(sp[i].x + radius, nextY, false) &&
                    !checkCollision(sp[i].x - radius, nextY, false) &&
                    !checkCollision(sp[i].x, nextY + radius, false) &&
                    !checkCollision(sp[i].x, nextY - radius, false)) {
                    sp[i].y = nextY;
                } else {
                    if (sp[i].state == STATE_PATROL) {
                        float newAngle = ((rand() % 360) * DEG2RAD);
                        sp[i].angle = newAngle;
                    }
                }

                // if (sp[i].state == STATE_PATROL) {
            }

            // animatiom
            if (ANIM_TIMER == 0 && sp[i].state != STATE_DEAD && sp[i].anim_start >= 0) {
                sp[i].anim_done = false;
                sp[i].anim_currframe += 1;
                if (sp[i].anim_currframe >= sp[i].anim_len) {
                    sp[i].anim_currframe = 0;
                    sp[i].anim_done = true;
                }
                if (sp[i].anim_done && sp[i].anim_dontloop) {
                    return;
                }
                // handle animations
                if (sp[i].state > STATE_SHOOT) {
                    sp[i].map = sp[i].anim_start + sp[i].anim_currframe * 8;
                } else {
                    // handles a quirk where pain frame shows up in the middle of dying animation?
                    if (sp[i].state == STATE_DYING && sp[i].anim_start + sp[i].anim_currframe == sp[i].anim_pain) {
                        sp[i].anim_currframe += 1;
                    }
                    sp[i].map = sp[i].anim_start + sp[i].anim_currframe;
                }
            }

        } else {
            continue;
        }
    }
}

void updateInteractibles() {
    for (int i = 0; i < PLANESIZE; i++) {
        switch (interactables[i].type) {
            // push walls... needs anim
            case 98:
                if (interactables[i].state == 1) {
                    logText("A push wall? cool beans!");
                    interactables[i].state = 0;
                    map[interactables[i].mappos] = 0;
                }

            case 92:
            case 93:
            case 94:
            case 95:
            case 90:
            case 91:
            case 100:
                // locked door 1
                if ((interactables[i].type == 92 || interactables[i].type == 93) && !hasKey1 && interactables[i].state == 1) {
                    logText("The son of a gun is locked!");
                    interactables[i].state = 0;
                    break;
                }
                // locked door 2
                if ((interactables[i].type == 94 || interactables[i].type == 95) && !hasKey2 && interactables[i].state == 1) {
                    logText("The son of a gun is locked!");
                    interactables[i].state = 0;
                    break;
                }
                // close the mug after some time
                if (interactables[i].state3 == 1 && interactables[i].state == 0 && interactables[i].state4 * dt > 0) {
                    interactables[i].state4 -= 60 * dt;
                    if (interactables[i].state4 <= 0) {
                        interactables[i].state = 1;
                        interactables[i].state3 = 1;
                    }
                }
                // opening
                if (interactables[i].state == 1 && interactables[i].state3 == 0) {
                    interactables[i].state2 += 60 * dt;

                    if (interactables[i].state2 >= 64) {
                        interactables[i].solid = false;
                        interactables[i].state = 0;
                        interactables[i].state3 = 1;
                        interactables[i].state4 = 600;
                    }
                }
                // closing
                if (interactables[i].state == 1 && interactables[i].state3 == 1) {
                    interactables[i].state2 -= 60 * dt;
                    interactables[i].solid = true;
                    if (interactables[i].state2 <= 0) {
                        interactables[i].state = 0;
                        interactables[i].state3 = 0;
                    }
                }
            default:
                continue;
        }
    }
}

void playerMovement() {
    float radius = 19.0f;

    // Total movement input
    float moveX = pdx * speed * dt + pdy * strafespeed * dt;
    float moveY = pdy * speed * dt - pdx * strafespeed * dt;

    float try_px = px + moveX;
    float try_py = py + moveY;

    // Try X axis movement separately
    if (!checkCollision(try_px + radius, py, true) &&
        !checkCollision(try_px - radius, py, true) &&
        !checkCollision(try_px, py + radius, true) &&
        !checkCollision(try_px, py - radius, true)) {
        px = try_px;
    }

    // Try Y axis movement separately
    if (!checkCollision(px + radius, try_py, true) &&
        !checkCollision(px - radius, try_py, true) &&
        !checkCollision(px, try_py + radius, true) &&
        !checkCollision(px, try_py - radius, true)) {
        py = try_py;
    }

    checkInteract(px, py);
}

int main(int argc, char *argv[]) {
    // level select
    if (argc > 1) {
        clevel = atoi(argv[1]);
    } else {
        clevel = 0;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "JCWolf");
    init();
    renderTex = LoadRenderTexture(renderWidth, renderHeight);

    SetTargetFPS(60);
    char myString[50];

    while (!WindowShouldClose()) {
        dt = GetFrameTime();

        if (phealth > 0 && phealth < 101) {
            buttons();
            playerMovement();
            updateInteractibles();
            updateSprites();
        } else {
            flashScreen(RED);
            if (logTimer == 0) {
                // very primitive death screen timer using the logged death message.
                // next work on a restart level function
                exit(1);
            }
        }
        ANIM_TIMER += 20 * dt;
        if (ANIM_TIMER >= 3) {
            ANIM_TIMER = 0;
            if (logTimer > 0) {
                logTimer -= 1;
            }

            if (fxTimer > 0) {
                fxTimer -= 1;
            }
        }

        BeginTextureMode(renderTex);
        ClearBackground(DARKGRAY);

        if (mode) {
            drawGame();
            drawDoors();
            sortSprites();
            drawSprites();
        }

        if (shooting) {
            shootFrame += 15 * dt;
            // Weapon speed logic is hardcoded here.
            if ((int)shootFrame >= 5 || ((int)shootFrame >= 4 && selectedGun == 3) && IsKeyDown(KEY_LEFT_CONTROL)) {
                if (IsKeyDown(KEY_LEFT_CONTROL) && ammo > 0) {
                    if (selectedGun > 1) {
                        shootFrame = 2;
                    } else {
                        shootFrame = 0;
                    }

                    if (selectedGun > 0) {
                        ammo--;
                    }
                } else {
                    shootFrame = 0;
                    shooting = false;
                }
            }
        }

        if (mode) {
            float gunScale = renderHeight / 2.0f;  // or tweak to taste
            float gunWidth = gunScale * 2.0f;
            float gunHeight = gunScale * 2.0f;
            float gunX = (renderWidth / 2.0f) - (gunWidth / 2.0f);
            float gunY = renderHeight - gunHeight;

            DrawTexturePro(
                spTex[416 + (int)shootFrame + selectedGun * 5],
                (Rectangle){0, 0, 64, 64},  // full sprite
                (Rectangle){gunX, gunY, gunWidth, gunHeight},
                (Vector2){0, 0}, 0, WHITE);
            (spTex[GUN_TEXTURES_START], (Rectangle){0, 0, 64, 64}, (Rectangle){32, -55, 256, 256}, (Vector2){0, 0}, 0, WHITE);
        }

        EndTextureMode();

        BeginDrawing();
        ClearBackground(borderColor);

        screenWidth = GetScreenWidth();
        screenHeight = GetScreenHeight();

        if (!mode) {
            drawMap2D();
        } else {
            DrawTexturePro(
                renderTex.texture,
                (Rectangle){0, 0, renderWidth, -renderHeight},               // flip Y axis for RenderTexture
                (Rectangle){50, 50, screenWidth - 100, screenHeight - 100},  // stretch to window size
                (Vector2){0, 0}, 0.0f, WHITE);
            if (fxTimer > 0) {
                float alphaRatio = (float)fxTimer / maxFxTimer;
                int alpha = (int)(100 * alphaRatio);
                Color feedbackColorNow = (Color){feedbackColor.r, feedbackColor.g, feedbackColor.b, alpha};

                // Flash overlay
                DrawRectangle(50, 50, screenWidth - 100, screenHeight - 100, feedbackColorNow);
            }
        }
        // these 50s are preparing to make room for hud and have the resizable output window.

        // snprintf(myString, 50, "JCWOLF x%f y%f fps: %d", px, py, GetFPS());
        // DrawText(myString, 10, 10, 22, BLACK);

        if (logTimer > 0) {
            float alphaRatio = (float)logTimer / maxLogTimer;
            int alpha = (int)(255 * alphaRatio);
            Color fadeRed = (Color){255, 0, 0, alpha};
            DrawTextEx(font, gameLog, (Vector2){10, 25}, 22, 0, fadeRed);
        }

        snprintf(myString, 100, "HEALTH: %d   AMMO: %d    SCORE: %d", phealth, ammo, score);
        DrawText(myString, 10, screenHeight - 24, 22, WHITE);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
