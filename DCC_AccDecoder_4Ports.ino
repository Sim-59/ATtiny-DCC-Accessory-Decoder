// DCC Accessories decoder
// (c) Michael Hochmuth https://github.com/Sim-59/ATtiny-Accessory-Decoder                          2026-06-01
// 4 output ports
// using two module addresses (MADA) with 4 ports, every port with a gate for on/off
// second address for blinking
// CV reading at programming track (PT) is possible with a temporarily circuit for 60 mA load at one port
//
// Modul Address = LSB + MSB*64, 4 Ports at Modul, with CV29-Bit6 = 0, 
//      CV1 = 6 bit LSB, default 1
//      x x x x  x x x x
//          +-+--+-+-+-+----- LSB 0 ... 63 für Decoder (Modul) Address Mode
//      CV9 = 3 Bit MSB, default 0
//      x x x x  x x x x
//                 +-+-+----- MSB (0 ... 7) * 64
//
// Output Address = LSB + MSB*256 with CV29-Bit6 = 1
//      CV1 = 8 bit LSB, default 1
//      x x x x  x x x x
//      +-+-+-+--+-+-+-+----- LSB 0 ... 255 für Output Address Mode
//      CV9 = 3 Bit MSB, default 0
//      x x x x  x x x x
//                 +-+-+----- MSB (0 ... 7) * 256
//
// CV3 / CV4 - ON TIME for PORT1 / PORT2, default 0 = continous
// x x x x  x x x x
//       +--+-+-+-+----- 5 bit pulse length if PORT1 and PORT2 are alternating ports, number * 256 ms (256 ms ... 7.93 sec)
//                       e.g. usable for switches without voltage cut-off
//                       0  (all 5 bits) for continous ON if PORT is activated
// 
// CV33 - default 0 for independent ports
// x x x x  x x x x
//              +-+----- "0-0" (0) = independent PORT1/PORT2, default 
//                       "0-1" (1) = alternating PORT1/PORT2 with Port1-gate on/off
//                       "1-0" (2) = alternating PORT1/PORT2 with Port1-gate on / Port2-gate on
//
// CV34, default 0b00000100 (4) for 1 sec blink frequency
// x x x x  x x x x
//          +-+-+-+------ 4 bit for blinking periode in s (0.25 ... 3.75 sec) 
//
// A blinking output is generated with writing to Modul-Address + 1 and corresponding port number 
// or Output-Address + 4 in Output Address Mode
// but only if a blinking periode in CV11 is set and port is defined as independent
//
// CV29 = Konfiguration, default 128
// x x x x  x x x x 
// | +------------------ "0" = Decoder Address Mode, "1" = (64) Output Address Mode 
// +-------------------- "1" = (128) Accessory Decoder Mode, is set for accessories
//
// writing to CV8 is resetting the decoder
//    CV1 = 1 default accessory address-LSB 
//    CV9 = 0 default accessory address-MSB 
//    CV3 = 0
//    CV4 = 0
//    CV29 = 128, Dekoder Address Mode
//    CV33 = 0
//    CV34 = 4
//

#include <NmraDcc.h>
NmraDcc Dcc;

//define the destination board
//#define UNO
#define PCB_10
//#define PCB_11

#if defined UNO
  #define DCC_PIN         2
  #define DCC_ACK_PIN     12
  #define PORT1_PIN       3
  #define PORT2_PIN       4
  #define PORT3_PIN       5
  #define PORT4_PIN       6
  #define PROG_NEXT_PIN   13
  #define DEBUG
 
#elif defined PCB_10            // for ATtiny85 old PCB version MuFu4P 1.0
  #define DCC_PIN         2
  #define DCC_ACK_PIN     3     // 60 mA-circuit can be temporarily connected to pin 4 for CV reading
  #define PORT1_PIN       0     // ws
  #define PORT2_PIN       1     // ge
  #define PORT3_PIN       3     // gn (optACK)
  #define PORT4_PIN       4     // vi

#elif defined PCB_11            // for ATtiny85 PCB MuFu4P 1.1 and  MuFuDec 1.0
  #define DCC_PIN         2
  #define DCC_ACK_PIN     3     // 60 mA-circuit can be temporarily connected to pin 4 for CV reading
  #define PORT1_PIN       0     // ws
  #define PORT2_PIN       4     // ge
  #define PORT3_PIN       1     // gn
  #define PORT4_PIN       3     // vi (optACK)
