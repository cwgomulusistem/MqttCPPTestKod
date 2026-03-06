/**
 * @file LedAnimationService.cpp
 * @brief LED Animasyon Servisi implementasyonu (WS2812 / RMT)
 * @version 1.4.0
 */

#include "LedAnimationService.h"
#include "../config/Logger.h"

#include <Arduino.h>
#include <math.h>
#include <new>
#include <string.h>

LedAnimationService::LedAnimationService()
    : _rmt(nullptr), _rmtData(nullptr), _rmtDataSize(0), _initialized(false),
      _running(false), _baseMode(LED_ANIM_OFF), _baseMask(0),
      _temporaryActive(false), _temporaryMode(LED_ANIM_OFF), _temporaryMask(0),
      _temporaryUntilMs(0), _frame(0), _lastFrameMs(0), _idleState({}),
      _networkState({}), _paymentState({}), _lastLoggedMode(LED_ANIM_OFF),
      _modeLogInitialized(false) {
  memset(&_config, 0, sizeof(_config));
  memset(&_taskConfig, 0, sizeof(_taskConfig));
  memset(_pixelBuffer, 0, sizeof(_pixelBuffer));
  memset(&_idleState, 0, sizeof(_idleState));
  memset(&_networkState, 0, sizeof(_networkState));
  memset(&_paymentState, 0, sizeof(_paymentState));
  _idleState.lastLoggedStage = 0xFF;
}

LedAnimationService::~LedAnimationService() { stop(); }

bool LedAnimationService::init(const LedConfig *config,
                               const LedTaskConfig *taskConfig) {
  if (!config || !taskConfig) {
    LOG_E("LED: NULL config");
    return false;
  }

  memcpy(&_config, config, sizeof(_config));
  memcpy(&_taskConfig, taskConfig, sizeof(_taskConfig));

  if (_config.led_count == 0 || _config.led_count > MAX_LED_PIXELS) {
    LOG_E("LED: Invalid count=%u (max=%u)", _config.led_count, MAX_LED_PIXELS);
    return false;
  }

  if (_taskConfig.animation_step_ms < 10) {
    _taskConfig.animation_step_ms = 10;
  }

  if (!_commandQueue.create(_taskConfig.queue_size, sizeof(LedMessage))) {
    LOG_E("LED: Queue create failed");
    return false;
  }

  _rmt = rmtInit(_config.data_pin, RMT_TX_MODE, RMT_MEM_64);
  if (!_rmt) {
    LOG_E("LED: rmtInit failed (pin=%u)", _config.data_pin);
    return false;
  }
  rmtSetTick(_rmt, 100.0f);

  _rmtDataSize = static_cast<size_t>(_config.led_count) * 24U;
  _rmtData = new (std::nothrow) rmt_data_t[_rmtDataSize];
  if (!_rmtData) {
    LOG_E("LED: rmt buffer alloc failed (%u bits)",
          static_cast<unsigned>(_rmtDataSize));
    rmtDeinit(_rmt);
    _rmt = nullptr;
    return false;
  }

  clearStrip();

  _baseMode = LED_ANIM_IDLE;
  _baseMask = fullMask();
  _temporaryActive = false;
  resetAnimation();
  _initialized = true;

  LOG_I("LED: Service initialized (count=%u, pin=%u, step=%u)",
        _config.led_count, _config.data_pin, _taskConfig.animation_step_ms);
  return true;
}

bool LedAnimationService::start() {
  if (!_initialized) {
    LOG_E("LED: Not initialized");
    return false;
  }

  if (_running) {
    LOG_W("LED: Already running");
    return false;
  }

  _running = true;
  bool success = _task.start("LedTask", taskLoop, this, _taskConfig.task_stack,
                             _taskConfig.task_priority, 1);
  if (!success) {
    _running = false;
    LOG_E("LED: Task start failed");
    return false;
  }

  LOG_I("LED: Task started (stack=%u, priority=%u)", _taskConfig.task_stack,
        _taskConfig.task_priority);
  return true;
}

void LedAnimationService::stop() {
  if (_running) {
    _running = false;
    _task.stop();
  }

  clearStrip();

  if (_rmtData) {
    delete[] _rmtData;
    _rmtData = nullptr;
    _rmtDataSize = 0;
  }

  if (_rmt) {
    rmtDeinit(_rmt);
    _rmt = nullptr;
  }

  if (_initialized) {
    LOG_I("LED: Service stopped");
  }

  _initialized = false;
}

bool LedAnimationService::isRunning() const { return _running; }

void LedAnimationService::taskLoop(void *param) {
  LedAnimationService *self = static_cast<LedAnimationService *>(param);
  LedMessage msg = {};

  while (self->_running) {
    while (self->_commandQueue.receive(&msg, 0)) {
      self->processCommand(&msg);
    }

    self->updateAnimation();
    vTaskDelay(pdMS_TO_TICKS(self->_taskConfig.animation_step_ms));
  }

  self->clearStrip();
  vTaskDelete(nullptr);
}

void LedAnimationService::processCommand(const LedMessage *msg) {
  if (!msg) {
    return;
  }

  LOG_D("LED: cmd=%s mode=%s mask=0x%08lX duration=%u",
        LedCommand_toString(msg->command), LedAnimMode_toString(msg->mode),
        (unsigned long)msg->mask, msg->duration_ms);

  switch (msg->command) {
  case LED_CMD_SET_MODE:
    _baseMode = msg->mode;
    _temporaryActive = false;
    resetAnimation();
    break;

  case LED_CMD_FLASH_SUCCESS:
    scheduleTemporary(LED_ANIM_FLASH_SUCCESS,
                      msg->duration_ms > 0 ? msg->duration_ms : 1200, 0);
    break;

  case LED_CMD_FLASH_ERROR:
    scheduleTemporary(LED_ANIM_FLASH_ERROR,
                      msg->duration_ms > 0 ? msg->duration_ms : 1200, 0);
    break;

  case LED_CMD_SET_MASK:
    if (msg->duration_ms > 0) {
      scheduleTemporary(LED_ANIM_SOLID_MASK, msg->duration_ms,
                        msg->mask & fullMask());
    } else {
      _baseMode = LED_ANIM_SOLID_MASK;
      _baseMask = msg->mask & fullMask();
      _temporaryActive = false;
      resetAnimation();
    }
    break;

  default:
    break;
  }
}

