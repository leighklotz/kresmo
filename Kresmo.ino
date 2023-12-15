// Kresmo - Leigh Klotz WA5ZNU
// - Kresmo is from Kresmos in Greek: χρησμός are the ambiguous answers given by an Oracle
// - I use OpenAI-compatible API from Oobabooga Text Generation WebUI, witbout auth in this example
// - LLM is used in completion mode
// - JSON parsing is eliminated in favor or text processing; check your API response for suitability.

#include <Arduino.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
#define SDA_PIN 5
#define SCL_PIN 6

#include <WiFi.h>
#include "config.h"

static const char PROGRAM_NAME[] = "KRESMO";
static const char PROGRAM_NAME_GREEK[] = "χρησμός";

#define PROMPT "Here is a random and brief pithy saying: \\\""

#define TITLE_FONT u8g2_font_10x20_t_greek
#define MAIN_FONT u8g2_font_6x13_tf
#define MAIN_FONT_WIDTH (6)
#define MAIN_FONT_HEIGHT (12)
#define BUFFER_SIZE (6 * 13)

static const int MAX_RESPONSE_DISPLAY_SIZE = 512;
static const char JSON_PREFIX[] = "\"results\": [{\"text\": \"";
static const char JSON_SUFFIX[] = "}]}";

// EastRising 0.42" OLED
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
uint8_t u8log_buffer[BUFFER_SIZE];
U8G2LOG u8g2log;

WiFiClient client;
String response_text;
int max_chars_per_screen = 0;

const char body[] = "{"
                    "\"prompt\": \"" /**/ PROMPT /**/ "\","
                    "\"max_new_tokens\":128,"
                    "\"do_sample\":true,"
                    "\"temperature\":0.7,"
                    "\"top_p\":0.1,"
                    "\"typical_p\":1,"
                    "\"epsilon_cutoff\":0,"
                    "\"eta_cutoff\":0,"
                    "\"tfs\":1,"
                    "\"top_a\":0,"
                    "\"repetition_penalty\":1.18,"
                    "\"mirostat_mode\":1,"
                    "\"mirostat_tau\":5,"
                    "\"mirostat_eta\":0.1,"
                    "\"seed\":-1,"
                    "\"add_bos_token\":true,"
                    "\"truncation_length\":2048,"
                    "\"ban_eos_token\":false,"
                    "\"skip_special_tokens\":true"
                    "}";

void show_title(const char *title) {
  u8g2.clearBuffer();
  drawGreekKeyBorder();
  u8g2.setFont(TITLE_FONT);
  int center_line_y = (u8g2.getDisplayHeight() + u8g2.getMaxCharHeight() / 2) / 2;
  int left_margin = (u8g2.getDisplayWidth() - u8g2.getUTF8Width(title)) / 2;
  u8g2log.setLineHeightOffset(-2);
  u8g2.drawUTF8(left_margin, center_line_y, title);
  u8g2.sendBuffer();
  delay(1000);
}

void setup() {
  Serial.begin(460800);
  Serial.println("χρησμός - WA5ZNU 3");
  Wire.begin(SDA_PIN, SCL_PIN);

  u8g2.begin();
  u8g2.clearBuffer();
  int cols = u8g2.getDisplayWidth() / MAIN_FONT_WIDTH;
  int rows = u8g2.getDisplayHeight() / MAIN_FONT_HEIGHT;
  max_chars_per_screen = rows * cols;
  Serial.printf("u8g2log.begin(cols=%d, rows=%d, chars=%d)\n", cols, rows, max_chars_per_screen);
  u8g2log.begin(u8g2, cols, rows, u8log_buffer);
  u8g2log.setRedrawMode(1);  // 0: Update screen with newline, 1: Update screen for every char

  show_title(PROGRAM_NAME);
  show_title(PROGRAM_NAME_GREEK);
  u8g2.setFont(MAIN_FONT);

  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  printWifiStatus();
  u8g2.clear();
}

void makeRequest() {
  Serial.println("\nStarting connection to server");
  response_text.clear();
  if (client.connect(server, port)) {
    Serial.printf("connected to server http://%s/%s", server, path);
    client.printf("POST %s HTTP/1.1\r\n", path);
    client.printf("Host: %s\r\n", server);
    client.printf("Content-Length: %d\r\n", strlen(body));
    client.printf("Accept: text/plain\r\n");
    client.printf("Connection: close\r\n");
    client.println();
    client.println(body);
    Serial.println(body);
    client.flush();
  }
}