#endif

#define CV_PORT1_TIME_ON  3
#define CV_PORT2_TIME_ON  4

#define CV_ACCESSORY_DECODER_MODE  33
#define CV_BLINK_TIME              34

#define MODE_INDEPENDENT_PORTS  0
#define MODE_ALTERNATING_PORTS1 1
#define MODE_ALTERNATING_PORTS2 2


struct CVPair {
  uint16_t CV;
  uint8_t Value;
};

int AccDecoderAddr, AccPortMode, SwitchOnTime1, SwitchOnTime2, BlinkPeriod, OutputAddr, AccDecoderPort;
bool ProgModeActivated = false;
bool SwitchTime1Activated = false;
bool SwitchTime2Activated = false;
bool BlinkPort1 = false;
bool BlinkPort2 = false;
bool BlinkPort3 = false;
bool BlinkPort4 = false;

unsigned long currentPortMillis, startBlinkMillis, startPort1Millis, startPort2Millis;

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
 
  startBlinkMillis = millis();

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
      SwitchTime1Activated = true;
      startPort1Millis = millis();
      #if defined DEBUG  
        Serial.println("PORT1 activated");   
      #endif   

      if (((AccPortMode & 0x03) == MODE_ALTERNATING_PORTS1) || ((AccPortMode & 0x03) == MODE_ALTERNATING_PORTS2))  {
        digitalWrite(PORT2_PIN, LOW);
        BlinkPort2 = false;
        #if defined DEBUG  
          Serial.println("PORT2 deactivated");
        #endif
      }
    } else {
      if ((AccPortMode & 0x03) == MODE_ALTERNATING_PORTS1) {
        digitalWrite(PORT2_PIN, HIGH);
        BlinkPort2 = false;
        SwitchTime2Activated = true;
        startPort2Millis = millis();
        #if defined DEBUG  
          Serial.println("PORT2 activated");
        #endif
        digitalWrite(PORT1_PIN, LOW);
        BlinkPort1 = false;
        #if defined DEBUG  
          Serial.println("PORT1 deactivated");
        #endif
      }
      if (((AccPortMode & 0x03) == MODE_INDEPENDENT_PORTS) &&  (SwitchOnTime2 == 0)) {  // Pulslänge in jedem Fall, auch wenn Gate zurückschaltet
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
      SwitchTime2Activated = true;
      startPort2Millis = millis();
      BlinkPort2 = false;
      #if defined DEBUG  
        Serial.println("PORT2 activated");  
      #endif   
      digitalWrite(PORT1_PIN, LOW);
      BlinkPort1 = false;
      #if defined DEBUG  
        Serial.println("PORT1 deactivated");
      #endif
    }
  }

  if(((MADA_addr == AccDecoderAddr) && (MADA_port == 2)) && ((AccPortMode & 0x03) == MODE_INDEPENDENT_PORTS)) {
    if(MADA_gate == 1) {
      digitalWrite(PORT2_PIN, HIGH);
      SwitchTime2Activated = true;
      startPort2Millis = millis();
      BlinkPort2 = false;
      #if defined DEBUG  
        Serial.println("PORT2 activated");
      #endif      
    } else {
      if (SwitchOnTime2 == 0) {       // Pulslänge in jedem Fall, auch wenn Gate zurückschaltet
        digitalWrite(PORT2_PIN, LOW);
        BlinkPort2 = false;
        #if defined DEBUG  
          Serial.println("PORT2 deactivated");
        #endif
      }
    }
  }

  if((MADA_addr == AccDecoderAddr) && (MADA_port == 3)) {
    if(MADA_gate == 1) {
      digitalWrite(PORT3_PIN, HIGH);
      BlinkPort3 = false;
      #if defined DEBUG  
        Serial.println("PORT3 activated"); 
      #endif     
    } else {
      digitalWrite(PORT3_PIN, LOW);
      BlinkPort3 = false;
      #if defined DEBUG  
        Serial.println("PORT3 deactivated");
      #endif
    }
  }

  if((MADA_addr == AccDecoderAddr) && (MADA_port == 4)) {
    if(MADA_gate == 1) {
      digitalWrite(PORT4_PIN, HIGH);
      BlinkPort4 = false;
      #if defined DEBUG  
        Serial.println("PORT4 activated"); 
      #endif     
    } else {
      digitalWrite(PORT4_PIN, LOW);
      BlinkPort4 = false;
      #if defined DEBUG  
        Serial.println("PORT4 deactivated");
      #endif
    }
  }

  if((MADA_addr == AccDecoderAddr+1) && (MADA_port == 1) && ((AccPortMode & 0x03) == MODE_INDEPENDENT_PORTS)) {
    if(MADA_gate == 0) {
      BlinkPort1 = false;
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
//  {CV_29_CONFIG, CV29_ACCESSORY_DECODER | CV29_OUTPUT_ADDRESS_MODE},
  {CV_29_CONFIG, CV29_ACCESSORY_DECODER},
  {CV_PORT1_TIME_ON,0},                               // Time on Port1
  {CV_PORT2_TIME_ON,0},                               // Time on Port2
  {CV_ACCESSORY_DECODER_MODE,MODE_INDEPENDENT_PORTS}, // independent ports
  {CV_BLINK_TIME,4},                                  // 1s blink frequeny
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

  // configure pins
  pinMode(DCC_ACK_PIN, OUTPUT);
  digitalWrite(DCC_ACK_PIN, LOW);
  pinMode(PORT1_PIN, OUTPUT);
  digitalWrite(PORT1_PIN, LOW);
  pinMode(PORT2_PIN, OUTPUT);
  digitalWrite(PORT2_PIN, LOW);
  pinMode(PORT3_PIN, OUTPUT);
  digitalWrite(PORT3_PIN, LOW);
  pinMode(PORT4_PIN, OUTPUT);
  digitalWrite(PORT4_PIN, LOW);
  #if defined UNO
    pinMode(PROG_NEXT_PIN, INPUT_PULLUP);
  #endif

  if ((Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_LSB) == 255) || (Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_MSB) == 255)) {
    FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs) / sizeof(CVPair);
  } else {
    cv29_Bits = Dcc.getCV(CV_29_CONFIG);
    // Bit6 = 1 -> Output-Address-Mode, Bit6 = 0 -> Decoder-Address-Mode
    if ((cv29_Bits & 0x40) != 0) {
      OutputAddr = Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_LSB) + (Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_MSB) << 8);
      AccDecoderAddr = int(OutputAddr-1) / 4 + 1;
      AccDecoderPort = int(OutputAddr-1) % 4 + 1;
    } else {
      AccDecoderAddr = (Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_LSB & 0x03F)) + (Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_MSB) << 6);
    }

    AccPortMode = (Dcc.getCV(CV_ACCESSORY_DECODER_MODE) & 0x03);
    if (AccPortMode > 2) AccPortMode = 0;
    SwitchOnTime1 = (Dcc.getCV(CV_PORT1_TIME_ON) & 0x1F);
    SwitchOnTime2 = (Dcc.getCV(CV_PORT2_TIME_ON) & 0x1F);
    BlinkPeriod = (Dcc.getCV(CV_BLINK_TIME) & 0x0F);

    #if defined DEBUG  
        Serial.print(Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_MSB));
        Serial.print("-");
        Serial.println(Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_LSB));
        Serial.print("CV29:            "); Serial.print(cv29_Bits,BIN);
        if ((cv29_Bits & 0x40) != 0) Serial.println(" -> Output Address Mode");
        else  Serial.println(" -> Dekoder Address Mode");
        if ((cv29_Bits & 0x40) != 0) {
          Serial.print("Output-Adresse:  "); Serial.print(OutputAddr);
          Serial.print("  Dekoder-Adresse: "); Serial.print(AccDecoderAddr);
          Serial.print("-"); Serial.println(AccDecoderPort);
        } else {
          Serial.print("Dekoder-Adresse: "); Serial.println(AccDecoderAddr);
        }
        if ((AccPortMode & 0x03) == MODE_ALTERNATING_PORTS1) Serial.println("PORT1/2 alternating mode 1"); 
        if ((AccPortMode & 0x03) == MODE_ALTERNATING_PORTS2) Serial.println("PORT1/2 alternaring mode 2"); 
        if ((AccPortMode & 0x03) == 0) Serial.println("PORT1/2 independent mode");
        Serial.print("Pulslänge1:    "); Serial.print(SwitchOnTime1); Serial.println(" * 256 ms");
        Serial.print("Pulslänge2:    "); Serial.print(SwitchOnTime2); Serial.println(" * 256 ms");
        Serial.print("Blinkperiode : "); Serial.print(BlinkPeriod); Serial.println(" * 250 ms");
    #endif
  }

  #if defined UNO
    pinMode(PROG_NEXT_PIN, INPUT_PULLUP);
  #endif

  // init NmraDcc library (PIN, manufacturer, version...) 
  Dcc.pin(digitalPinToInterrupt(DCC_PIN), DCC_PIN, 1);
  Dcc.initAccessoryDecoder(MAN_ID_DIY, 10, cv29_Bits & FLAGS_OUTPUT_ADDRESS_MODE, 0);   // CV8=Manufacturer-ID=13, CV7=Manufacturer-VERS=10

  // Wait for the first command to programme the address after setting PROG_NEXT_PIN to LOW and releasing
  #if defined UNO
    if (digitalRead(PROG_NEXT_PIN) == 0) {
      #if defined DEBUG  
          Serial.println("ProgModeKey pressed");
      #endif
      Dcc.setAccDecDCCAddrNextReceived(1);
    }
  #endif