void LedAnimationService::scheduleTemporary(LedAnimMode mode,
                                            uint16_t duration_ms,
                                            uint32_t mask) {
  _temporaryActive = true;
  _temporaryMode = mode;
  _temporaryMask = mask;
  _temporaryUntilMs = millis() + duration_ms;
  resetAnimation();
}

void LedAnimationService::updateAnimation() {
  uint32_t now = millis();

  if (_temporaryActive && now >= _temporaryUntilMs) {
    _temporaryActive = false;
    resetAnimation();
  }

  const LedRgb colorOff = {0, 0, 0};
  const LedRgb colorOn = {160, 160, 160};
  const LedRgb colorSuccess = {0, 180, 0};
  const LedRgb colorError = {180, 0, 0};

  LedAnimMode mode = _temporaryActive ? _temporaryMode : _baseMode;
  uint32_t mask = 0;
  uint32_t full = fullMask();

  logModeIfChanged(mode);

  switch (mode) {
  case LED_ANIM_OFF:
    fillPixelBuffer(colorOff);
    pushPixelBuffer();
    break;

  case LED_ANIM_IDLE:
    runIdleWaitingAnimation(now);
    break;

  case LED_ANIM_WAIT_NETWORK:
    runNetworkWaitingAnimation(now);
    break;

  case LED_ANIM_WAIT_PAYMENT:
    runPaymentWaitingAnimation(now);
    break;

  case LED_ANIM_SOLID_ON:
    applyMask(full, colorOn);
    break;

  case LED_ANIM_SOLID_MASK:
    mask = (_temporaryActive ? _temporaryMask : _baseMask) & full;
    applyMask(mask, colorOn);
    break;

  case LED_ANIM_FLASH_SUCCESS:
    mask = ((_frame % 4U) < 2U) ? full : 0U;
    _frame++;
    applyMask(mask, colorSuccess);
    break;

  case LED_ANIM_FLASH_ERROR:
  case LED_ANIM_ERROR:
    mask = ((_frame & 0x01U) == 0U) ? full : 0U;
    _frame++;
    applyMask(mask, colorError);
    break;

  default:
    fillPixelBuffer(colorOff);
    pushPixelBuffer();
    break;
  }
}

void LedAnimationService::fillPixelBuffer(LedRgb color) {
  for (uint8_t i = 0; i < _config.led_count && i < MAX_LED_PIXELS; i++) {
    _pixelBuffer[i] = color;
  }
}

void LedAnimationService::applyMask(uint32_t mask, LedRgb color) {
  const LedRgb off = {0, 0, 0};
  for (uint8_t i = 0; i < _config.led_count && i < MAX_LED_PIXELS; i++) {
    _pixelBuffer[i] = (mask & (1UL << i)) ? color : off;
  }
  pushPixelBuffer();
}

void LedAnimationService::pushPixelBuffer() {
  if (!_rmt || !_rmtData || _rmtDataSize == 0) {
    return;
  }

  size_t out = 0;
  for (uint8_t i = 0; i < _config.led_count && i < MAX_LED_PIXELS; i++) {
    const LedRgb &c = _pixelBuffer[i];
    uint8_t grb[3] = {c.g, c.r, c.b};

    for (uint8_t col = 0; col < 3; col++) {
      for (uint8_t bit = 0; bit < 8; bit++) {
        bool one = (grb[col] & (1U << (7 - bit))) != 0;
        _rmtData[out].level0 = 1;
        _rmtData[out].duration0 = one ? 8 : 4;
        _rmtData[out].level1 = 0;
        _rmtData[out].duration1 = one ? 4 : 8;
        out++;
      }
    }
  }

  rmtWriteBlocking(_rmt, _rmtData, _rmtDataSize);
}

LedAnimationService::LedRgb LedAnimationService::hsbToRgb(float h, float s,
                                                          float b) const {
  while (h < 0.0f) {
    h += 1.0f;
  }
  while (h >= 1.0f) {
    h -= 1.0f;
  }

  if (s <= 0.0f) {
    uint8_t v = static_cast<uint8_t>(b * 255.0f);
    return {v, v, v};
  }

  float hf = h * 6.0f;
  int i = static_cast<int>(floorf(hf));
  float f = hf - i;
  float p = b * (1.0f - s);
  float q = b * (1.0f - s * f);
  float t = b * (1.0f - s * (1.0f - f));

  float rf = 0.0f;
  float gf = 0.0f;
  float bf = 0.0f;

  switch (i % 6) {
  case 0:
    rf = b;
    gf = t;
    bf = p;
    break;
  case 1:
    rf = q;
    gf = b;
    bf = p;
    break;
  case 2:
    rf = p;
    gf = b;
    bf = t;
    break;
  case 3:
    rf = p;
    gf = q;
    bf = b;
    break;
  case 4:
    rf = t;
    gf = p;
    bf = b;
    break;
  default:
    rf = b;
    gf = p;
    bf = q;
    break;
  }

  return {static_cast<uint8_t>(rf * 255.0f), static_cast<uint8_t>(gf * 255.0f),
          static_cast<uint8_t>(bf * 255.0f)};
}

