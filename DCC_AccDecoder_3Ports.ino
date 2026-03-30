// Accessories decoder with programming function at programming track
// (c) Michael Hochmuth https://github.com/Sim-59/ATtiny-Accessories-Decoder
// Some ideas from Luca Dentella, 29.12.2018 https://github.com/lucadentella/arduino-dcc
// Decoder for 3 outputs, one module address with 3 output ports, every port with 2 gates, port4 for blinking
// Decoder-Address = LSB + MSB*64
// CV1 = 6 bit LSB, 
// x x x x  x x x x
//     +-+--+-+-+-+----- LSB 0 ... 63
//
// CV9 = 3 Bit MSB
// x x x x  x x x x
//            +-+-+----- MSB (0 ... 7) * 64
//
// setting CV8 = 0 is resetting the decoder, default accessories address-LSB CV1 = 1, Address-MSB CV9 = 0, CV10 = 0, CV11 = 0
//
// PORT1 and PORT2 as independent or alternating ports, 
// impulse length for alternating ports is configurable
// PORT3 is always independent
//
// CV10
// x x x x  x x x x
//          | | | +----- 0 = independent PORTS1/PORT2, 1 = alternating PORT1/PORT2, e.g. signals, switches
//          +-+-+------- 0 = static PORT3, PORT2, PORT1, 1 = Blinking mode is possible
//                       Blinking will be activatet with writing port4, gate=1 and deactivated with gate=0, 
//                       but only if a blinking periode in CV11 is given and impulse length = 0 = static
// CV11
// x x x x  x x x x
// | | | +--+-+-+-+----- 5 bit pulse length if PORT1 and PORT2 are alternating ports, number * 256 ms (256 ms ... 7.93 s)
// | | |                 e.g. usable for switches without voltage cut-off
// | | |                 if this value is 0  (all 5 bits), then the respective port is the complete time ON
// +-+-+---------------- 3 bit for blinking periode in s (1 ... 7 s, [s * 64]) 
//                       Blinking will be activatet with writing port4, gate=1 and deactivated with gate=0, 
//                       but only if a blinking periode is given and impulse length = 0 = static

// include NmraDcc library

#include <NmraDcc.h>
NmraDcc Dcc;

//define the destination board
//#define UNO
#define ATTINY85

// for Tests, within the first 10s after poweron or reset the first command programs the decoder address 
// #define BOOTPROGMODE

#if defined UNO
  #define DCC_PIN         2
  #define DCC_ACK_PIN     12
  #define PORT1_PIN       3
  #define PORT2_PIN       4
  #define PORT3_PIN       5
  #define PROG_NEXT_PIN   6
  #define DEBUG
#endif

#if defined ATTINY85
  #define DCC_PIN         2
  #define DCC_ACK_PIN     3
  #define PORT1_PIN       0
  #define PORT2_PIN       1
  #define PORT3_PIN       4
#endif

#define CV_ACCESSORY_DECODER_MODE  10
#define CV_ACCESSORY_TIME_MODE     11

#define MODE_INDEPENDENT_PORTS 0
#define MODE_ALTERNATING_PORTS 1
#define PORT1_BLINK_MODE 2
#define PORT2_BLINK_MODE 4
#define PORT3_BLINK_MODE 8


struct CVPair {
  uint16_t CV;
  uint8_t Value;
};

int AccDecoderAddr, AccPortMode, SwitchOnTime, BlinkPeriod;
bool ProgModeActivated = false;
bool SwitchTimeActivated = false;
bool BlinkMode = false;
bool Port1active = false;
bool Port2active = false;
bool Port3active = false;

#if defined UNO
  bool BootProgMode = false;
  int debounce;
  unsigned long currentBootMillis, startBootMillis;
#endif

unsigned long currentPortMillis, startPortMillis;

