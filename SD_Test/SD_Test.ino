/*
  ESP32-2432S028 (CYD / Cheap Yellow Display) SD Card Test
  
  The CYD uses SPI for the SD card slot.
  SD card SPI pins (VSPI):
    CS   = GPIO 5
    MOSI = GPIO 23
    MISO = GPIO 19
    SCK  = GPIO 18

  Upload this sketch and open Serial Monitor at 115200 baud.
*/

#include "FS.h"
#include "SD.h"
#include "SPI.h"

// CYD SD card pins
#define SD_CS    5
#define SD_MOSI  23
#define SD_MISO  19
#define SD_SCK   18

SPIClass sdSPI(VSPI);

void setup() {
  Serial.begin(115200);
  delay(2000);  // Give serial monitor time to connect

  Serial.println("================================");
  Serial.println("  CYD SD Card Test Starting");
  Serial.println("================================");

  // Explicitly initialize SPI for the SD card
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  Serial.println("Mounting SD card (SPI)...");
  Serial.printf("  CS=%d, MOSI=%d, MISO=%d, SCK=%d\n", SD_CS, SD_MOSI, SD_MISO, SD_SCK);

  if (!SD.begin(SD_CS, sdSPI, 4000000)) {  // 4 MHz to start slow & reliable
    Serial.println("\nERROR: SD card mount failed!");
    Serial.println("Troubleshooting:");
    Serial.println("  1. Is an SD card inserted? (push it in until it clicks)");
    Serial.println("  2. Try a different SD card (max 32GB, FAT32 formatted)");
    Serial.println("  3. Check if card has a lock switch (slide it to unlocked)");
    Serial.println("  4. Format the card as FAT32 on your PC first");
    return;
  }
  Serial.println("SD card mounted successfully!");

  // Get card type
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("ERROR: No SD card detected!");
    return;
  }

  Serial.print("Card Type: ");
  switch (cardType) {
    case CARD_MMC:  Serial.println("MMC");   break;
    case CARD_SD:   Serial.println("SD");    break;
    case CARD_SDHC: Serial.println("SDHC");  break;
    default:        Serial.println("Unknown"); break;
  }

  // Print card size
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("Card Size: %llu MB\n", cardSize);
  Serial.printf("Total Space: %llu MB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("Used Space: %llu MB\n", SD.usedBytes() / (1024 * 1024));

  // Test 1: Write a file
  Serial.println("\n--- Test 1: Write File ---");
  writeFile(SD, "/test.txt", "Hello from CYD!\n");

  // Test 2: Read the file back
  Serial.println("\n--- Test 2: Read File ---");
  readFile(SD, "/test.txt");

  // Test 3: Append to file
  Serial.println("\n--- Test 3: Append to File ---");
  appendFile(SD, "/test.txt", "This line was appended.\n");

  // Test 4: Read again to verify append
  Serial.println("\n--- Test 4: Read After Append ---");
  readFile(SD, "/test.txt");

  // Test 5: List root directory
  Serial.println("\n--- Test 5: List Root Directory ---");
  listDir(SD, "/", 1);

  // Test 6: Delete test file
  Serial.println("\n--- Test 6: Delete File ---");
  deleteFile(SD, "/test.txt");

  Serial.println("\n================================");
  Serial.println("  All SD Card Tests Complete!");
  Serial.println("================================");
}

void loop() {
  // Nothing to do here
}

// --- Helper Functions ---

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("  Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("  Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.printf("  DIR : %s\n", file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.printf("  FILE: %-20s  SIZE: %d bytes\n", file.name(), file.size());
    }
    file = root.openNextFile();
  }
}

void readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("  Failed to open file for reading");
    return;
  }

  Serial.print("  Content: ");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("  Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("  Write successful");
  } else {
    Serial.println("  Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("  Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("  Append successful");
  } else {
    Serial.println("  Append failed");
  }
  file.close();
}

void deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\n", path);

  if (fs.remove(path)) {
    Serial.println("  Delete successful");
  } else {
    Serial.println("  Delete failed");
  }
}