// The loop is a state machine
int last_state = -1;
int redisplays = 0;
enum { START = 0,
       MAKE_REQUEST = 1,
       ACCEPT_RESPONSE = 2,
       READ_HEADER = 3,
       READ_BODY = 4,
       DISPLAY_BODY = 5 } STATE = START;

void loop() {
  if (STATE != last_state) {
    Serial.printf("* STATE is %d\n", STATE);
    last_state = STATE;
  }
  switch (STATE) {
    case START:
    case MAKE_REQUEST:
      makeRequest();
      STATE = ACCEPT_RESPONSE;
      break;

    case ACCEPT_RESPONSE:
      if (client.available()) {
        STATE = READ_HEADER;
      }
      break;

    case READ_HEADER:
      if (client.available()) {
        while (client.available()) {
          String line = client.readStringUntil('\n');
          if (line == "\r") {
            Serial.println("-----");
            STATE = READ_BODY;
            break;
          }
          Serial.println(line);
        }
      }
      break;

    case READ_BODY:
      while (client.available()) {
        String line = client.readStringUntil('\n');
        line.replace("\\n", "\n");
        response_text += line;
      }
      if (response_text.length() > 0) {
        STATE = DISPLAY_BODY;
      }
      break;

    case DISPLAY_BODY:
      if (redisplays++ < 3) {
        String answer = response_text.substring(response_text.indexOf(JSON_PREFIX) + strlen(JSON_PREFIX),
                                                response_text.indexOf(JSON_SUFFIX));
        Serial.printf("Display body round %d\n", redisplays);
        Serial.println(answer);
        u8g2.clear();
        u8g2.sendBuffer();
        u8g2.setFont(MAIN_FONT);
        {
          Serial.printf("max_chars_per_screen=%d\n", max_chars_per_screen);
          int screen_start = 0;
          while (screen_start < answer.length()) {
            int screen_end = screen_start + max_chars_per_screen - 1;
            Serial.printf("answer[0:%d].substring(screen_start, screen_end)=", answer.length(), screen_start, screen_end);
            String line = answer.substring(screen_start, screen_end);
            line.replace("\n", " ");
            line.replace("\\", "");
            Serial.printf("'%s'\n", line.c_str());
            u8g2log.print(line);
            Serial.printf("screen_start=%d screen_end=%d\n", screen_start, screen_end);
            screen_start = screen_end;
            delay(4000);
          }
        }
        Serial.printf("DISPLAY_BODY round %d done\n", redisplays);
      } else {
        STATE = START;
        redisplays = 0;
      }
      break;
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.printf("SSID: %s\n", WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.printf("IP Address: %s\n", ip.toString());

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.printf("signal strength (RSSI): %l dBm\n", rssi);
}

// Draw a simple border around the display to represent the Greek Key design
void drawGreekKeyBorder() {
  int w = u8g2.getDisplayWidth();
  int h = u8g2.getDisplayHeight();
  for (int i = 0; i < w; i += 4) {  // Top and bottom
    u8g2.drawPixel(i + 0, 0);
    u8g2.drawPixel(i + 1, 1);
    u8g2.drawPixel(i + 2, 1);
    u8g2.drawPixel(i + 3, 0);

    u8g2.drawPixel(i + 0, h - 1);
    u8g2.drawPixel(i + 1, h - 2);
    u8g2.drawPixel(i + 2, h - 2);
    u8g2.drawPixel(i + 3, h - 1);
  }
  for (int i = 0; i < h; i += 4) {  // Left and right
    u8g2.drawPixel(1, i + 0);
    u8g2.drawPixel(0, i + 1);
    u8g2.drawPixel(0, i + 2);
    u8g2.drawPixel(1, i + 3);

    u8g2.drawPixel(w - 1, i + 0);
    u8g2.drawPixel(w - 2, i + 1);
    u8g2.drawPixel(w - 2, i + 2);
    u8g2.drawPixel(w - 1, i + 3);
  }
}
