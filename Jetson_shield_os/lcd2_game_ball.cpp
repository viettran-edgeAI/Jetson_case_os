#include "lcd2_game_ball.h"

#include <math.h>

namespace {

constexpr uint8_t kSpriteInk = 1;
constexpr uint8_t kSpritePaper = 0;

constexpr float kBallRadius = 6.0f;
constexpr int16_t kPaddleW = 58;
constexpr int16_t kPaddleH = 10;
constexpr int16_t kPaddleBottomPad = 20;
constexpr float kBallInitSpeed = 10.6f;
constexpr int16_t kBrickW = 26;
constexpr int16_t kBrickH = 12;
constexpr int16_t kBrickGapX = 2;
constexpr int16_t kBrickGapY = 2;
constexpr int16_t kBricksTopMargin = 42;
constexpr uint8_t kBallInitLives = 3;
constexpr uint32_t kGameDeadPauseMs = 900U;
constexpr uint8_t kBallSpeedMinPercent = 70;
constexpr uint8_t kBallSpeedMaxPercent = 220;
constexpr uint8_t kBallSpeedDefaultPercent = 140;
constexpr int16_t kRightPanelW = 36;
constexpr int16_t kHudBarH = 24;
constexpr int16_t kVSliderTopY = 36;
constexpr int16_t kVSliderBotPad = 64;
constexpr int16_t kVSliderTrackW = 14;

int16_t paddleY(uint16_t height) {
    return static_cast<int16_t>(height - kPaddleBottomPad - kPaddleH);
}

int16_t panelLeft(uint16_t width) {
    return static_cast<int16_t>(width - kRightPanelW);
}

int16_t vSliderLeft(uint16_t width) {
    return static_cast<int16_t>(panelLeft(width) + (kRightPanelW - kVSliderTrackW) / 2);
}

int16_t vSliderBotY(uint16_t height) {
    return static_cast<int16_t>(height - kVSliderBotPad);
}

int16_t bricksStartX(uint16_t width) {
    const int16_t playW = panelLeft(width);
    return static_cast<int16_t>((playW - (LCD2BallGameState::kBrickCols * (kBrickW + kBrickGapX) - kBrickGapX)) / 2);
}

float speedScale(uint8_t speedPercent) {
    const int16_t clamped = constrain(static_cast<int16_t>(speedPercent),
                                      static_cast<int16_t>(kBallSpeedMinPercent),
                                      static_cast<int16_t>(kBallSpeedMaxPercent));
    return static_cast<float>(clamped) / 100.0f;
}

float baseBallSpeed(uint8_t speedPercent) {
    return kBallInitSpeed * speedScale(speedPercent);
}

bool touchInSpeedSlider(int16_t touchX, int16_t touchY, uint16_t width, uint16_t height) {
    const int16_t hitPad = 10;
    const int16_t pLeft = panelLeft(width);
    return (touchX >= (pLeft - hitPad)) &&
           (touchX <= static_cast<int16_t>(width)) &&
           (touchY >= (kVSliderTopY - hitPad)) &&
           (touchY <= (vSliderBotY(height) + hitPad));
}

uint8_t speedPercentFromTouchY(int16_t touchY, uint16_t height) {
    const int16_t top = kVSliderTopY;
    const int16_t bot = vSliderBotY(height);
    const int16_t clampedY = constrain(touchY, top, bot);
    const int16_t span = max<int16_t>(1, static_cast<int16_t>(bot - top));
    // top of track = max speed, bottom = min speed
    const int32_t numer = static_cast<int32_t>(bot - clampedY) *
                          static_cast<int32_t>(kBallSpeedMaxPercent - kBallSpeedMinPercent);
    const int16_t mapped = static_cast<int16_t>(kBallSpeedMinPercent + ((numer + (span / 2)) / span));
    return static_cast<uint8_t>(constrain(mapped,
                                          static_cast<int16_t>(kBallSpeedMinPercent),
                                          static_cast<int16_t>(kBallSpeedMaxPercent)));
}

void applySpeedPercent(LCD2BallGameState& state, uint8_t newSpeedPercent) {
    const uint8_t clampedNew = static_cast<uint8_t>(constrain(static_cast<int16_t>(newSpeedPercent),
                                                              static_cast<int16_t>(kBallSpeedMinPercent),
                                                              static_cast<int16_t>(kBallSpeedMaxPercent)));
    if (state.speedPercent == clampedNew) {
        return;
    }

    const float oldSpeed = max(0.01f, baseBallSpeed(state.speedPercent));
    const float newSpeed = max(0.01f, baseBallSpeed(clampedNew));
    const float ratio = newSpeed / oldSpeed;

    state.bvx *= ratio;
    state.bvy *= ratio;
    state.speedPercent = clampedNew;
}

void resetBallPosition(LCD2BallGameState& state, uint16_t width, uint16_t height) {
    const int16_t paddleTop = paddleY(height);
    const int16_t playW = panelLeft(width);
    const float initSpeed = baseBallSpeed(state.speedPercent);
    state.bx = static_cast<float>(playW / 2);
    state.by = static_cast<float>(paddleTop - static_cast<int16_t>(kBallRadius) - 2);
    state.bvx = initSpeed * ((random(2) == 0) ? 0.55f : -0.55f);
    state.bvy = -initSpeed * 0.92f;
}

void resetBricks(LCD2BallGameState& state) {
    for (uint8_t r = 0; r < LCD2BallGameState::kBrickRows; ++r) {
        for (uint8_t c = 0; c < LCD2BallGameState::kBrickCols; ++c) {
            state.bricks[r][c] = false;
        }
    }

    const int16_t rows = LCD2BallGameState::kBrickRows;
    const int16_t cols = LCD2BallGameState::kBrickCols;
    const uint8_t shape = static_cast<uint8_t>(random(5));
    const float center = static_cast<float>(cols - 1) * 0.5f;
    const bool risingSlope = (random(2) == 0);
    const float wavePhase = static_cast<float>(random(0, 628)) / 100.0f;

    uint8_t brickCount = 0;
    for (int16_t c = 0; c < cols; ++c) {
        const float dist = fabsf(static_cast<float>(c) - center);
        int16_t fill = rows;

        switch (shape) {
            case 0: // Pyramid
                fill = static_cast<int16_t>(rows - static_cast<int16_t>(dist * 1.35f));
                break;
            case 1: // Valley
                fill = static_cast<int16_t>(2 + static_cast<int16_t>(dist * 1.15f));
                break;
            case 2: { // Slope
                const int16_t idx = risingSlope ? c : static_cast<int16_t>(cols - 1 - c);
                fill = static_cast<int16_t>(1 + ((idx * (rows - 1)) / max<int16_t>(1, cols - 1)));
                break;
            }
            case 3: { // Wave
                const float wave = (sinf((static_cast<float>(c) * 1.05f) + wavePhase) + 1.0f) * 0.5f;
                fill = static_cast<int16_t>(1 + static_cast<int16_t>(wave * static_cast<float>(rows - 1)));
                break;
            }
            default: { // Plateau with noisy edges
                const float edge = dist / max(1.0f, center);
                fill = static_cast<int16_t>(rows - static_cast<int16_t>(edge * 2.4f));
                if (random(100) < 35) {
                    fill += (random(2) == 0) ? -1 : 1;
                }
                break;
            }
        }

        if (random(100) < 20) {
            fill += (random(2) == 0) ? -1 : 1;
        }
        fill = constrain(fill, 1, rows);

        for (int16_t r = 0; r < fill; ++r) {
            state.bricks[r][c] = true;
            ++brickCount;
        }
    }

    // Add sparse holes for higher levels while keeping the shape recognizable.
    const uint16_t levelBoost = static_cast<uint16_t>(min<uint16_t>(state.level, 10U));
    const uint8_t holeChance = static_cast<uint8_t>(4U + levelBoost * 2U);
    for (int16_t r = 1; r < rows; ++r) {
        for (int16_t c = 0; c < cols; ++c) {
            if (!state.bricks[r][c]) {
                continue;
            }
            if (random(100) >= holeChance) {
                continue;
            }
            if (brickCount <= static_cast<uint8_t>(cols * 2)) {
                continue;
            }
            state.bricks[r][c] = false;
            --brickCount;
        }
    }

    state.bricksLeft = brickCount;
}

void drawBrick(TFT_eSprite& spr, int16_t x, int16_t y, int16_t w, int16_t h) {
    spr.fillRect(x, y, w, h, kSpriteInk);
    spr.drawRect(x, y, w, h, kSpritePaper);
}

bool segmentHitsAabb(float x0,
                     float y0,
                     float x1,
                     float y1,
                     float left,
                     float top,
                     float right,
                     float bottom,
                     float& hitT,
                     float& hitNx,
                     float& hitNy) {
    const float dx = x1 - x0;
    const float dy = y1 - y0;

    float tEnter = 0.0f;
    float tExit = 1.0f;
    float nx = 0.0f;
    float ny = 0.0f;

    if (fabsf(dx) < 0.0001f) {
        if (x0 < left || x0 > right) {
            return false;
        }
    } else {
        float tx1 = (left - x0) / dx;
        float tx2 = (right - x0) / dx;
        float txEnter = tx1;
        float txExit = tx2;
        float axisNx = -1.0f;
        if (tx1 > tx2) {
            txEnter = tx2;
            txExit = tx1;
            axisNx = 1.0f;
        }

        if (txEnter > tEnter) {
            tEnter = txEnter;
            nx = axisNx;
            ny = 0.0f;
        }
        if (txExit < tExit) {
            tExit = txExit;
        }
        if (tEnter > tExit) {
            return false;
        }
    }

    if (fabsf(dy) < 0.0001f) {
        if (y0 < top || y0 > bottom) {
            return false;
        }
    } else {
        float ty1 = (top - y0) / dy;
        float ty2 = (bottom - y0) / dy;
        float tyEnter = ty1;
        float tyExit = ty2;
        float axisNy = -1.0f;
        if (ty1 > ty2) {
            tyEnter = ty2;
            tyExit = ty1;
            axisNy = 1.0f;
        }

        if (tyEnter > tEnter) {
            tEnter = tyEnter;
            nx = 0.0f;
            ny = axisNy;
        }
        if (tyExit < tExit) {
            tExit = tyExit;
        }
        if (tEnter > tExit) {
            return false;
        }
    }

    if (tEnter < 0.0f || tEnter > 1.0f) {
        return false;
    }

    hitT = tEnter;
    hitNx = nx;
    hitNy = ny;
    return true;
}

void collideBallWithPaddleSweep(LCD2BallGameState& state, float prevBx, float prevBy, int16_t paddleTop) {
    if (state.by <= prevBy) {
        return;
    }

    const float pLeft = static_cast<float>(state.paddleX);
    const float pRight = pLeft + static_cast<float>(kPaddleW);
    const float pTop = static_cast<float>(paddleTop);
    const float pBottom = pTop + static_cast<float>(kPaddleH);

    float hitT = 0.0f;
    float hitNx = 0.0f;
    float hitNy = 0.0f;
    const bool hit = segmentHitsAabb(prevBx,
                                     prevBy,
                                     state.bx,
                                     state.by,
                                     pLeft - kBallRadius,
                                     pTop - kBallRadius,
                                     pRight + kBallRadius,
                                     pBottom + kBallRadius,
                                     hitT,
                                     hitNx,
                                     hitNy);
    if (!hit) {
        return;
    }

    const float moveX = state.bx - prevBx;
    const float moveY = state.by - prevBy;
    const float impactX = prevBx + moveX * hitT;
    const float impactY = prevBy + moveY * hitT;
    const float remainT = 1.0f - hitT;

    state.bx = impactX;
    state.by = impactY;

    if (hitNy < -0.5f) {
        state.by = pTop - kBallRadius - 0.5f;
        state.bvy = -fabsf(state.bvy);

        const float hitOffset = (state.bx - (pLeft + static_cast<float>(kPaddleW) * 0.5f)) /
                                (static_cast<float>(kPaddleW) * 0.5f);
        state.bvx += hitOffset * 1.3f;

        const float speed2 = state.bvx * state.bvx + state.bvy * state.bvy;
        const float maxSpeed = baseBallSpeed(state.speedPercent) * 2.0f;
        const float kMaxSpeed2 = maxSpeed * maxSpeed;
        if (speed2 > kMaxSpeed2) {
            const float scale = sqrtf(kMaxSpeed2 / speed2);
            state.bvx *= scale;
            state.bvy *= scale;
        }
    } else if (hitNx > 0.5f) {
        state.bx = pRight + kBallRadius + 0.5f;
        state.bvx = fabsf(state.bvx);
    } else if (hitNx < -0.5f) {
        state.bx = pLeft - kBallRadius - 0.5f;
        state.bvx = -fabsf(state.bvx);
    } else {
        state.by = pBottom + kBallRadius + 0.5f;
        state.bvy = fabsf(state.bvy);
    }

    if (remainT > 0.0f) {
        state.bx += state.bvx * remainT;
        state.by += state.bvy * remainT;
    }
}

} // namespace

