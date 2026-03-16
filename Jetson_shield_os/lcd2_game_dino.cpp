#include "lcd2_game_dino.h"

#include "dino_game_objects.h"

namespace {

constexpr uint8_t kSpriteInk = 1;
constexpr uint8_t kSpritePaper = 0;

struct BitmapAsset {
    const unsigned char* data;
    int16_t w;
    int16_t h;
};

constexpr BitmapAsset kDinoJumpFrame = {dino_0, 30, 32};
constexpr BitmapAsset kDinoRunFrames[] = {
    {dino_1, 30, 32},
    {dino_2, 30, 32},
};

constexpr BitmapAsset kSunAsset = {sun, 50, 50};
constexpr BitmapAsset kSunAltAsset = {sun_1, 50, 50};
constexpr BitmapAsset kMoonAsset = {moon, 30, 29};

constexpr BitmapAsset kCloudDefs[] = {
    {cloud_0, 30, 17},
    {cloud_1, 45, 24},
};

constexpr BitmapAsset kStarDefs[] = {
    {star_0, 7, 7},
    {star_1, 11, 11},
        {star_2, 5, 5},
        {star_3, 9, 9},
};

constexpr BitmapAsset kCactusDefs[] = {
    {cactus_0, 10, 19},
    {cactus_1, 12, 26},
    {cactus_2, 14, 28},
    {cactus_3, 16, 35},
};

constexpr uint8_t kCloudTypeCount = static_cast<uint8_t>(sizeof(kCloudDefs) / sizeof(kCloudDefs[0]));
constexpr uint8_t kCactusTypeCount = static_cast<uint8_t>(sizeof(kCactusDefs) / sizeof(kCactusDefs[0]));
constexpr uint8_t kStarTypeCount = static_cast<uint8_t>(sizeof(kStarDefs) / sizeof(kStarDefs[0]));

constexpr int16_t kDinoW = 30;
constexpr int16_t kDinoH = 32;
constexpr int16_t kDinoX = 29;
constexpr int16_t kDinoJumpVY = -60;  // doubled: jump/fall animation is 2× faster
constexpr int16_t kDinoGravity = 14;   // doubled: same arc height, half the air time
constexpr uint32_t kDinoInitSpawnMs = 1100U;
constexpr uint32_t kDinoMinSpawnMs = 700U;
constexpr int16_t kDinoInitSpeed = 16;
constexpr int16_t kDinoMaxSpeed = 48;
constexpr uint8_t kDinoInitLives = 3;
constexpr int16_t kDinoGroundPad = 46;
constexpr uint32_t kGameDeadPauseMs = 1200U;
constexpr uint32_t kSpeedRampOdometerPx = 1600U;  // 100 "game-km": at base speed 16 px/tick → 100 ticks/ramp
constexpr int16_t kSpawnXMinOffset = 8;
constexpr int16_t kSpawnXMaxOffset = 28;
constexpr uint8_t kSpawnClusterSearchTries = 24;
constexpr int16_t kSurvivalSimMaxFrames = 128;
constexpr int16_t kClusterGapMinPx = 12;
constexpr int16_t kClusterGapMaxPx = 26;
constexpr int16_t kCloudMinY = 18;
constexpr int16_t kCloudTopPad = 86;
constexpr int16_t kCloudRespawnMinOffset = 8;
constexpr int16_t kCloudRespawnMaxOffset = 90;
constexpr int8_t kCloudMinSpeed = 1;
constexpr int8_t kCloudMaxSpeed = 2;
constexpr uint16_t kSpeedInitKmh = 45;
constexpr uint8_t kSpeedStepKmh = 1;
constexpr uint32_t kSkyPhaseDurationMs = 20000U;
constexpr uint32_t kSkyPhaseJitterMs = 500U;
constexpr uint32_t kSunAnimPeriod = 24U;  // ticks per sun animation frame (~400 ms at 60fps)
constexpr int16_t kSkyTopY = 18;
constexpr int16_t kSkyBottomPad = 92;
constexpr int16_t kSkyObjectRightPad = 104;
constexpr int16_t kSunRightPad = 74;   // kSkyObjectRightPad - 30px rightward shift
constexpr int16_t kSunY = 24;          // 6px lower than original 18
constexpr int16_t kMoonY = 22;
constexpr int16_t kGroundStepPx = 10;
constexpr uint8_t kStarMinActiveCount = 5;
constexpr uint8_t kStarMaxActiveCount = 8;
constexpr uint8_t kStarLifetimeMinSec = 6;
constexpr uint8_t kStarLifetimeMaxSec = 12;
constexpr uint8_t kStarBlinkMinSec = 1;
constexpr uint8_t kStarBlinkMaxSec = 8;
constexpr uint8_t kStarPlacementMaxTries = 72;
constexpr int16_t kStarMinGapPx = 10;
constexpr int16_t kStarPlacementInset = 2;
constexpr int16_t kScoreTileY = 0;
constexpr int16_t kScoreTileW = 86;
constexpr int16_t kScoreTileH = 18;
constexpr int16_t kSpeedTileX = 0;
constexpr int16_t kSpeedTileY = 14;
constexpr int16_t kSpeedTileW = 108;
constexpr int16_t kSpeedTileH = 20;
constexpr int16_t kExitTileW = 34;
constexpr int16_t kExitTileH = 22;
constexpr int16_t kExitTileOffsetX = 36;
constexpr int16_t kExitTileY = 2;
constexpr int16_t kLivesTileYOffset = 2;
constexpr int16_t kLivesTileW = 68;
constexpr int16_t kLivesTileH = 18;
constexpr int16_t kHudAvoidPad = 1;

constexpr size_t monoBitmapBytes(int16_t w, int16_t h) {
    return static_cast<size_t>(((w + 7) / 8) * h);
}

template <size_t N>
constexpr bool bitmapSizeMatches(const unsigned char (&)[N], int16_t w, int16_t h) {
    return N == monoBitmapBytes(w, h);
}

static_assert(bitmapSizeMatches(dino_0, 30, 32), "dino_0 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(dino_1, 30, 32), "dino_1 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(dino_2, 30, 32), "dino_2 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(cloud_0, 20, 11), "cloud_0 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(cloud_1, 30, 16), "cloud_1 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(cactus_0, 10, 19), "cactus_0 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(cactus_1, 12, 26), "cactus_1 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(cactus_2, 14, 28), "cactus_2 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(cactus_3, 16, 35), "cactus_3 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(sun, 50, 50), "sun metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(moon, 30, 29), "moon metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(star_0, 7, 7), "star_0 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(star_1, 11, 11), "star_1 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(sun_1, 50, 50), "sun_1 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(star_2, 5, 5), "star_2 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(star_3, 9, 9), "star_3 metadata must match bitmap payload size");

uint8_t hash8(uint32_t value) {
    value ^= (value << 13);
    value ^= (value >> 17);
    value ^= (value << 5);
    return static_cast<uint8_t>(value & 0xFFU);
}

const BitmapAsset& cactusAsset(uint8_t type) {
    const uint8_t safeType = (type < kCactusTypeCount) ? type : 0;
    return kCactusDefs[safeType];
}

const BitmapAsset& cloudAsset(uint8_t type) {
    const uint8_t safeType = (type < kCloudTypeCount) ? type : 0;
    return kCloudDefs[safeType];
}

const BitmapAsset& starAsset(uint8_t type) {
    const uint8_t safeType = (type < kStarTypeCount) ? type : 0;
    return kStarDefs[safeType];
}

uint8_t randomCactusType() {
    return static_cast<uint8_t>(random(kCactusTypeCount));
}

uint8_t randomCloudType() {
    return static_cast<uint8_t>(random(kCloudTypeCount));
}

int8_t randomCloudSpeed() {
    return static_cast<int8_t>(random(kCloudMinSpeed, kCloudMaxSpeed + 1));
}

uint8_t randomStarType() {
    return static_cast<uint8_t>(random(kStarTypeCount));
}

uint8_t randomStarLifetimeSec() {
    return static_cast<uint8_t>(random(kStarLifetimeMinSec, kStarLifetimeMaxSec + 1));
}

uint8_t randomStarBlinkFrequencySec() {
    return static_cast<uint8_t>(random(kStarBlinkMinSec, kStarBlinkMaxSec + 1));
}

uint32_t nextSkyToggleDelayMs() {
    return static_cast<uint32_t>(static_cast<int32_t>(kSkyPhaseDurationMs) +
                                 random(-static_cast<long>(kSkyPhaseJitterMs),
                                        static_cast<long>(kSkyPhaseJitterMs) + 1L));
}

uint16_t currentSpeedKmh(const LCD2DinoGameState& state) {
    const int16_t steps = max<int16_t>(0, static_cast<int16_t>(state.speed - kDinoInitSpeed));
    return static_cast<uint16_t>(kSpeedInitKmh + steps * kSpeedStepKmh);
}

int16_t cloudMaxY(int16_t ground) {
    return max<int16_t>(kCloudMinY + 4, static_cast<int16_t>(ground - kCloudTopPad));
}

int16_t skyBottomY(int16_t ground) {
    return max<int16_t>(kSkyTopY + 16, static_cast<int16_t>(ground - kSkyBottomPad));
}

int16_t groundY(uint16_t height) {
    return static_cast<int16_t>(height - kDinoGroundPad);
}

bool intersectsCactus(int16_t dinoY,
                      int16_t obsX,
                      int16_t obsW,
                      int16_t obsH,
                      int16_t ground) {
    const int16_t dinoLeft = static_cast<int16_t>(kDinoX + 5);
    const int16_t dinoRight = static_cast<int16_t>(kDinoX + kDinoW - 5);
    const int16_t dinoTop = static_cast<int16_t>(dinoY - kDinoH + 5);
    const int16_t dinoBottom = static_cast<int16_t>(dinoY - 4);

    const int16_t obsLeft = static_cast<int16_t>(obsX + 1);
    const int16_t obsRight = static_cast<int16_t>(obsX + obsW - 1);
    const int16_t obsTop = static_cast<int16_t>(ground - obsH);

    return (dinoRight > obsLeft) &&
           (dinoLeft < obsRight) &&
           (dinoBottom > obsTop) &&
           (dinoTop < ground);
}

int32_t drawDitheredBitmap(TFT_eSprite& spr,
                           int16_t x,
                           int16_t y,
                           const unsigned char* bitmap,
                           int16_t w,
                           int16_t h,
                           uint32_t phase) {
    if (bitmap == nullptr || w <= 0 || h <= 0) {
        return 0;
    }

    const int16_t rowBytes = static_cast<int16_t>((w + 7) / 8);
    int32_t drawnPixels = 0;
    for (int16_t py = 0; py < h; ++py) {
        for (int16_t px = 0; px < w; ++px) {
            const int16_t byteIndex = static_cast<int16_t>(py * rowBytes + (px / 8));
            const uint8_t bits = pgm_read_byte(bitmap + byteIndex);
            if ((bits & static_cast<uint8_t>(0x80U >> (px & 7))) == 0U) {
                continue;
            }

            // Checkerboard fill approximates gray on the 1-bit sprite.
            if (((x + px + y + py + static_cast<int16_t>(phase)) & 1) == 0) {
                spr.drawPixel(static_cast<int16_t>(x + px), static_cast<int16_t>(y + py), kSpriteInk);
                ++drawnPixels;
            }
        }
    }
    return drawnPixels;
}

void drawFallbackCloud(TFT_eSprite& spr, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t phase) {
    const int16_t r = max<int16_t>(2, h / 3);
    spr.fillCircle(static_cast<int16_t>(x + w / 4), static_cast<int16_t>(y + h / 2), r, kSpriteInk);
    spr.fillCircle(static_cast<int16_t>(x + w / 2), static_cast<int16_t>(y + h / 3), static_cast<int16_t>(r + 1), kSpriteInk);
    spr.fillCircle(static_cast<int16_t>(x + (w * 3) / 4), static_cast<int16_t>(y + h / 2), r, kSpriteInk);
    spr.fillRect(static_cast<int16_t>(x + 2), static_cast<int16_t>(y + h / 2),
                 static_cast<int16_t>(max<int16_t>(2, w - 4)),
                 static_cast<int16_t>(max<int16_t>(1, h / 3)),
                 kSpriteInk);

    for (int16_t py = 0; py < h; ++py) {
        for (int16_t px = 0; px < w; ++px) {
            if (((x + px + y + py + static_cast<int16_t>(phase)) & 1) != 0) {
                spr.drawPixel(static_cast<int16_t>(x + px), static_cast<int16_t>(y + py), kSpritePaper);
            }
        }
    }
}

void drawCloudSprite(TFT_eSprite& spr,
                     int16_t x,
                     int16_t y,
                     const BitmapAsset& cloud,
                     uint32_t phase) {
    const int32_t drawn = drawDitheredBitmap(spr, x, y, cloud.data, cloud.w, cloud.h, phase);
    if (drawn == 0) {
        drawFallbackCloud(spr, x, y, cloud.w, cloud.h, phase);
    }
}

void drawInvertedBitmap(TFT_eSprite& spr,
                        int16_t x,
                        int16_t y,
                        const unsigned char* bitmap,
                        int16_t w,
                        int16_t h,
                        uint8_t color) {
    if (bitmap == nullptr || w <= 0 || h <= 0) {
        return;
    }

    const int16_t rowBytes = static_cast<int16_t>((w + 7) / 8);
    for (int16_t py = 0; py < h; ++py) {
        for (int16_t px = 0; px < w; ++px) {
            const int16_t byteIndex = static_cast<int16_t>(py * rowBytes + (px / 8));
            const uint8_t bits = pgm_read_byte(bitmap + byteIndex);
            const bool bitSet = (bits & static_cast<uint8_t>(0x80U >> (px & 7))) != 0U;
            if (!bitSet) {
                spr.drawPixel(static_cast<int16_t>(x + px), static_cast<int16_t>(y + py), color);
            }
        }
    }
}

bool rectsOverlap(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                  int16_t bx, int16_t by, int16_t bw, int16_t bh) {
    return (ax < static_cast<int16_t>(bx + bw)) &&
           (static_cast<int16_t>(ax + aw) > bx) &&
           (ay < static_cast<int16_t>(by + bh)) &&
           (static_cast<int16_t>(ay + ah) > by);
}

bool overlapsWithPad(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                     int16_t bx, int16_t by, int16_t bw, int16_t bh,
                     int16_t pad) {
    return rectsOverlap(ax,
                        ay,
                        aw,
                        ah,
                        static_cast<int16_t>(bx - pad),
                        static_cast<int16_t>(by - pad),
                        static_cast<int16_t>(bw + pad * 2),
                        static_cast<int16_t>(bh + pad * 2));
}

bool overlapsMoonOrHudTiles(int16_t x,
                            int16_t y,
                            int16_t w,
                            int16_t h,
                            int16_t screenW,
                            int16_t ground) {
    const int16_t moonX = static_cast<int16_t>(screenW - kMoonAsset.w - kSkyObjectRightPad);
    const int16_t moonY = min<int16_t>(kMoonY, static_cast<int16_t>(skyBottomY(ground) - kMoonAsset.h));
    if (overlapsWithPad(x, y, w, h, moonX, moonY, kMoonAsset.w, kMoonAsset.h, kHudAvoidPad)) {
        return true;
    }

    const int16_t scoreTileX = static_cast<int16_t>(screenW / 2 - (kScoreTileW / 2));
    if (overlapsWithPad(x, y, w, h, scoreTileX, kScoreTileY, kScoreTileW, kScoreTileH, kHudAvoidPad)) {
        return true;
    }

    if (overlapsWithPad(x, y, w, h, kSpeedTileX, kSpeedTileY, kSpeedTileW, kSpeedTileH, kHudAvoidPad)) {
        return true;
    }

    const int16_t exitTileX = static_cast<int16_t>(screenW - kExitTileOffsetX);
    if (overlapsWithPad(x, y, w, h, exitTileX, kExitTileY, kExitTileW, kExitTileH, kHudAvoidPad)) {
        return true;
    }

    const int16_t livesTileX = static_cast<int16_t>(screenW - kLivesTileW - 6);
    return overlapsWithPad(x, y, w, h, livesTileX, kLivesTileYOffset, kLivesTileW, kLivesTileH, kHudAvoidPad);
}

bool canPlaceStarAt(const LCD2DinoGameState& state,
                    uint8_t placedCount,
                    int16_t x,
                    int16_t y,
                    int16_t screenW,
                    int16_t ground,
                    int16_t minY,
                    int16_t maxY,
                    uint8_t type) {
    const BitmapAsset& star = starAsset(type);
    if (x < kStarPlacementInset || y < minY) {
        return false;
    }

    const int16_t maxX = static_cast<int16_t>(screenW - star.w - kStarPlacementInset);
    if (x > maxX || y > maxY) {
        return false;
    }

    if (overlapsMoonOrHudTiles(x, y, star.w, star.h, screenW, ground)) {
        return false;
    }

    for (uint8_t i = 0; i < placedCount; ++i) {
        const BitmapAsset& other = starAsset(state.starType[i]);
        if (overlapsWithPad(x,
                            y,
                            star.w,
                            star.h,
                            state.starX[i],
                            state.starY[i],
                            other.w,
                            other.h,
                            kStarMinGapPx)) {
            return false;
        }
    }

    return true;
}

bool findStarPlacement(const LCD2DinoGameState& state,
                       uint8_t placedCount,
                       int16_t screenW,
                       int16_t ground,
                       int16_t minY,
                       int16_t maxY,
                       uint8_t type,
                       int16_t* outX,
                       int16_t* outY) {
    if (outX == nullptr || outY == nullptr) {
        return false;
    }

    const BitmapAsset& star = starAsset(type);
    const int16_t minX = kStarPlacementInset;
    const int16_t maxX = static_cast<int16_t>(screenW - star.w - kStarPlacementInset);
    const int16_t localMaxY = static_cast<int16_t>(maxY - star.h);
    if (maxX < minX || localMaxY < minY) {
        return false;
    }

    for (uint8_t tries = 0; tries < kStarPlacementMaxTries; ++tries) {
        const int16_t candX = static_cast<int16_t>(random(minX, maxX + 1));
        const int16_t candY = static_cast<int16_t>(random(minY, localMaxY + 1));
        if (canPlaceStarAt(state, placedCount, candX, candY, screenW, ground, minY, localMaxY, type)) {
            *outX = candX;
            *outY = candY;
            return true;
        }
    }

    for (int16_t candY = minY; candY <= localMaxY; candY += 2) {
        for (int16_t candX = minX; candX <= maxX; candX += 2) {
            if (canPlaceStarAt(state, placedCount, candX, candY, screenW, ground, minY, localMaxY, type)) {
                *outX = candX;
                *outY = candY;
                return true;
            }
        }
    }

    return false;
}

void activateStar(LCD2DinoGameState& state, uint8_t index, uint32_t nowMs) {
    if (index >= LCD2DinoGameState::kStarCount) {
        return;
    }

    state.starIsActivated[index] = true;
    state.starActivationStartMs[index] = nowMs;
    state.starLifetimeSec[index] = randomStarLifetimeSec();
    state.starBlinkFrequencySec[index] = randomStarBlinkFrequencySec();
    state.starTwinkle[index] = static_cast<uint8_t>(random(24));
}

void deactivateStar(LCD2DinoGameState& state, uint8_t index) {
    if (index >= LCD2DinoGameState::kStarCount) {
        return;
    }

    state.starIsActivated[index] = false;
}

uint8_t activeStarCount(const LCD2DinoGameState& state) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < LCD2DinoGameState::kStarCount; ++i) {
        if (state.starIsActivated[i]) {
            ++count;
        }
    }
    return count;
}

