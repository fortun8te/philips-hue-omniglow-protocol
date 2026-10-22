#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "driver/rmt.h"
#include "freertos/ringbuf.h"

// OmniGlow: DATA IN via levelshifter on GPIO35; replay/output on GPIO26.
constexpr gpio_num_t DATA_IN = GPIO_NUM_35;
constexpr gpio_num_t DATA_OUT = GPIO_NUM_26;
constexpr rmt_channel_t RX_CH = RMT_CHANNEL_0;
constexpr rmt_channel_t TX_CH = RMT_CHANNEL_1;
constexpr uint16_t FRAME_BITS = 6448;
constexpr uint16_t FRAME_BYTES = 806;
constexpr uint16_t BODY_BYTES = 800;
constexpr uint16_t IDLE_TICKS = 20000;   // 1 ms at 20 MHz
constexpr uint16_t FILTER_TICKS = 10;   // 500 ns at 20 MHz

// Vul lokaal in; commit nooit echte WiFi-gegevens naar GitHub.
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

WebServer web(80);
RingbufHandle_t ringBuffer = nullptr;
uint8_t capture[FRAME_BYTES];
uint8_t lastFrame[FRAME_BYTES];
uint16_t bitCount = 0, byteCount = 0;
uint8_t currentByte = 0;
uint32_t frameNumber = 0;
bool haveFrame = false;
static rmt_item32_t txItems[FRAME_BITS];

constexpr uint8_t MAX_RESULTS = 20;
struct StoredCapture {
  char label[32];
  uint8_t data[FRAME_BYTES];
};
StoredCapture results[MAX_RESULTS];
uint8_t resultCount = 0;
bool captureArmed = false;
bool resultJustSaved = false;
char pendingLabel[32] = "";
String captureMessage = "";

const char *testSequence[] = {
  "off", "on_white_100", "red_10", "red_20", "red_50", "red_75", "red_100",
  "green_100", "blue_100", "warmwhite_10", "warmwhite_50", "warmwhite_100",
  "coolwhite_10", "coolwhite_50", "coolwhite_100", "temperature_2700K",
  "temperature_4000K", "temperature_6500K"
};
constexpr uint8_t TEST_COUNT = sizeof(testSequence) / sizeof(testSequence[0]);
uint8_t currentTest = 0;

void resetCapture() {
  bitCount = 0; byteCount = 0; currentByte = 0;
  memset(capture, 0, sizeof(capture));
}

void addBit(uint8_t bit) {
  if (bitCount >= FRAME_BITS) return;
  currentByte = (currentByte << 1) | bit;
  bitCount++;
  if ((bitCount & 7) == 0) {
    if (byteCount < FRAME_BYTES) capture[byteCount++] = currentByte;
    currentByte = 0;
  }
}

void processItem(const rmt_item32_t &item) {
  static uint16_t n = 0;

  if (n < 20) {
    Serial.printf(
      "L0=%u D0=%u | L1=%u D1=%u\n",
      item.level0,
      item.duration0,
      item.level1,
      item.duration1
    );
    n++;
  }

  constexpr uint16_t longHigh = 12;

  if (item.level0 == 1 && item.duration0) {
    addBit(item.duration0 >= longHigh);
  }

  if (item.level1 == 1 && item.duration1) {
    addBit(item.duration1 >= longHigh);
  }
}

//void setupRx() {
//  rmt_config_t c = RMT_DEFAULT_CONFIG_RX(DATA_IN, RX_CH);
//  c.clk_div = 4;
//  c.rx_config.filter_en = true;
//  c.rx_config.filter_ticks_thresh = FILTER_TICKS;
//  c.rx_config.idle_threshold = IDLE_TICKS;
//  if (rmt_config(&c) != ESP_OK || rmt_driver_install(RX_CH, 8192, 0) != ESP_OK) {
//    Serial.println("RMT RX fout"); while (true) delay(1000);
//  }
//  rmt_get_ringbuf_handle(RX_CH, &ringBuffer);
//  rmt_rx_start(RX_CH, true);
//}

void setupRx() {
  rmt_config_t c = RMT_DEFAULT_CONFIG_RX(DATA_IN, RX_CH);

  c.clk_div = 4;  // 20 MHz, 50 ns per tick

  // Belangrijk: korte OmniGlow-LOW-pulsen niet wegfilteren
  c.rx_config.filter_en = false;

  c.rx_config.idle_threshold = IDLE_TICKS;

  if (rmt_config(&c) != ESP_OK ||
      rmt_driver_install(RX_CH, 8192, 0) != ESP_OK) {
    Serial.println("RMT RX fout");
    while (true) {
      delay(1000);
    }
  }

  rmt_get_ringbuf_handle(RX_CH, &ringBuffer);
  rmt_rx_start(RX_CH, true);
}
void setupTx() {
  rmt_config_t c = RMT_DEFAULT_CONFIG_TX(DATA_OUT, TX_CH);
  c.clk_div = 4;
  c.tx_config.loop_en = false;
  c.tx_config.carrier_en = false;
  c.tx_config.idle_output_en = true;
  c.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  if (rmt_config(&c) != ESP_OK || rmt_driver_install(TX_CH, 0, 0) != ESP_OK) {
    Serial.println("RMT TX fout"); while (true) delay(1000);
  }
}

