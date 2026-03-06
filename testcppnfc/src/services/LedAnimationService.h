/**
 * @file LedAnimationService.h
 * @brief LED Animasyon Servisi (Queue + Task)
 * @version 1.0.0
 */

#ifndef LED_ANIMATION_SERVICE_H
#define LED_ANIMATION_SERVICE_H

#include <stddef.h>
#include <stdint.h>
#include <esp32-hal-rmt.h>

#include "../config/Config.h"
#include "../system/OsWrappers.h"
#include "LedTypes.h"

class LedAnimationService {
private:
  struct LedRgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
  };

  static constexpr uint8_t MAX_LED_PIXELS = 22;

  enum WaitingPhase : uint8_t {
    WAITING_SWEEP_OUT = 0, // Grup-1: 11->1, Grup-2: 12->22
    WAITING_SWEEP_BACK,    // Grup-1: 1->11, Grup-2: 22->12
    WAITING_FILL_NEXT_COLOR
  };

  struct IdleWaitingState {
    bool initialized;
    bool effectPhaseActive;
    uint8_t effectStage;
    WaitingPhase phase;
    int groupAIndex;
    int groupBIndex;
    uint8_t fillStep;
    uint8_t colorPairIndex;
    uint8_t dualCycleCount;
    uint32_t lastStepMs;
    uint32_t effectStageStartMs;
    uint8_t lastLoggedStage;
    uint32_t rainbowLastMs;
    bool rainbowDirection;
    uint8_t rainbowInit;
    int rainbowTailHead;
    float rainbowColor[3][MAX_LED_PIXELS];
    uint32_t snakeLastMs;
    int snakeHead;
    int snakeDir;
  };

  struct NetworkWaitingState {
    bool initialized;
    uint32_t counter;
    uint32_t lastStepMs;
    uint32_t tailLastMs;
    uint32_t mixLastMs;
    uint32_t snakeLastMs;
    bool tailDirection;
    uint8_t tailInit;
    int tailHead;
    float tailColor[3][MAX_LED_PIXELS];
    int mixForward;
    int mixStep;
    uint8_t mixColor[2][MAX_LED_PIXELS];
    int snakeHead1;
    int snakeHead2;
    int snakeStep;
  };

  struct PaymentWaitingState {
    bool initialized;
    uint32_t counter;
    uint32_t lastStepMs;
    uint32_t tailLastMs;
    uint32_t mixLastMs;
    uint32_t snakeLastMs;
    bool tailDirection;
    uint8_t tailInit;
    int tailHead;
    float tailColor[3][MAX_LED_PIXELS];
    int mixForward;
    int mixStep;
    uint8_t mixColor[1][MAX_LED_PIXELS];
    int snakeHead1;
    int snakeHead2;
    int snakeStep;
  };

  // RAII wrappers
  Task _task;
  Queue _commandQueue;

  // Konfig
  LedConfig _config;
  LedTaskConfig _taskConfig;
  rmt_obj_t *_rmt;
  rmt_data_t *_rmtData;
  size_t _rmtDataSize;
  LedRgb _pixelBuffer[MAX_LED_PIXELS];

  // Durum
  bool _initialized;
  bool _running;

  // Kalici mod
  LedAnimMode _baseMode;
  uint32_t _baseMask;

  // Gecici efekt (flash vb.)
  bool _temporaryActive;
  LedAnimMode _temporaryMode;
  uint32_t _temporaryMask;
  uint32_t _temporaryUntilMs;

  // Animasyon bilgisi
  uint8_t _frame;
  uint32_t _lastFrameMs;
  IdleWaitingState _idleState;
  NetworkWaitingState _networkState;
  PaymentWaitingState _paymentState;
  LedAnimMode _lastLoggedMode;
  bool _modeLogInitialized;

  static void taskLoop(void *param);

  void processCommand(const LedMessage *msg);
  void updateAnimation();

  void logModeIfChanged(LedAnimMode mode);
  void logIdleStageIfChanged(uint8_t stage, uint8_t dualCycleCount,
                             uint8_t totalDualCycles);

  void runIdleWaitingAnimation(uint32_t nowMs);
  void runRainbowStage(uint8_t ledCount, uint32_t nowMs);
  void runSnakeStage(uint8_t ledCount, uint32_t nowMs);
  void runNetworkWaitingAnimation(uint32_t nowMs);
  void runPaymentWaitingAnimation(uint32_t nowMs);
  void runYellowTailAnimation(uint8_t ledCount);
  void runRedTailAnimation(uint8_t ledCount);
  void runYellowMixAnimation(uint8_t ledCount);
  void runRedMixAnimation(uint8_t ledCount);
  void runYellowSnakeAnimation(uint8_t ledCount, uint8_t length);
  void runRedSnakeAnimation(uint8_t ledCount, uint8_t length);
  void placeChannelSegment(uint8_t ledCount, int head, uint8_t length,
                           uint8_t *channel, int direction, uint8_t value);

  void applyMask(uint32_t mask, LedRgb color);
  void pushPixelBuffer();
  void fillPixelBuffer(LedRgb color);

  LedRgb hsbToRgb(float h, float s, float b) const;
  LedRgb waitingPaletteColor(uint8_t paletteIndex) const;

  uint32_t fullMask() const;
  void resetAnimation();
  void scheduleTemporary(LedAnimMode mode, uint16_t duration_ms, uint32_t mask);
  void clearStrip();

public:
  LedAnimationService();
  ~LedAnimationService();

  // Copy/Move engelle
  LedAnimationService(const LedAnimationService &) = delete;
  LedAnimationService &operator=(const LedAnimationService &) = delete;

  bool init(const LedConfig *config, const LedTaskConfig *taskConfig);
  bool start();
  void stop();
  bool isRunning() const;

  bool sendCommand(const LedMessage *msg, uint32_t timeout_ms = 50);

  // Kisa API
  bool setMode(LedAnimMode mode);
  bool setMask(uint32_t mask, uint16_t duration_ms = 0);
  bool flashSuccess(uint16_t duration_ms = 1200);
  bool flashError(uint16_t duration_ms = 1200);
  bool on();
  bool off();

  UBaseType_t getStackHighWaterMark() const;
};

#endif // LED_ANIMATION_SERVICE_H