void notifyDccAccState(uint16_t Addr, uint16_t BoardAddr, uint8_t OutputAddr, uint8_t State) {
 
  int port = (OutputAddr >> 1) + 1;
  int gate = OutputAddr & 0x01;

  startPortMillis = millis();

  #if defined DEBUG  
    Serial.print("Packet received (BoardAddr=");
    Serial.print(BoardAddr, DEC);
    Serial.print(", port=");
    Serial.print(port, DEC);
    Serial.print(", gate=");
    Serial.print(gate, DEC);
  #endif

// check if the command is for our address and output
  if((BoardAddr == AccDecoderAddr) && (port == 1)) {
    if(gate == 0) {
      if ((AccPortMode & 0x01) == MODE_ALTERNATING_PORTS) {
        digitalWrite(PORT2_PIN, HIGH);
        Port2active = true;
        SwitchTimeActivated = true;
        #if defined DEBUG  
          Serial.println("PORT2 activated");
        #endif
      }
      digitalWrite(PORT1_PIN, LOW);
      Port1active = false;
      #if defined DEBUG  
        Serial.println("PORT1 deactivated");
      #endif
    } else {  // gate == 1
      if ((AccPortMode & 0x01) == MODE_ALTERNATING_PORTS) {
        digitalWrite(PORT2_PIN, LOW);
        Port2active = false;
        #if defined DEBUG  
          Serial.println("PORT2 deactivated");
        #endif
      }
      digitalWrite(PORT1_PIN, HIGH);
      Port1active = true;
      SwitchTimeActivated = true;
      #if defined DEBUG  
        Serial.println("PORT1 activated");   
      #endif   
    }
  }

  if((BoardAddr == AccDecoderAddr) && (port == 2) && ((AccPortMode & 0x01) == MODE_INDEPENDENT_PORTS)) {
    if(gate == 0) {
      digitalWrite(PORT2_PIN, LOW);
      Port2active = false;
      #if defined DEBUG  
        Serial.println("PORT2 deactivated");
      #endif
    } else {
      digitalWrite(PORT2_PIN, HIGH);
      Port2active = true;
      #if defined DEBUG  
        Serial.println("PORT2 activated");
      #endif      
    }
  }

  if((BoardAddr == AccDecoderAddr) && (port == 3)) {
    if(gate == 0) {
      digitalWrite(PORT3_PIN, LOW);
      Port3active = false;
      #if defined DEBUG  
        Serial.println("PORT3 deactivated");
      #endif
    } else {
      digitalWrite(PORT3_PIN, HIGH);
      Port3active = true;
      #if defined DEBUG  
        Serial.println("PORT3 activated"); 
      #endif     
    }
  }

  if((BoardAddr == AccDecoderAddr) && (port == 4)) {
    if(gate == 0) {
      BlinkMode = false;
      // Herstellen Ausgangszustand
      if(Port1active) digitalWrite(PORT1_PIN, HIGH);
      if(Port2active) digitalWrite(PORT2_PIN, HIGH);
      if(Port3active) digitalWrite(PORT3_PIN, HIGH);
      #if defined DEBUG  
        Serial.println("BlinkMode off"); 
      #endif     
    } else {
      BlinkMode = true;
      delay(10);
      #if defined DEBUG  
        Serial.println("BlinkMode on"); 
      #endif     
    }
  }

}

CVPair FactoryDefaultCVs[] = {
  // These two CVs define the Long DCC Address, CV1 = 6 bit LSB, CV9 = 3 bit MSB
  {CV_ACCESSORY_DECODER_ADDRESS_MSB, 0},
  {CV_ACCESSORY_DECODER_ADDRESS_LSB, DEFAULT_ACCESSORY_DECODER_ADDRESS},
  {CV_29_CONFIG, CV29_ACCESSORY_DECODER},
  {CV_ACCESSORY_DECODER_MODE,MODE_INDEPENDENT_PORTS},
  {CV_ACCESSORY_TIME_MODE,0},
};

//uint8_t FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs) / sizeof(CVPair);
// set to 0, to ensure that no reset init is performed within the loop
uint8_t FactoryDefaultCVIndex = 0;

void notifyCVResetFactoryDefault() {
  // Make FactoryDefaultCVIndex non-zero and equal to num CV's to be reset
  // to flag to the loop() function that a reset to Factory Defaults needs to be done
  FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs) / sizeof(CVPair);
};

// This function is called by the NmraDcc library when a DCC ACK needs to be sent
// Calling this function should cause an increased 60ma current drain on the power supply for 6ms to ACK a CV Read 
void notifyCVAck(void) {
  digitalWrite(DCC_ACK_PIN, HIGH );
  delay( 8 );  
  digitalWrite(DCC_ACK_PIN, LOW );
  #if defined DEBUG  
    Serial.println("notifyCVAck") ;
  #endif
}

void setup() {

  // Serial output for debugging
  #if defined DEBUG  
    Serial.begin(115200);
    Serial.println("DCC Accessory Decoder");
    Serial.println();
  #endif


  if ((Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_LSB) == 255) || (Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_MSB) == 255)) {
    FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs) / sizeof(CVPair);
  } else {
    AccDecoderAddr = (Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_LSB & 0x03F)) + (Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_MSB) << 6);
    AccPortMode = Dcc.getCV(CV_ACCESSORY_DECODER_MODE);
    SwitchOnTime = Dcc.getCV(CV_ACCESSORY_TIME_MODE) & 0x1F;
    BlinkPeriod = (Dcc.getCV(CV_ACCESSORY_TIME_MODE) & 0xE0) >> 5;

    #if defined DEBUG  
        Serial.print(Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_MSB));
        Serial.print("-");
        Serial.println(Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_LSB));
        Serial.print("Adresse:      "); Serial.println(AccDecoderAddr);
        if (AccPortMode & 0x01) Serial.println("PORT1/2 alternaring mode"); else Serial.println("PORT1/2 independent mode");
        if (AccPortMode & 0x02) Serial.println("Port1 BlinkMode enabled");
        if (AccPortMode & 0x04) Serial.println("Port2 BlinkMode enabled");
        if (AccPortMode & 0x08) Serial.println("Port3 BlinkMode enabled");
        Serial.print("Pulslänge:    "); Serial.println(SwitchOnTime);
        Serial.print("Blinkperiode: "); Serial.println(BlinkPeriod);
    #endif
  }

  // configure pins
  pinMode(PORT1_PIN, OUTPUT);
  pinMode(PORT2_PIN, OUTPUT);
  pinMode(PORT3_PIN, OUTPUT);
  #if defined UNO
    pinMode(PROG_NEXT_PIN, INPUT_PULLUP);
  #endif
  pinMode(DCC_ACK_PIN, OUTPUT);
  digitalWrite(PORT1_PIN, LOW);
  digitalWrite(PORT2_PIN, LOW);
  digitalWrite(PORT3_PIN, LOW);

  // init NmraDcc library (PIN, manufacturer, version...) 
  Dcc.pin(digitalPinToInterrupt(DCC_PIN), DCC_PIN, 1);
  Dcc.initAccessoryDecoder(MAN_ID_DIY, 10, 0, 0);   // CV8=Manufacturer-ID=13, CV7=Manufacturer-VERS=10

  #if defined BOOTPROGMODE
    startBootMillis = millis();
    Dcc.setAccDecDCCAddrNextReceived(1);
    #if defined DEBUG
      Serial.println("BootProgMode on");
    #endif
    BootProgMode=true;
  #endif

  pinMode(DCC_ACK_PIN, OUTPUT);
  digitalWrite(DCC_ACK_PIN, LOW);

  #if defined DEBUG  
    Serial.println("Decoder initialized");
  #endif
}