bool activateRandomInactiveStar(LCD2DinoGameState& state, uint32_t nowMs) {
    uint8_t inactiveIndices[LCD2DinoGameState::kStarCount];
    uint8_t inactiveCount = 0;
    for (uint8_t i = 0; i < LCD2DinoGameState::kStarCount; ++i) {
        if (!state.starIsActivated[i]) {
            inactiveIndices[inactiveCount++] = i;
        }
    }

    if (inactiveCount == 0) {
        return false;
    }

    const uint8_t pick = static_cast<uint8_t>(random(inactiveCount));
    activateStar(state, inactiveIndices[pick], nowMs);
    return true;
}

bool deactivateRandomActiveStar(LCD2DinoGameState& state) {
    uint8_t activeIndices[LCD2DinoGameState::kStarCount];
    uint8_t count = 0;
    for (uint8_t i = 0; i < LCD2DinoGameState::kStarCount; ++i) {
        if (state.starIsActivated[i]) {
            activeIndices[count++] = i;
        }
    }

    if (count == 0) {
        return false;
    }

    const uint8_t pick = static_cast<uint8_t>(random(count));
    deactivateStar(state, activeIndices[pick]);
    return true;
}

// Draws a cactus bitmap with pixel-precise bounds clipping and a 2px ground anchor stub.
void drawCactusSprite(TFT_eSprite& spr,
                      const unsigned char* bitmap,
                      int16_t x, int16_t y,
                      int16_t w, int16_t h) {
    if (bitmap == nullptr || w <= 0 || h <= 0) {
        return;
    }
    const int16_t sw = static_cast<int16_t>(spr.width());
    const int16_t sh = static_cast<int16_t>(spr.height());
    const int16_t rowBytes = static_cast<int16_t>((w + 7) / 8);
    for (int16_t py = 0; py < h; ++py) {
        const int16_t sy = static_cast<int16_t>(y + py);
        if (sy < 0 || sy >= sh) { continue; }
        for (int16_t px = 0; px < w; ++px) {
            const int16_t sx = static_cast<int16_t>(x + px);
            if (sx < 0 || sx >= sw) { continue; }
            const uint8_t b = pgm_read_byte(bitmap + py * rowBytes + px / 8);
            if (b & (static_cast<uint8_t>(0x80U) >> (px & 7U))) {
                spr.drawPixel(sx, sy, kSpriteInk);
            }
        }
    }
    // 2-pixel anchor below the cactus base so it looks planted in the ground
    const int16_t anchorX = static_cast<int16_t>(x + w / 2);
    for (int16_t dy = 0; dy < 2; ++dy) {
        const int16_t sy = static_cast<int16_t>(y + h + dy);
        if (sy >= 0 && sy < sh && anchorX >= 0 && anchorX < sw) {
            spr.drawPixel(anchorX, sy, kSpriteInk);
        }
    }
}

