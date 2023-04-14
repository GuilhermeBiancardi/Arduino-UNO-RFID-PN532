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

    // Para Leonardo/Micro/Zero
    // while (!Serial) delay(10);

    // Inicia o Leitor
    nfc.begin();

    // Imprime dados da placa
    NFCHardwareInfos();

}

void loop(void) {

    // 16 Bytes (caracteres) por bloco
    uint8_t data[17] = { "@GuilhermeAw.com" };

    // KeyA chave padrão
    uint8_t KeyA[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

    // KeyB chave padrão
    uint8_t KeyB[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

    // Verifica se a Tag foi reconhecida.
    if (NFCTagConnection()) {
        // Verifica se o UID tem 4 bytes e verifica se o setor do bloco informado foi autenticado
        if (NFCBlockConnection(4, 0, KeyA)) {
            // Escreve dados no bloco 4
            NFCWriteTag4Bytes(4, data);
            // Lê os dados do Bloco 4
            NFCReadTag4Bytes(4);
            // Lê todos os dados disponíveis
            NFCTagDumpInfo4Bytes(KeyB);
        } else {
            // UID 7 bytes
            NFCReadTag7Bytes(4);
        }
    }

    delay(2000);

}

void NFCHardwareInfos() {

    // Armazena dados de versão se o leitor for reconhecido.
    uint32_t versiondata = nfc.getFirmwareVersion();
    if (!versiondata) {
        Serial.print("Não foi possível encontrar uma placa PN53x");
        while (1);
    }

    // Tudo OK, Imprime algumas informações da placa.
    Serial.print("Found chip PN5"); Serial.println((versiondata >> 24) & 0xFF, HEX);
    Serial.print("Firmware ver. "); Serial.print((versiondata >> 16) & 0xFF, DEC);
    Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);

}

bool NFCTagConnection() {

    // Armazena dados da conexão
    uint8_t connection;

    // Aguarda uma Tag ISO14443A (Mifare, etc.) ser encontrada pela placa.
    // Quando encontrada armazenará o UID da Tag na variável uidLength
    // Se o UID tiver 4 bytes = (Mifare Classic) ou 7 bytes = (Mifare Ultralight)
    connection = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

    if (connection) {

        // Imprime informações da Tag encontrada.
        Serial.println("Encontrado Tag ISO14443A");
        Serial.print("-- Tamanho da UID: "); Serial.print(uidLength, DEC); Serial.println(" bytes");
        Serial.print("-- Valor da UID: ");
        nfc.PrintHex(uid, uidLength);
        Serial.println("");

        return true;

    } else {

        return false;

    }

}

bool NFCBlockConnection(int block, int keyType, uint8_t* key) {

    // Armazena dados da conexão
    uint8_t connection;

    if (uidLength == 4) {

        Serial.println("Tag Mifare Classic (4 byte UID)");

        if (keyType == 0) {
            Serial.println("Tentando autenticar o bloco informado com a chave padrão KeyA.");
        }

        if (keyType == 1) {
            Serial.println("Tentando autenticar o bloco informado com a chave padrão KeyB.");
        }

        // Tenta autenticar o setor do bloco informado com a KeyA
        connection = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, block, keyType, key);

        if (connection) {
            Serial.println("Bloco autenticado.");
            return true;
        } else {
            Serial.println("Erro na autenticação.");
            return false;
        }

    } else {
        return false;
    }

}

// Ignora os blocos que contém as chaves de acesso e as informações do fabricante.
bool IgnoreReservedBlocks(int AtualBlock) {
    bool found = false;
    // Blocos reservados
    int array[] = { 0, 3, 7, 11, 15, 19, 23, 27, 31, 35, 39, 43, 47, 51, 55, 59, 63 };
    for (int i = 0; i < 17; i++) {
        if (array[i] == AtualBlock) {
            found = true;
            break;
        }
    }
    return found;
}

