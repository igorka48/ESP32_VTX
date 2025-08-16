#include <Arduino.h>
#include <ESP_8_BIT_GFX.h>
#include <VTXControl.h>


#define SMARTAUDIO_TX_PIN 17   // подключен к SmartAudio пину VTX
#define SMARTAUDIO_RX_PIN -1   // можно -1, если не подключён
#define DEFAULT_POWER_LEVEL 0 // 0=25mW, 1=100mW, 2=250mW

// A list of 8-bit color values that work well in a cycle.
uint8_t colorCycle[] = {
  0xFF, // White
  0xFE, // Lowering blue
  0xFD,
  0xFC, // No blue
  0xFD, // Raising blue
  0xFE,
  0xFF, // White
  0xF3, // Lowering green
  0xE7,
  0xE3, // No green
  0xE7, // Raising green
  0xF3,
  0xFF, // White
  0x9F, // Lowering red
  0x5F,
  0x1F, // No red
  0x5F, // Raising red
  0x9F,
  0xFF
};


// Create an instance of the graphics library
ESP_8_BIT_GFX videoOut(true /* = NTSC */, 8 /* = RGB332 color */);

//HardwareSerial smartAudio(2); // Use UART2
VTXControl *vtx;


String inputLine = "";
int bandLetterToIndex(char bandChar) {
  switch (toupper(bandChar)) {
    case 'A': return 0;
    case 'B': return 1;
    case 'C': return 2;
    case 'D': return 3;
    case 'E': return 4;
    case 'R': return 5;
    default: return -1;
  }
}

unsigned char crc8tab[256] = {
0x00, 0xD5, 0x7F, 0xAA, 0xFE, 0x2B, 0x81, 0x54, 0x29, 0xFC, 0x56, 0x83, 0xD7, 0x02, 0xA8, 0x7D,
0x52, 0x87, 0x2D, 0xF8, 0xAC, 0x79, 0xD3, 0x06, 0x7B, 0xAE, 0x04, 0xD1, 0x85, 0x50, 0xFA, 0x2F,
0xA4, 0x71, 0xDB, 0x0E, 0x5A, 0x8F, 0x25, 0xF0, 0x8D, 0x58, 0xF2, 0x27, 0x73, 0xA6, 0x0C, 0xD9,
0xF6, 0x23, 0x89, 0x5C, 0x08, 0xDD, 0x77, 0xA2, 0xDF, 0x0A, 0xA0, 0x75, 0x21, 0xF4, 0x5E, 0x8B,
0x9D, 0x48, 0xE2, 0x37, 0x63, 0xB6, 0x1C, 0xC9, 0xB4, 0x61, 0xCB, 0x1E, 0x4A, 0x9F, 0x35, 0xE0,
0xCF, 0x1A, 0xB0, 0x65, 0x31, 0xE4, 0x4E, 0x9B, 0xE6, 0x33, 0x99, 0x4C, 0x18, 0xCD, 0x67, 0xB2,
0x39, 0xEC, 0x46, 0x93, 0xC7, 0x12, 0xB8, 0x6D, 0x10, 0xC5, 0x6F, 0xBA, 0xEE, 0x3B, 0x91, 0x44,
0x6B, 0xBE, 0x14, 0xC1, 0x95, 0x40, 0xEA, 0x3F, 0x42, 0x97, 0x3D, 0xE8, 0xBC, 0x69, 0xC3, 0x16,
0xEF, 0x3A, 0x90, 0x45, 0x11, 0xC4, 0x6E, 0xBB, 0xC6, 0x13, 0xB9, 0x6C, 0x38, 0xED, 0x47, 0x92,
0xBD, 0x68, 0xC2, 0x17, 0x43, 0x96, 0x3C, 0xE9, 0x94, 0x41, 0xEB, 0x3E, 0x6A, 0xBF, 0x15, 0xC0,
0x4B, 0x9E, 0x34, 0xE1, 0xB5, 0x60, 0xCA, 0x1F, 0x62, 0xB7, 0x1D, 0xC8, 0x9C, 0x49, 0xE3, 0x36,
0x19, 0xCC, 0x66, 0xB3, 0xE7, 0x32, 0x98, 0x4D, 0x30, 0xE5, 0x4F, 0x9A, 0xCE, 0x1B, 0xB1, 0x64,
0x72, 0xA7, 0x0D, 0xD8, 0x8C, 0x59, 0xF3, 0x26, 0x5B, 0x8E, 0x24, 0xF1, 0xA5, 0x70, 0xDA, 0x0F,
0x20, 0xF5, 0x5F, 0x8A, 0xDE, 0x0B, 0xA1, 0x74, 0x09, 0xDC, 0x76, 0xA3, 0xF7, 0x22, 0x88, 0x5D,
0xD6, 0x03, 0xA9, 0x7C, 0x28, 0xFD, 0x57, 0x82, 0xFF, 0x2A, 0x80, 0x55, 0x01, 0xD4, 0x7E, 0xAB,
0x84, 0x51, 0xFB, 0x2E, 0x7A, 0xAF, 0x05, 0xD0, 0xAD, 0x78, 0xD2, 0x07, 0x53, 0x86, 0x2C, 0xF9};

