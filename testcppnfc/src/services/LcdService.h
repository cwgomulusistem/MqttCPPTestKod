/**
 * @file LcdService.h
 * @brief LCD Servis Katmanı - Kuyruk Tabanlı Ekran Yönetimi
 * @version 1.0.0
 *
 * FreeRTOS Queue ile diğer servislerden ekran komutları alır.
 * Smart Sleep mimarisi: xQueueReceive + lv_timer_handler hibrit bekleme.
 * Thread-safe API: sendCommand() ile kuyruga mesaj at.
 */

#ifndef LCD_SERVICE_H
#define LCD_SERVICE_H

#include "../config/Config.h"
#include "../drivers/ILI9341Driver.h"
#include "../screens/FuntoriaScreenContext.h"
#include "../system/OsWrappers.h"
#include "LcdTypes.h"

/**
 * @class LcdService
 * @brief LCD Ekran Servis Sınıfı (Kuyruk Tabanlı)
 *
 * Mimari:
 * - Diğer servisler sendCommand() ile kuyruğa ScreenMessage atar
 * - Task loop kuyruktan mesaj alır ve LVGL ekranını günceller
 * - Smart Sleep: kuyruk mesajı veya LVGL timer süresi ile uyanır
 *
 * Thread Safety:
 * - sendCommand() herhangi bir task'tan çağrılabilir
 * - sendCommandFromISR() interrupt context'ten çağrılabilir
 * - Tüm LVGL işlemleri tek task içinde yapılır (single-threaded LVGL)
 */
class LcdService {
private:
  // Hardware state
  ILI9341DriverState _driver;
  TFT_eSPI _tft; // TFT_eSPI instance (global BSS)

  // RAII wrappers
  Mutex _displayLock;
  Task _task;
  Queue _commandQueue;

  // Durum
  bool _running;
  ScreenCommand _currentScreen;

  // LVGL UI objeler
  lv_obj_t *_scrMain;   // Ana ekran objesi
  lv_obj_t *_lblTitle;  // Başlık label
  lv_obj_t *_lblBody;   // Ana metin label
  lv_obj_t *_lblFooter; // Alt metin label

  // Auto-idle timer
  uint32_t _autoIdleTime; // Otomatik idle'a dönüş zamanı (0 = devre dışı)

  // Task loop (static - FreeRTOS uyumluluğu)
  static void taskLoop(void *param);

  // Komut işleme
  void processCommand(const ScreenMessage *msg);

  // LVGL UI oluşturma
  void createUI();
  void showIdleScreen();
  void showNfcReadScreen(const char *uid, const char *type);
  void showPaymentOkScreen(const char *userId, uint32_t balance);
  void showPaymentFailScreen(const char *reason);
  void showErrorScreen(const char *error);
  void showPhoneDetectedScreen();
  void showSetupRequiredScreen(const char *deviceId, const char *mac);
  void showCustomText(const char *line1, const char *line2);

  // Yardımcı
  FuntoriaScreenContext buildScreenContext() const;

public:
  /**
   * @brief Constructor
   */
  LcdService();

  /**
   * @brief Destructor - Otomatik cleanup
   */
  ~LcdService();

  // Copy/Move engelle
  LcdService(const LcdService &) = delete;
  LcdService &operator=(const LcdService &) = delete;

  // =================================================================
  // YAŞAM DÖNGÜSÜ
  // =================================================================

  /**
   * @brief Servisi başlat (donanım + LVGL init)
   * @param config LCD konfigürasyonu
   * @param taskConfig Task konfigürasyonu
   * @return true başarılıysa
   */
  bool init(const LcdConfig *config, const LcdTaskConfig *taskConfig);

  /**
   * @brief Task'ı başlat
   * @return true başarılıysa
   */
  bool start();

  /**
   * @brief Task'ı durdur
   */
  void stop();

  /**
   * @brief Servis çalışıyor mu?
   */
  bool isRunning() const;

  // =================================================================
  // DİĞER SERVİSLERDEN ÇAĞRILACAK API (Thread-Safe, Kuyruk Üzerinden)
  // =================================================================

  /**
   * @brief Kuyruğa ekran komutu gönder
   * @param msg Gönderilecek mesaj pointer'ı
   * @param timeout_ms Bekleme süresi (ms, 0 = hemen dön)
   * @return true kuyruğa eklendiyse
   */
  bool sendCommand(const ScreenMessage *msg, uint32_t timeout_ms = 100);

  /**
   * @brief ISR içinden kuyruğa komut gönder
   * @param msg Gönderilecek mesaj pointer'ı
   * @return true kuyruğa eklendiyse
   */
  bool sendCommandFromISR(const ScreenMessage *msg);

  // =================================================================
  // KISAYOL FONKSİYONLARI (Kolay kullanım için)
  // =================================================================

  bool showIdle();
  bool showNfcRead(const char *uid, const char *type);
  bool showPaymentOk(const char *userId, uint32_t balance);
  bool showPaymentFail(const char *reason);
  bool showError(const char *error);
  bool showPhoneDetected();
  bool showSetupRequired(const char *deviceId, const char *mac);
  bool setBacklight(uint8_t brightness);

  // =================================================================
  // DEBUG
  // =================================================================

  /**
   * @brief Task stack durumunu al
   */
  UBaseType_t getStackHighWaterMark() const;
};

#endif // LCD_SERVICE_H
