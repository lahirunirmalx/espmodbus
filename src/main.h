
#ifndef __MAIN_H__
#define __MAIN_H__ 
#include <cstdint>
#include <Fonts/FreeMono9pt7b.h> 
#include <iostream>
#include <string>
#include <cstring>
#include <queue>



// Define OLED display settings
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define OLED_ADDRESS 0x3C
//Radio
#define PIN_CE 27
#define PIN_CS 26
// SPI
#define PIN_SCK 14
#define PIN_MISO 12
#define PIN_MOSI 13
// Wire
#define PIN_SS 15
#define PIN_SDA 21
#define PIN_SCL 22
//PS2
#define PIN_DATA 32
#define PIN_IRQ 35

#define BUFFER_SIZE 32 
#define MAX_MESSAGES 15
#define MAX_STRING_LENGTH BUFFER_SIZE



// Define the structure for the received message
struct Message {
    uint8_t address[6];
    char text[MAX_STRING_LENGTH];
    bool isRead; // Flag to track if the message is read

    // Constructor for convenience
    Message(const uint8_t addr[6], const char* msg) : isRead(false) {
        std::memcpy(address, addr, 6);
        std::strncpy(text, msg, MAX_STRING_LENGTH);
        text[MAX_STRING_LENGTH - 1] = '\0'; // Ensure null-termination
    }
    Message() : isRead(false) {
           
    }
};

void storeMessage(const Message& receivedMsg);
void addressToString(const byte address[6], char *output);
void stringToAddress(const std::string &input, byte address[6]);
void addressUp();
void addressDown();
void onBootButtonPressed();
void onBootButtonLongPress();

#endif // __MAIN_H__