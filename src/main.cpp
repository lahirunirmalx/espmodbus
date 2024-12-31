#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RF24.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PS2Keyboard.h>
#include <string>
#include <sstream>
#include <OneButton.h>

#include <main.h>
#include <screen.h>
 
#define COMMUNICATION_ADDRESS "00001" 

#define HOME_SCREEN_SIZE 4
#define TOP_MENU_PAD 2
#define BOOT_BUTTON_PIN 0

// Initialize the radio and display keyboard
PS2Keyboard keyboard;
RF24 radio(PIN_CE, PIN_CS);
Adafruit_SSD1306 display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, OLED_ADDRESS);
OneButton bootButton(BOOT_BUTTON_PIN, true); 
char receivedText[32] = "";
std::string inputText  = ""; 

std::queue<Message> messageInbox;

// Define the address for communication
 uint8_t address[6] = COMMUNICATION_ADDRESS;  
int selected_item = 0;  
uint8_t sendAddress[6] = {0};
char strSendAddress[7]=""; 
bool success = true;

typedef enum
{
     
    INIT = 0,
    HOME,
    CHECK, 
    NEW_MESSAGE, 
    READ_MESSAGE, 
    WRITE_ADDRESS,
    SET_ADDRESS,
    WRITE_MESSAGE,
    SET_MESSAGE,
    SEND_MESSAGE,
    DELETE_MESSAGE,
    SELECT_MESSAGE, 
    ERROR
} communicator;

communicator app = communicator::HOME;
void setup() {
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
  Wire.begin(PIN_SDA, PIN_SCL);
  keyboard.begin(PIN_DATA, PIN_IRQ); 
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  bootButton.attachClick(onBootButtonPressed);
  bootButton.attachLongPressStart(onBootButtonLongPress); 
  // Initialize the radio
  if (!radio.begin()) {
    display.println("Radio hardware error!");
    display.display();
    while (1);
  }

  radio.openReadingPipe(1, address);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_1MBPS);
  radio.startListening();

  display.println("Receiver ready");
  display.display();
  app = communicator::HOME;

  

}

 

