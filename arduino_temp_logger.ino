/*
 * arduino_temp_logger
 * 
 * 温度センサーから温度をI2C経由で読み込んで、1分おきにEEPROMに時系列に保存する。
 * 再起動しても継続して記録していく。
 * 通常稼働時はLEDが点滅する。
 * EEPROMがいっぱいになると、LEDを点灯させたままになる。
 * シリアル経由でデータの読み込みおよびデータのリセットができる。
 */
#include <EEPROM.h>
#include <Wire.h>

#define LED_PIN 13
#define EEPROM_SIZE (1024-2)  // EEPROMの最後の2バイトはオフセット格納用に確保
#define INTERVAL_MS 60000
#define LM75B_address 0x48 // A0=A1=A2=Low

int offset; // 現在書き込んでいるEEPROMのアドレス

// 最後に書いていたEEPROMのアドレスをEEPROMから読み込む
int read_Offset()
{
  int o;
  o = EEPROM.read(EEPROM_SIZE-2) << 8;
  o += EEPROM.read(EEPROM_SIZE-1);
  return o;
}

// 最後に書いたEEPROMのアドレスをEEPROMに保存する
void write_Offset(int offset)
{
  EEPROM.write(EEPROM_SIZE-2, (byte)(offset >> 8));
  EEPROM.write(EEPROM_SIZE-1, (byte)offset);
}

// 指定したEEPROMのアドレスから温度を読み込む
double read_Temp(int addr)
{
  double temp = 0.0;
  
  // 1バイト目が整数部、2バイト目が小数部
  temp += EEPROM.read(addr);
  temp += EEPROM.read(addr+sizeof(byte)) / 100.0;

  return temp;
}

// 指定したEEPROMのアドレスに温度を書き込む
int write_Temp(int addr, double temp)
{
  byte b;

  // 1バイト目が整数部、2バイト目が小数部
  b = (byte)temp;
  EEPROM.write(addr, b);
  addr += sizeof(byte);

  b = (byte)((temp-b)*100);
  EEPROM.write(addr, b);
  addr += sizeof(byte);

  return addr;
}

// シリアルからの入力を確認、処理する
// データ読み込みまたはデータクリア
void check_Serial()
{
  int ch;
  ch = Serial.read();
  if (ch == 'r')
  {
    Serial.println("-----");
    for (int i = 0; i < offset; i += sizeof(byte)*2)
    {
      double temp = read_Temp(i);
      Serial.print("[");
      Serial.print(i);
      Serial.print("]");
      Serial.println(temp);
    }
    Serial.println("-----");
  }
  else if (ch == 'c')
  {
    offset = 0;
    write_Offset(offset);
    Serial.print("clear\n");
  }
}

// 指定した時間、LEDを点滅させながら待つ
void delay_blink(unsigned long delay_ms)
{
  int status = LOW;
  
  while (delay_ms > 0)
  {
    check_Serial();

    if (status == HIGH)
    {
      status = LOW;
    }
    else
    {
      status = HIGH;
    }
    digitalWrite(LED_PIN, status);

    if (delay_ms >= 1000)
    {
      delay(1000);
      delay_ms -= 1000;
    }
    else
    {
      delay(delay_ms);
      delay_ms = 0;
    }
  }
}

// センサーから温度を読む
double get_Temperature()
{
  signed int temp_data = 0;
  double temp = 0.0;

  Wire.requestFrom(LM75B_address,2);
  while (Wire.available())
  {
    temp_data |= (Wire.read() << 8);         //温度レジスタの上位8bit取得
    temp_data |= Wire.read();                //温度レジスタの下位8bit取得(有効3bit)
  }
  temp = (temp_data >> 5) * 0.125;
  return temp;
}

// 初期化
void setup() {
  // put your setup code here, to run once:
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);

  // 最後に書き込んでいたEEPROMのアドレスをレストア
  offset = read_Offset();
  Serial.print("offset: ");
  Serial.println(offset);
  
  // 初期化した記録を残す
  offset = write_Temp(offset, 0);
  write_Offset(offset);

  // 温度センサの初期化/I2C
  Wire.begin();
  Wire.beginTransmission(LM75B_address);
  Wire.write(0x00);                            //温度読み出しモードに設定
  Wire.endTransmission();
}

// メインルーチン
void loop() {
  unsigned long start_time;
  unsigned long delay_ms;
  double temp = 0.0;
  
  // put your main code here, to run repeatedly:
  start_time = millis();

  // EEPROMがいっぱいだったらLEDを点滅しっぱなしにする
  if (offset >= EEPROM_SIZE)
  {
    digitalWrite(LED_PIN, HIGH);   // LEDをオン
    return;
  }

  // 温度をセンサーから読み込んで、EEPROMに書き込む
  temp = get_Temperature();
  Serial.print("[");
  Serial.print(offset);
  Serial.print("] ");
  Serial.println(temp);
      
  if (offset < EEPROM_SIZE)
  {
    offset = write_Temp(offset, temp);
    write_Offset(offset);
  }
  
  // 次に記録する時間までLEDを点滅させながら待機
  delay_ms = INTERVAL_MS - (millis() - start_time);
  delay_blink(delay_ms);
}