bool hasActiveObstacle(const LCD2DinoGameState& state) {
    for (uint8_t i = 0; i < LCD2DinoGameState::kMaxObs; ++i) {
        if (state.obsActive[i]) {
            return true;
        }
    }
    return false;
}

bool canSurviveCluster(int16_t speed,
                       const int16_t obsXArr[],
                       const uint8_t obsTypeArr[],
                       uint8_t count, int16_t ground) {
    const int16_t safeSpeed = max<int16_t>(1, speed);
    int16_t simObsX[LCD2DinoGameState::kMaxObs];

    for (int16_t jumpFrame = 0; jumpFrame < kSurvivalSimMaxFrames; ++jumpFrame) {
        for (uint8_t n = 0; n < count; ++n) simObsX[n] = obsXArr[n];
        int16_t simY = ground;
        int16_t simVY = 0;
        bool simOnGround = true;
        bool collision = false;

        for (int16_t frame = 0; frame < kSurvivalSimMaxFrames; ++frame) {
            if (frame == jumpFrame && simOnGround) {
                simVY = kDinoJumpVY;
                simOnGround = false;
            }

            simVY = static_cast<int16_t>(simVY + kDinoGravity);
            simY = static_cast<int16_t>(simY + simVY);
            if (simY >= ground) {
                simY = ground;
                simVY = 0;
                simOnGround = true;
            }

            bool allGone = true;
            for (uint8_t n = 0; n < count; ++n) {
                simObsX[n] = static_cast<int16_t>(simObsX[n] - safeSpeed);
                const BitmapAsset& cactus = cactusAsset(obsTypeArr[n]);
                if (intersectsCactus(simY, simObsX[n], cactus.w, cactus.h, ground)) {
                    collision = true;
                    break;
                }
                if (simObsX[n] + cactus.w >= 0) {
                    allGone = false;
                }
            }
            if (collision) break;
            if (allGone) break;
        }

        if (!collision) {
            return true;
        }
    }

    return false;
}