void loop() {
  static uint32_t sendCount = 0;
  bootButton.tick();
  // Check if data is available
  //Serial.println(app);
  if (radio.available()) {
   Message receivedMessage; 
   radio.read(&receivedMessage, sizeof(receivedMessage));
   storeMessage(receivedMessage); 
   Serial.println("message read : ");
   app = communicator::NEW_MESSAGE;
  }
   // Serial.println("Status :: ");
   // Serial.println(app);

  if (keyboard.available()) {
    
    // read the next key
    char c = keyboard.read();
    
    // check for some of the special keys
    if (c == PS2_ENTER) {  
         switch(app) {
          case communicator::NEW_MESSAGE:{ 
            app = communicator::WRITE_ADDRESS;
            break;
          }
          case communicator::WRITE_ADDRESS:{
            app = communicator::SET_ADDRESS;
            break;
          } 
          case communicator::WRITE_MESSAGE:{
            app = communicator::SEND_MESSAGE;
            break;
          }
          case communicator::HOME:{
            app = communicator::WRITE_ADDRESS;
            break;
          }
          case communicator::SELECT_MESSAGE:{
            if(!messageInbox.empty()){
               app = communicator::READ_MESSAGE;
            }
           
            break;
          }
          case communicator::READ_MESSAGE:{
            app = communicator::WRITE_ADDRESS;
            break;
          }
         }
    } else if (c == PS2_TAB) {
      
    } else if (c == PS2_ESC) {
          app = communicator::HOME;
    } else if (c == PS2_PAGEDOWN) {
      switch(app) {
          case communicator::WRITE_ADDRESS:{
             addressDown();
            break;
          }
           
         }
    } else if (c == PS2_PAGEUP) {
     switch(app) {
          case communicator::WRITE_ADDRESS:{
            addressUp();
            break;
          }
           
         }
    } else if (c == PS2_LEFTARROW) {
      Serial.print("[Left]");
    } else if (c == PS2_RIGHTARROW) {
      Serial.print("[Right]");
    } else if (c == PS2_UPARROW) {
      switch(app) {
          case communicator::HOME:{
            selected_item = (selected_item > 0) ? selected_item - 1 : 0;
            break;
          }
           
         }
    } else if (c == PS2_DOWNARROW) {
      switch(app) {
          case communicator::HOME:{
            selected_item = (selected_item < (int)messageInbox.size() - 1) ? selected_item + 1 : (int)messageInbox.size() - 1; 
            
            break;
          }
           
         }
   } else if (c == PS2_DELETE) { 
    if((app == communicator::WRITE_MESSAGE || app == communicator::WRITE_ADDRESS )){
         if (!inputText.empty()) {
                inputText.pop_back();   
            }
    }else if(app == communicator::READ_MESSAGE  ){
        app == communicator::DELETE_MESSAGE  ;
    }else if(app == communicator::NEW_MESSAGE){
            app == communicator::DELETE_MESSAGE  ;
    }
   } else {
      
      if((app == communicator::WRITE_MESSAGE ||app == communicator::WRITE_ADDRESS )&& inputText.length() < BUFFER_SIZE){
          inputText += c; 
      }
      
    }

 
  
  }

  switch (app)
  {
   case communicator::READ_MESSAGE:{
       auto it = messageInbox.front();
    for (int i = 0; i < selected_item; i++) {
        messageInbox.pop();
        messageInbox.push(it);
        it = messageInbox.front();
    }
    addressToString(it.address,strSendAddress);
    printHomeScreen(strSendAddress,1,messageInbox.size(),messageInbox.size());  
    //display.println("Message");
    display.setFont(&FreeMono9pt7b); 
    display.setCursor(0, 26); 
    display.println(it.text); 
    display.setFont(); 
    break;
  }   
  case communicator::NEW_MESSAGE:{
      Serial.println(" New message");
  }  
  case communicator::HOME:{
    Serial.println("Start Home");
    printHomeScreen(COMMUNICATION_ADDRESS,1,messageInbox.size(),messageInbox.size()); 
    //display.setCursor(0, 26); 
    //Serial.println(app);
    if (messageInbox.empty()) {
        display.println("<< Inbox Empty >>");
        //return;
    }
    //display.setTextColor(BLACK, WHITE);
    

    display.setTextSize(1);
    display.setTextColor(WHITE);

    int top_pad = (selected_item >= HOME_SCREEN_SIZE) ? selected_item - TOP_MENU_PAD : 0;
    int max_menu_index = std::min((int)messageInbox.size(), top_pad + HOME_SCREEN_SIZE);

    auto inboxCopy = messageInbox;
    for (int i = 0; i < top_pad; i++) {
        inboxCopy.pop();
    }

    for (int i = top_pad; i < max_menu_index; i++) {
        const Message &msg = inboxCopy.front();
        inboxCopy.pop();

        display.setTextColor(i == selected_item ? BLACK : WHITE, WHITE);
        char menuLine[20];
        char output1[7];
        char output2[14];
        char output3[2]="";
        addressToString(msg.address,output1);
        trim_string(output2, msg.text,9);
        if(!msg.isRead){
          output3[0] ='*';
          output3[1] ='\0';
        }
        snprintf(menuLine, sizeof(menuLine), "[%6s] %s %-1s", output1, output2,output3);
        display.println(menuLine);
    }
    break;
  } 
   
  case communicator::WRITE_ADDRESS:{
    printHomeScreen(COMMUNICATION_ADDRESS,1,messageInbox.size(),messageInbox.size()); 
     if(inputText.empty()){
      inputText = strSendAddress;
     }
     display.println("Address");
     display.setTextSize(2); 
     display.setCursor(10, 25); 
     display.println(inputText.c_str());
     
    break;
  } 
  case communicator::SET_ADDRESS:{
    printHomeScreen(COMMUNICATION_ADDRESS,1,messageInbox.size(),messageInbox.size()); 
     if (inputText.length() == 5) {  
      stringToAddress(inputText,sendAddress); 
      app= communicator::WRITE_MESSAGE; 
      inputText ="";
     }else{
      app= communicator::WRITE_ADDRESS;
     }
    
    break;
  } 
  case communicator::WRITE_MESSAGE:{
    addressToString(sendAddress,strSendAddress);
    printHomeScreen(strSendAddress,1,messageInbox.size(),inputText.size());  
    //display.println("Message");
    display.setFont(&FreeMono9pt7b); 
    display.setCursor(0, 26); 
    display.println(inputText.c_str()); 
    display.setFont(); 
      //app= communicator::ERROR;
    break;
  }
  case communicator::SEND_MESSAGE:{ 
     Serial.println("start sending message ...");
    addressToString(sendAddress,strSendAddress);
    Message sendMessage;
    std::memcpy(sendMessage.address, address, 6);
    std::strncpy(sendMessage.text, inputText.c_str(), MAX_STRING_LENGTH);
    inputText ="";
    radio.stopListening(); 
     

    radio.openWritingPipe(sendAddress);
    radio.setRetries(0, 2);
     success = radio.write(&sendMessage, sizeof(sendMessage));
    
     app = communicator::HOME;
    if (success) {
      Serial.println("success");
     app = communicator::HOME;
     radio.startListening();
       uint8_t arc = radio.getARC();
        printHomeScreen(strSendAddress,1,arc,arc); 
        display.setFont(&FreeMono9pt7b); 
    display.setCursor(0, 26); 
    display.println("SENT"); 
    display.setFont(); 
    }else{
      sendCount ++;
       Serial.println("fail");
       printHomeScreen(strSendAddress,1,sendCount,messageInbox.size());  
       //display.println("Message");
    display.setFont(&FreeMono9pt7b); 
    display.setCursor(0, 30); 
    display.println("ERROR ..."); 
    display.setFont();   
    app = communicator::ERROR; 
    }
     
      //app= communicator::ERROR;
    break;
  }
  default:
    break;
  }
 
  display.display(); 
}

