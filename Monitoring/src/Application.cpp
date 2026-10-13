#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "Application.h"
#include "Sensors.h"
#include "Prediction.h"
#include "Controller.h"
#include "RelayDriver.h"
#include "WebServerModule.h"
namespace {
Sensors sensors;
History history;
SensorSnapshot latest;
Controller controller;
RelayDriver relays;
WebServerModule website;
SemaphoreHandle_t mutex = nullptr;
bool ready = false;
ApplicationStatus status() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  const auto value = controller.status();
  xSemaphoreGive(mutex); return value;
}
ControlReply command(ControlAction action) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  const auto reply = controller.command(action, millis());
  // Apply authoritative command state immediately, including mode-change OFF.
  relays.apply(controller.status().commanded);
  xSemaphoreGive(mutex); return reply;
}
void safetyTask(void *) {
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    relays.apply(controller.tick(millis()));
    xSemaphoreGive(mutex);
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(Config::SAFETY_TICK_MS));
  }
}
}
void Application::begin() {
  // Driver must initialize verified outputs OFF before application services.
  relays.begin();
  Serial.begin(115200);
  mutex = xSemaphoreCreateMutex();
  if (!mutex) { Serial.println("Mutex allocation failed; services stopped, outputs off."); return; }
  if (xTaskCreatePinnedToCore(safetyTask, "control-safety", 4096, nullptr, 2, nullptr, 0) != pdPASS) {
    Serial.println("Safety task creation failed; services stopped, outputs off."); return;
  }
  sensors.begin();
  website.begin(history, latest, status, command);
  ready = true;
  Serial.println("MONITORING READY | manual / fermentation | synthetic demo RF | relay configuration checked");
}
void Application::tick() {
  if (!ready) { delay(20); return; }
  SensorSnapshot completed;
  if (sensors.tick(completed)) {
    // Inference is OUTSIDE the safety mutex so a future slow model cannot block runtime checks.
    xSemaphoreTake(mutex, portMAX_DELAY);
    const ControlContext context = controller.context(millis());
    xSemaphoreGive(mutex);
    const PredictionResult prediction = predictActuators(completed, context);
    latest = completed; history.append(completed);
    xSemaphoreTake(mutex, portMAX_DELAY);
    controller.updateSample(completed, prediction);
    xSemaphoreGive(mutex);
    Serial.printf("Sample %lu completed | distance=%s EC=%s pH=%s CO2=%s moisture=%s temperature=%s\n",
      (unsigned long)completed.sequence, statusName(completed.distance.status), statusName(completed.ec.status),
      statusName(completed.ph.status), statusName(completed.co2.status), statusName(completed.moisture.status), statusName(completed.temperature.status));
  }
  website.tick();
  delay(1);
}