LedAnimationService::LedRgb
LedAnimationService::waitingPaletteColor(uint8_t paletteIndex) const {
  constexpr uint8_t kPairCount = 16; // 16 zit cift = 32 farkli renk
  uint8_t pairIndex = (paletteIndex / 2U) % kPairCount;
  bool oppositeColor = (paletteIndex & 0x01U) != 0;

  float hue = static_cast<float>(pairIndex) / static_cast<float>(kPairCount);
  if (oppositeColor) {
    hue += 0.5f;
  }

  return hsbToRgb(hue, 0.98f, 0.90f);
}

void LedAnimationService::logModeIfChanged(LedAnimMode mode) {
  if (!_modeLogInitialized || _lastLoggedMode != mode) {
    LOG_I("LED anim: %s", LedAnimMode_toString(mode));
    _lastLoggedMode = mode;
    _modeLogInitialized = true;
  }
}

void LedAnimationService::logIdleStageIfChanged(uint8_t stage,
                                                uint8_t dualCycleCount,
                                                uint8_t totalDualCycles) {
  if (_idleState.lastLoggedStage == stage) {
    return;
  }

  if (stage == 0U) {
    LOG_I("LED waiting: dual_started cycle=%u/%u",
          static_cast<unsigned>(dualCycleCount + 1U),
          static_cast<unsigned>(totalDualCycles));
  } else if (stage == 1U) {
    LOG_I("LED waiting: rainbow_started");
  } else if (stage == 2U) {
    LOG_I("LED waiting: snake_started");
  } else {
    LOG_I("LED waiting: stage=%u", static_cast<unsigned>(stage));
  }

  _idleState.lastLoggedStage = stage;
}