String hexData(const uint8_t *data, uint16_t len) {
  String s; s.reserve(len * 3 + 100);
  for (uint16_t i = 0; i < len; i++) {
    if (data[i] < 16) s += '0';
    s += String(data[i], HEX); s += (i + 1 == len ? "" : " ");
    if ((i + 1) % 32 == 0) s += "<br>";
  }
  s.toUpperCase(); return s;
}

String footer() {
  if (!haveFrame) return "geen frame";
  return hexData(lastFrame + BODY_BYTES, 6);
}

String storedResultsText() {
  String out;
  out.reserve((size_t)resultCount * 2500 + 500);
  out += "OmniGlow captures\n";
  out += "Aantal resultaten: " + String(resultCount) + "\n\n";
  for (uint8_t r = 0; r < resultCount; r++) {
    out += "LABEL: ";
    out += results[r].label;
    out += "\nBYTES: ";
    for (uint16_t i = 0; i < FRAME_BYTES; i++) {
      if (results[r].data[i] < 16) out += '0';
      out += String(results[r].data[i], HEX);
      if (i + 1 < FRAME_BYTES) out += ' ';
    }
    out.toUpperCase();
    out += "\n\n";
  }
  return out;
}

void printStoredResult(uint8_t index) {
  Serial.println();
  Serial.print("CAPTURE ");
  Serial.print(index + 1);
  Serial.print(" LABEL: ");
  Serial.println(results[index].label);
  Serial.print("BYTES: ");
  for (uint16_t i = 0; i < FRAME_BYTES; i++) {
    if (results[index].data[i] < 16) Serial.print('0');
    Serial.print(results[index].data[i], HEX);
    if (i + 1 < FRAME_BYTES) Serial.print(' ');
  }
  Serial.println();
}

void rootPage() {
  String suggested = currentTest < TEST_COUNT ? testSequence[currentTest] : "extra_capture";
  String p = "<!doctype html><html><head><meta charset='utf-8'>";
  p += "<style>body{font-family:monospace;background:#111;color:#eee;margin:20px}pre{white-space:pre-wrap;color:#9f9;word-break:break-all}button{font-size:18px;padding:8px}input{font-size:18px;padding:8px;width:220px}</style></head><body>";
  p += "<h1>OmniGlow analyzer</h1><p>DATA IN: GPIO35 &nbsp; DATA OUT: GPIO26</p>";
  p += "<p>IP: " + WiFi.localIP().toString() + "<br>Frames: " + String(frameNumber);
  p += haveFrame ? "<br>Status: volledig frame" : "<br>Status: wachten";
  p += "<br>Opgeslagen captures: " + String(resultCount) + "/" + String(MAX_RESULTS) + "</p>";
  p += "<h2>Test " + String(currentTest + 1) + "/" + String(TEST_COUNT) + ": " + suggested + "</h2>";
  p += "<p>Stel deze waarde in de Hue-app in en klik daarna op CAPTURE.</p>";
  if (captureArmed) p += "<p id='status'><b>Capture actief voor label: " + String(pendingLabel) + "</b></p>";
  else p += "<p id='status'><b>" + (captureMessage.length() ? captureMessage : "Klaar voor de volgende test") + "</b></p>";
  p += "<form action='/capture' method='get'><label>Label: </label><input id='label' name='label' value='" + suggested + "' maxlength='31'> <button type='submit'>CAPTURE</button></form>";
  p += "<p>De labelnaam mag je aanpassen voordat je op CAPTURE klikt.</p>";
  p += "<p><a href='/clear'><button>Alle captures wissen</button></a> <a href='/results'><button>Alle resultaten</button></a> <a href='/replay'><button>Replay via GPIO26</button></a></p>";
  p += "<h2>Footer</h2><pre>" + footer() + "</pre><h2>806 bytes</h2><pre>";
  p += haveFrame ? hexData(lastFrame, FRAME_BYTES) : "Nog geen volledig frame.";
  p += "</pre><script>setInterval(()=>fetch('/status').then(r=>r.json()).then(x=>{document.getElementById('status').innerHTML='<b>'+x.message+'</b>'; if(x.saved) location.reload();}),1000);</script></body></html>";
  web.send(200, "text/html; charset=utf-8", p);
}

