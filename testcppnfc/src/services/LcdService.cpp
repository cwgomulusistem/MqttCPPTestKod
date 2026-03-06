/**
 * @file LcdService.cpp
 * @brief LCD Servis İmplementasyonu - Smart Sleep Task Loop
 * @version 1.0.0
 *
 * Akıllı Bekleme Mimarisi:
 * - xQueueReceive ile kuyruktan komut bekle
 * - lv_timer_handler() ile LVGL periyodik güncelleme
 * - Hibrit: ya mesaj gelince ya da LVGL timer'ı dolunca uyanır
 *
 * Tüm LVGL işlemleri bu tek task içinde yapılır (thread-safe).
 */

#include "LcdService.h"
#include "../config/Logger.h"
#include "../screens/FuntoriaScreens.h"
#include <string.h>

// =============================================================================
// CONSTRUCTOR / DESTRUCTOR
// =============================================================================

LcdService::LcdService()
    : _running(false), _currentScreen(SCREEN_CMD_SHOW_IDLE), _scrMain(nullptr),
      _lblTitle(nullptr), _lblBody(nullptr), _lblFooter(nullptr),
      _autoIdleTime(0) {}

LcdService::~LcdService() { stop(); }

// =============================================================================
// YAŞAM DÖNGÜSÜ
// =============================================================================

bool LcdService::init(const LcdConfig *config,
                      const LcdTaskConfig *taskConfig) {
  if (!config || !taskConfig) {
    LOG_E("LCD: NULL config!");
    return false;
  }

  // Kuyruk oluştur
  if (!_commandQueue.create(taskConfig->queue_size, sizeof(ScreenMessage))) {
    LOG_E("LCD: Queue create failed!");
    return false;
  }
  LOG_D("LCD: Queue created (size=%d)", taskConfig->queue_size);

  // ILI9341 driver başlat (TFT_eSPI + LVGL)
  if (!ILI9341Driver_init(&_driver, &_tft, config)) {
    LOG_E("LCD: Driver init failed!");
    return false;
  }

  // LVGL UI oluştur
  createUI();

  // İlk ekran: Idle
  showIdleScreen();
  lv_timer_handler();

  LOG_I("LCD: Service initialized");
  return true;
}

bool LcdService::start() {
  if (_running) {
    LOG_W("LCD: Already running!");
    return false;
  }

  _running = true;

  // Task başlat - extern AppConfig'ten stack/priority al
  extern AppConfig g_appConfig;
  if (!_task.start("LcdTask", taskLoop, this, g_appConfig.lcd_task.task_stack,
                   g_appConfig.lcd_task.task_priority)) {
    LOG_E("LCD: Task start failed!");
    _running = false;
    return false;
  }

  LOG_I("LCD: Task started (stack=%d, priority=%d)",
        g_appConfig.lcd_task.task_stack, g_appConfig.lcd_task.task_priority);
  return true;
}

void LcdService::stop() {
  _running = false;
  // Task kendini silecek (vTaskDelete(nullptr))
  // RAII ile Task destructor temizleyecek
}

bool LcdService::isRunning() const { return _running; }

// =============================================================================
// TASK LOOP - testesp32LCD LOOP AKISI
// =============================================================================

void LcdService::taskLoop(void *param) {
  LcdService *self = static_cast<LcdService *>(param);
  ScreenMessage msg;
  uint32_t last_tick_ms = millis();

  while (self->_running) {
    // testesp32LCD loop: kuyruğu bloklamadan boşalt
    while (xQueueReceive(self->_commandQueue.handle(), &msg, 0) == pdTRUE) {
      self->processCommand(&msg);
    }

    // Auto-idle kontrolü
    if (self->_autoIdleTime > 0 && millis() >= self->_autoIdleTime) {
      self->_autoIdleTime = 0;
      self->showIdleScreen();
    }

    // testesp32LCD loop: tick + handler + sabit delay
    const uint32_t now = millis();
    lv_tick_inc(now - last_tick_ms);
    last_tick_ms = now;
    lv_timer_handler();

    vTaskDelay(pdMS_TO_TICKS(5));
  }

  vTaskDelete(nullptr);
}

// =============================================================================
// KOMUT İŞLEME
// =============================================================================