void LedAnimationService::runIdleWaitingAnimation(uint32_t nowMs) {
  constexpr uint8_t kDualRepeatsBeforeEffects = 5;
  constexpr uint32_t kRainbowDurationMs = 6500;
  constexpr uint32_t kSnakeDurationMs = 6500;
  constexpr uint8_t kIdleStageDual = 0;
  constexpr uint8_t kIdleStageRainbow = 1;
  constexpr uint8_t kIdleStageSnake = 2;

  const uint8_t ledCount =
      (_config.led_count > MAX_LED_PIXELS) ? MAX_LED_PIXELS : _config.led_count;
  if (ledCount < 4) {
    const LedRgb off = {0, 0, 0};
    fillPixelBuffer(off);
    pushPixelBuffer();
    return;
  }

  const int groupASize = ledCount / 2;
  const int groupBSize = ledCount - groupASize;
  const int groupAStart = 0;
  const int groupAEnd = groupASize - 1;
  const int groupBStart = groupASize;
  const int groupBEnd = ledCount - 1;
  const int fillLength = (groupASize < groupBSize) ? groupASize : groupBSize;

  if (fillLength <= 0) {
    const LedRgb off = {0, 0, 0};
    fillPixelBuffer(off);
    pushPixelBuffer();
    return;
  }

  if (!_idleState.initialized) {
    _idleState.initialized = true;
    _idleState.effectPhaseActive = false;
    _idleState.effectStage = kIdleStageDual;
    _idleState.phase = WAITING_SWEEP_OUT;
    _idleState.groupAIndex = groupAEnd;
    _idleState.groupBIndex = groupBStart;
    _idleState.fillStep = 0;
    _idleState.colorPairIndex = 0;
    _idleState.dualCycleCount = 0;
    _idleState.lastStepMs = 0;
    _idleState.effectStageStartMs = 0;
    _idleState.lastLoggedStage = 0xFF;
  }

  if (_idleState.effectPhaseActive) {
    if (_idleState.effectStage == kIdleStageRainbow) {
      logIdleStageIfChanged(kIdleStageRainbow, _idleState.dualCycleCount,
                            kDualRepeatsBeforeEffects);
      runRainbowStage(ledCount, nowMs);

      if (_idleState.effectStageStartMs != 0 &&
          (nowMs - _idleState.effectStageStartMs) >= kRainbowDurationMs) {
        LOG_I("LED waiting: rainbow_finished switching=snake");
        _idleState.effectStage = kIdleStageSnake;
        _idleState.effectStageStartMs = nowMs;
        _idleState.lastLoggedStage = 0xFF;
        _idleState.snakeLastMs = 0;
        _idleState.snakeHead = 0;
        _idleState.snakeDir = 1;
      }
      return;
    }

    if (_idleState.effectStage == kIdleStageSnake) {
      logIdleStageIfChanged(kIdleStageSnake, _idleState.dualCycleCount,
                            kDualRepeatsBeforeEffects);
      runSnakeStage(ledCount, nowMs);

      if (_idleState.effectStageStartMs != 0 &&
          (nowMs - _idleState.effectStageStartMs) >= kSnakeDurationMs) {
        LOG_I("LED waiting: snake_finished switching=dual");
        _idleState.effectPhaseActive = false;
        _idleState.effectStage = kIdleStageDual;
        _idleState.phase = WAITING_SWEEP_OUT;
        _idleState.groupAIndex = groupAEnd;
        _idleState.groupBIndex = groupBStart;
        _idleState.fillStep = 0;
        _idleState.lastStepMs = 0;
        _idleState.dualCycleCount = 0;
        _idleState.lastLoggedStage = 0xFF;
      }
      return;
    }

    _idleState.effectPhaseActive = false;
    _idleState.effectStage = kIdleStageDual;
    _idleState.lastLoggedStage = 0xFF;
  }

  uint8_t currentPair = _idleState.colorPairIndex % 16U;
  uint8_t nextPair = (currentPair + 1U) % 16U;

  LedRgb colorA = waitingPaletteColor(static_cast<uint8_t>(currentPair * 2U));
  LedRgb colorB =
      waitingPaletteColor(static_cast<uint8_t>(currentPair * 2U + 1U));
  LedRgb nextColorA = waitingPaletteColor(static_cast<uint8_t>(nextPair * 2U));
  LedRgb nextColorB =
      waitingPaletteColor(static_cast<uint8_t>(nextPair * 2U + 1U));

  const uint16_t stepMs =
      (_taskConfig.animation_step_ms < 15) ? 15 : _taskConfig.animation_step_ms;

  if (_idleState.lastStepMs != 0 && (nowMs - _idleState.lastStepMs) < stepMs) {
    return;
  }
  _idleState.lastStepMs = nowMs;

  const LedRgb off = {0, 0, 0};
  fillPixelBuffer(off);

  logIdleStageIfChanged(kIdleStageDual, _idleState.dualCycleCount,
                        kDualRepeatsBeforeEffects);

  switch (_idleState.phase) {
  case WAITING_SWEEP_OUT:
    if (_idleState.groupAIndex >= groupAStart &&
        _idleState.groupAIndex <= groupAEnd) {
      _pixelBuffer[_idleState.groupAIndex] = colorA;
    }
    if (_idleState.groupBIndex >= groupBStart &&
        _idleState.groupBIndex <= groupBEnd) {
      _pixelBuffer[_idleState.groupBIndex] = colorB;
    }
    pushPixelBuffer();

    _idleState.groupAIndex--;
    _idleState.groupBIndex++;
    if (_idleState.groupAIndex < groupAStart ||
        _idleState.groupBIndex > groupBEnd) {
      _idleState.phase = WAITING_SWEEP_BACK;
      _idleState.groupAIndex = groupAStart;
      _idleState.groupBIndex = groupBEnd;
    }
    break;

  case WAITING_SWEEP_BACK:
    if (_idleState.groupAIndex >= groupAStart &&
        _idleState.groupAIndex <= groupAEnd) {
      _pixelBuffer[_idleState.groupAIndex] = colorA;
    }
    if (_idleState.groupBIndex >= groupBStart &&
        _idleState.groupBIndex <= groupBEnd) {
      _pixelBuffer[_idleState.groupBIndex] = colorB;
    }
    pushPixelBuffer();

    _idleState.groupAIndex++;
    _idleState.groupBIndex--;
    if (_idleState.groupAIndex > groupAEnd ||
        _idleState.groupBIndex < groupBStart) {
      _idleState.phase = WAITING_FILL_NEXT_COLOR;
      _idleState.fillStep = 0;
    }
    break;

  case WAITING_FILL_NEXT_COLOR:
    for (int i = 0; i <= _idleState.fillStep && i < fillLength; i++) {
      int idxA = groupAEnd - i;
      int idxB = groupBStart + i;

      if (idxA >= groupAStart && idxA <= groupAEnd) {
        _pixelBuffer[idxA] = nextColorA;
      }
      if (idxB >= groupBStart && idxB <= groupBEnd) {
        _pixelBuffer[idxB] = nextColorB;
      }
    }
    pushPixelBuffer();

    if (_idleState.fillStep < static_cast<uint8_t>(fillLength - 1)) {
      _idleState.fillStep++;
    } else {
      _idleState.colorPairIndex = nextPair;
      _idleState.phase = WAITING_SWEEP_OUT;
      _idleState.groupAIndex = groupAEnd;
      _idleState.groupBIndex = groupBStart;
      _idleState.fillStep = 0;

      _idleState.dualCycleCount++;
      if (_idleState.dualCycleCount >= kDualRepeatsBeforeEffects) {
        LOG_I("LED waiting: dual_finished repeats=%u switching=rainbow",
              static_cast<unsigned>(kDualRepeatsBeforeEffects));
        _idleState.effectPhaseActive = true;
        _idleState.effectStage = kIdleStageRainbow;
        _idleState.effectStageStartMs = nowMs;
        _idleState.dualCycleCount = 0;
        _idleState.lastLoggedStage = 0xFF;
        _idleState.rainbowInit = 0;
        _idleState.rainbowLastMs = 0;
        _idleState.snakeLastMs = 0;
      }
    }
    break;

  default:
    _idleState.phase = WAITING_SWEEP_OUT;
    _idleState.groupAIndex = groupAEnd;
    _idleState.groupBIndex = groupBStart;
    _idleState.fillStep = 0;
    break;
  }
}

void LedAnimationService::runRainbowStage(uint8_t ledCount, uint32_t nowMs) {
  if (ledCount < 4) {
    return;
  }

  const uint16_t stepMs = 20;
  if (_idleState.rainbowLastMs != 0 &&
      (nowMs - _idleState.rainbowLastMs) < stepMs) {
    return;
  }
  _idleState.rainbowLastMs = nowMs;

  if (_idleState.rainbowInit == 0U) {
    _idleState.rainbowDirection = true;
    _idleState.rainbowTailHead = 0;
    _idleState.rainbowInit = 1U;
    memset(_idleState.rainbowColor, 0, sizeof(_idleState.rainbowColor));
  }

  const LedRgb off = {0, 0, 0};
  fillPixelBuffer(off);

  const uint8_t tailLength = (ledCount >= 8U) ? 6U : 4U;
  const float baseHue =
      fmodf(static_cast<float>(nowMs % 10000U) / 10000.0f, 1.0f);

  for (uint8_t i = 0; i < tailLength; i++) {
    int idx = _idleState.rainbowDirection
                  ? (_idleState.rainbowTailHead - static_cast<int>(i))
                  : (_idleState.rainbowTailHead + static_cast<int>(i));
    if (idx < 0 || idx >= ledCount || idx >= MAX_LED_PIXELS) {
      continue;
    }

    float hue = baseHue + (static_cast<float>(i) * 0.12f);
    float brightness = 1.0f - (static_cast<float>(i) * 0.13f);
    if (brightness < 0.25f) {
      brightness = 0.25f;
    }

    _pixelBuffer[idx] = hsbToRgb(hue, 0.99f, brightness);
  }

  pushPixelBuffer();

  if (_idleState.rainbowDirection) {
    _idleState.rainbowTailHead++;
    if (_idleState.rainbowTailHead >= static_cast<int>(ledCount - 1)) {
      _idleState.rainbowTailHead = ledCount - 1;
      _idleState.rainbowDirection = false;
    }
  } else {
    _idleState.rainbowTailHead--;
    if (_idleState.rainbowTailHead <= 0) {
      _idleState.rainbowTailHead = 0;
      _idleState.rainbowDirection = true;
    }
  }
}

