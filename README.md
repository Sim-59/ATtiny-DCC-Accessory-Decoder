# Accessory decoder with ATtiny85-Digispark-Board
**Properties**
- Based at Arduino-ATtiny85 (micronucleus) hardware
- using the [NmraDcc library](https://github.com/mrrwa/NmraDcc) from [MRRWA](http://mrrwa.org/)
- Two consecutive module accessory decoder addresses (MADA) are used
  - Each address uses 4 Ports with a gate setting to on/off
  - The second address is used to realize a blinking port
- With a jumper an ACK pulse is possible for reading back the CVs at programming track

`Decoder-Address = LSB (CV1) + MSB (CV9) * 64`\
`CV1 = 6 bit LSB`\
`x x x x  x x x x`\
`    +-+--+-+-+-+----- LSB 0 ... 63`

`CV9 = 3 Bit MSB`\
`x x x x  x x x x`\
`           +-+-+----- MSB (0 ... 7) * 64`

`CV29`\
`x x x x  x x x x `\
`| +------------------ "0" = Decoder Address Mode "1" = Output Address Mode`\
`+-------------------- "1" = Accessory Decoder Mode, is set for accessories`

`CV10 - default 0 for independent ports`\
`x x x x  x x x x`\
`             +-+----- "0-0" (0) = independent PORTS1/PORT2`\
`                      "0-1" (1) = alternating PORT1/PORT2 with Port1-gate on/off`\
`                      "1-0" (2) = alternating PORT1/PORT2 with Port1-gate on / Port2-gate on`

`CV11 - default 0b01000000 (64) for 1 sec blink frequency`\
`x x x x  x x x x`\
`| | | +--+-+-+-+----- 5 bit pulse length if PORT1 and PORT2 are alternating ports`\
`| | |                 number * 256 ms (256 ms ... 7.93 s)`\
`| | |                 e.g. usable for switches without voltage cut-off`\
`| | |                 0  (all 5 bits) for permanently time ON`\
`+-+-+---------------- 3 bit for blinking periode in s (0.5 ... 3.5 s)`

A blinking port will be activated with writing to Address+1 and corresponding port number
  - but only if a blinking periode in CV11 is set and port is defined as independent

Writing to CV8 is resetting the decoder with defaults
  - CV1 = 1, CV9 = 0, CV10 = 0, CV11 = 0x40 (64) 
  - PORT1 and PORT2 are independent ports
  - PORT3 and PORT4 are always independent
  - Blinking periode 1 sec
