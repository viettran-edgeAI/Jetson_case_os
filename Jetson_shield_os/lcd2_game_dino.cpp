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
    {cloud_0, 45, 26},
    {cloud_1, 70, 27},
    {cloud_2, 60, 32},
    {cloud_3, 36, 23},
};

constexpr uint8_t kNightCloudTypes[] = {0U, 1U};
constexpr uint8_t kDayCloudTypes[] = {2U, 3U};

constexpr BitmapAsset kBirdDefsFwd[] = {
    {bird_0, 33, 19},
    {bird_1, 33, 24},
    {bird_2, 33, 22},
};

constexpr BitmapAsset kBirdDefsRev[] = {
    {bird_0_rev, 33, 19},
    {bird_1_rev, 33, 24},
    {bird_2_rev, 33, 22},
};

constexpr BitmapAsset kMountainDefs[] = {
    {mountain_0, 100, 40},
    {mountain_1, 150, 52},
    {mountain_2, 100, 30},
    {mountain_3, 150, 48},
    {mountain_4, 100, 45},
    {mountain_5, 150, 55},
};

constexpr BitmapAsset kStarDefs[] = {
    {star_0, 5, 5},
    {star_1, 7, 7},
    {star_2, 9, 9},
    {star_3, 11, 11},
};

constexpr BitmapAsset kCactusDefs[] = {
    {cactus_0, 10, 19},
    {cactus_1, 12, 26},
    {cactus_2, 14, 28},
    {cactus_3, 16, 35},
};

constexpr int16_t kTallestCactusH = 35;