void LedAnimationService::runSnakeStage(uint8_t ledCount, uint32_t nowMs) {
  if (ledCount < 4) {
    return;
  }

  const uint16_t stepMs = 22;
  if (_idleState.snakeLastMs != 0 && (nowMs - _idleState.snakeLastMs) < stepMs) {
    return;
  }
  _idleState.snakeLastMs = nowMs;

  if (_idleState.snakeDir == 0) {
    _idleState.snakeDir = 1;
    _idleState.snakeHead = 0;
  }

  const LedRgb off = {0, 0, 0};
  fillPixelBuffer(off);

  const uint8_t snakeLength = (ledCount >= 10U) ? 5U : 4U;
  LedRgb snakeColor = waitingPaletteColor(static_cast<uint8_t>(
      (_idleState.colorPairIndex % 16U) * 2U));

  for (uint8_t i = 0; i < snakeLength; i++) {
    int idx = _idleState.snakeHead + static_cast<int>(i);
    if (idx < 0 || idx >= ledCount || idx >= MAX_LED_PIXELS) {
      continue;
    }

    uint8_t fade = static_cast<uint8_t>(255U - (i * 40U));
    _pixelBuffer[idx] = {
        static_cast<uint8_t>((static_cast<uint16_t>(snakeColor.r) * fade) / 255U),
        static_cast<uint8_t>((static_cast<uint16_t>(snakeColor.g) * fade) / 255U),
        static_cast<uint8_t>((static_cast<uint16_t>(snakeColor.b) * fade) / 255U)};
  }

  pushPixelBuffer();

  _idleState.snakeHead += _idleState.snakeDir;
  if (_idleState.snakeHead <= 0) {
    _idleState.snakeHead = 0;
    _idleState.snakeDir = 1;
  } else if (_idleState.snakeHead + static_cast<int>(snakeLength) >= ledCount) {
    _idleState.snakeHead = ledCount - snakeLength;
    _idleState.snakeDir = -1;
  }
}

void LedAnimationService::runNetworkWaitingAnimation(uint32_t nowMs) {
  (void)nowMs;

  const uint8_t ledCount =
      (_config.led_count > MAX_LED_PIXELS) ? MAX_LED_PIXELS : _config.led_count;
  if (ledCount == 0) {
    return;
  }

  if (!_networkState.initialized) {
    memset(&_networkState, 0, sizeof(_networkState));
    _networkState.initialized = true;
    _networkState.tailDirection = true;
    _networkState.tailInit = 1;
    _networkState.tailHead = 4;
    _networkState.mixForward = 0;
    _networkState.mixStep = 1;
    _networkState.snakeHead1 = 0;
    _networkState.snakeHead2 = static_cast<int>(ledCount) - 1;
    _networkState.snakeStep = 1;
    _networkState.counter = 0;
  }

  const uint32_t step1 = 0x7FB;
  const uint32_t step2 = 0x17F1;
  const uint32_t step3 = 0x27E7;

  if (_networkState.counter < step1) {
    runYellowTailAnimation(ledCount);
  } else if (_networkState.counter < step2) {
    runYellowMixAnimation(ledCount);
  } else if (_networkState.counter < step3) {
    runYellowSnakeAnimation(ledCount, 4);
  } else {
    _networkState.counter = 0;
  }

  _networkState.counter++;
}

void LedAnimationService::runPaymentWaitingAnimation(uint32_t nowMs) {
  (void)nowMs;

  const uint8_t ledCount =
      (_config.led_count > MAX_LED_PIXELS) ? MAX_LED_PIXELS : _config.led_count;
  if (ledCount == 0) {
    return;
  }

  if (!_paymentState.initialized) {
    memset(&_paymentState, 0, sizeof(_paymentState));
    _paymentState.initialized = true;
    _paymentState.tailDirection = true;
    _paymentState.tailInit = 1;
    _paymentState.tailHead = 4;
    _paymentState.mixForward = 0;
    _paymentState.mixStep = 1;
    _paymentState.snakeHead1 = 0;
    _paymentState.snakeHead2 = static_cast<int>(ledCount) - 1;
    _paymentState.snakeStep = 1;
    _paymentState.counter = 0;
  }

  const uint32_t step1 = 0x7FB;
  const uint32_t step2 = 0x17F1;
  const uint32_t step3 = 0x27E7;

  if (_paymentState.counter < step1) {
    runRedTailAnimation(ledCount);
  } else if (_paymentState.counter < step2) {
    runRedMixAnimation(ledCount);
  } else if (_paymentState.counter < step3) {
    runRedSnakeAnimation(ledCount, 4);
  } else {
    _paymentState.counter = 0;
  }

  _paymentState.counter++;
}