void LcdService::processCommand(const ScreenMessage *msg) {
  if (!msg)
    return;

  LOG_D("LCD: Processing command: %s", ScreenCommand_toString(msg->command));

  switch (msg->command) {
  case SCREEN_CMD_SHOW_IDLE:
    showIdleScreen();
    break;

  case SCREEN_CMD_SHOW_NFC_READ:
    showNfcReadScreen(msg->text1, msg->text2);
    break;

  case SCREEN_CMD_SHOW_PAYMENT_OK:
    showPaymentOkScreen(msg->text1, msg->value);
    break;

  case SCREEN_CMD_SHOW_PAYMENT_FAIL:
    showPaymentFailScreen(msg->text1);
    break;

  case SCREEN_CMD_SHOW_ERROR:
    showErrorScreen(msg->text1);
    break;

  case SCREEN_CMD_SHOW_PHONE_DETECTED:
    showPhoneDetectedScreen();
    break;

  case SCREEN_CMD_SHOW_SETUP_REQUIRED:
    showSetupRequiredScreen(msg->text1, msg->text2);
    break;

  case SCREEN_CMD_SET_BACKLIGHT:
    ILI9341Driver_setBacklight(&_driver, (uint8_t)msg->value);
    break;

  case SCREEN_CMD_CUSTOM_TEXT:
    showCustomText(msg->text1, msg->text2);
    break;

  default:
    LOG_W("LCD: Unknown command: %d", msg->command);
    break;
  }

  // Auto-idle timer ayarla
  if (msg->duration_ms > 0 && msg->command != SCREEN_CMD_SHOW_IDLE) {
    _autoIdleTime = millis() + msg->duration_ms;
  } else {
    _autoIdleTime = 0;
  }

  _currentScreen = msg->command;
}

// =============================================================================
// LVGL UI OLUŞTURMA
// =============================================================================

