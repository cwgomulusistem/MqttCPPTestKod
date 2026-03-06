/**
 * @file OsWrappers.h
 * @brief FreeRTOS RAII Wrappers (Header-Only)
 * @version 2.0.0 - Hibrit Mimari
 *
 * C++ RAII ile FreeRTOS kaynak yönetimi.
 * Mutex::ScopedLock ile deadlock riski SIFIR.
 * Inline olduğu için zero overhead.
 */

#ifndef OS_WRAPPERS_H
#define OS_WRAPPERS_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

/**
 * @class Mutex
 * @brief FreeRTOS Mutex için RAII Wrapper
 *
 * Constructor'da otomatik oluşturma, destructor'da otomatik silme.
 * ScopedLock ile fonksiyon sonunda otomatik unlock - deadlock İMKANSIZ.
 */
class Mutex {
private:
  SemaphoreHandle_t _handle;

public:
  Mutex() : _handle(nullptr) { _handle = xSemaphoreCreateMutex(); }

  ~Mutex() {
    if (_handle) {
      vSemaphoreDelete(_handle);
      _handle = nullptr;
    }
  }

  // Copy/Move engelle (tek sahiplik)
  Mutex(const Mutex &) = delete;
  Mutex &operator=(const Mutex &) = delete;
  Mutex(Mutex &&) = delete;
  Mutex &operator=(Mutex &&) = delete;

  /**
   * @brief Mutex'i kilitle
   * @param timeout Bekleme süresi (tick)
   * @return Kilit başarılıysa true
   */
  bool lock(TickType_t timeout = portMAX_DELAY) {
    if (!_handle)
      return false;
    return xSemaphoreTake(_handle, timeout) == pdTRUE;
  }

  /**
   * @brief Mutex'i serbest bırak
   */
  void unlock() {
    if (_handle) {
      xSemaphoreGive(_handle);
    }
  }

  /**
   * @brief Mutex geçerli mi kontrol et
   */
  bool isValid() const { return _handle != nullptr; }

  /**
   * @class ScopedLock
   * @brief Scope-Based Locking (EN ÖNEMLİ KISIM)
   *
   * Fonksiyon bitince OTOMATİK unlock.
   * Return, exception, break - ne olursa olsun kilit açılır.
   * Deadlock İMKANSIZ.
   *
   * Kullanım:
   * @code
   * void myFunction() {
   *     Mutex::ScopedLock lock(myMutex);
   *     if (!lock.isLocked()) return;  // Timeout kontrolü
   *
   *     // ... güvenli işlemler ...
   *
   * }  // Scope bitince otomatik unlock
   * @endcode
   */
  class ScopedLock {
  private:
    Mutex &_mutex;
    bool _locked;

  public:
    /**
     * @brief Constructor - Otomatik lock
     * @param m Kilitlenecek mutex referansı
     * @param timeout Bekleme süresi (tick)
     */
    explicit ScopedLock(Mutex &m, TickType_t timeout = portMAX_DELAY)
        : _mutex(m), _locked(m.lock(timeout)) {}

    /**
     * @brief Destructor - Otomatik unlock
     */
    ~ScopedLock() {
      if (_locked) {
        _mutex.unlock();
      }
    }

    // Copy/Move engelle
    ScopedLock(const ScopedLock &) = delete;
    ScopedLock &operator=(const ScopedLock &) = delete;
    ScopedLock(ScopedLock &&) = delete;
    ScopedLock &operator=(ScopedLock &&) = delete;

    /**
     * @brief Kilit başarılı mı kontrol et
     * @return Lock başarılıysa true
     */
    bool isLocked() const { return _locked; }

    /**
     * @brief bool'a dönüşüm (if içinde kullanım için)
     */
    explicit operator bool() const { return _locked; }
  };
};

/**
 * @class Task
 * @brief FreeRTOS Task için RAII Wrapper
 *
 * Constructor'da oluşturma opsiyonel, destructor'da otomatik silme.
 * start() ile task başlatılır, stop() veya destructor ile silinir.
 */
class Task {
private:
  TaskHandle_t _handle;

public:
  Task() : _handle(nullptr) {}

  ~Task() { stop(); }

  // Copy/Move engelle
  Task(const Task &) = delete;
  Task &operator=(const Task &) = delete;
  Task(Task &&) = delete;
  Task &operator=(Task &&) = delete;

  /**
   * @brief Task'ı başlat
   * @param name Task adı (debug için)
   * @param func Task fonksiyonu
   * @param param Task parametresi (genellikle this)
   * @param stackSize Stack boyutu (byte)
   * @param priority Task önceliği
   * @param core Çalışacağı core (-1 = herhangi)
   * @return Başarılıysa true
   */
  bool start(const char *name, TaskFunction_t func, void *param,
             uint16_t stackSize = 4096, UBaseType_t priority = 1,
             BaseType_t core = -1) {
    if (_handle)
      return false; // Zaten çalışıyor

    BaseType_t result;
    if (core >= 0) {
      result = xTaskCreatePinnedToCore(func, name, stackSize, param, priority,
                                       &_handle, core);
    } else {
      result = xTaskCreate(func, name, stackSize, param, priority, &_handle);
    }

    return result == pdPASS;
  }