void LedAnimationService::runYellowTailAnimation(uint8_t ledCount) {
  if (ledCount == 0) {
    return;
  }

  const uint16_t stepMs = 20;
  const uint32_t now = millis();
  if (_networkState.tailLastMs != 0 && (now - _networkState.tailLastMs) < stepMs) {
    return;
  }
  _networkState.tailLastMs = now;

  if (_networkState.tailInit == 1) {
    const float h = 0.15f;
    const float s = 0.99f;
    const float b = 0.90f;
    for (int i = 0; i < 4 && i < ledCount && i < MAX_LED_PIXELS; i++) {
      _networkState.tailColor[0][i] = h;
      _networkState.tailColor[1][i] = s;
      _networkState.tailColor[2][i] = b;
    }
    _networkState.tailInit = 2;
  }

  if (_networkState.tailHead >= 4 && _networkState.tailDirection &&
      _networkState.tailHead != ledCount) {
    for (int i = _networkState.tailHead - 4; i < _networkState.tailHead; i++) {
      if (i < 0 || i + 1 >= MAX_LED_PIXELS) {
        continue;
      }
      float h = _networkState.tailColor[0][i];
      float s = _networkState.tailColor[1][i];
      float b = _networkState.tailColor[2][i];
      _networkState.tailColor[0][i + 1] = h;
      _networkState.tailColor[1][i + 1] = s;
      _networkState.tailColor[2][i + 1] = b;
    }
    _networkState.tailHead++;
    if (_networkState.tailHead > ledCount) {
      _networkState.tailHead = ledCount;
    }
  } else if (_networkState.tailHead == ledCount && _networkState.tailDirection) {
    float temp[3][4] = {{0.0f}};
    for (int i = ledCount - 4; i < ledCount; i++) {
      if (i < 0 || i >= MAX_LED_PIXELS) {
        continue;
      }
      for (int j = 0; j < 3; j++) {
        temp[j][ledCount - i - 1] = _networkState.tailColor[j][i];
      }
    }
    for (int i = ledCount - 4; i < ledCount; i++) {
      if (i < 0 || i >= MAX_LED_PIXELS) {
        continue;
      }
      for (int j = 0; j < 3; j++) {
        int t = i - (ledCount - 4);
        if (t >= 0 && t < 4) {
          _networkState.tailColor[j][i] = temp[j][t];
        }
      }
    }
    _networkState.tailHead = ledCount - 4;
    _networkState.tailDirection = false;
  } else if (_networkState.tailHead > 0 && !_networkState.tailDirection) {
    for (int i = _networkState.tailHead; i < _networkState.tailHead + 4; i++) {
      if (i <= 0 || i >= MAX_LED_PIXELS) {
        continue;
      }
      float h = _networkState.tailColor[0][i];
      float s = _networkState.tailColor[1][i];
      float b = _networkState.tailColor[2][i];
      _networkState.tailColor[0][i - 1] = h;
      _networkState.tailColor[1][i - 1] = s;
      _networkState.tailColor[2][i - 1] = b;
    }
    _networkState.tailHead--;
    if (_networkState.tailHead < 0) {
      _networkState.tailHead = 0;
    }
  } else if (_networkState.tailHead == 0 && !_networkState.tailDirection) {
    float temp[3][4] = {{0.0f}};
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 3; j++) {
        temp[j][4 - i - 1] = _networkState.tailColor[j][i];
      }
    }
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 3; j++) {
        _networkState.tailColor[j][i] = temp[j][i];
      }
    }
    _networkState.tailHead = 4;
    _networkState.tailDirection = true;
  }

  if (_networkState.tailDirection && _networkState.tailHead != 4) {
    int tailBack = _networkState.tailHead - 4 - 1;
    if (tailBack >= 0 && tailBack < MAX_LED_PIXELS) {
      _networkState.tailColor[0][tailBack] = 0.0f;
      _networkState.tailColor[1][tailBack] = 0.0f;
      _networkState.tailColor[2][tailBack] = 0.0f;
    }
  } else if (!_networkState.tailDirection &&
             _networkState.tailHead != (ledCount - 4)) {
    int tailBack = _networkState.tailHead + 4 + 1;
    if (tailBack >= 0 && tailBack < MAX_LED_PIXELS) {
      _networkState.tailColor[0][tailBack] = 0.0f;
      _networkState.tailColor[1][tailBack] = 0.0f;
      _networkState.tailColor[2][tailBack] = 0.0f;
    }
  }

  for (int i = 0; i < ledCount && i < MAX_LED_PIXELS; i++) {
    _pixelBuffer[i] = hsbToRgb(_networkState.tailColor[0][i],
                               _networkState.tailColor[1][i],
                               _networkState.tailColor[2][i]);
  }

  pushPixelBuffer();
}

