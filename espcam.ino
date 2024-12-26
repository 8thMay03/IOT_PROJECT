#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <base64.h>
#include "esp_camera.h"
#include <ArduinoJson.h>

// Cấu hình WiFi
const char* ssid = "ChuHieu";
const char* password = "03032003";

// Cấu hình HiveMQ với SSL
const char* mqtt_server = "a8b2eda48398497e9e05534a941a1d4b.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;  // Cổng SSL mặc định
const char* mqtt_user = "chuhieu";  // Tài khoản HiveMQ
const char* mqtt_pass = "123";      // Mật khẩu HiveMQ
// const char* mqtt_server = "localhost";
// const int mqtt_port = 1883;  // Cổng SSL mặc định
// const char* mqtt_user = "";  // Tài khoản HiveMQ
// const char* mqtt_pass = "";      // Mật khẩu HiveMQ
const char* mqtt_topic = "iot_app/image";  // Topic để gửi ảnh
const char* result_topic = "esp32cam/result";

WiFiClientSecure espClient;
PubSubClient client(espClient);
const int bufferSize = 1024 * 23;

// Cấu hình chân cho ESP32-CAM
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM    5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22
#define LED_GPIO_NUM 4  // Định nghĩa chân cho đèn flash, điều chỉnh nếu cần

unsigned long lastCaptureTime = 0;  // Lưu thời điểm gửi ảnh cuối cùng
bool faceDetected = false;  // Biến để đánh dấu khi nhận diện khuôn mặt thành công

void setup() {
  Serial.begin(115200);

  // Kết nối WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");

  // Thiết lập SSL
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);  // Đăng ký callback để nhận tin nhắn từ MQTT
  
  // Khởi động camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    delay(500);
    esp_camera_deinit();
    esp_camera_init(&config);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32-CAM", mqtt_user, mqtt_pass)) {
      client.setBufferSize(32768);
      Serial.println("connected");
      client.subscribe(result_topic);  // Subscribe vào topic phản hồi
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message received on topic ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, message);

  // Kiểm tra nếu có lỗi khi phân tích JSON
  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    return;
  }

  // Lấy giá trị của "status" từ JSON
  int status = doc["status"];  // 1 hoặc 0

  // Kiểm tra nếu message nhận diện thành công
  if (status == 1) {
    faceDetected = true;
    lastCaptureTime = millis();  // Đặt lại thời điểm nhận diện thành công
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentTime = millis();

  // Kiểm tra nếu đã nhận diện khuôn mặt và chờ 2 giây sau lần gửi ảnh cuối cùng
  if (faceDetected && (currentTime - lastCaptureTime >= 2000)) {
    // Reset biến nhận diện
    faceDetected = false;
  }

  if (!faceDetected && (currentTime - lastCaptureTime >= 200)) {  // Gửi ảnh mỗi 10 giây nếu không có nhận diện
    delay(100);

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      delay(1000);
      return;
    }

    if (fb != NULL) {
      String buffer = base64::encode(fb->buf, fb->len);
      String topic = String(mqtt_topic);
      bool status = client.publish(topic.c_str(), buffer.c_str(), true);
      if (status) {
          Serial.println("Image sent to topic: " + topic);
      }
      delay(1000);
    }
    esp_camera_fb_return(fb);
    ledcAttach(LED_GPIO_NUM, 5000, 0);
    lastCaptureTime = currentTime;  // Cập nhật thời gian gửi ảnh
  }
}