bool pickSurvivableCluster(int16_t speed,
                           int16_t screenW,
                           uint8_t count,
                           int16_t ground,
                           int16_t* clusterXs,
                           uint8_t* clusterTypes) {
    for (uint8_t attempt = 0; attempt < kSpawnClusterSearchTries; ++attempt) {
        clusterXs[0] = static_cast<int16_t>(screenW + random(kSpawnXMinOffset, kSpawnXMaxOffset + 1));
        clusterTypes[0] = randomCactusType();

        for (uint8_t n = 1; n < count; ++n) {
            clusterTypes[n] = randomCactusType();
            const BitmapAsset& prev = cactusAsset(clusterTypes[n - 1]);
            clusterXs[n] = static_cast<int16_t>(clusterXs[n - 1] + prev.w +
                           random(kClusterGapMinPx, kClusterGapMaxPx + 1));
        }

        if (canSurviveCluster(speed, clusterXs, clusterTypes, count, ground)) {
            return true;
        }
    }

    return false;
}

void clearObstacles(LCD2DinoGameState& state) {
    for (uint8_t i = 0; i < LCD2DinoGameState::kMaxObs; ++i) {
        state.obsActive[i] = false;
        state.obsX[i] = 0;
        state.obsH[i] = 0;
        state.obsType[i] = 0;
    }
}

