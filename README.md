# Color music (music reactive LED strip) for Arduino
## Schematic / Circuit / Wiring
            ----------------              -------------------
            | Audio Source |              | Power Source 5V |
            ----------------              -------------------
               |  |                          Vs        GND
               |  |                          |          |
               |  |                          |          --- GND
               |  |  -------------------------
               |  |  |
            |-----------|
            | Amplifier |
            |-----------|
               Vout  GND
                |     |
                |     --- GND
                |   
                |   
GND --- |R100k|-|-|R100k| ---
                |           | 
                |           |
             |C10uF|        |
                |           |
                |           |
               ADC         3V3
            |-----------------|
            | Microcontroller |GND --- GND
            |-----------------|
               GPIO        3V3
                |           |
                |           |               
                |           |
               DI          VDD
            |-------------------|
            | LED Strip WS2812b |GND --- GND
            |-------------------|
