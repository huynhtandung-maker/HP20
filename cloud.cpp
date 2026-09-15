#include "cloud.h"
#include "thingsboard_ca.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
static QueueHandle_t jobs = nullptr, results = nullptr;
static void worker(void*) {
  auto* job = new CloudJob;
  for (;;) {
    if (xQueueReceive(jobs, job, portMAX_DELAY) != pdTRUE) continue;
    WiFiClientSecure client; client.setCACert(job->ca);
    client.setHandshakeTimeout(8);
    HTTPClient http; http.setConnectTimeout(5000); http.setTimeout(5000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    String url = "https://" + String(job->host) + "/api/v1/" + job->token + "/telemetry";
    int code = -1;
    if (http.begin(client, url)) {
      http.addHeader("Content-Type", "application/json");
      code = http.POST(reinterpret_cast<uint8_t*>(job->payload), strlen(job->payload));
      http.end();
    }
    memset(job, 0, sizeof(*job));
    xQueueOverwrite(results, &code);
  }
}
bool cloudBegin() {
  jobs = xQueueCreate(1, sizeof(CloudJob)); results = xQueueCreate(1, sizeof(int));
  return jobs && results && xTaskCreate(worker, "cloud", 12288, nullptr, 1, nullptr) == pdPASS;
}
bool cloudSubmit(const Config& c, const String& payload) {
  if (!jobs || payload.length() >= 256) return false;
  auto* j = new CloudJob{};
  strlcpy(j->host, c.host.c_str(), sizeof(j->host));
  strlcpy(j->token, c.token.c_str(), sizeof(j->token));
  const char* ca = hp20::tbtrust::effectiveCa(c);
  if (!ca) { memset(j, 0, sizeof(*j)); delete j; return false; }
  strlcpy(j->ca, ca, sizeof(j->ca));
  strlcpy(j->payload, payload.c_str(), sizeof(j->payload));
  bool ok = xQueueSend(jobs, j, 0) == pdTRUE;
  memset(j, 0, sizeof(*j)); delete j; return ok;
}
bool cloudResult(int& code) { return results && xQueueReceive(results, &code, 0) == pdTRUE; }
