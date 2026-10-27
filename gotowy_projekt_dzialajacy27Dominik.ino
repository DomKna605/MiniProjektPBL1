#include <WiFiS3.h>
#include <Servo.h>

// ============================================================================
// 1. KONFIGURACJA PROJEKTU
// ============================================================================

// Dane Twojej sieci Wi-Fi (2.4 GHz) - zachowane z Twojego routera
const char ssid[] = "siec w ktorej zostanie udostepnina strona";
const char pass[] = "haslo do podanej sieci";

// PRÓG ODLEGŁOŚCI HC-SR04 (w centymetrach):
const float PROG_ODLEGLOSCI_CM = 11.0;

// Nowe kąty serwa zgodnie z wytycznymi:
const int KAT_ZAMKNIETY = 0;   // 0 stopni = zamek zamknięty / zaryglowany
const int KAT_OTWARTY   = 90;  // 90 stopni = zamek otwarty

// ============================================================================
// 2. PRZYPISANIE PINÓW (ARDUINO UNO R4 WIFI)
// ============================================================================

const int REED_PIN  = 2;  // Kontaktron RC-33
const int TRIG_PIN  = 4;  // HC-SR04 TRIG
const int ECHO_PIN  = 5;  // HC-SR04 ECHO
const int SERVO_PIN = 9;  // Serwo FS90 Sygnał (PWM)

// ============================================================================
// 3. ZMIENNE STANU
// ============================================================================

Servo serwo;
int servoAngle = KAT_ZAMKNIETY;

bool magnes = false;
float dystans = 0.0;

unsigned long ostatniPomiarCzas = 0;
const unsigned long INTERWAL_POMIARU_MS = 300;

WiFiServer server(80);