void LcdService::createUI() {
  // testesp32LCD ile ayni sekilde ayri bir screen olusturup aktif et.
  _scrMain = lv_obj_create(nullptr);
  if (!_scrMain) {
    LOG_E("LCD: Failed to create main screen");
    return;
  }

#if LVGL_VERSION_MAJOR >= 9
  lv_screen_load(_scrMain);
#else
  lv_scr_load(_scrMain);
#endif

  lv_obj_remove_style_all(_scrMain);
  lv_obj_set_size(_scrMain, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_style_bg_opa(_scrMain, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(_scrMain, lv_color_hex(0x13071D), 0);

  // Başlık label (üst kısım)
  _lblTitle = lv_label_create(_scrMain);
  lv_obj_set_width(_lblTitle, LCD_WIDTH - 20);
  lv_obj_set_style_text_align(_lblTitle, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(_lblTitle, lv_color_hex(0xA855F7), 0);
  lv_obj_set_style_text_font(_lblTitle, &lv_font_montserrat_20, 0);
  lv_obj_align(_lblTitle, LV_ALIGN_TOP_MID, 0, 30);

  // Ana metin label (orta kısım)
  _lblBody = lv_label_create(_scrMain);
  lv_obj_set_width(_lblBody, LCD_WIDTH - 20);
  lv_obj_set_style_text_align(_lblBody, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(_lblBody, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(_lblBody, &lv_font_montserrat_16, 0);
  lv_obj_align(_lblBody, LV_ALIGN_CENTER, 0, 0);

  // Alt metin label (alt kısım)
  _lblFooter = lv_label_create(_scrMain);
  lv_obj_set_width(_lblFooter, LCD_WIDTH - 20);
  lv_obj_set_style_text_align(_lblFooter, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(_lblFooter, lv_color_hex(0x94A3B8), 0);
  lv_obj_set_style_text_font(_lblFooter, &lv_font_montserrat_14, 0);
  lv_obj_align(_lblFooter, LV_ALIGN_BOTTOM_MID, 0, -30);

  LOG_D("LCD: UI created");
}

FuntoriaScreenContext LcdService::buildScreenContext() const {
  FuntoriaScreenContext ctx = {};
  ctx.root = _scrMain;
  ctx.title = _lblTitle;
  ctx.body = _lblBody;
  ctx.footer = _lblFooter;
  return ctx;
}

// =============================================================================
// EKRAN GÖSTERİMLERİ
// =============================================================================

void LcdService::showIdleScreen() {
  FuntoriaScreenContext ctx = buildScreenContext();
  FuntoriaScreen_showWaiting(&ctx);
}

void LcdService::showNfcReadScreen(const char *uid, const char *type) {
  FuntoriaScreenContext ctx = buildScreenContext();
  FuntoriaScreen_showNfcRead(&ctx, uid, type);
}

void LcdService::showPaymentOkScreen(const char *userId, uint32_t balance) {
  FuntoriaScreenContext ctx = buildScreenContext();
  FuntoriaScreen_showPaymentOk(&ctx, userId, balance);
}

void LcdService::showPaymentFailScreen(const char *reason) {
  FuntoriaScreenContext ctx = buildScreenContext();
  FuntoriaScreen_showPaymentFail(&ctx, reason);
}

void LcdService::showErrorScreen(const char *error) {
  FuntoriaScreenContext ctx = buildScreenContext();
  FuntoriaScreen_showError(&ctx, error);
}

void LcdService::showPhoneDetectedScreen() {
  FuntoriaScreenContext ctx = buildScreenContext();
  FuntoriaScreen_showPhoneDetected(&ctx);
}

void LcdService::showSetupRequiredScreen(const char *deviceId, const char *mac) {
  FuntoriaScreenContext ctx = buildScreenContext();
  FuntoriaScreen_showSetupRequired(&ctx, deviceId, mac);
}

void LcdService::showCustomText(const char *line1, const char *line2) {
  FuntoriaScreenContext ctx = buildScreenContext();
  FuntoriaScreen_showCustomText(&ctx, line1, line2);
}

// =============================================================================
// THREAD-SAFE API (Kuyruk Üzerinden)
// =============================================================================

bool LcdService::sendCommand(const ScreenMessage *msg, uint32_t timeout_ms) {
  if (!msg || !_commandQueue.isValid())
    return false;
  return _commandQueue.send(msg, pdMS_TO_TICKS(timeout_ms));
}

bool LcdService::sendCommandFromISR(const ScreenMessage *msg) {
  if (!msg || !_commandQueue.isValid())
    return false;
  return _commandQueue.sendFromISR(msg);
}

// =============================================================================
// KISAYOL FONKSİYONLARI
// =============================================================================

bool LcdService::showIdle() {
  ScreenMessage msg = {};
  msg.command = SCREEN_CMD_SHOW_IDLE;
  return sendCommand(&msg);
}

bool LcdService::showNfcRead(const char *uid, const char *type) {
  ScreenMessage msg = {};
  msg.command = SCREEN_CMD_SHOW_NFC_READ;
  if (uid)
    strncpy(msg.text1, uid, sizeof(msg.text1) - 1);
  if (type)
    strncpy(msg.text2, type, sizeof(msg.text2) - 1);
  msg.duration_ms = 3000; // 3 saniye sonra idle'a dön
  return sendCommand(&msg);
}

bool LcdService::showPaymentOk(const char *userId, uint32_t balance) {
  ScreenMessage msg = {};
  msg.command = SCREEN_CMD_SHOW_PAYMENT_OK;
  if (userId)
    strncpy(msg.text1, userId, sizeof(msg.text1) - 1);
  msg.value = balance;
  msg.duration_ms = 5000; // 5 saniye sonra idle'a dön
  return sendCommand(&msg);
}

bool LcdService::showPaymentFail(const char *reason) {
  ScreenMessage msg = {};
  msg.command = SCREEN_CMD_SHOW_PAYMENT_FAIL;
  if (reason)
    strncpy(msg.text1, reason, sizeof(msg.text1) - 1);
  msg.duration_ms = 5000;
  return sendCommand(&msg);
}

bool LcdService::showError(const char *error) {
  ScreenMessage msg = {};
  msg.command = SCREEN_CMD_SHOW_ERROR;
  if (error)
    strncpy(msg.text1, error, sizeof(msg.text1) - 1);
  msg.duration_ms = 5000;
  return sendCommand(&msg);
}

bool LcdService::showPhoneDetected() {
  ScreenMessage msg = {};
  msg.command = SCREEN_CMD_SHOW_PHONE_DETECTED;
  msg.duration_ms = 5000;
  return sendCommand(&msg);
}

bool LcdService::showSetupRequired(const char *deviceId, const char *mac) {
  ScreenMessage msg = {};
  msg.command = SCREEN_CMD_SHOW_SETUP_REQUIRED;
  if (deviceId) {
    strncpy(msg.text1, deviceId, sizeof(msg.text1) - 1);
  }
  if (mac) {
    strncpy(msg.text2, mac, sizeof(msg.text2) - 1);
  }
  msg.duration_ms = 0;
  return sendCommand(&msg);
}

bool LcdService::setBacklight(uint8_t brightness) {
  ScreenMessage msg = {};
  msg.command = SCREEN_CMD_SET_BACKLIGHT;
  msg.value = brightness;
  return sendCommand(&msg);
}

// =============================================================================
// DEBUG
// =============================================================================

UBaseType_t LcdService::getStackHighWaterMark() const {
  return _task.getStackHighWaterMark();
}
