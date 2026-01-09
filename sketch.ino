#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// RTOS Handles
QueueHandle_t commandQueue;
TaskHandle_t t_Input;
TaskHandle_t t_Display;
TaskHandle_t t_Blink;

// Shared Global Variable
int currentDelay = 1000; // Default 1 second blink

// Data Structure for the Queue
struct Command {
  char text[20];
  int blinkRate;
};

// ==========================================
// TASK 1: THE HEART (Blink LED)
// ==========================================
void HeartTask(void *pvParameters) {
  pinMode(2, OUTPUT);

  for (;;) {
    digitalWrite(2, HIGH);
    vTaskDelay(100 / portTICK_PERIOD_MS);   // LED ON for 100 ms

    digitalWrite(2, LOW);
    vTaskDelay(currentDelay / portTICK_PERIOD_MS); // Blink interval
  }
}

// ==========================================
// TASK 2: THE EAR (Serial Input)
// ==========================================
void InputTask(void *pvParameters) {
  Serial.begin(115200);

  for (;;) {
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      input.trim();

      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, input);

      if (!error) {
        Command cmd;

        // Extract data from JSON
        strlcpy(cmd.text, doc["msg"] | "NULL", sizeof(cmd.text));
        cmd.blinkRate = doc["delay"] | 1000;

        // Send to Queue
        xQueueSend(commandQueue, &cmd, portMAX_DELAY);

        Serial.println("Command sent to queue!");
      } else {
        Serial.println("JSON Error");
      }
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// ==========================================
// TASK 3: THE FACE (OLED Display)
// ==========================================
void DisplayTask(void *pvParameters) {
  Command receivedCmd;

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }

  // Initial Screen
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println("Waiting...");
  display.display();

  for (;;) {
    // Wait indefinitely for data from Queue
    if (xQueueReceive(commandQueue, &receivedCmd, portMAX_DELAY)) {

      // Update global variable for HeartTask
      currentDelay = receivedCmd.blinkRate;

      // Update OLED
      display.clearDisplay();
      display.setCursor(0, 20);
      display.println(receivedCmd.text);
      display.display();

      Serial.println("Screen Updated.");
    }
  }
}

void setup() {
  // Create Queue
  commandQueue = xQueueCreate(5, sizeof(Command));

  // Create Tasks
  xTaskCreate(InputTask, "Input Task", 4096, NULL, 2, &t_Input);
  xTaskCreate(DisplayTask, "Display Task", 4096, NULL, 1, &t_Display);
  xTaskCreate(HeartTask, "Heart Task", 2048, NULL, 3, &t_Blink);
}

void loop() {}
