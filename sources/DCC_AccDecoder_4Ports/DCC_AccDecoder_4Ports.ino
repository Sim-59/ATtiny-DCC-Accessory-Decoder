// DCC Accessories decoder
// (c) Michael Hochmuth https://github.com/Sim-59/ATtiny-Accessory-Decoder                          2026-04-15
// 4 output ports
// using two module addresses (MADA) with 4 ports, every port with a gate for on/off
// second address for blinking
// CV reading at programming track (PT) is possible with a temporarily circuit for 60 mA load at one port
//
// Decoder-Address = LSB + MSB*64
// CV1 = 6 bit LSB, 
// x x x x  x x x x
//     +-+--+-+-+-+----- LSB 0 ... 63
//
// CV9 = 3 Bit MSB
// x x x x  x x x x
//            +-+-+----- MSB (0 ... 7) * 64
//
// CV10 - default 0 for independent ports
// x x x x  x x x x
//              +-+----- "0-0" (0) = independent PORT1/PORT2, 
//                       "0-1" (1) = alternating PORT1/PORT2 with Port1-gate on/off
//                       "1-0" (2) = alternating PORT1/PORT2 with Port1-gate on / Port2-gate on
//
// CV11 - default 0b01000000 (64) for 1 sec blink frequency
// x x x x  x x x x
// | | | +--+-+-+-+----- 5 bit pulse length if PORT1 and PORT2 are alternating ports, number * 256 ms (256 ms ... 7.93 sec)
// | | |                 e.g. usable for switches without voltage cut-off
// | | |                 0  (all 5 bits) for permanently ON
// +-+-+---------------- 3 bit for blinking periode in s (0.5 ... 3.5 sec) 
//
// A blinking output is generated with writing to Address+1 and corresponding port number
// but only if a blinking periode in CV11 is set and port is defined as independent
//
// CV29
// x x x x  x x x x 
// | +------------------ "0" = Decoder Address Mode, "1" = Output Address Mode 
// +-------------------- "1" = Accessory Decoder Mode, is set for accessories
//
// writing to CV8 is resetting the decoder
//    CV1 = 1 default accessory address-LSB 
//    CV9 = 0 default accessory address-MSB 
//    CV10 = 0
//    CV11 = 64
//


#include <NmraDcc.h>
NmraDcc Dcc;

//define the destination board
//#define UNO

#if defined UNO
  #define DCC_PIN         2
  #define DCC_ACK_PIN     12
  #define PORT1_PIN       3
  #define PORT2_PIN       4
  #define PORT3_PIN       5
  #define PORT4_PIN       6
  #define PROG_NEXT_PIN   8
  #define DEBUG
  long int debounce;
#else                           // for ATtiny85
  #define DCC_PIN         2
  #define DCC_ACK_PIN     3     // 60 mA-circuit can be temporarily connected to pin 4 for CV reading
  #define PORT1_PIN       0
  #define PORT2_PIN       1
  #define PORT3_PIN       3
  #define PORT4_PIN       4
#endif

#define CV_ACCESSORY_DECODER_MODE  10
#define CV_ACCESSORY_TIME_MODE     11

#define MODE_INDEPENDENT_PORTS 0
#define MODE_ALTERNATING_PORTS1 1
#define MODE_ALTERNATING_PORTS2 2


struct CVPair {
  uint16_t CV;
  uint8_t Value;
};

int AccDecoderAddr, AccPortMode, SwitchOnTime, BlinkPeriod;
bool ProgModeActivated = false;
bool SwitchTimeActivated = false;
bool BlinkPort1 = false;
bool BlinkPort2 = false;
bool BlinkPort3 = false;
bool BlinkPort4 = false;

unsigned long currentPortMillis, startPortMillis;