void LedAnimationService::runRedTailAnimation(uint8_t ledCount) {
  if (ledCount == 0) {
    return;
  }

  const uint16_t stepMs = 20;
  const uint32_t now = millis();
  if (_paymentState.tailLastMs != 0 && (now - _paymentState.tailLastMs) < stepMs) {
    return;
  }
  _paymentState.tailLastMs = now;

  if (_paymentState.tailInit == 1) {
    const float h = 0.0f;
    const float s = 0.99f;
    const float b = 0.90f;
    for (int i = 0; i < 4 && i < ledCount && i < MAX_LED_PIXELS; i++) {
      _paymentState.tailColor[0][i] = h;
      _paymentState.tailColor[1][i] = s;
      _paymentState.tailColor[2][i] = b;
    }
    _paymentState.tailInit = 2;
  }

  if (_paymentState.tailHead >= 4 && _paymentState.tailDirection &&
      _paymentState.tailHead != ledCount) {
    for (int i = _paymentState.tailHead - 4; i < _paymentState.tailHead; i++) {
      if (i < 0 || i + 1 >= MAX_LED_PIXELS) {
        continue;
      }
      float h = _paymentState.tailColor[0][i];
      float s = _paymentState.tailColor[1][i];
      float b = _paymentState.tailColor[2][i];
      _paymentState.tailColor[0][i + 1] = h;
      _paymentState.tailColor[1][i + 1] = s;
      _paymentState.tailColor[2][i + 1] = b;
    }
    _paymentState.tailHead++;
    if (_paymentState.tailHead > ledCount) {
      _paymentState.tailHead = ledCount;
    }
  } else if (_paymentState.tailHead == ledCount && _paymentState.tailDirection) {
    float temp[3][4] = {{0.0f}};
    for (int i = ledCount - 4; i < ledCount; i++) {
      if (i < 0 || i >= MAX_LED_PIXELS) {
        continue;
      }
      for (int j = 0; j < 3; j++) {
        temp[j][ledCount - i - 1] = _paymentState.tailColor[j][i];
      }
    }
    for (int i = ledCount - 4; i < ledCount; i++) {
      if (i < 0 || i >= MAX_LED_PIXELS) {
        continue;
      }
      for (int j = 0; j < 3; j++) {
        int t = i - (ledCount - 4);
        if (t >= 0 && t < 4) {
          _paymentState.tailColor[j][i] = temp[j][t];
        }
      }
    }
    _paymentState.tailHead = ledCount - 4;
    _paymentState.tailDirection = false;
  } else if (_paymentState.tailHead > 0 && !_paymentState.tailDirection) {
    for (int i = _paymentState.tailHead; i < _paymentState.tailHead + 4; i++) {
      if (i <= 0 || i >= MAX_LED_PIXELS) {
        continue;
      }
      float h = _paymentState.tailColor[0][i];
      float s = _paymentState.tailColor[1][i];
      float b = _paymentState.tailColor[2][i];
      _paymentState.tailColor[0][i - 1] = h;
      _paymentState.tailColor[1][i - 1] = s;
      _paymentState.tailColor[2][i - 1] = b;
    }
    _paymentState.tailHead--;
    if (_paymentState.tailHead < 0) {
      _paymentState.tailHead = 0;
    }
  } else if (_paymentState.tailHead == 0 && !_paymentState.tailDirection) {
    float temp[3][4] = {{0.0f}};
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 3; j++) {
        temp[j][4 - i - 1] = _paymentState.tailColor[j][i];
      }
    }
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 3; j++) {
        _paymentState.tailColor[j][i] = temp[j][i];
      }
    }
    _paymentState.tailHead = 4;
    _paymentState.tailDirection = true;
  }

  if (_paymentState.tailDirection && _paymentState.tailHead != 4) {
    int tailBack = _paymentState.tailHead - 4 - 1;
    if (tailBack >= 0 && tailBack < MAX_LED_PIXELS) {
      _paymentState.tailColor[0][tailBack] = 0.0f;
      _paymentState.tailColor[1][tailBack] = 0.0f;
      _paymentState.tailColor[2][tailBack] = 0.0f;
    }
  } else if (!_paymentState.tailDirection &&
             _paymentState.tailHead != (ledCount - 4)) {
    int tailBack = _paymentState.tailHead + 4 + 1;
    if (tailBack >= 0 && tailBack < MAX_LED_PIXELS) {
      _paymentState.tailColor[0][tailBack] = 0.0f;
      _paymentState.tailColor[1][tailBack] = 0.0f;
      _paymentState.tailColor[2][tailBack] = 0.0f;
    }
  }

  for (int i = 0; i < ledCount && i < MAX_LED_PIXELS; i++) {
    _pixelBuffer[i] = hsbToRgb(_paymentState.tailColor[0][i],
                               _paymentState.tailColor[1][i],
                               _paymentState.tailColor[2][i]);
  }

  pushPixelBuffer();
}

void LedAnimationService::runYellowMixAnimation(uint8_t ledCount) {
  const uint32_t now = millis();
  if (_networkState.mixLastMs != 0 && (now - _networkState.mixLastMs) <= 30) {
    return;
  }
  _networkState.mixLastMs = now;

  if (_networkState.mixStep == 1) {
    if (_networkState.mixForward < ledCount) {
      _networkState.mixColor[0][_networkState.mixForward] = 0x7F;
      _networkState.mixColor[1][_networkState.mixForward] = 0x7F;
      _networkState.mixForward++;
    } else {
      _networkState.mixForward = 0;
      _networkState.mixStep = 2;
    }
  } else {
    if (_networkState.mixForward < ledCount) {
      int currentLed = static_cast<int>(ledCount) - _networkState.mixForward - 1;
      if (currentLed >= 0 && currentLed < ledCount) {
        _networkState.mixColor[0][currentLed] = 0x00;
        _networkState.mixColor[1][currentLed] = 0x00;
      }
      _networkState.mixForward++;
    } else {
      _networkState.mixForward = 0;
      _networkState.mixStep = 1;
    }
  }

  for (uint8_t i = 0; i < ledCount && i < MAX_LED_PIXELS; i++) {
    _pixelBuffer[i] = {_networkState.mixColor[0][i], _networkState.mixColor[1][i],
                       0};
  }

  pushPixelBuffer();
}

void LedAnimationService::runRedMixAnimation(uint8_t ledCount) {
  const uint32_t now = millis();
  if (_paymentState.mixLastMs != 0 && (now - _paymentState.mixLastMs) <= 30) {
    return;
  }
  _paymentState.mixLastMs = now;

  if (_paymentState.mixStep == 1) {
    if (_paymentState.mixForward <= ledCount) {
      if (_paymentState.mixForward < ledCount) {
        _paymentState.mixColor[0][_paymentState.mixForward] = 0x7F;
      }
      _paymentState.mixForward++;
    } else {
      _paymentState.mixForward = 0;
      _paymentState.mixStep = 2;
    }
  } else {
    if (_paymentState.mixForward <= ledCount) {
      int idx = static_cast<int>(ledCount) - _paymentState.mixForward;
      if (idx >= 0 && idx < ledCount) {
        _paymentState.mixColor[0][idx] = 0x00;
      }
      _paymentState.mixForward++;
    } else {
      _paymentState.mixForward = 0;
      _paymentState.mixStep = 1;
    }
  }

  for (uint8_t i = 0; i < ledCount && i < MAX_LED_PIXELS; i++) {
    _pixelBuffer[i] = {_paymentState.mixColor[0][i], 0, 0};
  }

  pushPixelBuffer();
}