void printMenuTitle(const char *top_menu) {
  display.clearDisplay();
  display.setTextSize(1); 
  display.setCursor(0, 0);
  static char menuName1[8] = "";
  trim_string(menuName1,top_menu,8);
  static char menuName[10] = "";
  sprintf(menuName, "< %-8s", F(menuName1));
  display.println(menuName);
  display.setTextSize(1); 
  
}
void printHomeScreen(const char *top_menu,uint32_t ch,uint32_t second_bar,uint32_t count){
   display.clearDisplay();
   display.setTextSize(1); 
   display.setCursor(0, 0);
   display.setTextColor(WHITE);
   static char row1[20] = "";
   sprintf(row1, "[Node] %-6s [ch] %d", F(top_menu),ch);
   display.println(row1);
   static char row2[20] = "";
   std::string  bar = getBarString(second_bar);
   sprintf(row2, "[%15s]  %02d", F(bar.c_str()),count);
   display.println(row2);
 
}

void trim_string(char *str, const char *org, uint32_t length) {
  strcpy(str, org);
  if (strlen(org) > length) {
    str[length] = '\0'; // Null-terminate the string at the desired length
  }
}

std::string getBarString(uint32_t length) {
    if (length <= 0) {
        return "";  
    }
    return std::string(length, '='); 
}

void storeMessage(const Message& receivedMsg) {
    if (messageInbox.size() >= MAX_MESSAGES) {  
        messageInbox.pop();
    } 
    messageInbox.push(receivedMsg);
}
void addressToString(const uint8_t address[6], char *output) { 
    for (int i = 0; i < 6; i++) {
        output[i] = (char)address[i];
    }
    output[6] = '\0';
}
void stringToAddress(const std::string &input, uint8_t address[6]) { 
    for (int i = 0; i < input.length(); ++i) {  
        address[i] = (int)input[i];
        
    }  
}

void addressUp() {
    int number =0;
    if(!inputText.empty()){
       number = std::stoi(inputText);
    }   
    number++;                    
    inputText = std::to_string(number);  
 
    while (inputText.length() < 5) {
        inputText = "0" + inputText;
    }
}

 
void addressDown() {
    int number =0;
    if(!inputText.empty()){
       number = std::stoi(inputText);
    }   
    if (number > 0) {  
        number--; 
    }
    inputText = std::to_string(number);  
 
    while (inputText.length() < 5) {
        inputText = "0" + inputText;
    }
}

 
void onBootButtonPressed() {
   stringToAddress("00001",sendAddress);
   long time = millis();
   inputText = "PING @ " + std::to_string(time);
   app = communicator::SEND_MESSAGE;
}

 
void onBootButtonLongPress() {
    if(messageInbox.empty()){
       onBootButtonPressed();
    }
    else{
      selected_item = 0;
    app = communicator::READ_MESSAGE;
    }
}