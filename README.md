# DCC accessory decoder with ATtiny85
**Properties**
- Possible hardware - ATtiny85 Digispark Board with additional PCB [ATtiny-MuFu4P](http://simandit.de/simwiki/doku.php?id=modellbahn:umbauten:dcc-dekoder#funktions-dekoder_mit_digispark-board) for power supply and signaling from DCC-Signal or complete PCB ATtiny-MuFuDec
- The Digispark board can be programmed via USB using the Micronucleus bootloader, I recommend to use no bootloader, because bootloader needs 300 ms (with upgrade) ... 5s (default) at power on. 
- The new [ATtiny-MuFuDec](https://simandit.de/simwiki/doku.php?id=modellbahn:umbauten:dcc-dekoder#dcc-signal-dekoder_mit_attiny85) must be programmed via ISP using an AVR programmer.
- a good information source for programming ATtiny85 is found at [Wolles Elektronikkiste](https://wolles-elektronikkiste.de/attiny-mit-arduino-code-programmieren)
- The sketch is using the [NmraDcc library](https://github.com/mrrwa/NmraDcc) from [MRRWA](http://mrrwa.org/)
- 4 output ports
- Two consecutive module accessory decoder addresses (MADA)
  - Second address uses for blinking output
  - Each address uses 4 ports with a gate setting to on/off
- CV reading at programming track (PT) is possible with a temporarily circuit for 60 mA load at one port

`Decoder-Address = LSB (CV1) + MSB (CV9) * 64`\
`CV1 = 6 bit LSB, default 1`\
`x x x x  x x x x`\
`    +-+--+-+-+-+----- LSB 0 ... 63`

`CV9 = 3 Bit MSB, default 0`\
`x x x x  x x x x`\
`           +-+-+----- MSB (0 ... 7) * 64`

`CV3 / CV4 Time On Function for PB0 / PB4 - default 0`\
`x x x x  x x x x`\
`      +--+-+-+-+----- 5 bit pulse length if PORT1 and PORT2 are alternating ports`\
`                      number * 256 ms (256 ms ... 7.93 s)`\
`                      e.g. usable for switches without voltage cut-off`\
`                      0  (all 5 bits) for continous ON if activate`\

`CV29 Configuration, default 192`\
`x x x x  x x x x `\
`| +------------------ "0" = Decoder Address Mode "1" = Output Address Mode`\
`+-------------------- "1" = Accessory Decoder Mode, is set for accessories`

`CV33 Decoder configuration - default 0 for independent ports`\
`x x x x  x x x x`\
`             +-+----- "0-0" (0) = independent PORT1/PORT2`\
`                      "0-1" (1) = alternating PORT1/PORT2 with Port1-gate on/off`\
`                      "1-0" (2) = alternating PORT1/PORT2 with Port1-gate on / Port2-gate on`

`CV34 Blinking periode - default 4 for 1 sec blink frequency`\
`x x x x  x x x x`\
`         +-+-+-+----- 4 bit for blinking periode in s (0.25 ... 3.75 s)`\

A blinking output is generated with writing to Address+1 and corresponding port number
  - but only if a blinking periode in CV34 is set and port is defined as independent

Writing to CV8 is resetting the decoder with defaults
  - CV1 = 1, CV9 = 0, CV3/CV4 = 0, CV33 = 0, CV34 = 4 
  - PORT1 and PORT2 are independent ports
  - PORT3 and PORT4 are always independent
  - Blinking periode 1 sec