void LedAnimationService::runYellowSnakeAnimation(uint8_t ledCount,
                                                  uint8_t length) {
  const uint32_t now = millis();
  if (_networkState.snakeLastMs != 0 && (now - _networkState.snakeLastMs) <= 25) {
    return;
  }
  _networkState.snakeLastMs = now;

  uint8_t color[3][MAX_LED_PIXELS] = {{0}};

  if (_networkState.snakeHead1 <= static_cast<int>(ledCount + length)) {
    placeChannelSegment(ledCount, _networkState.snakeHead1, length, color[0], 1,
                        0x7F);
    placeChannelSegment(ledCount, _networkState.snakeHead1, length, color[1], 1,
                        0x7F);
    _networkState.snakeHead1++;
    _networkState.snakeHead2--;
  } else {
    _networkState.snakeHead1 = 0;
    _networkState.snakeHead2 = static_cast<int>(ledCount) - 1;
    _networkState.snakeStep++;
    if (_networkState.snakeStep > 6) {
      _networkState.snakeStep = 1;
    }
  }

  for (uint8_t i = 0; i < ledCount && i < MAX_LED_PIXELS; i++) {
    _pixelBuffer[i] = {color[0][i], color[1][i], 0};
  }

  pushPixelBuffer();
}

void LedAnimationService::runRedSnakeAnimation(uint8_t ledCount, uint8_t length) {
  const uint32_t now = millis();
  if (_paymentState.snakeLastMs != 0 && (now - _paymentState.snakeLastMs) <= 25) {
    return;
  }
  _paymentState.snakeLastMs = now;

  uint8_t color[3][MAX_LED_PIXELS] = {{0}};

  if (_paymentState.snakeHead1 <= static_cast<int>(ledCount + length)) {
    placeChannelSegment(ledCount, _paymentState.snakeHead1, length, color[0], 1,
                        0x7F);
    _paymentState.snakeHead1++;
    _paymentState.snakeHead2--;
  } else {
    _paymentState.snakeHead1 = 0;
    _paymentState.snakeHead2 = static_cast<int>(ledCount) - 1;
    _paymentState.snakeStep++;
    if (_paymentState.snakeStep > 6) {
      _paymentState.snakeStep = 1;
    }
  }

  for (uint8_t i = 0; i < ledCount && i < MAX_LED_PIXELS; i++) {
    _pixelBuffer[i] = {color[0][i], 0, 0};
  }

  pushPixelBuffer();
}

void LedAnimationService::placeChannelSegment(uint8_t ledCount, int head,
                                              uint8_t length, uint8_t *channel,
                                              int direction, uint8_t value) {
  int start = 0;
  int finish = 0;

  if (direction == 1) {
    start = head - length;
    if (start < 0) {
      start = 0;
    }

    finish = head;
    if (finish > ledCount) {
      finish = ledCount;
    }
  } else {
    start = head;
    if (start < 0) {
      start = 0;
    }

    finish = head + length;
    if (finish > ledCount) {
      finish = ledCount;
    }
  }

  for (int i = 0; i < ledCount && i < MAX_LED_PIXELS; i++) {
    channel[i] = 0x00;
  }

  for (int i = start; i < finish && i < MAX_LED_PIXELS; i++) {
    channel[i] = value;
  }
}

uint32_t LedAnimationService::fullMask() const {
  if (_config.led_count >= 32) {
    return 0xFFFFFFFFUL;
  }
  return (1UL << _config.led_count) - 1UL;
}

void LedAnimationService::clearStrip() {
  const LedRgb off = {0, 0, 0};
  fillPixelBuffer(off);
  pushPixelBuffer();
}

void LedAnimationService::resetAnimation() {
  _frame = 0;
  _lastFrameMs = 0;

  memset(&_idleState, 0, sizeof(_idleState));
  _idleState.lastLoggedStage = 0xFF;

  memset(&_networkState, 0, sizeof(_networkState));
  memset(&_paymentState, 0, sizeof(_paymentState));
}

bool LedAnimationService::sendCommand(const LedMessage *msg,
                                      uint32_t timeout_ms) {
  if (!msg || !_commandQueue.isValid()) {
    return false;
  }
  return _commandQueue.send(msg, pdMS_TO_TICKS(timeout_ms));
}

bool LedAnimationService::setMode(LedAnimMode mode) {
  LedMessage msg = {};
  msg.command = LED_CMD_SET_MODE;
  msg.mode = mode;
  return sendCommand(&msg);
}

bool LedAnimationService::setMask(uint32_t mask, uint16_t duration_ms) {
  LedMessage msg = {};
  msg.command = LED_CMD_SET_MASK;
  msg.mask = mask;
  msg.duration_ms = duration_ms;
  return sendCommand(&msg);
}

bool LedAnimationService::flashSuccess(uint16_t duration_ms) {
  LedMessage msg = {};
  msg.command = LED_CMD_FLASH_SUCCESS;
  msg.duration_ms = duration_ms;
  return sendCommand(&msg);
}

bool LedAnimationService::flashError(uint16_t duration_ms) {
  LedMessage msg = {};
  msg.command = LED_CMD_FLASH_ERROR;
  msg.duration_ms = duration_ms;
  return sendCommand(&msg);
}

bool LedAnimationService::on() { return setMode(LED_ANIM_SOLID_ON); }

bool LedAnimationService::off() { return setMode(LED_ANIM_OFF); }

UBaseType_t LedAnimationService::getStackHighWaterMark() const {
  return _task.getStackHighWaterMark();
}