// This function is called whenever a normal DCC Turnout Packet is received and we're in Board Addressing Mode, CV29-Bit6 = 0
void notifyDccAccTurnoutBoard( uint16_t BoardAddr, uint8_t OutputPair, uint8_t Direction, uint8_t OutputPower ) {
  #if defined DEBUG
    Serial.print("notifyDccAccTurnoutBoard: ") ;
    Serial.print(BoardAddr,DEC) ;
    Serial.print(',');
    Serial.print(OutputPair,DEC) ;
    Serial.print(',');
    Serial.print(Direction,DEC) ;
    Serial.print(',');
    Serial.println(OutputPower, HEX) ;
  #endif

  Run_MADA(BoardAddr, OutputPair+1, Direction);
}

// This function is called whenever a normal DCC Turnout Packet is received and we're in Output Addressing Mode, CV29-Bit6 = 1
void notifyDccAccTurnoutOutput( uint16_t Addr, uint8_t Direction, uint8_t OutputPower ) {
  #if defined DEBUG
    Serial.print("notifyDccAccTurnoutOutput: ") ;
    Serial.print(Addr,DEC) ;
    Serial.print(',');
    Serial.print(Direction,DEC) ;
    Serial.print(',');
    Serial.println(OutputPower, HEX) ;
  #endif

  uint16_t BoardAddr = int(Addr-1) / 4 + 1;
  uint8_t Port = int(Addr-1) % 4 + 1;

  Run_MADA(BoardAddr, Port, Direction);
}

