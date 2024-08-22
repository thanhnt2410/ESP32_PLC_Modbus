#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "HardwareSerial.h"
#include "DFRobot_RTU.h"

#define RXD2 16
#define TXD2 17
DFRobot_RTU Modbus_Master(/*s =*/&Serial2);


// Thông tin WiFi
const char* ssid = "Galvin";
const char* password = "12341234";

// Khởi tạo giá trị biến đếm sản phẩm và tốc độ động cơ
uint16_t productCount;  // Thay thế bằng giá trị thực tế
uint16_t motorSpeed;   // Thay thế bằng giá trị thực tế (đơn vị RPM)
uint16_t motorSpeedControl;
uint16_t oldValue;
// Khởi tạo server
AsyncWebServer server(80);

void setup() {
  // Kết nối WiFi
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Đang kết nối WiFi...");
  }
  Serial.println("Kết nối WiFi thành công!");
  Serial.println(WiFi.localIP());
  oldValue = 0;
  // Cấu hình server để trả về trang HTML
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    const char* html = R"rawliteral(
      <!DOCTYPE html>
      <html lang='vi'>
      <head>
        <meta charset='UTF-8'>
        <meta name='viewport' content='width=device-width, initial-scale=1.0'>
        <style>
          body { 
            font-family: 'Arial', sans-serif; 
            text-align: center; 
            padding: 50px; 
            background: linear-gradient(135deg, #FFDEE9, #B5FFFC); 
            color: #333333; 
            margin: 0;
          }
          h1 { 
            color: #666666; 
            margin-bottom: 40px; 
            font-size: 2.5em;
            text-transform: uppercase;
            letter-spacing: 2px;
          }
          .card { 
            background-color: #ffffff; 
            border-radius: 15px; 
            padding: 30px; 
            box-shadow: 0px 10px 20px rgba(0, 0, 0, 0.1); 
            margin-bottom: 20px; 
            width: 90%;
            max-width: 400px;
            transition: transform 0.3s ease;
          }
          .card:hover {
            transform: scale(1.05);
          }
          .card-title { 
            font-size: 24px; 
            color: #888888; 
            margin-bottom: 10px; 
            text-transform: uppercase;
            letter-spacing: 1px;
          }
          .card-value { 
            font-size: 36px; 
            color: #333333; 
            font-weight: bold; 
          }
          .card-subtitle { 
            font-size: 18px; 
            color: #777777; 
            margin-top: 10px; 
          }
          .container { 
            display: flex; 
            flex-direction: column; 
            align-items: center; 
          }
          input[type='number'] { 
            padding: 10px; 
            border-radius: 5px; 
            border: 1px solid #ccc; 
            font-size: 18px; 
            margin-bottom: 20px; 
            width: 100%;
            max-width: 300px;
            text-align: center;
          }
          button { 
            padding: 10px 20px; 
            background-color: #FF6F61; 
            border: none; 
            border-radius: 5px; 
            color: #ffffff; 
            font-size: 18px; 
            cursor: pointer;
            transition: background-color 0.3s ease;
          }
          button:hover { 
            background-color: #FF3D33; 
          }
        </style>
        <title>ESP32 Web Server</title>
      </head>
      <body>
        <h1>Giám sát và điều khiển động cơ</h1>
        <div class='container'>
          <div class='card'>
            <div class='card-title'>Số lượng sản phẩm</div>
            <div class='card-value' id='productCount'>-</div>
          </div>
          <div class='card'>
            <div class='card-title'>Tốc độ động cơ</div>
            <div class='card-value' id='motorSpeed'>-</div>
            <input type='number' id='speedInput' value='120' min='0' max='500'>
            <button id='submitBtn'>OK</button>
            <div class='card-subtitle'>RPM</div>
          </div>
        </div>
        <script>
          // Cập nhật dữ liệu từ ESP32
          setInterval(function() {
            fetch('/update').then(response => response.json()).then(data => {
              document.getElementById('productCount').textContent = data.productCount;
              document.getElementById('motorSpeed').textContent = data.motorSpeed;
            });
          }, 1000);

          // Gửi giá trị tốc độ động cơ khi nhấn nút OK
          document.getElementById('submitBtn').addEventListener('click', function() {
            let newSpeed = document.getElementById('speedInput').value/60*1600;
            fetch('/setSpeed?value=' + newSpeed);
          });
        </script>
      </body>
      </html>
    )rawliteral";

    request->send(200, "text/html", html);
  });

  // Cấu hình server để trả về dữ liệu cập nhật
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"productCount\": " + String(productCount) + ", \"motorSpeed\": " + String(motorSpeed) + "}";
    request->send(200, "application/json", json);
  });

  // Cấu hình server để nhận giá trị tốc độ động cơ từ web
  server.on("/setSpeed", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("value")) {
      motorSpeedControl = request->getParam("value")->value().toInt();
      Serial.println("Tốc độ động cơ mới: " + String(motorSpeedControl) + " RPM");
    }
    request->send(200, "text/plain", "OK");
  });

  // Bắt đầu server
  server.begin();
}

void loop() {
  productCount = Modbus_Master.readHoldingRegister(1, 4096);//(1 = Slave ID:1 , 4096 =  Address : 0 (D0))
  delay(50);
  motorSpeed = Modbus_Master.readHoldingRegister(1, 4097)/1600*60;//(1 = Slave ID:1 , 4096 =  Address : 0 (D0));
  delay(50);
  if (motorSpeedControl != oldValue && motorSpeedControl != 0) {
    Modbus_Master.writeHoldingRegister(1, 4546, motorSpeedControl); //(1 = Slave ID:1 , 4546 =  Address)
    delay(50);
    Modbus_Master.writeCoilsRegister(1, 2048, 0);
    delay(50);
    Modbus_Master.writeCoilsRegister(1, 2048, 1);
    delay(50);
    oldValue = motorSpeedControl;
  }
  if (motorSpeedControl == 0) {
    Modbus_Master.writeCoilsRegister(1, 2048, 0);
    delay(50);
  }

}
