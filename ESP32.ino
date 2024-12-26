#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Cấu hình WiFi
const char* ssid = "ChuHieu";
const char* password = "03032003";

// Cấu hình HiveMQ với SSL
const char* mqtt_server = "a8b2eda48398497e9e05534a941a1d4b.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;  // Cổng SSL mặc định
const char* mqtt_user = "chuhieu";  // Tài khoản HiveMQ
const char* mqtt_pass = "123";      // Mật khẩu HiveMQ
const char* result_topic = "esp32cam/result";
const char* control_light_topic = "iotapp/control_light";
const char* control_door_topic = "iotapp/control_door";

WiFiClientSecure espClient;
PubSubClient client(espClient);
const int bufferSize = 1024 * 23;

const int servoPin = 18;
const int redPin = 21;
const int bluePin = 19;
const int whitePin1 = 25;
const int whitePin2 = 26;
const int whitePin3 = 27;

const int soundSensorPin = 34; // Chân kết nối cảm biến âm thanh
const int soundThreshold = 2000;

bool soundState = false;

Servo servo;

int current_angle = 50;
int status = 0;

void setup() {
  Serial.begin(115200);

  // Kết nối WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Đang kết nối WiFi...");
  }
  Serial.println("WiFi đã kết nối!");

  // Cấu hình MQTT
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  // Kết nối MQTT
  while (!client.connected()) {
    Serial.println("Đang kết nối MQTT...");
    if (client.connect("ESP32-CAM", mqtt_user, mqtt_pass)) {
      Serial.println("Đã kết nối MQTT!");
    } else {
      Serial.print("Kết nối MQTT thất bại, mã lỗi: ");
      Serial.println(client.state());
      delay(2000);
    }
  }

  // Đăng ký topic nhận status
  client.subscribe(result_topic);
  client.subscribe(control_light_topic);
  client.subscribe(control_door_topic);

  // Cấu hình servo
  servo.attach(servoPin, 500, 2400);

  // Cấu hình LED
  pinMode(redPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  pinMode(whitePin1, OUTPUT);
  pinMode(whitePin2, OUTPUT);
  pinMode(whitePin3, OUTPUT);
  digitalWrite(redPin, LOW);
  digitalWrite(bluePin, LOW);
  digitalWrite(whitePin1, LOW);
  digitalWrite(whitePin2, LOW);
  digitalWrite(whitePin3, LOW);

  // Cấu hình cảm biến âm thanh
  pinMode(soundSensorPin, INPUT);

}

void loop() {
  // Duy trì kết nối MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Đọc giá trị từ cảm biến âm thanh
  int soundValue = analogRead(soundSensorPin);
  Serial.print("Giá trị âm thanh: ");
  Serial.println(soundValue);

  // Kiểm tra nếu âm thanh vượt ngưỡng
  if (soundValue > soundThreshold && !soundState) {
    soundState = true;
    // Bật 3 LED trắng
    digitalWrite(whitePin1, HIGH);
    digitalWrite(whitePin2, HIGH);
    digitalWrite(whitePin3, HIGH);
    Serial.println("Âm thanh vượt ngưỡng, bật đèn trắng.");
    delay(3000);
    digitalWrite(whitePin1, LOW);
    digitalWrite(whitePin2, LOW);
    digitalWrite(whitePin3, LOW);
    soundState = false;
  }

  // Điều khiển LED và servo dựa trên status
  if (status == 1) {
    // Bật LED xanh
    digitalWrite(bluePin, HIGH);
    digitalWrite(redPin, LOW);

    // Mở cửa
    for (current_angle = 50; current_angle <= 140; current_angle += 5) {
      servo.write(current_angle);
      delay(15);
    }
    // Giữ cửa 3s rồi đóng
    delay(3000);
    for (current_angle = 140; current_angle >= 50; current_angle -= 5) {
      servo.write(current_angle);
      delay(15);
    }

    // Reset trạng thái sau khi xử lý
    status = 0;
  } else {
    // Bật LED đỏ
    digitalWrite(redPin, HIGH);
    digitalWrite(bluePin, LOW);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Nhận được tin nhắn từ topic: ");
  Serial.println(topic);

  // Chuyển payload thành chuỗi
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Nội dung: ");
  Serial.println(message);

  // Kiểm tra nếu topic là "esp32cam/result"
  if (String(topic) == result_topic) {
    status = message[message.length() - 2] - '0';
    Serial.print("Cập nhật status: ");
    Serial.println(status);
  }

  // Kiểm tra nếu topic là "iotapp/control_light"
  if (String(topic) == control_light_topic) {
    if (message == "1") {
      // Bật 3 đèn trắng
      digitalWrite(whitePin1, HIGH);
      digitalWrite(whitePin2, HIGH);
      digitalWrite(whitePin3, HIGH);
      Serial.println("Bật đèn trắng.");
    } else if (message == "0") {
      // Tắt 3 đèn trắng
      digitalWrite(whitePin1, LOW);
      digitalWrite(whitePin2, LOW);
      digitalWrite(whitePin3, LOW);
      Serial.println("Tắt đèn trắng.");
    }
  }

  if (String(topic) == control_door_topic) {
    if (message == "1") {
      // Mở cửa
      for (current_angle = 50; current_angle <= 140; current_angle += 5) {
        servo.write(current_angle);
        delay(15);
      }
    } else {
      for (current_angle = 140; current_angle >= 50; current_angle -= 5) {
        servo.write(current_angle);
        delay(15);
      }
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Mất kết nối MQTT, đang thử lại...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_pass)) {
      Serial.println("Đã kết nối lại MQTT!");
      client.subscribe(result_topic);
      client.subscribe(control_light_topic);
      client.subscribe(control_door_topic);
    } else {
      Serial.print("Thử lại sau 5 giây, mã lỗi: ");
      Serial.println(client.state());
      delay(5000);
    }
  }
}