void Run_MADA( uint16_t MADA_addr, uint8_t MADA_port, uint8_t MADA_gate ) {
 
  startPortMillis = millis();

  #if defined DEBUG  
    Serial.print("MADA-Adresse=");
    Serial.print(MADA_addr, DEC);
    Serial.print(", MADA-Port=");
    Serial.print(MADA_port, DEC);
    Serial.print(", MADA_gate=");
    Serial.println(MADA_gate, DEC);
  #endif

// check if the command is for our address and output
  if((MADA_addr == AccDecoderAddr) && (MADA_port == 1)) {
    if(MADA_gate == 1) {
      digitalWrite(PORT1_PIN, HIGH);
      SwitchTimeActivated = true;
      #if defined DEBUG  
        Serial.println("PORT1 activated");   
      #endif   

      if (((AccPortMode & 0x03) == MODE_ALTERNATING_PORTS1) || ((AccPortMode & 0x03) == MODE_ALTERNATING_PORTS2))  {
        digitalWrite(PORT2_PIN, LOW);
        #if defined DEBUG  
          Serial.println("PORT2 deactivated");
        #endif
      }
    } else {
      if ((AccPortMode & 0x03) == MODE_ALTERNATING_PORTS1) {
        digitalWrite(PORT2_PIN, HIGH);
        SwitchTimeActivated = true;
        #if defined DEBUG  
          Serial.println("PORT2 activated");
        #endif
        digitalWrite(PORT1_PIN, LOW);
        #if defined DEBUG  
          Serial.println("PORT1 deactivated");
        #endif
      }
    }
  }

  if((MADA_addr == AccDecoderAddr) && (MADA_port == 2) && (AccPortMode & 0x03) == MODE_ALTERNATING_PORTS2) {
    if(MADA_gate == 1) {
      digitalWrite(PORT2_PIN, HIGH);
      SwitchTimeActivated = true;
      #if defined DEBUG  
        Serial.println("PORT2 activated");  
      #endif   
      digitalWrite(PORT1_PIN, LOW);
      #if defined DEBUG  
        Serial.println("PORT1 deactivated");
      #endif
    }
  }

  if(((MADA_addr == AccDecoderAddr) && (MADA_port == 2)) && ((AccPortMode & 0x03) == MODE_INDEPENDENT_PORTS)) {
    if(MADA_gate == 0) {
      digitalWrite(PORT2_PIN, LOW);
      BlinkPort2 = false;
      #if defined DEBUG  
        Serial.println("PORT2 deactivated");
      #endif
    } else {
      digitalWrite(PORT2_PIN, HIGH);
      BlinkPort2 = false;
      #if defined DEBUG  
        Serial.println("PORT2 activated");
      #endif      
    }
  }

  if((MADA_addr == AccDecoderAddr) && (MADA_port == 3)) {
    if(MADA_gate == 0) {
      digitalWrite(PORT3_PIN, LOW);
      BlinkPort3 = false;
      #if defined DEBUG  
        Serial.println("PORT3 deactivated");
      #endif
    } else {
      digitalWrite(PORT3_PIN, HIGH);
      BlinkPort3 = false;
      #if defined DEBUG  
        Serial.println("PORT3 activated"); 
      #endif     
    }
  }

  if((MADA_addr == AccDecoderAddr) && (MADA_port == 4)) {
    if(MADA_gate == 0) {
      digitalWrite(PORT4_PIN, LOW);
      BlinkPort4 = false;
      #if defined DEBUG  
        Serial.println("PORT4 deactivated");
      #endif
    } else {
      digitalWrite(PORT4_PIN, HIGH);
      BlinkPort4 = false;
      #if defined DEBUG  
        Serial.println("PORT4 activated"); 
      #endif     
    }
  }

  if((MADA_addr == AccDecoderAddr+1) && (MADA_port == 1) && ((AccPortMode & 0x03) == MODE_INDEPENDENT_PORTS)) {
    if(MADA_gate == 0) {
      BlinkPort1 = false;
      // Restore Initial State
      digitalWrite(PORT1_PIN, LOW);
      #if defined DEBUG  
        Serial.println("BlinkPort1 off"); 
      #endif     
    } else {
      BlinkPort1 = true;
      #if defined DEBUG  
        Serial.println("BlinkPort1 on"); 
      #endif     
    }
  }
  if((MADA_addr == AccDecoderAddr+1) && (MADA_port == 2) && ((AccPortMode & 0x03) == MODE_INDEPENDENT_PORTS)) {
    if(MADA_gate == 0) {
      BlinkPort2 = false;
      digitalWrite(PORT2_PIN, LOW);
      #if defined DEBUG  
        Serial.println("BlinkPort2 off"); 
      #endif     
    } else {
      BlinkPort2 = true;
      #if defined DEBUG  
        Serial.println("BlinkPort2 on"); 
      #endif     
    }
  }
  if((MADA_addr == AccDecoderAddr+1) && (MADA_port == 3)) {
    if(MADA_gate == 0) {
      BlinkPort3 = false;
      digitalWrite(PORT3_PIN, LOW);
      #if defined DEBUG  
        Serial.println("BlinkPort3 off"); 
      #endif     
    } else {
      BlinkPort3 = true;
      #if defined DEBUG  
        Serial.println("BlinkPort3 on"); 
      #endif     
    }
  }
  if((MADA_addr == AccDecoderAddr+1) && (MADA_port == 4)) {
    if(MADA_gate == 0) {
      BlinkPort4 = false;
      digitalWrite(PORT4_PIN, LOW);
      #if defined DEBUG  
        Serial.println("BlinkPort4 off"); 
      #endif     
    } else {
      BlinkPort4 = true;
//      digitalWrite(PORT4_PIN, HIGH);
      #if defined DEBUG  
        Serial.println("BlinkPort4 on"); 
      #endif     
    }
  }
}