// Funkcja wykonująca pomiar odległości w cm
float zmierzOdlegloscCm(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long czas = pulseIn(echoPin, HIGH, 25000);
  if (czas == 0) return 999.0;
  return (czas * 0.0343) / 2.0;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(REED_PIN, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  serwo.attach(SERVO_PIN);
  serwo.write(servoAngle);

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Błąd: Moduł Wi-Fi niedostępny!");
    while (true);
  }

  Serial.print("Łączenie z siecią Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    delay(500);
    Serial.print(".");
  }

  server.begin();
  Serial.println("\nPołączono pomyślnie!");
  Serial.print("Adres panelu sterowania: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  // 1. Odczyt kontaktronu (LOW = magnes obecny / Pudełko włożone)
  magnes = (digitalRead(REED_PIN) == LOW);

  // 2. Cykliczny pomiar odległości
  if (millis() - ostatniPomiarCzas >= INTERWAL_POMIARU_MS) {
    ostatniPomiarCzas = millis();
    dystans = zmierzOdlegloscCm(TRIG_PIN, ECHO_PIN);
  }

  // 3. Obsługa żądań HTTP
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    String request = "";

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();

        if (c == '\n') {
          if (currentLine.length() == 0) {

            // Komenda: Otwórz zamek (ustaw 90 stopni)
            if (request.indexOf("GET /servo/open") >= 0) {
              servoAngle = KAT_OTWARTY;
              serwo.write(servoAngle);
              client.println("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nOK");
            }
            // Komenda: Zamknij i zarygluj zamek (ustaw 0 stopni)
            else if (request.indexOf("GET /servo/close") >= 0) {
              servoAngle = KAT_ZAMKNIETY;
              serwo.write(servoAngle);
              client.println("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nOK");
            }
            // Zapytanie o status (JSON)
            else if (request.indexOf("GET /status") >= 0) {
              bool itemPresent = (dystans <= PROG_ODLEGLOSCI_CM);

              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: application/json");
              client.println("Connection: close");
              client.println();
              client.print("{");
              client.print("\"k\":"); client.print(magnes ? "true" : "false");
              client.print(",\"servo\":"); client.print(servoAngle);
              client.print(",\"dist\":"); client.print(dystans, 1);
              client.print(",\"item\":"); client.print(itemPresent ? "true" : "false");
              client.print(",\"openAngle\":"); client.print(KAT_OTWARTY);
              client.print(",\"closeAngle\":"); client.print(KAT_ZAMKNIETY);
              client.print(",\"thresh\":"); client.print(PROG_ODLEGLOSCI_CM, 1);
              client.println("}");
            }
            // Serwowanie głównego interfejsu WWW
            else {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/html; charset=utf-8");
              client.println("Connection: close");
              client.println();

              client.println("<!DOCTYPE html><html lang=\"pl\"><head>");
              client.println("<meta charset=\"utf-8\">");
              client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
              client.println("<title>Szafka IoT - Przedłużacz</title>");
              client.println("<style>");
              client.println("body { font-family: system-ui, sans-serif; background: #0f172a; color: #f8fafc; text-align: center; padding: 20px; margin: 0; }");
              client.println(".card { background: #1e293b; border-radius: 14px; padding: 24px; max-width: 360px; margin: 20px auto; box-shadow: 0 4px 20px rgba(0,0,0,0.4); border: 1px solid #334155; }");
              client.println(".status-lamp { display: inline-block; padding: 12px 20px; border-radius: 999px; font-weight: bold; font-size: 15px; margin: 12px 0; width: 90%; }");
              client.println(".lamp-green { background: #16a34a; color: #ffffff; box-shadow: 0 0 14px rgba(22,163,74,0.5); }");
              client.println(".lamp-yellow { background: #eab308; color: #0f172a; box-shadow: 0 0 14px rgba(234,179,8,0.5); }");
              client.println(".lamp-red { background: #dc2626; color: #ffffff; box-shadow: 0 0 14px rgba(220,38,38,0.5); }");
              client.println(".info-row { display: flex; justify-content: space-between; background: #0f172a; padding: 10px 14px; border-radius: 8px; margin-top: 8px; font-size: 14px; }");
              client.println(".btn { background: #2563eb; color: white; border: none; padding: 14px 24px; font-size: 16px; font-weight: bold; border-radius: 8px; cursor: pointer; margin-top: 18px; width: 100%; transition: 0.2s; }");
              client.println(".btn:disabled { background: #475569; cursor: not-allowed; opacity: 0.6; }");
              client.println(".btn-confirm { background: #eab308; color: #0f172a; border: none; padding: 14px 20px; font-size: 15px; font-weight: bold; border-radius: 8px; cursor: pointer; margin-top: 14px; width: 100%; }");
              client.println(".alert-box { display: none; background: #854d0e; border: 1px solid #facc15; border-radius: 10px; padding: 16px; margin-top: 16px; text-align: left; }");
              client.println("</style></head><body>");

              client.println("<h1>Panel Szafki IoT</h1>");
              client.println("<p style=\"color:#94a3b8; font-size:14px;\">Wzorzec progu: <strong id=\"thresh-val\">--</strong> cm</p>");

              client.println("<div class=\"card\">");
              client.println("<h2>Komora Przedłużacza</h2>");
              client.println("<div id=\"lamp\" class=\"status-lamp lamp-yellow\">Łączenie...</div>");
              client.println("<div class=\"info-row\"><span>Czy pudełko włożone? :</span><strong id=\"k-val\">--</strong></div>");
              client.println("<div class=\"info-row\"><span>Dystans HC-SR04:</span><strong id=\"dist-val\">-- cm</strong></div>");
              client.println("<div class=\"info-row\"><span>Przedłużacz wewnątrz:</span><strong id=\"item-val\">--</strong></div>");
              client.println("<div class=\"info-row\"><span>Kąt serwa (Rygiel):</span><strong id=\"servo-val\">--</strong></div>");
              client.println("<button id=\"open-btn\" class=\"btn\" onclick=\"openBox()\">OTWÓRZ KOMORĘ</button>");

              client.println("<div id=\"confirmBox\" class=\"alert-box\">");
              client.println("<strong style=\"color:#fef08a;\">&#9888; Wykryto domknięcie drzwiczek!</strong>");
              client.println("<p style=\"margin:6px 0 0; font-size:13px;\">Magnes dotknął kontaktronu. Potwierdź, aby zaryglować zamek serwem:</p>");
              client.println("<button class=\"btn-confirm\" onclick=\"closeBox()\">ZABLOKUJ I ZAMKNIJ (0&deg;)</button>");
              client.println("</div>");

              client.println("</div>");

              client.println("<script>");
              client.println("function updateStatus() {");
              client.println("  fetch('/status').then(r => r.json()).then(d => {");
              client.println("    document.getElementById('thresh-val').textContent = d.thresh;");
              client.println("    document.getElementById('k-val').textContent = d.k ? 'Tak' : 'Nie';");
              client.println("    document.getElementById('dist-val').textContent = (d.dist > 400 ? 'Poza zakresem' : d.dist + ' cm');");
              client.println("    document.getElementById('item-val').textContent = d.item ? 'OBECNY' : 'BRAK';");
              client.println("    document.getElementById('servo-val').textContent = d.servo + '°';");
              client.println("    const isUnlocked = (d.servo === d.openAngle);");
              client.println("    document.getElementById('open-btn').disabled = isUnlocked;");
              client.println("    const lamp = document.getElementById('lamp');");
              client.println("    if (isUnlocked) {");
              client.println("      lamp.className = 'status-lamp lamp-yellow';");
              client.println("      lamp.textContent = 'W UŻYCIU (OTWARTA 90°)';");
              client.println("    } else if (!d.k) {");
              client.println("      lamp.className = 'status-lamp lamp-red';");
              client.println("      lamp.textContent = 'BŁĄD: NIEDOMKNIĘTE!';");
              client.println("    } else if (d.item) {");
              client.println("      lamp.className = 'status-lamp lamp-green';");
              client.println("      lamp.textContent = 'DOSTĘPNA (PRZEDŁUŻACZ JEST)';");
              client.println("    } else {");
              client.println("      lamp.className = 'status-lamp lamp-red';");
              client.println("      lamp.textContent = 'WYPOŻYCZONA (PUSTA)';");
              client.println("    }");
              client.println("    const box = document.getElementById('confirmBox');");
              client.println("    box.style.display = (isUnlocked && d.k) ? 'block' : 'none';");
              client.println("  }).catch(e => console.error(e));");
              client.println("}");

              client.println("function openBox() { fetch('/servo/open').then(() => updateStatus()); }");
              client.println("function closeBox() { fetch('/servo/close').then(() => updateStatus()); }");
              client.println("setInterval(updateStatus, 500);");
              client.println("updateStatus();");
              client.println("</script></body></html>");
            }
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
          if (request.length() < 40) request += c;
        }
      }
    }
    client.stop();
  }
}