void seedGroundProfile(LCD2DinoGameState& state) {
    for (uint8_t i = 0; i < LCD2DinoGameState::kGroundBumpCount; ++i) {
        const long roll = random(100);
        state.groundBump[i] = (roll < 58) ? 0 : (roll < 88) ? 1 : 2;
    }
}

void seedStars(LCD2DinoGameState& state, uint16_t width, uint16_t height, uint32_t nowMs) {
    const int16_t screenW = static_cast<int16_t>(width);
    const int16_t ground = groundY(height);
    const int16_t maxY = skyBottomY(ground);
    const int16_t minY = kSkyTopY;

    for (uint8_t i = 0; i < LCD2DinoGameState::kStarCount; ++i) {
        state.starType[i] = randomStarType();
        state.starTwinkle[i] = static_cast<uint8_t>(random(24));
        state.starLifetimeSec[i] = randomStarLifetimeSec();
        state.starBlinkFrequencySec[i] = randomStarBlinkFrequencySec();
        state.starActivationStartMs[i] = nowMs;
        state.starIsActivated[i] = false;

        int16_t starX = kStarPlacementInset;
        int16_t starY = minY;
        const bool hasPlacement = findStarPlacement(state,
                                                    i,
                                                    screenW,
                                                    ground,
                                                    minY,
                                                    maxY,
                                                    state.starType[i],
                                                    &starX,
                                                    &starY);
        if (!hasPlacement) {
            // Fallback keeps game stable on tiny displays where strict spacing cannot fit all stars.
            const int16_t fallbackSpanX = max<int16_t>(1, static_cast<int16_t>(screenW - 16));
            const int16_t fallbackSpanY = max<int16_t>(1, static_cast<int16_t>(maxY - minY + 1));
            starX = static_cast<int16_t>(kStarPlacementInset + ((i * 13) % fallbackSpanX));
            starY = static_cast<int16_t>(minY + ((i * 7) % fallbackSpanY));
        }

        state.starX[i] = starX;
        state.starY[i] = starY;
    }

    const uint8_t activeCount = static_cast<uint8_t>(random(kStarMinActiveCount, kStarMaxActiveCount + 1));
    for (uint8_t i = 0; i < activeCount; ++i) {
        if (!activateRandomInactiveStar(state, nowMs)) {
            break;
        }
    }
}