CVPair FactoryDefaultCVs[] = {
  // These two CVs define the Long DCC Address, CV1 = 6 bit LSB, CV9 = 3 bit MSB
  {CV_ACCESSORY_DECODER_ADDRESS_MSB, 0},
  {CV_ACCESSORY_DECODER_ADDRESS_LSB, DEFAULT_ACCESSORY_DECODER_ADDRESS},
  {CV_29_CONFIG, CV29_ACCESSORY_DECODER | CV29_OUTPUT_ADDRESS_MODE},
  {CV_ACCESSORY_DECODER_MODE,MODE_INDEPENDENT_PORTS}, // independent ports
  {CV_ACCESSORY_TIME_MODE,0b01000000},                // 1s blink frequeny
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

  int cv29_Bits;

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
    cv29_Bits = Dcc.getCV(CV_29_CONFIG);
    AccPortMode = Dcc.getCV(CV_ACCESSORY_DECODER_MODE);
    SwitchOnTime = Dcc.getCV(CV_ACCESSORY_TIME_MODE) & 0x1F;
    BlinkPeriod = (Dcc.getCV(CV_ACCESSORY_TIME_MODE) & 0xE0) >> 5;

    #if defined DEBUG  
        Serial.print(Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_MSB));
        Serial.print("-");
        Serial.println(Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_LSB));
        Serial.print("Adresse:      "); Serial.println(AccDecoderAddr);
        Serial.print("CV29:         "); Serial.println(cv29_Bits,BIN);
        if (AccPortMode & 0x01) Serial.println("PORT1/2 alternaring mode"); else Serial.println("PORT1/2 independent mode");
        Serial.print("Pulslänge:    "); Serial.println(SwitchOnTime);
        Serial.print("Blinkperiode: "); Serial.println(BlinkPeriod);
    #endif
  }

  // configure pins
  pinMode(PORT1_PIN, OUTPUT);
  pinMode(PORT2_PIN, OUTPUT);
  pinMode(PORT3_PIN, OUTPUT);
  pinMode(PORT4_PIN, OUTPUT);
  #if defined UNO
    pinMode(PROG_NEXT_PIN, INPUT_PULLUP);
  #endif
  pinMode(DCC_ACK_PIN, OUTPUT);
  digitalWrite(PORT1_PIN, LOW);
  digitalWrite(PORT2_PIN, LOW);
  digitalWrite(PORT3_PIN, LOW);
  digitalWrite(PORT4_PIN, LOW);

  // init NmraDcc library (PIN, manufacturer, version...) 
  Dcc.pin(digitalPinToInterrupt(DCC_PIN), DCC_PIN, 1);
  Dcc.initAccessoryDecoder(MAN_ID_DIY, 10, cv29_Bits & FLAGS_OUTPUT_ADDRESS_MODE, 0);   // CV8=Manufacturer-ID=13, CV7=Manufacturer-VERS=10

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
  if (((AccPortMode & 0x03) != 0) && (SwitchOnTime != 0) && SwitchTimeActivated == true) {
    int PortMillis = SwitchOnTime << 8;
    if ((currentPortMillis-startPortMillis > PortMillis)) {
      SwitchTimeActivated = false;
      digitalWrite(PORT1_PIN, LOW);
      digitalWrite(PORT2_PIN, LOW);
      #if defined DEBUG  
        Serial.println("PORT1 and PORT2 deactivated");
      #endif
    }
  }

  // BlinkPort PORT1 and PORT2 if they are not in impulse mode
  if (BlinkPeriod && BlinkPort1 && ((AccPortMode & 0x01) == MODE_INDEPENDENT_PORTS)) {
    if (((currentPortMillis-startPortMillis)  % (BlinkPeriod*500)) < BlinkPeriod*250) {
      digitalWrite(PORT1_PIN, HIGH);
    } else {
      digitalWrite(PORT1_PIN, LOW);
    }
  }

  if (BlinkPeriod && BlinkPort2 && ((AccPortMode & 0x01) == MODE_INDEPENDENT_PORTS)) {
    if (((currentPortMillis-startPortMillis)  % (BlinkPeriod*500)) < BlinkPeriod*250) {
      digitalWrite(PORT2_PIN, HIGH);
    } else {
      digitalWrite(PORT2_PIN, LOW);
    }
  }

  // BlinkPort PORT3
  if (BlinkPeriod && BlinkPort3) {
    if (((currentPortMillis-startPortMillis) % (BlinkPeriod*500)) < BlinkPeriod*250) {
      digitalWrite(PORT3_PIN, HIGH);
    } else {
      digitalWrite(PORT3_PIN, LOW);
    }
  }

  // BlinkPort PORT4
  if (BlinkPeriod && BlinkPort4) {
    if (((currentPortMillis-startPortMillis) % (BlinkPeriod*500)) < BlinkPeriod*250) {
      digitalWrite(PORT4_PIN, HIGH);
    } else {
      digitalWrite(PORT4_PIN, LOW);
    }
  }

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