//  delay(200);  // uncomment this if Micronucleus Bootloader is used.

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
//  if (((AccPortMode & 0x03) != 0) && (SwitchOnTime1 != 0) && SwitchTime1Activated == true) {
  if ((SwitchOnTime1 != 0) && SwitchTime1Activated == true) {
    int PortMillis = SwitchOnTime1 << 8;
    if ((currentPortMillis-startPort1Millis > PortMillis)) {
      SwitchTime1Activated = false;
      digitalWrite(PORT1_PIN, LOW);
      #if defined DEBUG  
        Serial.println("PORT1 Time Out");
      #endif
    }
  }

//  if (((AccPortMode & 0x03) != 0) && (SwitchOnTime2 != 0) && SwitchTime2Activated == true) {
  if ((SwitchOnTime2 != 0) && SwitchTime2Activated == true) {
    int PortMillis = SwitchOnTime2 << 8;
    if ((currentPortMillis-startPort2Millis > PortMillis)) {
      SwitchTime2Activated = false;
      digitalWrite(PORT2_PIN, LOW);
      #if defined DEBUG  
        Serial.println("PORT2 Time Out");
      #endif
    }
  }

  // BlinkPort PORT1 and PORT2 if they are not in impulse mode
  if (BlinkPeriod && BlinkPort1 && ((AccPortMode & 0x01) == MODE_INDEPENDENT_PORTS)) {
    if (((currentPortMillis-startBlinkMillis)  % (BlinkPeriod*250)) < BlinkPeriod*125) {
      digitalWrite(PORT1_PIN, HIGH);
    } else {
      digitalWrite(PORT1_PIN, LOW);
    }
  }

  if (BlinkPeriod && BlinkPort2 && ((AccPortMode & 0x01) == MODE_INDEPENDENT_PORTS)) {
    if (((currentPortMillis-startBlinkMillis)  % (BlinkPeriod*250)) < BlinkPeriod*125) {
      digitalWrite(PORT2_PIN, HIGH);
    } else {
      digitalWrite(PORT2_PIN, LOW);
    }
  }

  // BlinkPort PORT3
  if (BlinkPeriod && BlinkPort3) {
    if (((currentPortMillis-startBlinkMillis) % (BlinkPeriod*250)) < BlinkPeriod*125) {
      digitalWrite(PORT3_PIN, HIGH);
    } else {
      digitalWrite(PORT3_PIN, LOW);
    }
  }

  // BlinkPort PORT4
  if (BlinkPeriod && BlinkPort4) {
    if (((currentPortMillis-startBlinkMillis) % (BlinkPeriod*250)) < BlinkPeriod*125) {
      digitalWrite(PORT4_PIN, HIGH);
    } else {
      digitalWrite(PORT4_PIN, LOW);
    }
  }

}
