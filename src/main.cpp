#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

// -------------------------------------------------

// Se estiver usando o módulo PN532 com comunicação SPI, defina os pinos para a comunicação SPI.
// #define PN532_SCK  (2)   // Pino do clock (SCK)
// #define PN532_MOSI (3)   // Pino de dados de saída (MOSI)
// #define PN532_SS   (4)   // Pino de seleção de escravo (SS)
// #define PN532_MISO (5)   // Pino de dados de entrada (MISO)

// Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

// Se estiver usando o modo I2C, defina os pinos analógicos correspondentes
// ao IRQ e RESET.  Use os valores A4 e A5 respectivamentes nesse modo.
#define PN532_IRQ   (A4)
#define PN532_RESET (A5)

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

// Ou use o hardware Serial:
// #define PN532_RESET (A5)

// Adafruit_PN532 nfc(PN532_RESET, &Serial);

// -------------------------------------------------

// Buffer para armazenar o UID retornado da Tag
uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };

// Tamanho do UID da Tag (4 ou 7 bytes dependendo do tipo da Tag ISO14443A)
uint8_t uidLength;

bool NFCTagConnection();
void NFCHardwareInfos();
void NFCReadTag7Bytes(int page);
void NFCReadTag4Bytes(int block);
void NFCTagDumpInfo4Bytes(uint8_t* key);
void NFCWriteTag4Bytes(int bloco, uint8_t data[17]);
bool NFCBlockConnection(int block, int keyType, uint8_t* key);

/**
 * Cada cartão RFID tem 16 setores (0 ao 15) Obs: Alguns podem ter mais, atente-se a isso.
 * Cada setor tem 4 blocos (setor 0 tem os blocos 0, 1, 2 e 3. setor 1 tem os blocos 4, 5, 6 e 7...)
 * O primeiro bloco do setor 0 é reservado para armazenar dados do fabricante (ou seja bloco 0 do setro 0).
 * O ultimo bloco de cada setor é reservado para armazenar a chave de acesso do setor (bloco 3 do setor 0, bloco 7 do setor 1 etc...)
 * Ou seja o setor 0 tem apenas 2 blocos para escrita, ja que o bloco 0 e 3 estão reservados.
 * Os demais blocos 1, 2 ... 14, 15 tem 3 blocos para escrita.
*/

void setup(void) {
    Serial.begin(9600);
    while (!Serial) delay(10); // for Leonardo/Micro/Zero

    Serial.println("Hello!");

    nfc.begin();

    uint32_t versiondata = nfc.getFirmwareVersion();
    if (!versiondata) {
        Serial.print("Didn't find PN53x board");
        while (1); // halt
    }

    // Got ok data, print it out!
    Serial.print("Found chip PN5"); Serial.println((versiondata >> 24) & 0xFF, HEX);
    Serial.print("Firmware ver. "); Serial.print((versiondata >> 16) & 0xFF, DEC);
    Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);

    // Set the max number of retry attempts to read from a card
    // This prevents us from waiting forever for a card, which is
    // the default behaviour of the PN532.
    nfc.setPassiveActivationRetries(0xFF);

    Serial.println("As Target... Approach the NFC PN532 Board to a PoS or terminal!");
    delay(200);
}

void loop(void) {
    uint8_t apdubuffer[255] = {}, apdulen = 0;
    uint8_t ppse[] = { 0x8E, 0x6F, 0x23, 0x84, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x11, 0xBF, 0x0C, 0x0E, 0x61, 0x0C, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x87, 0x01, 0x01, 0x90, 0x00 };
    nfc.AsTarget();
    (void)nfc.getDataTarget(apdubuffer, &apdulen); //Read initial APDU
    if (apdulen > 0) {
        for (uint8_t i = 0; i < apdulen; i++) {
            Serial.print(" 0x"); Serial.print(apdubuffer[i], HEX);
        }
        Serial.println("");
    }
    nfc.setDataTarget(ppse, sizeof(ppse));   //Mimic a smart card response with a PPSE APDU
    nfc.getDataTarget(apdubuffer, &apdulen); //Read respond from the PoS or Terminal
    if (apdulen > 0) {
        for (uint8_t i = 0; i < apdulen; i++) {
            Serial.print(" 0x"); Serial.print(apdubuffer[i], HEX);
        }
        Serial.println("");
    }
    delay(1000);
}