void loop() {
  Dcc.process();

  currentPortMillis = millis();

  // Set to default values
  if (FactoryDefaultCVIndex && Dcc.isSetCVReady()) {
    FactoryDefaultCVIndex--;  // Decrement first as initially it is the size of the array
    Dcc.setCV(FactoryDefaultCVs[FactoryDefaultCVIndex].CV, FactoryDefaultCVs[FactoryDefaultCVIndex].Value);
    #if defined DEBUG  
        Serial.println("Reset");
    #endif
  }

  // ImpulsMode at alternating PORT!/PORT2
  if ((AccPortMode == MODE_ALTERNATING_PORTS) && (SwitchOnTime != 0)) {
    int PortMillis = SwitchOnTime << 8;
    if ((currentPortMillis-startPortMillis > PortMillis) && SwitchTimeActivated == true) {
      SwitchTimeActivated = false;
      digitalWrite(PORT1_PIN, LOW);
      digitalWrite(PORT2_PIN, LOW);
      #if defined DEBUG  
        Serial.println("PORT1 and PORT2 deactivated");
      #endif
    }
  }

  // BlinkMode PORT1 and PORT2 if they are not in impulse mode
  if (((AccPortMode & PORT1_BLINK_MODE) == PORT1_BLINK_MODE) && Port1active && BlinkPeriod && BlinkMode && SwitchOnTime == 0) {
    if (((currentPortMillis-startPortMillis)  % (BlinkPeriod*1000)) < BlinkPeriod*500) {
      digitalWrite(PORT1_PIN, HIGH);
    } else {
      digitalWrite(PORT1_PIN, LOW);
    }
  }

  if (((AccPortMode & PORT2_BLINK_MODE) == PORT2_BLINK_MODE) && Port2active && BlinkPeriod && BlinkMode && SwitchOnTime == 0) {
    if (((currentPortMillis-startPortMillis)  % (BlinkPeriod*1000)) < BlinkPeriod*500) {
      digitalWrite(PORT2_PIN, HIGH);
    } else {
      digitalWrite(PORT2_PIN, LOW);
    }
  }

  // BlinkMode PORT3
  if (((AccPortMode & PORT3_BLINK_MODE) == PORT3_BLINK_MODE) && Port3active && BlinkPeriod && BlinkMode) {
    if (((currentPortMillis-startPortMillis) % (BlinkPeriod*1000)) < BlinkPeriod*500) {
      digitalWrite(PORT3_PIN, HIGH);
    } else {
      digitalWrite(PORT3_PIN, LOW);
    }
  }

  // For test - Wait 10 seconds for the first command to programme the address at boot time
  #if defined BOOTPROGMODE
    if ((currentBootMillis-startBootMillis > 10000) && BootProgMode==true) {
      BootProgMode=false;
      Dcc.setAccDecDCCAddrNextReceived(0);
      #if defined DEBUG  
          Serial.println("BootProgMode off");
      #endif
    } 
  #endif

  // Wait for the first command to programme the address after setting PROG_NEXT_PIN to LOW and releasing
  #if defined UNO
    // Set address to first incoming command
    if (debounce > 0) debounce--;
    if ((digitalRead(PROG_NEXT_PIN) == 0) && (ProgModeActivated == false) && (debounce == 0)) {
      ProgModeActivated = true;
      debounce = 30000;
      Dcc.setAccDecDCCAddrNextReceived(1);
      #if defined DEBUG  
          Serial.println("ProgModeKey pressed");
      #endif
    }
    if ((digitalRead(PROG_NEXT_PIN) == 1) && (debounce == 0) && (ProgModeActivated == true)) {
      ProgModeActivated = false;    
      debounce = 30000;
      #if defined DEBUG  
          Serial.println("ProgModeKey released");
      #endif
    }
  #endif
}