void seedClouds(LCD2DinoGameState& state, uint16_t width, uint16_t height) {
    const int16_t screenW = static_cast<int16_t>(width);
    const int16_t ground = groundY(height);
    const int16_t maxY = cloudMaxY(ground);

    for (uint8_t i = 0; i < LCD2DinoGameState::kMaxClouds; ++i) {
        state.cloudType[i] = randomCloudType();
        state.cloudSpeed[i] = randomCloudSpeed();
        state.cloudY[i] = static_cast<int16_t>(random(kCloudMinY, maxY + 1));

        const int16_t span = max<int16_t>(1, screenW / LCD2DinoGameState::kMaxClouds);
        state.cloudX[i] = static_cast<int16_t>(i * span + random(span));
    }
}

void tickSky(LCD2DinoGameState& state, uint16_t width, uint16_t height, uint32_t nowMs) {
    if (static_cast<int32_t>(nowMs - state.nextSkyToggleMs) < 0) {
        return;
    }

    state.isNight = !state.isNight;
    state.nextSkyToggleMs = nowMs + nextSkyToggleDelayMs();
    if (state.isNight) {
        seedStars(state, width, height, nowMs);
    } else {
        seedClouds(state, width, height);
    }
}

void tickStars(LCD2DinoGameState& state, uint32_t nowMs) {
    for (uint8_t i = 0; i < LCD2DinoGameState::kStarCount; ++i) {
        if (!state.starIsActivated[i]) {
            continue;
        }

        const uint32_t lifeMs = static_cast<uint32_t>(state.starLifetimeSec[i]) * 1000U;
        if (lifeMs > 0U && (nowMs - state.starActivationStartMs[i]) >= lifeMs) {
            state.starIsActivated[i] = false;
        }
    }

    uint8_t active = activeStarCount(state);
    while (active < kStarMinActiveCount) {
        if (!activateRandomInactiveStar(state, nowMs)) {
            break;
        }
        ++active;
    }

    const uint8_t targetMax = static_cast<uint8_t>(random(kStarMinActiveCount, kStarMaxActiveCount + 1));
    while (active > targetMax) {
        if (!deactivateRandomActiveStar(state)) {
            break;
        }
        --active;
    }
}

void tickClouds(LCD2DinoGameState& state, int16_t screenW, int16_t ground) {
    const int16_t maxY = cloudMaxY(ground);
    for (uint8_t i = 0; i < LCD2DinoGameState::kMaxClouds; ++i) {
        const BitmapAsset& cloud = cloudAsset(state.cloudType[i]);
        const int16_t speed = max<int16_t>(1, state.cloudSpeed[i]);
        state.cloudX[i] = static_cast<int16_t>(state.cloudX[i] - speed);

        if (state.cloudX[i] + cloud.w < 0) {
            state.cloudType[i] = randomCloudType();
            state.cloudSpeed[i] = randomCloudSpeed();
            const BitmapAsset& respawnCloud = cloudAsset(state.cloudType[i]);
            state.cloudX[i] = static_cast<int16_t>(screenW + random(kCloudRespawnMinOffset, kCloudRespawnMaxOffset + 1));
            state.cloudY[i] = static_cast<int16_t>(random(kCloudMinY, maxY + 1));

            if (state.cloudX[i] + respawnCloud.w < screenW) {
                state.cloudX[i] = static_cast<int16_t>(screenW + kCloudRespawnMinOffset);
            }
        }
    }
}

void drawSkyScene(const LCD2DinoGameState& state,
                  TFT_eSprite& spr,
                  int16_t screenW,
                  int16_t ground,
                  uint32_t nowMs) {
    if (state.isNight) {
        uint8_t drawnStars = 0;
        bool starDrawn[LCD2DinoGameState::kStarCount] = {false};

        for (uint8_t i = 0; i < LCD2DinoGameState::kStarCount; ++i) {
            if (!state.starIsActivated[i]) {
                continue;
            }

            const uint32_t blinkMs = static_cast<uint32_t>(state.starBlinkFrequencySec[i]) * 1000U;
            if (blinkMs == 0U) {
                continue;
            }

            const uint32_t elapsedMs = nowMs - state.starActivationStartMs[i];
            const uint32_t blinkPhase = elapsedMs + static_cast<uint32_t>(state.starTwinkle[i]) * 251U;
            const uint32_t phaseBucket = blinkPhase / blinkMs;
            if ((phaseBucket & 0x01U) != 0U) {
                continue;
            }

            uint8_t type = state.starType[i];
            if ((((phaseBucket / 2U) & 0x01U) != 0U) && kStarTypeCount > 1U) {
                type = static_cast<uint8_t>((type + 1U) % kStarTypeCount);
            }

            const BitmapAsset& star = starAsset(type);
            drawInvertedBitmap(spr, state.starX[i], state.starY[i], star.data, star.w, star.h, kSpriteInk);
            starDrawn[i] = true;
            ++drawnStars;
        }

        // Guarantee at least 4 visible stars even when several stars are in their OFF blink phase.
        if (drawnStars < kStarMinActiveCount) {
            for (uint8_t i = 0; i < LCD2DinoGameState::kStarCount && drawnStars < kStarMinActiveCount; ++i) {
                if (!state.starIsActivated[i] || starDrawn[i]) {
                    continue;
                }

                const BitmapAsset& star = starAsset(state.starType[i]);
                drawInvertedBitmap(spr, state.starX[i], state.starY[i], star.data, star.w, star.h, kSpriteInk);
                starDrawn[i] = true;
                ++drawnStars;
            }
        }

        const int16_t moonX = static_cast<int16_t>(screenW - kMoonAsset.w - kSkyObjectRightPad);
        const int16_t moonY = min<int16_t>(kMoonY, static_cast<int16_t>(skyBottomY(ground) - kMoonAsset.h));
        drawInvertedBitmap(spr, moonX, moonY, kMoonAsset.data, kMoonAsset.w, kMoonAsset.h, kSpriteInk);
        return;
    }

    const int16_t sunX = static_cast<int16_t>(screenW - kSunAsset.w - kSunRightPad);
    const int16_t sunY = min<int16_t>(kSunY, static_cast<int16_t>(skyBottomY(ground) - kSunAsset.h));
    // Alternate between sun[] and sun_1[] for a sunburst animation
    const BitmapAsset& sunFrame = ((state.score / kSunAnimPeriod) & 0x01U) ? kSunAltAsset : kSunAsset;
    drawInvertedBitmap(spr, sunX, sunY, sunFrame.data, sunFrame.w, sunFrame.h, kSpriteInk);

    for (uint8_t i = 0; i < LCD2DinoGameState::kMaxClouds; ++i) {
        const BitmapAsset& cloud = cloudAsset(state.cloudType[i]);
        // Clouds use transparent background: draw only cloud pixels, no rectangle fill.
        drawCloudSprite(spr, state.cloudX[i], state.cloudY[i], cloud, state.score + i);
    }
}

