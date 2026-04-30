//    _____          _         _____           _           _     __                     
//   / ____|        | |       |  __ \         | |         | |   / _|                    
//  | |     ___   __| | ___   | |__) |__  _ __| |_ ___  __| |  | |_ _ __ ___  _ __ ___  
//  | |    / _ \ / _` |/ _ \  |  ___/ _ \| '__| __/ _ \/ _` |  |  _| '__/ _ \| '_ ` _ \ 
//  | |___| (_) | (_| |  __/  | |  | (_) | |  | ||  __/ (_| |  | | | | | (_) | | | | | |
//   \_____\___/ \__,_|\___|  |_|   \___/|_|   \__\___|\__,_|  |_| |_|  \___/|_| |_| |_|
//    _____ ____      _____                                             _
//   / ____|  _ \    / ____|                                           | |              
//  | (___ | |_) |  | |     ___  _ __ ___  _ __   ___  _ __   ___ _ __ | |_ ___         
//   \___ \|  _ <   | |    / _ \| '_ ` _ \| '_ \ / _ \| '_ \ / _ \ '_ \| __/ __|        
//   ____) | |_) |  | |___| (_) | | | | | | |_) | (_) | | | |  __/ | | | |_\__ \        
//  |_____/|____/    \_____\___/|_| |_| |_| .__/ \___/|_| |_|\___|_| |_|\__|___/        
//                                        | |                                           
//                                        |_|  

// They wrote all the logic, not me!
// https://github.com/sbcshop/MicroSD-Breakout

#include <stdio.h>
#include <string>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19

void sdTest(std::string data){
    
}
int main()
{
    stdio_init_all();

    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    
    gpio_set_dir(PIN_CS, GPIO_OUT);// init to idle-high state
    gpio_put(PIN_CS, 1);

    while (true) {
        sdTest("hello!");
        sleep_ms(500);
    }
}
