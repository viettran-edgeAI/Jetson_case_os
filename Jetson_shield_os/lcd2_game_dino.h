#ifndef LCD2_GAME_DINO_H
#define LCD2_GAME_DINO_H

#include <Arduino.h>
#include <TFT_eSPI.h>

struct LCD2DinoGameState {
    static constexpr uint8_t kMaxObs = 3;
    static constexpr uint8_t kMaxClouds = 3;
    static constexpr uint8_t kMaxBirds = 2;
    static constexpr uint8_t kGroundBumpCount = 32;
    static constexpr uint8_t kStarCount = 15;

    int16_t dinoY;
    int16_t dinoVY;
    bool onGround;
    int16_t obsX[kMaxObs];
    int16_t obsH[kMaxObs];
    uint8_t obsType[kMaxObs];
    bool obsActive[kMaxObs];
    int16_t cloudX[kMaxClouds];
    int16_t cloudY[kMaxClouds];
    int8_t cloudSpeed[kMaxClouds];
    uint8_t cloudType[kMaxClouds];
    int8_t groundBump[kGroundBumpCount];
    int16_t starX[kStarCount];
    int16_t starY[kStarCount];
    uint8_t starType[kStarCount];
    uint8_t starTwinkle[kStarCount];
    bool starIsActivated[kStarCount];
    uint8_t starBlinkMode[kStarCount];
    uint8_t starLifetimeSec[kStarCount];
    uint8_t starBlinkFrequencySec[kStarCount];
    uint32_t starActivationStartMs[kStarCount];
    int16_t birdX[kMaxBirds];
    int16_t birdAltitude[kMaxBirds];
    int8_t birdSpeed[kMaxBirds];
    uint8_t birdFrame[kMaxBirds];
    uint8_t birdFlapStep[kMaxBirds];
    uint8_t birdFlapBurstsLeft[kMaxBirds];
    bool birdActive[kMaxBirds];
    bool birdBlinking[kMaxBirds];
    uint8_t birdBlinkTicksLeft[kMaxBirds];
    uint32_t birdNextAnimMs[kMaxBirds];
    uint32_t birdBlinkNextMs[kMaxBirds];
    uint32_t nextBirdSpawnMs;
    uint32_t score;
    uint8_t lives;
    int16_t speed;
    uint32_t nextSpawnMs;
    uint32_t spawnIntervalMs;
    uint32_t nextSkyToggleMs;
    uint32_t odometer;
    bool dead;
    uint32_t deadStartMs;
    bool jumpRequested;
    bool finished;
    bool isNight;
};

namespace lcd2_game_dino {

void reset(LCD2DinoGameState& state, uint16_t width, uint16_t height, uint32_t nowMs);
void tick(LCD2DinoGameState& state, uint16_t width, uint16_t height, uint32_t nowMs);
void render(const LCD2DinoGameState& state, TFT_eSprite& sprite, uint16_t width, uint16_t height);

} // namespace lcd2_game_dino

#endif // LCD2_GAME_DINO_H