void drawGroundEffect(const LCD2DinoGameState& state,
                     TFT_eSprite& spr,
                      int16_t screenW,
                      int16_t ground,
                      uint32_t phase) {
    const int16_t segmentCount = LCD2DinoGameState::kGroundBumpCount;
    const int16_t scroll = static_cast<int16_t>((phase / 2U) % segmentCount);

    for (int16_t x = 0; x < screenW; x += kGroundStepPx) {
        const int16_t idx = static_cast<int16_t>((scroll + (x / kGroundStepPx)) % segmentCount);
        const int16_t nextIdx = static_cast<int16_t>((idx + 1) % segmentCount);
        const int16_t x2 = min<int16_t>(screenW - 1, static_cast<int16_t>(x + kGroundStepPx));
        const int16_t y0 = static_cast<int16_t>(ground - state.groundBump[idx]);
        const int16_t y1 = static_cast<int16_t>(ground - state.groundBump[nextIdx]);
        spr.drawLine(x, y0, x2, y1, kSpriteInk);
    }

    for (int16_t x = 0; x < screenW; x += 2) {
        const uint8_t noise = hash8((phase * 5U) + static_cast<uint32_t>(x * 7) + 19U);
        const int16_t y = static_cast<int16_t>(ground + 2 + (noise & 0x01) + ((noise >> 4) & 0x01));
        if ((noise & 0x0FU) < 3U) {
            spr.drawPixel(x, y, kSpriteInk);
        } else if ((noise & 0x1FU) == 0x1AU && x < (screenW - 3)) {
            spr.drawFastHLine(x, y, static_cast<int16_t>(2 + (noise & 0x01)), kSpriteInk);
        }
    }
}

} // namespace