void NFCWriteTag4Bytes(int block, uint8_t data[17]) {

    // Armazena dados da conexão
    uint8_t connection;

    if (!IgnoreReservedBlocks(block)) {

        connection = nfc.mifareclassic_WriteDataBlock(4, data);

        if (connection) {
            Serial.println("Dados armazenados no bloco informado.");
        } else {
            Serial.println("Ooops ... Não foi possível escrever no bloco.");
        }

    } else {
        Serial.println("O bloco informado é reservado para a Key do Setor.");
    }

}

void NFCReadTag4Bytes(int block) {

    // Armazena dados da conexão
    uint8_t connection;

    // Armazena os dados lidos.
    uint8_t data[16];

    // Tenta ler o conteúdo do bloco 4
    connection = nfc.mifareclassic_ReadDataBlock(block, data);

    if (connection) {

        Serial.println("Dados contidos no bloco informado:");
        nfc.PrintHexChar(data, 16);
        Serial.println("");
        // Espero um pouco antes de ler o bloco novamente
        delay(1000);

    } else {
        Serial.println("Ooops ... Não foi possível autenticar o bloco. Tente outra Key.");
    }

}

void NFCReadTag7Bytes(int page) {

    // Armazena dados da conexão
    uint8_t connection;

    if (uidLength == 7) {

        Serial.println("Tag Mifare Ultralight (7 byte UID)");

        // Tenta ler a página 4
        Serial.println("Lendo página.");
        uint8_t data[32];
        connection = nfc.mifareultralight_ReadPage(page, data);

        if (connection) {

            // Dados sendo lidos
            nfc.PrintHexChar(data, page);
            Serial.println("");
            // Espero um pouco antes de ler a página novamente
            delay(1000);

        } else {
            Serial.println("Ooops ... Não foi possível ler a página.");
        }

    }

}

void NFCTagDumpInfo4Bytes(uint8_t* key) {

    // Armazena dados da conexão
    uint8_t connection;

    // Bloco Atual
    uint8_t currentblock;

    // Armazena se o bloco está autenticado
    bool authenticated = false;

    // Armazena os dados do bloco
    uint8_t data[16];

    for (currentblock = 0; currentblock < 64; currentblock++) {

        // Check if this is a new block so that we can reauthenticate
        if (nfc.mifareclassic_IsFirstBlock(currentblock)) {
            authenticated = false;
        }

        // Se o setor não tiver sido autenticado, faz isso primeiro
        if (!authenticated) {

            // Starting of a new sector ... try to to authenticate
            Serial.print("----------------------- Setor ");
            Serial.print(currentblock / 4, DEC);
            Serial.println("------------------------");

            // Chave padrão KeyA (0xFF 0xFF 0xFF 0xFF 0xFF 0xFF) para Mifare Classic
            // Ou KeyA (0xA0 0xA1 0xA2 0xA3 0xA4 0xA5) para NDEF Tags,
            // Mas o KeyB é o mesmo para os 2 (0xFF 0xFF 0xFF 0xFF 0xFF 0xFF)
            connection = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, currentblock, 1, key);

            if (connection) {
                authenticated = true;
            } else {
                Serial.println("Erro na authenticação.");
            }
        }

        // Se não estiver autenticado, pula o bloco
        if (!authenticated) {

            Serial.print("Block ");
            Serial.print(currentblock, DEC);
            Serial.println(" não foi possível autenticar.");

        } else {

            // Autenticado, lê o bloco
            connection = nfc.mifareclassic_ReadDataBlock(currentblock, data);

            if (connection) {

                Serial.print("Block ");
                Serial.print(currentblock, DEC);

                if (currentblock < 10) {
                    Serial.print("  ");
                } else {
                    Serial.print(" ");
                }

                nfc.PrintHexChar(data, 16);

            } else {
                Serial.print("Block ");
                Serial.print(currentblock, DEC);
                Serial.println("  não foi possível ler o bloco.");
            }

        }

    }

}