#include "../shared/PeakDetector.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WASM_EXPORT
#endif

struct PeakDetectorHandle {
  PeakDetector detector;
  PeakDetectorResult last;
};

extern "C" {

WASM_EXPORT PeakDetectorHandle *pd_create() {
  return new PeakDetectorHandle();
}

WASM_EXPORT void pd_destroy(PeakDetectorHandle *handle) {
  delete handle;
}

WASM_EXPORT void pd_configure(PeakDetectorHandle *handle,
                              int movingAverageMeasurements,
                              int emaAlphaPercent,
                              int peakArmRiseDb,
                              int peakDropDb,
                              int requiredDropReadings,
                              int learnMode) {
  if (!handle) return;
  PeakDetectorConfig config = {
    (uint8_t)movingAverageMeasurements,
    (uint8_t)emaAlphaPercent,
    (uint8_t)peakArmRiseDb,
    (uint8_t)peakDropDb,
    (uint8_t)requiredDropReadings,
    learnMode != 0
  };
  handle->detector.configure(config);
}

WASM_EXPORT void pd_reset(PeakDetectorHandle *handle, uint32_t startMs) {
  if (!handle) return;
  handle->detector.reset(startMs);
}

WASM_EXPORT int pd_process(PeakDetectorHandle *handle, uint32_t timeMs, int rawRssi) {
  if (!handle) return 0;
  handle->last = handle->detector.process(timeMs, rawRssi);
  return handle->last.peakDetected ? 1 : 0;
}

WASM_EXPORT int pd_has_rssi(PeakDetectorHandle *handle) {
  return handle && handle->last.hasRssi ? 1 : 0;
}

WASM_EXPORT int pd_peak_detected(PeakDetectorHandle *handle) {
  return handle && handle->last.peakDetected ? 1 : 0;
}

WASM_EXPORT int pd_raw_rssi(PeakDetectorHandle *handle) {
  return handle ? handle->last.rawRssi : 0;
}

WASM_EXPORT float pd_sma_rssi(PeakDetectorHandle *handle) {
  return handle ? handle->last.smaRssi : 0.0f;
}

WASM_EXPORT float pd_smooth_rssi(PeakDetectorHandle *handle) {
  return handle ? handle->last.smoothRssi : 0.0f;
}

WASM_EXPORT float pd_baseline_rssi(PeakDetectorHandle *handle) {
  return handle ? handle->last.baselineRssi : 0.0f;
}

WASM_EXPORT float pd_current_peak_rssi(PeakDetectorHandle *handle) {
  return handle ? handle->last.currentPeakRssi : 0.0f;
}

WASM_EXPORT uint32_t pd_current_peak_ms(PeakDetectorHandle *handle) {
  return handle ? handle->last.currentPeakMs : 0;
}

WASM_EXPORT int pd_drop_readings(PeakDetectorHandle *handle) {
  return handle ? handle->last.dropReadings : 0;
}

WASM_EXPORT int pd_peak_armed(PeakDetectorHandle *handle) {
  return handle && handle->last.peakArmed ? 1 : 0;
}

}