constexpr uint8_t kCloudTypeCount = static_cast<uint8_t>(sizeof(kCloudDefs) / sizeof(kCloudDefs[0]));
constexpr uint8_t kCactusTypeCount = static_cast<uint8_t>(sizeof(kCactusDefs) / sizeof(kCactusDefs[0]));
constexpr uint8_t kStarTypeCount = static_cast<uint8_t>(sizeof(kStarDefs) / sizeof(kStarDefs[0]));
constexpr uint8_t kBirdFrameCount = static_cast<uint8_t>(sizeof(kBirdDefsFwd) / sizeof(kBirdDefsFwd[0]));
constexpr uint8_t kMountainTypeCount = static_cast<uint8_t>(sizeof(kMountainDefs) / sizeof(kMountainDefs[0]));

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
constexpr uint8_t kSpawnClusterSearchTries = 12;
constexpr int16_t kSurvivalSimMaxFrames = 48;
constexpr int16_t kClusterGapMinPx = 12;
constexpr int16_t kClusterGapMaxPx = 26;
constexpr int16_t kCloudMinY = 18;
constexpr int16_t kCloudTopPad = 86;
constexpr int16_t kCloudSmallestH = 23;
constexpr int16_t kCloudLargestH = 32;
constexpr int16_t kCloudSizeLiftScale = 2;
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
constexpr uint8_t kStarPlacementMaxTries = 24;
constexpr int16_t kStarMinGapPx = 10;
constexpr int16_t kStarPlacementInset = 2;
constexpr uint8_t kStarBlinkModeOnOff = 0;
constexpr uint8_t kStarBlinkModeMorph = 1;
constexpr uint32_t kStarBlinkStepMs = 300U;
constexpr int16_t kStarSmallestH = 5;
constexpr int16_t kStarLargestH = 11;
constexpr int16_t kStarSizeLiftScale = 2;
constexpr int16_t kMountainTrackCount = 6;
constexpr int16_t kMountainStepPx = 150;
constexpr uint8_t kMountainVisibleMin = 3;
constexpr uint8_t kMountainVisibleMax = 4;
constexpr uint8_t kMountainScrollDiv = 3;
constexpr int16_t kMountainBackLiftPx = kTallestCactusH + 15;
constexpr int16_t kMountainLaneDepthJitterPx = 6;
constexpr uint32_t kBirdSpawnMinDelayMs = 1700U;
constexpr uint32_t kBirdSpawnMaxDelayMs = 4200U;
constexpr uint32_t kBirdFlapStepMs = 75U;
constexpr uint32_t kBirdGlideBeforeFlapMinMs = 450U;
constexpr uint32_t kBirdGlideBeforeFlapMaxMs = 1800U;
constexpr uint32_t kBirdHitBlinkStepMs = 90U;
constexpr uint8_t kBirdHitBlinkSteps = 6;
constexpr int16_t kBirdAltitudeMarginTop = 12;
constexpr int16_t kBirdAltitudeMarginBottom = 8;
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
constexpr uint8_t kNightCloudTickDiv = 2;
constexpr uint8_t kBirdSpeedPercent = 170;

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
static_assert(bitmapSizeMatches(cloud_0, 45, 26), "cloud_0 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(cloud_1, 70, 27), "cloud_1 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(cloud_2, 60, 32), "cloud_2 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(cloud_3, 36, 23), "cloud_3 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(cactus_0, 10, 19), "cactus_0 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(cactus_1, 12, 26), "cactus_1 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(cactus_2, 14, 28), "cactus_2 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(cactus_3, 16, 35), "cactus_3 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(sun, 50, 50), "sun metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(moon, 30, 29), "moon metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(star_0, 5, 5), "star_0 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(star_1, 7, 7), "star_1 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(sun_1, 50, 50), "sun_1 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(star_2, 9, 9), "star_2 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(star_3, 11, 11), "star_3 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(bird_0, 33, 19), "bird_0 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(bird_1, 33, 24), "bird_1 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(bird_2, 33, 22), "bird_2 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(bird_0_rev, 33, 19), "bird_0_rev metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(bird_1_rev, 33, 24), "bird_1_rev metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(bird_2_rev, 33, 22), "bird_2_rev metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(mountain_0, 100, 40), "mountain_0 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(mountain_1, 150, 52), "mountain_1 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(mountain_2, 100, 30), "mountain_2 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(mountain_3, 150, 48), "mountain_3 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(mountain_4, 100, 45), "mountain_4 metadata must match bitmap payload size");
static_assert(bitmapSizeMatches(mountain_5, 150, 55), "mountain_5 metadata must match bitmap payload size");

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

const BitmapAsset& birdAsset(uint8_t frame, int8_t direction) {
    const uint8_t safeFrame = (frame < kBirdFrameCount) ? frame : 0;
    return (direction < 0) ? kBirdDefsRev[safeFrame] : kBirdDefsFwd[safeFrame];
}

const BitmapAsset& mountainAsset(uint8_t type) {
    const uint8_t safeType = (type < kMountainTypeCount) ? type : 0;
    return kMountainDefs[safeType];
}

bool cloudUsesSolidInvertedInk(uint8_t type) {
    // Only cloud types 2+ should be rendered with a solid inverted fill.
    return type >= 2U;
}

constexpr uint8_t kNightMaxClouds = 2;

uint8_t maxClouds(bool isNight) {
    return isNight ? kNightMaxClouds : LCD2DinoGameState::kMaxClouds;
}

uint8_t randomCactusType() {
    return static_cast<uint8_t>(random(kCactusTypeCount));
}

const uint8_t* cloudTypePool(bool isNight) {
    return isNight ? kNightCloudTypes : kDayCloudTypes;
}

uint8_t cloudTypePoolSize(bool isNight) {
    return isNight
        ? static_cast<uint8_t>(sizeof(kNightCloudTypes) / sizeof(kNightCloudTypes[0]))
        : static_cast<uint8_t>(sizeof(kDayCloudTypes) / sizeof(kDayCloudTypes[0]));
}

uint8_t randomCloudType(bool isNight) {
    const uint8_t poolCount = cloudTypePoolSize(isNight);
    const uint8_t* pool = cloudTypePool(isNight);
    return pool[static_cast<uint8_t>(random(poolCount))];
}

int8_t randomCloudSpeed() {
    return static_cast<int8_t>(random(kCloudMinSpeed, kCloudMaxSpeed + 1));
}

int8_t randomCloudSignedSpeed(int8_t directionSign) {
    const int8_t magnitude = randomCloudSpeed();
    return (directionSign >= 0) ? magnitude : static_cast<int8_t>(-magnitude);
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

int16_t topHalfMaxY(uint16_t screenH) {
    return static_cast<int16_t>((screenH / 2U) - 1U);
}

int16_t cloudSpawnMaxTopY(uint16_t screenH, int16_t ground, int16_t cloudH) {
    const int16_t fromGround = static_cast<int16_t>(cloudMaxY(ground) - cloudH);
    const int16_t fromTopHalf = static_cast<int16_t>(topHalfMaxY(screenH) - cloudH);
    return max<int16_t>(kCloudMinY, min<int16_t>(fromGround, fromTopHalf));
}

int16_t cloudSizeLiftPx(int16_t cloudH) {
    const int16_t clamped = constrain(cloudH, kCloudSmallestH, kCloudLargestH);
    return static_cast<int16_t>((clamped - kCloudSmallestH) * kCloudSizeLiftScale);
}

int16_t cloudSpawnMaxTopYForAsset(uint16_t screenH, int16_t ground, const BitmapAsset& cloud) {
    const int16_t base = cloudSpawnMaxTopY(screenH, ground, cloud.h);
    return max<int16_t>(kCloudMinY, static_cast<int16_t>(base - cloudSizeLiftPx(cloud.h)));
}

int16_t starSpawnMaxTopY(uint16_t screenH, int16_t ground) {
    const int16_t skyBottom = max<int16_t>(kSkyTopY + 16, static_cast<int16_t>(ground - kSkyBottomPad));
    return min<int16_t>(skyBottom, topHalfMaxY(screenH));
}

int16_t starMorphNeighborType(uint8_t baseType, uint8_t blinkMode) {
    if (kStarTypeCount <= 1U) {
        return baseType;
    }

    if (baseType == 0U) {
        return 1;
    }
    if (baseType >= (kStarTypeCount - 1U)) {
        return static_cast<int16_t>(kStarTypeCount - 2U);
    }

    // Keep variety by allowing stars to pulse up or down one size tier.
    return (blinkMode == kStarBlinkModeMorph)
        ? static_cast<int16_t>(baseType + 1U)
        : static_cast<int16_t>(baseType - 1U);
}

void starPlacementBoundsForType(uint8_t type, uint8_t blinkMode, int16_t* outW, int16_t* outH) {
    const BitmapAsset& base = starAsset(type);
    const uint8_t neighborType = static_cast<uint8_t>(starMorphNeighborType(type, blinkMode));
    const BitmapAsset& neighbor = starAsset(neighborType);

    if (outW != nullptr) {
        *outW = max<int16_t>(base.w, neighbor.w);
    }
    if (outH != nullptr) {
        *outH = max<int16_t>(base.h, neighbor.h);
    }
}

int16_t starSizeLiftPx(int16_t starH) {
    const int16_t clamped = constrain(starH, kStarSmallestH, kStarLargestH);
    return static_cast<int16_t>((clamped - kStarSmallestH) * kStarSizeLiftScale);
}

int16_t starSpawnMaxTopYForType(uint16_t screenH, int16_t ground, uint8_t type, uint8_t blinkMode) {
    const int16_t base = starSpawnMaxTopY(screenH, ground);
    int16_t boundW = 0;
    int16_t boundH = 0;
    starPlacementBoundsForType(type, blinkMode, &boundW, &boundH);
    (void)boundW;
    return max<int16_t>(kSkyTopY + boundH, static_cast<int16_t>(base - starSizeLiftPx(boundH)));
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

// Draws a monochrome bitmap with transparent background, using the sprite's current ink color. Returns the number of pixels drawn.
int32_t drawDitheredBitmap(TFT_eSprite& spr,
                           int16_t x,
                           int16_t y,
                           const unsigned char* bitmap,
                           int16_t w,
                           int16_t h,
                           uint32_t phase,
                           bool invertBits = false,
                           bool grayDither = false) {
    (void)phase;
    if (bitmap == nullptr || w <= 0 || h <= 0) {
        return 0;
    }

    const int16_t rowBytes = static_cast<int16_t>((w + 7) / 8);
    int32_t drawnPixels = 0;
    for (int16_t py = 0; py < h; ++py) {
        for (int16_t px = 0; px < w; ++px) {
            const int16_t byteIndex = static_cast<int16_t>(py * rowBytes + (px / 8));
            const uint8_t bits = pgm_read_byte(bitmap + byteIndex);
            const bool bitSet = (bits & static_cast<uint8_t>(0x80U >> (px & 7))) != 0U;
            const bool shouldDraw = invertBits ? !bitSet : bitSet;
            if (!shouldDraw) {
                continue;
            }

            // Static checkerboard keeps gray clouds stable without per-frame flicker.
            if (grayDither && (((x + px) ^ (y + py)) & 0x01) != 0) {
                continue;
            }

            spr.drawPixel(static_cast<int16_t>(x + px), static_cast<int16_t>(y + py), kSpriteInk);
            ++drawnPixels;
        }
    }
    return drawnPixels;
}

// Draws a monochrome bitmap with transparent background, using the sprite's current ink color for unset bits. Returns the number of pixels drawn.
int32_t drawInvertedDitheredBitmap(TFT_eSprite& spr,
                                   int16_t x,
                                   int16_t y,
                                   const unsigned char* bitmap,
                                   int16_t w,
                                   int16_t h,
                                   uint32_t phase) {
    (void)phase;
    if (bitmap == nullptr || w <= 0 || h <= 0) {
        return 0;
    }

    const int16_t rowBytes = static_cast<int16_t>((w + 7) / 8);
    int32_t drawnPixels = 0;
    for (int16_t py = 0; py < h; ++py) {
        for (int16_t px = 0; px < w; ++px) {
            const int16_t byteIndex = static_cast<int16_t>(py * rowBytes + (px / 8));
            const uint8_t bits = pgm_read_byte(bitmap + byteIndex);
            const bool bitSet = (bits & static_cast<uint8_t>(0x80U >> (px & 7))) != 0U;
            if (bitSet) {
                continue;
            }

            spr.drawPixel(static_cast<int16_t>(x + px), static_cast<int16_t>(y + py), kSpriteInk);
            ++drawnPixels;
        }
    }
    return drawnPixels;
}

void drawInvertedBitmap(TFT_eSprite& spr,
                        int16_t x,
                        int16_t y,
                        const unsigned char* bitmap,
                        int16_t w,
                        int16_t h,
                        uint8_t color);

void drawFallbackCloud(TFT_eSprite& spr, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t phase) {
    (void)phase;
    const int16_t r = max<int16_t>(2, h / 3);
    spr.fillCircle(static_cast<int16_t>(x + w / 4), static_cast<int16_t>(y + h / 2), r, kSpriteInk);
    spr.fillCircle(static_cast<int16_t>(x + w / 2), static_cast<int16_t>(y + h / 3), static_cast<int16_t>(r + 1), kSpriteInk);
    spr.fillCircle(static_cast<int16_t>(x + (w * 3) / 4), static_cast<int16_t>(y + h / 2), r, kSpriteInk);
    spr.fillRect(static_cast<int16_t>(x + 2), static_cast<int16_t>(y + h / 2),
                 static_cast<int16_t>(max<int16_t>(2, w - 4)),
                 static_cast<int16_t>(max<int16_t>(1, h / 3)),
                 kSpriteInk);
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

void drawMountainSprite(TFT_eSprite& spr,
                        int16_t x,
                        int16_t y,
                        const BitmapAsset& mountain,
                        uint32_t phase) {
    drawDitheredBitmap(spr,
                       x,
                       y,
                       mountain.data,
                       mountain.w,
                       mountain.h,
                       phase,
                       true,
                       true);
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
                    uint8_t type,
                    uint8_t blinkMode) {
    int16_t starW = 0;
    int16_t starH = 0;
    starPlacementBoundsForType(type, blinkMode, &starW, &starH);
    if (x < kStarPlacementInset || y < minY) {
        return false;
    }

    const int16_t maxX = static_cast<int16_t>(screenW - starW - kStarPlacementInset);
    if (x > maxX || y > maxY) {
        return false;
    }

    if (overlapsMoonOrHudTiles(x, y, starW, starH, screenW, ground)) {
        return false;
    }

    for (uint8_t i = 0; i < placedCount; ++i) {
        int16_t otherW = 0;
        int16_t otherH = 0;
        starPlacementBoundsForType(state.starType[i], state.starBlinkMode[i], &otherW, &otherH);
        if (overlapsWithPad(x,
                            y,
                            starW,
                            starH,
                            state.starX[i],
                            state.starY[i],
                            otherW,
                            otherH,
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
                       uint8_t blinkMode,
                       int16_t* outX,
                       int16_t* outY) {
    if (outX == nullptr || outY == nullptr) {
        return false;
    }

    int16_t starW = 0;
    int16_t starH = 0;
    starPlacementBoundsForType(type, blinkMode, &starW, &starH);
    const int16_t minX = kStarPlacementInset;
    const int16_t maxX = static_cast<int16_t>(screenW - starW - kStarPlacementInset);
    const int16_t localMaxY = static_cast<int16_t>(maxY - starH);
    if (maxX < minX || localMaxY < minY) {
        return false;
    }

    for (uint8_t tries = 0; tries < kStarPlacementMaxTries; ++tries) {
        const int16_t candX = static_cast<int16_t>(random(minX, maxX + 1));
        const int16_t candY = static_cast<int16_t>(random(minY, localMaxY + 1));
        if (canPlaceStarAt(state,
                           placedCount,
                           candX,
                           candY,
                           screenW,
                           ground,
                           minY,
                           localMaxY,
                           type,
                           blinkMode)) {
            *outX = candX;
            *outY = candY;
            return true;
        }
    }

    int16_t scanBudget = 200;
    for (int16_t candY = minY; candY <= localMaxY && scanBudget > 0; candY += 2) {
        for (int16_t candX = minX; candX <= maxX && scanBudget > 0; candX += 2) {
            --scanBudget;
            if (canPlaceStarAt(state,
                               placedCount,
                               candX,
                               candY,
                               screenW,
                               ground,
                               minY,
                               localMaxY,
                               type,
                               blinkMode)) {
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
    const int16_t minY = kSkyTopY;

    for (uint8_t i = 0; i < LCD2DinoGameState::kStarCount; ++i) {
        state.starType[i] = randomStarType();
        state.starTwinkle[i] = static_cast<uint8_t>(random(24));
        state.starBlinkMode[i] = static_cast<uint8_t>((random(100) < 40) ? kStarBlinkModeMorph : kStarBlinkModeOnOff);
        state.starLifetimeSec[i] = randomStarLifetimeSec();
        state.starBlinkFrequencySec[i] = randomStarBlinkFrequencySec();
        state.starActivationStartMs[i] = nowMs;
        state.starIsActivated[i] = false;

        const int16_t maxY = starSpawnMaxTopYForType(height, ground, state.starType[i], state.starBlinkMode[i]);

        int16_t starX = kStarPlacementInset;
        int16_t starY = minY;
        const bool hasPlacement = findStarPlacement(state,
                                                    i,
                                                    screenW,
                                                    ground,
                                                    minY,
                                                    maxY,
                                                    state.starType[i],
                                                    state.starBlinkMode[i],
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

uint8_t pickBalancedCloudType(const LCD2DinoGameState& state,
                               uint8_t replaceIndex,
                               bool isNight,
                               uint8_t cloudCount) {
    uint8_t counts[kCloudTypeCount] = {0};
    for (uint8_t i = 0; i < cloudCount; ++i) {
        if (i == replaceIndex) {
            continue;
        }
        const uint8_t type = (state.cloudType[i] < kCloudTypeCount) ? state.cloudType[i] : 0;
        if ((isNight && type > 1U) || (!isNight && type < 2U)) {
            continue;
        }
        ++counts[type];
    }

    uint8_t minCount = 0xFFU;
    const uint8_t* pool = cloudTypePool(isNight);
    const uint8_t poolCount = cloudTypePoolSize(isNight);
    for (uint8_t i = 0; i < poolCount; ++i) {
        minCount = min<uint8_t>(minCount, counts[pool[i]]);
    }

    uint8_t candidates[kCloudTypeCount];
    uint8_t candidateCount = 0;
    for (uint8_t i = 0; i < poolCount; ++i) {
        const uint8_t type = pool[i];
        if (counts[type] == minCount) {
            candidates[candidateCount++] = type;
        }
    }

    if (candidateCount == 0) {
        return randomCloudType(isNight);
    }
    return candidates[static_cast<uint8_t>(random(candidateCount))];
}

int8_t pickCloudDirection(const LCD2DinoGameState& state,
                            uint8_t replaceIndex,
                            uint8_t cloudCount) {
    uint8_t leftCount = 0;
    uint8_t rightCount = 0;
    for (uint8_t i = 0; i < cloudCount; ++i) {
        if (i == replaceIndex) {
            continue;
        }
        if (state.cloudSpeed[i] < 0) {
            ++leftCount;
        } else {
            ++rightCount;
        }
    }

    if (leftCount == 0) {
        return -1;
    }
    if (rightCount == 0) {
        return 1;
    }
    return (random(2) == 0) ? -1 : 1;
}

void seedClouds(LCD2DinoGameState& state, uint16_t width, uint16_t height, bool isNight) {
    const int16_t screenW = static_cast<int16_t>(width);
    const int16_t ground = groundY(height);
    const uint8_t* pool = cloudTypePool(isNight);
    const uint8_t poolCount = cloudTypePoolSize(isNight);
    const uint8_t typeOffset = static_cast<uint8_t>(random(poolCount));
    const uint8_t cloudCount = maxClouds(isNight);
    const int16_t span = max<int16_t>(1, screenW / cloudCount);

    for (uint8_t i = 0; i < cloudCount; ++i) {
        state.cloudType[i] = pool[(typeOffset + i) % poolCount];
        const BitmapAsset& cloud = cloudAsset(state.cloudType[i]);
        const int16_t maxY = cloudSpawnMaxTopYForAsset(height, ground, cloud);
        state.cloudY[i] = (maxY > kCloudMinY)
            ? static_cast<int16_t>(random(kCloudMinY, maxY + 1))
            : kCloudMinY;

        const int8_t dir = (i & 0x01U) == 0U ? -1 : 1;
        state.cloudSpeed[i] = randomCloudSignedSpeed(dir);
        state.cloudX[i] = static_cast<int16_t>(i * span + random(span));
    }

    // Keep inactive cloud slots stable and off-screen so they don't affect logic
    for (uint8_t i = cloudCount; i < LCD2DinoGameState::kMaxClouds; ++i) {
        state.cloudType[i] = 0;
        state.cloudSpeed[i] = 0;
        state.cloudX[i] = -1000;
        state.cloudY[i] = 0;
    }
}

uint32_t nextBirdSpawnDelayMs() {
    return static_cast<uint32_t>(random(kBirdSpawnMinDelayMs, kBirdSpawnMaxDelayMs + 1U));
}

uint8_t activeBirdCount(const LCD2DinoGameState& state) {
    uint8_t active = 0;
    for (uint8_t i = 0; i < LCD2DinoGameState::kMaxBirds; ++i) {
        if (state.birdActive[i]) {
            ++active;
        }
    }
    return active;
}

int16_t birdAltitudeMinY() {
    const int16_t tallestHalf = static_cast<int16_t>(max<int16_t>(kBirdDefsFwd[1].h, kBirdDefsRev[1].h) / 2);
    return static_cast<int16_t>(kBirdAltitudeMarginTop + tallestHalf);
}

int16_t birdAltitudeMaxY(uint16_t height, int16_t ground) {
    const int16_t tallestHalf = static_cast<int16_t>(max<int16_t>(kBirdDefsFwd[1].h, kBirdDefsRev[1].h) / 2);
    const int16_t dinoHead = static_cast<int16_t>(ground - kDinoH);
    const int16_t topHalfLimit = static_cast<int16_t>(topHalfMaxY(height) - tallestHalf - kBirdAltitudeMarginBottom);
    const int16_t dinoHeadLimit = static_cast<int16_t>(dinoHead - 8);
    return min<int16_t>(topHalfLimit, dinoHeadLimit);
}

int16_t birdTopY(const LCD2DinoGameState& state, uint8_t birdIndex) {
    const BitmapAsset& frame = birdAsset(state.birdFrame[birdIndex], state.birdSpeed[birdIndex]);
    return static_cast<int16_t>(state.birdAltitude[birdIndex] - (frame.h / 2));
}

int16_t birdStepSpeed(int8_t baseSpeed) {
    const int16_t sign = (baseSpeed < 0) ? -1 : 1;
    const int16_t magnitude = abs(static_cast<int16_t>(baseSpeed));
    const int16_t scaled = static_cast<int16_t>((magnitude * kBirdSpeedPercent + 50) / 100);
    return static_cast<int16_t>(sign * max<int16_t>(1, scaled));
}

void clearBird(LCD2DinoGameState& state, uint8_t birdIndex) {
    state.birdActive[birdIndex] = false;
    state.birdBlinking[birdIndex] = false;
    state.birdBlinkTicksLeft[birdIndex] = 0;
    state.birdFrame[birdIndex] = 0;
    state.birdFlapStep[birdIndex] = 0;
    state.birdFlapBurstsLeft[birdIndex] = 0;
}

void resetBirds(LCD2DinoGameState& state, uint16_t width, uint16_t height, uint32_t nowMs) {
    (void)width;
    (void)height;
    for (uint8_t i = 0; i < LCD2DinoGameState::kMaxBirds; ++i) {
        state.birdX[i] = 0;
        state.birdAltitude[i] = 0;
        state.birdSpeed[i] = 0;
        state.birdNextAnimMs[i] = nowMs;
        state.birdBlinkNextMs[i] = nowMs;
        clearBird(state, i);
    }
    state.nextBirdSpawnMs = nowMs + nextBirdSpawnDelayMs();
}

void spawnBirdAtSlot(LCD2DinoGameState& state,
                     uint8_t birdIndex,
                     int16_t screenW,
                     uint16_t height,
                     int16_t ground,
                     uint32_t nowMs) {
    const int16_t minAltitude = birdAltitudeMinY();
    const int16_t maxAltitude = birdAltitudeMaxY(height, ground);
    if (maxAltitude <= minAltitude) {
        clearBird(state, birdIndex);
        return;
    }

    const int8_t direction = (random(2) == 0) ? -1 : 1;
    const int8_t speedMag = static_cast<int8_t>(random(2, 4));
    state.birdSpeed[birdIndex] = static_cast<int8_t>(direction * speedMag);
    state.birdAltitude[birdIndex] = static_cast<int16_t>(random(minAltitude, maxAltitude + 1));
    state.birdFrame[birdIndex] = 0;
    state.birdFlapStep[birdIndex] = 0;
    state.birdFlapBurstsLeft[birdIndex] = 2;
    state.birdBlinking[birdIndex] = false;
    state.birdBlinkTicksLeft[birdIndex] = 0;
    state.birdBlinkNextMs[birdIndex] = nowMs;
    state.birdNextAnimMs[birdIndex] = nowMs +
        static_cast<uint32_t>(random(kBirdGlideBeforeFlapMinMs, kBirdGlideBeforeFlapMaxMs + 1U));

    const BitmapAsset& frame = birdAsset(0, state.birdSpeed[birdIndex]);
    if (state.birdSpeed[birdIndex] < 0) {
        state.birdX[birdIndex] = static_cast<int16_t>(screenW + random(kCloudRespawnMinOffset, kCloudRespawnMaxOffset + 1));
    } else {
        state.birdX[birdIndex] = static_cast<int16_t>(-frame.w - random(kCloudRespawnMinOffset, kCloudRespawnMaxOffset + 1));
    }
    state.birdActive[birdIndex] = true;
}

void maybeSpawnBird(LCD2DinoGameState& state,
                    uint16_t width,
                    uint16_t height,
                    int16_t ground,
                    uint32_t nowMs) {
    if (activeBirdCount(state) >= LCD2DinoGameState::kMaxBirds ||
        static_cast<int32_t>(nowMs - state.nextBirdSpawnMs) < 0) {
        return;
    }

    uint8_t slot = LCD2DinoGameState::kMaxBirds;
    for (uint8_t i = 0; i < LCD2DinoGameState::kMaxBirds; ++i) {
        if (!state.birdActive[i]) {
            slot = i;
            break;
        }
    }

    if (slot < LCD2DinoGameState::kMaxBirds) {
        spawnBirdAtSlot(state, slot, static_cast<int16_t>(width), height, ground, nowMs);
    }
    state.nextBirdSpawnMs = nowMs + nextBirdSpawnDelayMs();
}

void tickBirds(LCD2DinoGameState& state, uint16_t width, uint16_t height, int16_t ground, uint32_t nowMs) {
    maybeSpawnBird(state, width, height, ground, nowMs);

    static const uint8_t flapSeq[6] = {1, 2, 0, 1, 2, 0};
    for (uint8_t i = 0; i < LCD2DinoGameState::kMaxBirds; ++i) {
        if (!state.birdActive[i]) {
            continue;
        }

        state.birdX[i] = static_cast<int16_t>(state.birdX[i] + birdStepSpeed(state.birdSpeed[i]));
        const BitmapAsset& frame = birdAsset(state.birdFrame[i], state.birdSpeed[i]);
        if ((state.birdSpeed[i] < 0 && state.birdX[i] + frame.w < 0) ||
            (state.birdSpeed[i] > 0 && state.birdX[i] > static_cast<int16_t>(width))) {
            clearBird(state, i);
            continue;
        }

        if (state.birdBlinking[i]) {
            if (static_cast<int32_t>(nowMs - state.birdBlinkNextMs[i]) >= 0) {
                state.birdBlinkNextMs[i] = nowMs + kBirdHitBlinkStepMs;
                if (state.birdBlinkTicksLeft[i] > 0) {
                    --state.birdBlinkTicksLeft[i];
                }
                if (state.birdBlinkTicksLeft[i] == 0) {
                    clearBird(state, i);
                }
            }
            continue;
        }

        if (state.birdFlapStep[i] == 0) {
            state.birdFrame[i] = 0;
            if (state.birdFlapBurstsLeft[i] > 0 && static_cast<int32_t>(nowMs - state.birdNextAnimMs[i]) >= 0) {
                state.birdFlapStep[i] = 1;
                state.birdFrame[i] = flapSeq[0];
                state.birdNextAnimMs[i] = nowMs + kBirdFlapStepMs;
            }
            continue;
        }

        if (static_cast<int32_t>(nowMs - state.birdNextAnimMs[i]) >= 0) {
            ++state.birdFlapStep[i];
            if (state.birdFlapStep[i] <= 6) {
                state.birdFrame[i] = flapSeq[state.birdFlapStep[i] - 1U];
                state.birdNextAnimMs[i] = nowMs + kBirdFlapStepMs;
            } else {
                state.birdFlapStep[i] = 0;
                state.birdFrame[i] = 0;
                if (state.birdFlapBurstsLeft[i] > 0) {
                    --state.birdFlapBurstsLeft[i];
                }
                state.birdNextAnimMs[i] = nowMs +
                    static_cast<uint32_t>(random(kBirdGlideBeforeFlapMinMs, kBirdGlideBeforeFlapMaxMs + 1U));
            }
        }
    }
}

void tickSky(LCD2DinoGameState& state, uint16_t width, uint16_t height, uint32_t nowMs) {
    if (static_cast<int32_t>(nowMs - state.nextSkyToggleMs) < 0) {
        return;
    }

    state.isNight = !state.isNight;
    state.nextSkyToggleMs = nowMs + nextSkyToggleDelayMs();
    seedClouds(state, width, height, state.isNight);
    if (state.isNight) {
        seedStars(state, width, height, nowMs);
        resetBirds(state, width, height, nowMs);
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

void tickClouds(LCD2DinoGameState& state, int16_t screenW, uint16_t screenH, int16_t ground, bool isNight) {
    const uint8_t cloudCount = maxClouds(isNight);
    for (uint8_t i = 0; i < cloudCount; ++i) {
        const BitmapAsset& cloud = cloudAsset(state.cloudType[i]);
        state.cloudX[i] = static_cast<int16_t>(state.cloudX[i] + state.cloudSpeed[i]);

        const bool offLeft = (state.cloudSpeed[i] < 0) && (state.cloudX[i] + cloud.w < 0);
        const bool offRight = (state.cloudSpeed[i] > 0) && (state.cloudX[i] > screenW);
        if (offLeft || offRight) {
            state.cloudType[i] = pickBalancedCloudType(state, i, isNight, cloudCount);
            const int8_t direction = pickCloudDirection(state, i, cloudCount);
            state.cloudSpeed[i] = randomCloudSignedSpeed(direction);

            const BitmapAsset& respawnCloud = cloudAsset(state.cloudType[i]);
            const int16_t maxY = cloudSpawnMaxTopYForAsset(screenH, ground, respawnCloud);
            state.cloudY[i] = (maxY > kCloudMinY)
                ? static_cast<int16_t>(random(kCloudMinY, maxY + 1))
                : kCloudMinY;

            const int16_t offset = static_cast<int16_t>(random(kCloudRespawnMinOffset, kCloudRespawnMaxOffset + 1));
            if (state.cloudSpeed[i] < 0) {
                state.cloudX[i] = static_cast<int16_t>(screenW + offset);
            } else {
                state.cloudX[i] = static_cast<int16_t>(-respawnCloud.w - offset);
            }
        }
    }
}

void drawDayMountains(TFT_eSprite& spr, int16_t screenW, int16_t ground, uint32_t phase) {
    const int32_t scrollPx = static_cast<int32_t>(phase / kMountainScrollDiv);
    const int16_t step = kMountainStepPx;
    const int16_t offset = static_cast<int16_t>(scrollPx % step);
    const uint8_t cycle = static_cast<uint8_t>((scrollPx / step) % kMountainTypeCount);

    uint8_t visibleCount = 0;
    for (int16_t lane = 0; lane < kMountainTrackCount; ++lane) {
        const int16_t x = static_cast<int16_t>(lane * step - offset);
        const uint8_t type = static_cast<uint8_t>((cycle + lane) % kMountainTypeCount);
        const BitmapAsset& mountain = mountainAsset(type);
        if (x + mountain.w <= 0 || x >= screenW) {
            continue;
        }
        ++visibleCount;
    }

    // Keep only 3-4 mountains visible; the rest stay in the off-screen ring buffer.
    const uint8_t targetVisible = (visibleCount <= kMountainVisibleMin) ? kMountainVisibleMin : kMountainVisibleMax;
    uint8_t drawnCount = 0;

    for (int16_t lane = 0; lane < kMountainTrackCount; ++lane) {
        if (drawnCount >= targetVisible) {
            break;
        }

        const int16_t x = static_cast<int16_t>(lane * step - offset);
        const uint8_t type = static_cast<uint8_t>((cycle + lane) % kMountainTypeCount);
        const BitmapAsset& mountain = mountainAsset(type);
        if (x + mountain.w <= 0 || x >= screenW) {
            continue;
        }

        const int16_t laneJitter = static_cast<int16_t>((lane * 3) % (kMountainLaneDepthJitterPx + 1));
        const int16_t y = static_cast<int16_t>(ground - kMountainBackLiftPx - laneJitter - mountain.h);
        drawMountainSprite(spr, x, y, mountain, phase + static_cast<uint32_t>(lane * 11));
        ++drawnCount;
    }
}

void drawCloudLayer(const LCD2DinoGameState& state, TFT_eSprite& spr, uint32_t phase) {
    const uint8_t cloudCount = maxClouds(state.isNight);
    for (uint8_t i = 0; i < cloudCount; ++i) {
        const uint8_t type = state.cloudType[i];
        const BitmapAsset& cloud = cloudAsset(type);

        // cloud_0 and cloud_1 are stored inverted (0=ink).
        if (type == 0U || type == 1U) {
            drawDitheredBitmap(spr,
                              state.cloudX[i],
                              state.cloudY[i],
                              cloud.data,
                              cloud.w,
                              cloud.h,
                              phase + i,
                              true,
                              true);
        } else if (cloudUsesSolidInvertedInk(type)) {
            drawInvertedBitmap(spr, state.cloudX[i], state.cloudY[i], cloud.data, cloud.w, cloud.h, kSpriteInk);
        } else {
            drawCloudSprite(spr, state.cloudX[i], state.cloudY[i], cloud, phase + i);
        }
    }
}

void drawBirds(const LCD2DinoGameState& state, TFT_eSprite& spr) {
    for (uint8_t i = 0; i < LCD2DinoGameState::kMaxBirds; ++i) {
        if (!state.birdActive[i]) {
            continue;
        }
        if (state.birdBlinking[i] && ((state.birdBlinkTicksLeft[i] & 0x01U) != 0U)) {
            continue;
        }

        const BitmapAsset& bird = birdAsset(state.birdFrame[i], state.birdSpeed[i]);
        drawInvertedBitmap(spr,
                           state.birdX[i],
                           birdTopY(state, i),
                           bird.data,
                           bird.w,
                           bird.h,
                           kSpriteInk);
    }
}

void drawSkyScene(const LCD2DinoGameState& state,
                  TFT_eSprite& spr,
                  int16_t screenW,
                  int16_t ground,
                  uint32_t nowMs) {
    if (state.isNight) {
        for (uint8_t i = 0; i < LCD2DinoGameState::kStarCount; ++i) {
            if (!state.starIsActivated[i]) {
                continue;
            }

            const uint32_t twinkleOffset = static_cast<uint32_t>(state.starTwinkle[i]) * 31U;
            const uint32_t elapsedMs = nowMs - state.starActivationStartMs[i] + twinkleOffset;
            const uint8_t triPhase = static_cast<uint8_t>((elapsedMs / kStarBlinkStepMs) % 3U);
            const uint8_t baseType = (state.starType[i] < kStarTypeCount) ? state.starType[i] : 0U;
            const uint8_t neighborType = static_cast<uint8_t>(starMorphNeighborType(baseType, state.starBlinkMode[i]));
            const uint8_t drawType = (triPhase == 1U) ? neighborType : baseType;
            const BitmapAsset& star = starAsset(drawType);
            drawInvertedBitmap(spr, state.starX[i], state.starY[i], star.data, star.w, star.h, kSpriteInk);
        }

        const int16_t moonX = static_cast<int16_t>(screenW - kMoonAsset.w - kSkyObjectRightPad);
        const int16_t moonY = min<int16_t>(kMoonY, static_cast<int16_t>(skyBottomY(ground) - kMoonAsset.h));
        drawInvertedBitmap(spr, moonX, moonY, kMoonAsset.data, kMoonAsset.w, kMoonAsset.h, kSpriteInk);
        drawCloudLayer(state, spr, nowMs);
        return;
    }

    drawDayMountains(spr, screenW, ground, state.score);

    const int16_t sunX = static_cast<int16_t>(screenW - kSunAsset.w - kSunRightPad);
    const int16_t sunY = min<int16_t>(kSunY, static_cast<int16_t>(skyBottomY(ground) - kSunAsset.h));
    // Alternate between sun[] and sun_1[] for a sunburst animation
    const BitmapAsset& sunFrame = ((state.score / kSunAnimPeriod) & 0x01U) ? kSunAltAsset : kSunAsset;
    // Sun is dithered for a gray appearance during daytime
    drawDitheredBitmap(spr, sunX, sunY, sunFrame.data, sunFrame.w, sunFrame.h, state.score, true, true);

    drawCloudLayer(state, spr, state.score);

    drawBirds(state, spr);
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
    seedClouds(state, width, height, state.isNight);
    resetBirds(state, width, height, nowMs);
}

void tick(LCD2DinoGameState& state, uint16_t width, uint16_t height, uint32_t nowMs) {
    if (state.finished) {
        return;
    }

    const int16_t screenW = static_cast<int16_t>(width);
    const int16_t ground = groundY(height);

    tickSky(state, width, height, nowMs);
    if (!state.isNight || ((state.score % kNightCloudTickDiv) == 0U)) {
        tickClouds(state, screenW, height, ground, state.isNight);
    }
    if (state.isNight) {
        tickStars(state, nowMs);
    } else {
        tickBirds(state, width, height, ground, nowMs);
    }

    if (state.dead) {
        state.jumpRequested = false;
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

    if (!state.onGround) {
        for (uint8_t i = 0; i < LCD2DinoGameState::kMaxBirds; ++i) {
            if (!state.birdActive[i] || state.birdBlinking[i]) {
                continue;
            }

            const BitmapAsset& bird = birdAsset(state.birdFrame[i], state.birdSpeed[i]);
            const int16_t birdLeft = static_cast<int16_t>(state.birdX[i] + 4);
            const int16_t birdRight = static_cast<int16_t>(state.birdX[i] + bird.w - 4);
            const int16_t birdTop = static_cast<int16_t>(birdTopY(state, i) + 2);
            const int16_t birdBottom = static_cast<int16_t>(birdTop + bird.h - 4);

            if (dinoRight > birdLeft && dinoLeft < birdRight &&
                dinoBottom > birdTop && dinoTop < birdBottom) {
                state.birdBlinking[i] = true;
                state.birdBlinkTicksLeft[i] = kBirdHitBlinkSteps;
                state.birdBlinkNextMs[i] = nowMs + kBirdHitBlinkStepMs;
                state.birdFrame[i] = 0;
                state.birdFlapStep[i] = 0;
                state.birdFlapBurstsLeft[i] = 0;
                state.score += 18;
            }
        }
    }

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