namespace lcd2_game_dino {

void reset(LCD2DinoGameState& state, uint16_t width, uint16_t height, uint32_t nowMs) {
    state.dinoY = groundY(height);
    state.dinoVY = 0;
    state.onGround = true;
    clearObstacles(state);
    seedGroundProfile(state);
    seedStars(state, width, height, nowMs);
    state.score = 0;
    state.lives = kDinoInitLives;
    state.speed = kDinoInitSpeed;
    state.spawnIntervalMs = kDinoInitSpawnMs;
    state.nextSpawnMs = nowMs + kDinoInitSpawnMs;
    state.nextSkyToggleMs = nowMs + nextSkyToggleDelayMs();
    state.odometer = 0U;
    state.dead = false;
    state.deadStartMs = 0;
    state.jumpRequested = false;
    state.finished = false;
    state.isNight = true;
    seedClouds(state, width, height);
}

void tick(LCD2DinoGameState& state, uint16_t width, uint16_t height, uint32_t nowMs) {
    if (state.finished) {
        return;
    }

    const int16_t screenW = static_cast<int16_t>(width);
    const int16_t ground = groundY(height);

    tickSky(state, width, height, nowMs);
    if (state.isNight) {
        tickStars(state, nowMs);
    } else {
        tickClouds(state, screenW, ground);
    }

    if (state.dead) {
        if ((nowMs - state.deadStartMs) >= kGameDeadPauseMs) {
            if (state.lives == 0) {
                state.finished = true;
                return;
            }

            state.dead = false;
            state.dinoY = ground;
            state.dinoVY = 0;
            state.onGround = true;
            state.jumpRequested = false;
            clearObstacles(state);
            state.nextSpawnMs = nowMs + state.spawnIntervalMs;
        }
        return;
    }

    if (state.jumpRequested && state.onGround) {
        state.dinoVY = kDinoJumpVY;
        state.onGround = false;
    }
    state.jumpRequested = false;

    state.dinoVY = static_cast<int16_t>(state.dinoVY + kDinoGravity);
    state.dinoY = static_cast<int16_t>(state.dinoY + state.dinoVY);
    if (state.dinoY >= ground) {
        state.dinoY = ground;
        state.dinoVY = 0;
        state.onGround = true;
    }

    for (uint8_t i = 0; i < LCD2DinoGameState::kMaxObs; ++i) {
        if (!state.obsActive[i]) {
            continue;
        }

        state.obsX[i] = static_cast<int16_t>(state.obsX[i] - state.speed);
        const BitmapAsset& cactus = cactusAsset(state.obsType[i]);
        if (state.obsX[i] + cactus.w < 0) {
            state.obsActive[i] = false;
        }
    }

    if (static_cast<int32_t>(nowMs - state.nextSpawnMs) >= 0 &&
        !hasActiveObstacle(state)) {
        // Decide cluster size: 1 (50%), 2 (30%), 3 (20%)
        const int16_t roll = static_cast<int16_t>(random(10));
        uint8_t clusterSize = (roll < 5) ? 1 : (roll < 8) ? 2 : 3;
        if (clusterSize > LCD2DinoGameState::kMaxObs) {
            clusterSize = LCD2DinoGameState::kMaxObs;
        }

        int16_t clusterXs[LCD2DinoGameState::kMaxObs];
        uint8_t clusterTypes[LCD2DinoGameState::kMaxObs];

        if (!pickSurvivableCluster(state.speed, screenW, clusterSize, ground, clusterXs, clusterTypes)) {
            // Fallback: one small cactus is always clearable.
            clusterSize = 1;
            clusterXs[0] = static_cast<int16_t>(screenW + random(kSpawnXMinOffset, kSpawnXMaxOffset + 1));
            clusterTypes[0] = 0;
        }

        uint8_t slot = 0;
        for (uint8_t n = 0; n < clusterSize; ++n) {
            while (slot < LCD2DinoGameState::kMaxObs && state.obsActive[slot]) ++slot;
            if (slot >= LCD2DinoGameState::kMaxObs) break;

            const BitmapAsset& cactus = cactusAsset(clusterTypes[n]);
            state.obsActive[slot] = true;
            state.obsX[slot] = clusterXs[n];
            state.obsType[slot] = clusterTypes[n];
            state.obsH[slot] = cactus.h;
            ++slot;
        }
        state.nextSpawnMs = nowMs + state.spawnIntervalMs;
    }

    const int16_t dinoLeft = static_cast<int16_t>(kDinoX + 3);
    const int16_t dinoRight = static_cast<int16_t>(kDinoX + kDinoW - 3);
    const int16_t dinoTop = static_cast<int16_t>(state.dinoY - kDinoH + 4);
    const int16_t dinoBottom = static_cast<int16_t>(state.dinoY - 4);

    for (uint8_t i = 0; i < LCD2DinoGameState::kMaxObs; ++i) {
        if (!state.obsActive[i]) {
            continue;
        }

        const BitmapAsset& cactus = cactusAsset(state.obsType[i]);
        const int16_t obsLeft = static_cast<int16_t>(state.obsX[i] + 1);
        const int16_t obsRight = static_cast<int16_t>(state.obsX[i] + cactus.w - 1);
        const int16_t obsTop = static_cast<int16_t>(ground - state.obsH[i]);

        if (dinoRight > obsLeft && dinoLeft < obsRight &&
            dinoBottom > obsTop && dinoTop < ground) {
            state.dead = true;
            state.deadStartMs = nowMs;
            if (state.lives > 0) {
                state.lives--;
            }
            return;
        }
    }

    state.score++;
    // Odometer-based acceleration: 1 km/h per 100 game-km (faster as speed grows)
    state.odometer += static_cast<uint32_t>(state.speed);
    if (state.odometer >= kSpeedRampOdometerPx) {
        state.odometer -= kSpeedRampOdometerPx;
        if (state.speed < kDinoMaxSpeed) {
            state.speed++;
        }
        if (state.spawnIntervalMs > kDinoMinSpawnMs) {
            state.spawnIntervalMs -= 50U;
        }
    }
}

void render(const LCD2DinoGameState& state, TFT_eSprite& sprite, uint16_t width, uint16_t height) {
    sprite.fillSprite(kSpritePaper);

    const int16_t screenW = static_cast<int16_t>(width);
    const int16_t ground = groundY(height);
    const uint16_t speedKmh = currentSpeedKmh(state);
    const uint32_t now32 = static_cast<uint32_t>(millis());

    char scoreBuf[16];
    snprintf(scoreBuf, sizeof(scoreBuf), "%lu", static_cast<unsigned long>(state.score));
    char speedBuf[20];
    snprintf(speedBuf, sizeof(speedBuf), "%u KM/H", speedKmh);
    sprite.setTextColor(kSpriteInk, kSpritePaper);
    sprite.setTextDatum(TL_DATUM);
    sprite.drawString("JETSON RUNNER", 4, 4, 1);
    sprite.drawString(speedBuf, 4, 18, 2);
    sprite.setTextDatum(TC_DATUM);
    sprite.drawString(scoreBuf, static_cast<int16_t>(screenW / 2), 4, 2);

    drawSkyScene(state, sprite, screenW, ground, now32);

    for (uint8_t i = 0; i < state.lives; ++i) {
        // Shifted left to leave the top-right EXIT button area clear
        const int16_t hx = static_cast<int16_t>(screenW - 66 - i * 14);
        sprite.fillCircle(hx, 10, 5, kSpriteInk);
    }

    drawGroundEffect(state, sprite, screenW, ground, state.score);

    for (uint8_t i = 0; i < LCD2DinoGameState::kMaxObs; ++i) {
        if (state.obsActive[i]) {
            const BitmapAsset& cactus = cactusAsset(state.obsType[i]);
                drawCactusSprite(sprite, cactus.data,
                                 state.obsX[i], static_cast<int16_t>(ground - cactus.h),
                                 cactus.w, cactus.h);
        }
    }

    const bool showDino = !state.dead || ((now32 & 256U) != 0U);
    if (showDino) {
        const bool running = state.onGround && !state.dead;
        const BitmapAsset& dinoFrame = running
            ? kDinoRunFrames[(state.score / 3U) & 1U]
            : kDinoJumpFrame;
        sprite.drawBitmap(kDinoX,
                          static_cast<int16_t>(state.dinoY - dinoFrame.h),
                          dinoFrame.data,
                          dinoFrame.w,
                          dinoFrame.h,
                          kSpriteInk);
    }

    if (state.dead) {
        sprite.setTextDatum(MC_DATUM);
        sprite.drawString("OUCH!", static_cast<int16_t>(screenW / 2), static_cast<int16_t>(ground - 60), 4);
        if (state.lives == 0) {
            sprite.drawString("GAME OVER", static_cast<int16_t>(screenW / 2), static_cast<int16_t>(ground - 100), 2);
        }
    }
}

} // namespace lcd2_game_dino