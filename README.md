# Accessory decoder with ATtiny85-Digispark-Board
**Properties**
- Based at Arduino-ATtiny85 (micronucleus) hardware
- using the [NmraDcc-Bibliothek](https://github.com/mrrwa/NmraDcc) fromvon [MRRWA](http://mrrwa.org/)
- Two decoder addresses are used
  - 4 Ports with one gates (on/off)
  - The second address is used for blinking function
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

`CV10`\
`x x x x  x x x x`\
`             +-+----- "0" = independent PORTS1/PORT2`\
`                      "1" = alternating PORT1/PORT2, e.g. signals, switches`

`CV11`\
`x x x x  x x x x`\
`| | | +--+-+-+-+----- 5 bit pulse length if PORT1 and PORT2 are alternating ports`\
`| | |                 number * 256 ms (256 ms ... 7.93 s)`\
`| | |                 e.g. usable for switches without voltage cut-off`\
`| | |                 0  (all 5 bits) for permanently time ON`\
`+-+-+---------------- 3 bit for blinking periode in s (0.5 ... 3.5 s)`

Blinking for a port will be activatet with writing to Address+1 and corresponding port number
  - activated with gate=1 and deactivated with gate=0
  - but only if a blinking periode in CV11 is set and port is independent
  - activating an other port resets the blinking of all ports, the blinking must be reactivated

Writing to CV8 is resetting the decoder with defaults
  - CV1 = 1, CV9 = 0, CV10 = 0, CV11 = 0
  - PORT1 and PORT2 as independent or alternating ports
  - PORT3 and PORT4 are always independent