namespace lcd2_game_ball {

void reset(LCD2BallGameState& state, uint16_t width, uint16_t height) {
    state.paddleX = static_cast<int16_t>((panelLeft(width) - kPaddleW) / 2);
    state.speedPercent = kBallSpeedDefaultPercent;
    state.level = 1;
    resetBallPosition(state, width, height);
    resetBricks(state);

    state.score = 0;
    state.lives = kBallInitLives;
    state.dead = false;
    state.deadStartMs = 0;
    state.finished = false;
}

void tick(LCD2BallGameState& state,
          uint16_t width,
          uint16_t height,
          uint32_t nowMs,
          bool touchActive,
          int16_t touchX,
          int16_t touchY) {
    if (state.finished) {
        return;
    }

    const int16_t screenH = static_cast<int16_t>(height);
    const int16_t paddleTop = paddleY(height);

    if (touchActive && touchInSpeedSlider(touchX, touchY, width, height)) {
        applySpeedPercent(state, speedPercentFromTouchY(touchY, height));
    }

    if (state.dead) {
        if ((nowMs - state.deadStartMs) >= kGameDeadPauseMs) {
            if (state.lives == 0) {
                state.finished = true;
                return;
            }
            state.dead = false;
            resetBallPosition(state, width, height);
        }
        return;
    }

    if (touchActive && !touchInSpeedSlider(touchX, touchY, width, height)) {
        const int16_t playW = panelLeft(width);
        int16_t wantX = static_cast<int16_t>(touchX - kPaddleW / 2);
        if (wantX < 0) {
            wantX = 0;
        }
        if (wantX > (playW - kPaddleW)) {
            wantX = static_cast<int16_t>(playW - kPaddleW);
        }
        state.paddleX = wantX;
    }

    const float prevBx = state.bx;
    const float prevBy = state.by;
    state.bx += state.bvx;
    state.by += state.bvy;

    if (state.bx - kBallRadius < 0.0f) {
        state.bx = kBallRadius;
        state.bvx = fabsf(state.bvx);
    }
    if (state.bx + kBallRadius > static_cast<float>(panelLeft(width))) {
        state.bx = static_cast<float>(panelLeft(width)) - kBallRadius;
        state.bvx = -fabsf(state.bvx);
    }
    if (state.by - kBallRadius < 30.0f) {
        state.by = 30.0f + kBallRadius;
        state.bvy = fabsf(state.bvy);
    }

    collideBallWithPaddleSweep(state, prevBx, prevBy, paddleTop);

    if (state.by - kBallRadius > static_cast<float>(screenH)) {
        state.dead = true;
        state.deadStartMs = nowMs;
        if (state.lives > 0) {
            state.lives--;
        }
        return;
    }

    const int16_t startX = bricksStartX(width);
    const int16_t ballX = static_cast<int16_t>(state.bx);
    const int16_t ballY = static_cast<int16_t>(state.by);
    const int16_t ballR = static_cast<int16_t>(kBallRadius);

    for (uint8_t r = 0; r < LCD2BallGameState::kBrickRows; ++r) {
        for (uint8_t c = 0; c < LCD2BallGameState::kBrickCols; ++c) {
            if (!state.bricks[r][c]) {
                continue;
            }

            const int16_t left = static_cast<int16_t>(startX + c * (kBrickW + kBrickGapX));
            const int16_t right = static_cast<int16_t>(left + kBrickW);
            const int16_t top = static_cast<int16_t>(kBricksTopMargin + r * (kBrickH + kBrickGapY));
            const int16_t bottom = static_cast<int16_t>(top + kBrickH);

            if (ballX + ballR < left || ballX - ballR > right ||
                ballY + ballR < top || ballY - ballR > bottom) {
                continue;
            }

            const int16_t overlapL = static_cast<int16_t>((ballX + ballR) - left);
            const int16_t overlapR = static_cast<int16_t>(right - (ballX - ballR));
            const int16_t overlapT = static_cast<int16_t>((ballY + ballR) - top);
            const int16_t overlapB = static_cast<int16_t>(bottom - (ballY - ballR));

            const int16_t minH = min(overlapL, overlapR);
            const int16_t minV = min(overlapT, overlapB);

            if (minH < minV) {
                state.bvx = -state.bvx;
            } else {
                state.bvy = -state.bvy;
            }

            state.bricks[r][c] = false;
            state.bricksLeft--;
            state.score += 10;

            if (state.bricksLeft == 0) {
                if (state.level < 0xFFFFU) {
                    state.level++;
                }
                resetBricks(state);
                state.score += 50;
                resetBallPosition(state, width, height);
            }
            return;
        }
    }
}

void render(const LCD2BallGameState& state, TFT_eSprite& sprite, uint16_t width, uint16_t height) {
    sprite.fillSprite(kSpritePaper);

    const int16_t pLeft = panelLeft(width);
    const int16_t screenW = static_cast<int16_t>(width);
    const int16_t screenH = static_cast<int16_t>(height);
    const int16_t paddleTop = paddleY(height);
    const int16_t startX = bricksStartX(width);

    char scoreBuf[20];
    snprintf(scoreBuf, sizeof(scoreBuf), "%lu", static_cast<unsigned long>(state.score));
    char speedBuf[12];
    snprintf(speedBuf, sizeof(speedBuf), "%u%%", static_cast<unsigned int>(state.speedPercent));
    sprite.setTextColor(kSpriteInk, kSpritePaper);
    sprite.setTextDatum(TL_DATUM);
    sprite.drawString("PADDLE BALL", 4, 4, 1);
    sprite.setTextDatum(TC_DATUM);
    sprite.drawString(scoreBuf, static_cast<int16_t>(screenW / 2), 4, 2);

    // Play-area top border and left side border
    sprite.drawFastHLine(0, kHudBarH, pLeft, kSpriteInk);
    sprite.drawFastVLine(0, kHudBarH, static_cast<int16_t>(screenH - kHudBarH), kSpriteInk);

    sprite.setTextDatum(TL_DATUM);

    for (uint8_t r = 0; r < LCD2BallGameState::kBrickRows; ++r) {
        for (uint8_t c = 0; c < LCD2BallGameState::kBrickCols; ++c) {
            if (!state.bricks[r][c]) {
                continue;
            }

            const int16_t brickX = static_cast<int16_t>(startX + c * (kBrickW + kBrickGapX));
            const int16_t brickY = static_cast<int16_t>(kBricksTopMargin + r * (kBrickH + kBrickGapY));
            drawBrick(sprite, brickX, brickY, kBrickW, kBrickH);
        }
    }

    sprite.fillRect(state.paddleX, paddleTop, kPaddleW, kPaddleH, kSpriteInk);
    sprite.drawFastHLine(static_cast<int16_t>(state.paddleX + 2), static_cast<int16_t>(paddleTop + 2), static_cast<int16_t>(kPaddleW - 4), kSpritePaper);

    const uint32_t now32 = static_cast<uint32_t>(millis());
    const bool showBall = !state.dead || ((now32 & 128U) != 0U);
    if (showBall) {
        sprite.fillCircle(static_cast<int16_t>(state.bx), static_cast<int16_t>(state.by), static_cast<int16_t>(kBallRadius), kSpriteInk);
    }

    // ------- Right panel -------
    sprite.drawFastVLine(pLeft, 0, screenH, kSpriteInk);

    const int16_t sLeft   = vSliderLeft(width);
    const int16_t sTop    = kVSliderTopY;
    const int16_t sBot    = vSliderBotY(height);
    const int16_t sH      = max<int16_t>(2, static_cast<int16_t>(sBot - sTop));
    const int16_t panelCX = static_cast<int16_t>(pLeft + kRightPanelW / 2);

    sprite.setTextDatum(TC_DATUM);
    sprite.drawString("SPEED", panelCX, 2, 1);
    sprite.drawString(speedBuf, panelCX, 12, 1);

    sprite.drawRect(sLeft, sTop, kVSliderTrackW, sH, kSpriteInk);
    sprite.fillRect(static_cast<int16_t>(sLeft + 1),
                    static_cast<int16_t>(sTop + 1),
                    static_cast<int16_t>(kVSliderTrackW - 2),
                    static_cast<int16_t>(sH - 2),
                    kSpritePaper);

    const int16_t sliderInnerH = max<int16_t>(1, static_cast<int16_t>(sH - 2));
    const int16_t vNumer = static_cast<int16_t>(state.speedPercent - kBallSpeedMinPercent);
    const int16_t vDenom = max<int16_t>(1, static_cast<int16_t>(kBallSpeedMaxPercent - kBallSpeedMinPercent));
    const int16_t filledH = static_cast<int16_t>((static_cast<int32_t>(sliderInnerH) * vNumer) / vDenom);
    if (filledH > 0) {
        sprite.fillRect(static_cast<int16_t>(sLeft + 1),
                        static_cast<int16_t>(sBot - 1 - filledH),
                        static_cast<int16_t>(kVSliderTrackW - 2),
                        filledH,
                        kSpriteInk);
    }
    const int16_t knobY = constrain(static_cast<int16_t>(sBot - 1 - filledH),
                                     static_cast<int16_t>(sTop + 1),
                                     static_cast<int16_t>(sBot - 1));
    sprite.drawFastHLine(static_cast<int16_t>(sLeft - 1),
                         knobY,
                         static_cast<int16_t>(kVSliderTrackW + 2),
                         kSpriteInk);

    sprite.setTextDatum(TC_DATUM);
    sprite.drawString("LFE", panelCX, static_cast<int16_t>(sBot + 4), 1);
    for (uint8_t i = 0; i < kBallInitLives; ++i) {
        const int16_t lifeY = static_cast<int16_t>(screenH - 10 - i * 14);
        if (i < state.lives) {
            sprite.fillCircle(panelCX, lifeY, 5, kSpriteInk);
        } else {
            sprite.drawCircle(panelCX, lifeY, 5, kSpriteInk);
        }
    }

    if (state.dead) {
        sprite.setTextDatum(MC_DATUM);
        sprite.drawString("MISS!", static_cast<int16_t>(pLeft / 2), static_cast<int16_t>(screenH / 2), 4);
        if (state.lives == 0) {
            sprite.drawString("GAME OVER", static_cast<int16_t>(pLeft / 2), static_cast<int16_t>(screenH / 2 + 48), 2);
        }
    }
}

} // namespace lcd2_game_ball