void clearPage() {
  haveFrame = false; resultCount = 0; currentTest = 0; captureArmed = false; captureMessage = ""; resetCapture();
  web.sendHeader("Location", "/"); web.send(303);
}

void statusPage() {
  String message = captureArmed ? "Capture actief voor label: " + String(pendingLabel) :
                   (captureMessage.length() ? captureMessage : "Klaar voor de volgende test");
  bool saved = resultJustSaved;
  resultJustSaved = false;
  String json = "{\"message\":\"" + message + "\",\"saved\":" + (saved ? "true" : "false") +
                ",\"test\":" + String(currentTest) + ",\"results\":" + String(resultCount) + "}";
  web.send(200, "application/json", json);
}

void startCapture() {
  String label = web.arg("label");
  label.trim();
  if (!label.length()) label = "onbenoemd";
  if (resultCount >= MAX_RESULTS) {
    web.send(409, "text/plain", "Maximum aantal captures bereikt");
    return;
  }
  label.toCharArray(pendingLabel, sizeof(pendingLabel));
  captureArmed = true;
  resultJustSaved = false;
  captureMessage = "Wachten op het volgende volledige frame";
  Serial.print("Capture aangevraagd: "); Serial.println(pendingLabel);
  web.sendHeader("Location", "/"); web.send(303);
}

void resultsPage() {
  String text = storedResultsText();
  web.send(200, "text/plain; charset=utf-8", text);
}

void replayFrame() {
  if (!haveFrame) {
    web.send(409, "text/plain", "Nog geen volledig frame beschikbaar");
    return;
  }
  for (uint16_t i = 0; i < FRAME_BYTES; i++) {
    for (int8_t bit = 7; bit >= 0; bit--) {
      uint16_t n = i * 8 + (7 - bit);
      bool one = (lastFrame[i] >> bit) & 1;
      uint16_t high = one ? 15 : 4;
      txItems[n].duration0 = high;
      txItems[n].level0 = 1;
      txItems[n].duration1 = 20 - high;
      txItems[n].level1 = 0;
    }
  }
  rmt_write_items(TX_CH, txItems, FRAME_BITS, true);
  web.send(200, "text/plain", "Frame naar GPIO26 verzonden");
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Verbinden met WiFi");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi-verbinding mislukt; controleer SSID/wachtwoord.");
    return;
  }

  web.on("/", rootPage);
  web.on("/clear", clearPage);
  web.on("/replay", replayFrame);
  web.on("/capture", startCapture);
  web.on("/results", resultsPage);
  web.on("/status", statusPage);
  web.begin();

  Serial.print("Analyzer beschikbaar op http://");
  Serial.print(WiFi.localIP());
  Serial.println('/');
}

void setup() {
  Serial.begin(921600); delay(500);
  Serial.println("OmniGlow ESP32 RMT analyzer + WiFi");
  Serial.println("DATA IN GPIO35 | DATA OUT GPIO26");
  resetCapture(); 
  pinMode(35, INPUT);
  gpio_set_pull_mode(GPIO_NUM_35, GPIO_FLOATING);
  setupRx(); setupTx();
  setupWiFi();
}

void loop() {
  web.handleClient();
  size_t size = 0;
  rmt_item32_t *items = (rmt_item32_t *)xRingbufferReceive(ringBuffer, &size, pdMS_TO_TICKS(5));
  if (!items) return;
  size_t count = size / sizeof(rmt_item32_t);
  for (size_t i = 0; i < count; i++) {
    if (!items[i].duration0 && !items[i].duration1) continue;
    processItem(items[i]);
    if (bitCount >= FRAME_BITS) {
      if (byteCount == FRAME_BYTES) {
        memcpy(lastFrame, capture, FRAME_BYTES);
        haveFrame = true; frameNumber++;
        Serial.printf("Frame #%lu ontvangen, 806 bytes\n", (unsigned long)frameNumber);
        if (captureArmed && resultCount < MAX_RESULTS) {
          memcpy(results[resultCount].data, capture, FRAME_BYTES);
          strncpy(results[resultCount].label, pendingLabel, sizeof(results[resultCount].label) - 1);
          results[resultCount].label[sizeof(results[resultCount].label) - 1] = '\0';
          printStoredResult(resultCount);
          resultCount++;
          captureArmed = false;
          resultJustSaved = true;
          captureMessage = "Capture opgeslagen: " + String(pendingLabel);
          if (currentTest < TEST_COUNT) currentTest++;
        }
      }
      resetCapture();
    }
  }
  vRingbufferReturnItem(ringBuffer, (uint8_t *)items);
}
