#ifndef LCD2_GAME_BALL_H
#define LCD2_GAME_BALL_H

#include <Arduino.h>
#include <TFT_eSPI.h>

struct LCD2BallGameState {
    static constexpr uint8_t kBrickCols = 8;
    static constexpr uint8_t kBrickRows = 5;

    float bx;
    float by;
    float bvx;
    float bvy;
    int16_t paddleX;
    bool bricks[kBrickRows][kBrickCols];
    uint8_t bricksLeft;
    uint32_t score;
    uint16_t level;
    uint8_t lives;
    uint8_t speedPercent;
    bool dead;
    uint32_t deadStartMs;
    bool finished;
};

namespace lcd2_game_ball {

void reset(LCD2BallGameState& state, uint16_t width, uint16_t height);
void tick(LCD2BallGameState& state,
          uint16_t width,
          uint16_t height,
          uint32_t nowMs,
          bool touchActive,
          int16_t touchX,
          int16_t touchY);
void render(const LCD2BallGameState& state, TFT_eSprite& sprite, uint16_t width, uint16_t height);

} // namespace lcd2_game_ball

#endif // LCD2_GAME_BALL_H