  /**
   * @brief Task'ı durdur ve sil
   */
  void stop() {
    if (_handle) {
      vTaskDelete(_handle);
      _handle = nullptr;
    }
  }

  /**
   * @brief Task çalışıyor mu kontrol et
   */
  bool isRunning() const { return _handle != nullptr; }

  /**
   * @brief Task handle'ı al (FreeRTOS API için)
   */
  TaskHandle_t handle() const { return _handle; }

  /**
   * @brief Kalan stack miktarını al (debug için)
   */
  UBaseType_t getStackHighWaterMark() const {
    if (!_handle)
      return 0;
    return uxTaskGetStackHighWaterMark(_handle);
  }
};

/**
 * @class Queue
 * @brief FreeRTOS Queue için RAII Wrapper
 *
 * Constructor'da otomatik oluşturma, destructor'da otomatik silme.
 * Type-safe send/receive ile kullanım kolaylığı.
 */
class Queue {
private:
  QueueHandle_t _handle;

public:
  Queue() : _handle(nullptr) {}

  ~Queue() { destroy(); }

  // Copy/Move engelle (tek sahiplik)
  Queue(const Queue &) = delete;
  Queue &operator=(const Queue &) = delete;
  Queue(Queue &&) = delete;
  Queue &operator=(Queue &&) = delete;

  /**
   * @brief Kuyruk oluştur
   * @param length Kuyruk kapasitesi (eleman sayısı)
   * @param itemSize Her elemanın boyutu (byte)
   * @return Başarılıysa true
   */
  bool create(UBaseType_t length, UBaseType_t itemSize) {
    if (_handle)
      return false; // Zaten oluşturulmuş
    _handle = xQueueCreate(length, itemSize);
    return _handle != nullptr;
  }

  /**
   * @brief Kuyruğu sil
   */
  void destroy() {
    if (_handle) {
      vQueueDelete(_handle);
      _handle = nullptr;
    }
  }

  /**
   * @brief Kuyruğa eleman gönder
   * @param item Gönderilecek elemanın pointer'ı
   * @param timeout Bekleme süresi (tick)
   * @return Başarılıysa true
   */
  bool send(const void *item, TickType_t timeout = portMAX_DELAY) {
    if (!_handle)
      return false;
    return xQueueSend(_handle, item, timeout) == pdTRUE;
  }

  /**
   * @brief Kuyruğa eleman gönder (öncelikli - başa ekle)
   * @param item Gönderilecek elemanın pointer'ı
   * @param timeout Bekleme süresi (tick)
   * @return Başarılıysa true
   */
  bool sendToFront(const void *item, TickType_t timeout = portMAX_DELAY) {
    if (!_handle)
      return false;
    return xQueueSendToFront(_handle, item, timeout) == pdTRUE;
  }

  /**
   * @brief ISR içinden kuyruğa eleman gönder
   * @param item Gönderilecek elemanın pointer'ı
   * @return Başarılıysa true
   */
  bool sendFromISR(const void *item) {
    if (!_handle)
      return false;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t result =
        xQueueSendFromISR(_handle, item, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
      portYIELD_FROM_ISR();
    }
    return result == pdTRUE;
  }

  /**
   * @brief Kuyruktan eleman al
   * @param item Hedef buffer pointer'ı
   * @param timeout Bekleme süresi (tick)
   * @return Başarılıysa true (eleman alındı)
   */
  bool receive(void *item, TickType_t timeout = portMAX_DELAY) {
    if (!_handle)
      return false;
    return xQueueReceive(_handle, item, timeout) == pdTRUE;
  }

  /**
   * @brief Kuyruktaki eleman sayısını al
   */
  UBaseType_t count() const {
    if (!_handle)
      return 0;
    return uxQueueMessagesWaiting(_handle);
  }

  /**
   * @brief Kuyruk boş mu?
   */
  bool isEmpty() const { return count() == 0; }

  /**
   * @brief Kuyruk geçerli mi?
   */
  bool isValid() const { return _handle != nullptr; }

  /**
   * @brief Ham handle'a erişim (FreeRTOS API için)
   */
  QueueHandle_t handle() const { return _handle; }
};

/**
 * @brief Milisaniyeyi tick'e çevir
 */
inline TickType_t msToTicks(uint32_t ms) { return pdMS_TO_TICKS(ms); }

#endif // OS_WRAPPERS_H