uint8_t crc8(const uint8_t * ptr, uint8_t len)
{
  uint8_t crc = 0;
  for (uint8_t i=0; i<len; i++) {
    crc = crc8tab[crc ^ *ptr++];
  }
  return crc;
}

// void sendCommand(const uint8_t *cmd, uint8_t len) {
//   smartAudio.flush();  // Очистим UART
//   smartAudio.write(cmd, len);
//   smartAudio.flush();  // Дождёмся окончания передачи
//   delay(100);
// }

/*
// Отправка настроек Band/Channel/Power
void sendSetSettings(uint8_t band, uint8_t channel, uint8_t power) {

 uint8_t payload[] = {
    0xAA, 0x55, 6,        // Заголовок и длина
    0x01,                // Команда SetSettings
    power,               // Power: 0=25mW, 1=100mW, 2=250mW
    0x00,                // PitMode: 0 = выключен
    band,                // Band: 0=A, 1=B, 2=E, 3=F, 4=R;
    channel,             // Channel: 1-8
    0x00                 // Reserved (часто 0)
  };
 
  payload[sizeof(payload) - 1] = crc8(payload, sizeof(payload) - 1);  // CRC

  
  sendCommand(payload, sizeof(payload));
  Serial.println("SetSettings отправлена.");
}
*/
/*
// Отправка SaveSettings (обязательно!)
void sendSaveSettings() {
  uint8_t saveCmd[] = {0xAA, 0x55, 0x02, 0x06, 0x00};
  saveCmd[4] = crc8(saveCmd, 4);
  sendCommand(saveCmd, sizeof(saveCmd));
  Serial.println("SaveSettings отправлена.");
}
*/
// Отправка команды с payload
void sendSmartAudioCommand(uint8_t commandID, uint8_t value) {
  //0xAA 0x55 0x07(Command 3) 0x01(Length) 0x00(All 40 Channels 0-40) 0xB8(CRC8) - SetChannel
  //0xAA 0x55 0x05(Command 2) 0x01(Length) 0x00(Power Level) 0x6B(CRC8)  - SetPower  
  uint8_t packet[] = {
    0xAA, 0x55,  // header
    commandID,   // команда (напр. 0x07  смена канала)
    0x01,        // длина payload
    value,       // значение (напр. канал или мощность)
    0x00         // CRC будет вставлен позже
  };
  packet[sizeof(packet) - 1] = crc8(packet, sizeof(packet) - 1);

// Print packet to Serial Monitor
  Serial.println("sendSmartAudioCommand:");
  for (uint8_t i = 0; i < sizeof(packet); i++) {
    Serial.print("0x");
    if (packet[i] < 0x10) Serial.print("0"); // Leading zero for single-digit hex
    Serial.print(packet[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  //smartAudio.write(packet, sizeof(packet));
}


/*
void setBand(uint8_t band) {
  sendSmartAudioCommand(0x02, band); // команда SetBand
}
*/


void setChannel(uint8_t band, uint8_t channel) {
 
  uint8_t chIndex = band * 8 + (channel - 1); 
  Serial.print("chIndex=");
  Serial.println(chIndex);
  vtx->setChannel(band * 8 + (channel - 1)); 
 // sendSmartAudioCommand(0x07, chIndex); 
  
}

void setPower(uint8_t powerIndex) {  
  sendSmartAudioCommand(0x05, powerIndex); 
}

void decimalTo16BitHex(uint16_t frequency, uint8_t *msb, uint8_t *lsb) {
    *msb = (frequency >> 8) & 0xFF;  // Старший байт (MSB)
    *lsb = frequency & 0xFF;         // Младший байт (LSB)
}

void setFrequency(uint16_t frequency) {  
  //0xAA 0x55 0x09(Command 4) 0x02(Length) 0x16 0xE9(Frequency 5865) 0xDC(CRC8) - SetFrequency
  uint8_t msb, lsb;
  decimalTo16BitHex(frequency, &msb, &lsb);
  printf("Частота %u МГц в 16-битном формате: 0x%02X 0x%02X\n", frequency, msb, lsb);
  uint8_t packet[] = {
    0xAA, 0x55,  // header
    0x09,   // команда (SetFrequency)
    0x02,        // длина payload
    msb,       
    lsb,
    0x00         // CRC будет вставлен позже
  };
  packet[sizeof(packet) - 1] = crc8(packet, sizeof(packet) - 1);
  //smartAudio.write(packet, sizeof(packet));
}


// Парсинг команды из терминала: B1 C4 P0
void parseCommand(String line) {
  Serial.println(line);
  line.toUpperCase();

  if (line.length() < 2) {
    Serial.println("Ошибка: слишком короткая команда.");
    return;
  }

  char bandChar = line.charAt(0);
  int band = bandLetterToIndex(bandChar);
  if (band == -1 && bandChar != 'H') {
    Serial.println("Ошибка: неверная буква Band (используйте A, B, C, D, E, R).");
    return;
  }

  int channel = -1;
  int power = DEFAULT_POWER_LEVEL;

  // Ожидаем: <BandLetter><ChannelNumber> [Power]
  // Пример: B6 1
  int firstDigitIndex = 1;
  while (firstDigitIndex < line.length() && !isdigit(line.charAt(firstDigitIndex))) {
    firstDigitIndex++;
  }

  if (firstDigitIndex >= line.length()) {
    Serial.println("Ошибка: не указан канал.");
    return;
  }

  int spaceIndex = line.indexOf(' ', firstDigitIndex);
  if (spaceIndex == -1) {
    channel = line.substring(firstDigitIndex).toInt();
  } else {
    channel = line.substring(firstDigitIndex, spaceIndex).toInt();
    power = line.substring(spaceIndex + 1).toInt();
  }

  if (bandChar== 'H') {
    setFrequency(channel); // Если 'H', то это частота в MHz
  } else {

    if (channel < 1 || channel > 8) {
      Serial.println("Ошибка: канал должен быть от 1 до 8.");
      return;
    }
    if (power < 0 || power > 500) {
      Serial.println("Ошибка: мощность должна быть 0, 1 или 2.");
      return;
    }

    Serial.print("Установка: Band ");
    Serial.print(bandChar);
    Serial.print(" (");
    Serial.print(band);
    Serial.print("), Channel ");
    Serial.print(channel);
    Serial.print(", Power ");
    Serial.println(power);
  
    //sendSetSettings(band, channel, power);
    setChannel(band, channel);
    delay(3000); // Задержка для обработки команды
   // setPower(power);
  }
  
  delay(500);
  //sendSaveSettings();
}


void setup() {
  
  // Initial setup of graphics library
  videoOut.begin();
  

  Serial.begin(115200);
  delay(500);
  Serial.println("=== SmartAudio VTX Controller ===");
  Serial.println("Формат: B4 <power>  → Band=B, Channel=4, Power=0 (по умолчанию 25мВт)");
  Serial.println("Пример: A5 1   → Band=A, Channel=5, Power=100мВт");
  Serial.println("Формат: H5865  → set frequence 5865 MHz");

  vtx = new VTXControl(VTXMode::SmartAudio, 17, 1400, false);

  //smartAudio.begin(4800, SERIAL_8N2, SMARTAUDIO_RX_PIN, SMARTAUDIO_TX_PIN);  // Half-duplex: только TX
  //smartAudio.begin(9600, SERIAL_8N1, SMARTAUDIO_RX_PIN, SMARTAUDIO_TX_PIN);
  delay(200);

}

void loop() {
while (Serial.available()) {
    char ch = Serial.read();
    Serial.print(ch);  // Echo the character back to the terminal
    if (ch == '\n' || ch == '\r') {      
      if (inputLine.length() > 0) {        
        parseCommand(inputLine);        
        inputLine = "";
      }
    } else {
      inputLine += ch;
    }
  }

  // Wait for the next frame to minimize chance of visible tearing
  videoOut.waitForFrame();

  // Get the current time and calculate a scaling factor
  unsigned long time = millis();
  float partial_second = (float)(time % 1000)/1000.0;

  // Use time scaling factor to calculate coordinates and colors
  uint8_t movingX = (uint8_t)(255*partial_second);
  uint8_t invertX = 255-movingX;
  uint8_t movingY = (uint8_t)(239*partial_second);
  uint8_t invertY = 239-movingY;

  uint8_t cycle = colorCycle[(uint8_t)(17*partial_second)];
  uint8_t invertC = 0xFF-cycle;

  // Clear screen
  videoOut.fillScreen(0);

  // Draw one rectangle
  videoOut.drawLine(movingX, 0,       255,     movingY, cycle);
  videoOut.drawLine(255,     movingY, invertX, 239,     cycle);
  videoOut.drawLine(invertX, 239,     0,       invertY, cycle);
  videoOut.drawLine(0,       invertY, movingX, 0,       cycle);

  // Draw a rectangle with inverted position and color
  videoOut.drawLine(invertX, 0,       255,     invertY, invertC);
  videoOut.drawLine(255,     invertY, movingX, 239,     invertC);
  videoOut.drawLine(movingX, 239,     0,       movingY, invertC);
  videoOut.drawLine(0,       movingY, invertX, 0,       invertC);

  // Draw text in the middle of the screen
  videoOut.setCursor(25, 80);
  videoOut.setTextColor(invertC);
  videoOut.setTextSize(2);
  videoOut.setTextWrap(false);
  videoOut.print("Adafruit GFX API");
  videoOut.setCursor(110, 120);
  videoOut.setTextColor(0xFF);
  videoOut.print("on");
  videoOut.setCursor(30, 160);
  videoOut.setTextColor(cycle);
  videoOut.print("ESP_8_BIT video");

}