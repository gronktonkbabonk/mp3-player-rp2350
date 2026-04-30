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


// possible fix for a future bug: if only 1/8 of the file is being read, remove /sizeof(uint8_t) when reading


#include <stdio.h>
#include <string.h>
#include <cstdint>
#include <cmath>
#include <stdexcept>
#include <errno.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

const int CMD_TIMEOUT = 100;

const int R1_IDLE_STATE = 1<<0;
const int R1_ILLEGAL_COMMAND = 1 << 2;
const uint8_t TOKEN_FF = 0xFF;
const uint8_t TOKEN_00 = 0x00;
const char TOKEN_CMD25 = 0xFC;
const char TOKEN_STOP_TRAIN = 0xFD;
const char TOKEN_DATA = 0xFE;

class SDCard
{
    public:
        uint8_t tokenbuf[1] = {};  
        spi_inst_t *spi;
        int cs;
        int sectors;
        int cdv;
        int nblocks;
        SDCard(spi_inst_t *spiInp, int csInp);
        void init_card();
        void init_card_v1();
        void init_card_v2();
        void readinto(uint8_t buf[]);
        int cmd(int cmd, int arg, uint8_t crc, int final = 0, bool release=true, bool skip1=true);
        void write(char token, uint8_t buf[], int cycles=0);
        void write_token(char token);
        void writeblocks(int block_num, uint8_t buf[]);
        void readblocks(int block_num, uint8_t buf[]);
        int ioctl(int op);
};

SDCard::SDCard(spi_inst_t *spiInp, int csInp)
{
    spi = spiInp;
    cs = csInp;
}

void SDCard::init_card(){
    spi_init(spi, 100*1000); //100Khz slower for init i guess? hey dude i just work here
    uint8_t enterToken[16];
    memset(enterToken, TOKEN_FF, 16);
    gpio_put(cs, 0);
    spi_write_blocking(spi, enterToken, 16);

    bool sdExists = false;

    for (size_t i = 0; i < 5; i++){
        if (cmd(0,0,0x95)==R1_IDLE_STATE) {
            sdExists = true;
            break;
        }
    }
    if(!sdExists) std::runtime_error("no SD card is present :( (or no response is being returned at all)");

    // now we can be confident the sd card is present!

    int r = cmd(8,0x01AA,0x67, false);

    if(r==R1_IDLE_STATE){
        init_card_v2();
    } else if (r==(R1_IDLE_STATE | R1_ILLEGAL_COMMAND)){
        init_card_v1();
    } else{
        std::runtime_error("Can't determine SD card version :(");
    }

    // now we know the sd card version also isnt that awesome

    // get number of sectors
    // CMD9: response R2 (R1 byte + 16-byte block read)

    if (cmd(9,0,0,0, false)!=0){
        std::runtime_error("No response from SD card :/");
    }
    uint8_t csd[16] = {};
    memset(csd,0,16); // i dont think this is necessary but its nice to have a zeroed array
    readinto(csd);
    if (csd[0] & 0xC0 == 0x40){
        sectors = ((csd[8] << 8 | csd[9]) + 1) * 1024;
    } else if (csd[0] & 0xC0 == 0x00){
        int c_size = csd[6] & 0b11 | csd[7] << 2 | (csd[8] & 0b11000000) << 4;
        int c_size_mult = ((csd[9] & 0b11) << 1) | csd[10] >> 7;
        sectors = (c_size + 1) * (pow(2, (c_size_mult + 2)));
    } else{
        std::runtime_error("Sd card format not supported");
    }

    if (cmd(16, 512, 0) != 0){
        std::runtime_error("can't set block size to 512");
    }


    spi_init(spi, 1000*1000); //1Mhz speed up for data transmission
}

void SDCard::init_card_v1(){
    for(size_t i = 0; i<CMD_TIMEOUT;i++){
        cmd(55,0,0);
        if(cmd(41,0,0) == 0){
            cdv = 512;
            return;
        }
    }
    std::runtime_error("Timeout while waiting for v1 card init");
}

void SDCard::init_card_v2(){
    for(size_t i = 0; i<CMD_TIMEOUT;i++){
        sleep_ms(50);
        cmd(58,0,0,4);
        cmd(55,0,0);
        if(cmd(41,0x40000000,0) == 6){
            cmd(58,0,0,4);
            cdv = 1;
            return;
        }
    }
    std::runtime_error("Timeout while waiting for v2 card init");
}

void SDCard::readinto(uint8_t buf[]){
    gpio_put(cs, 0);
    bool response = false;
    for (size_t i = 0; i < CMD_TIMEOUT; i++){
        spi_write_read_blocking(spi, &TOKEN_FF, tokenbuf, 1);
        if (tokenbuf[0] = TOKEN_DATA){
            response = true;
            break;
        }
        sleep_ms(1);
    }
    if (!response){
        gpio_put(cs, 1);
        std::runtime_error("Timeout waiting for response from Sd card :( got ghosted bruh");
    }
    
    uint8_t mv[sizeof(buf)/sizeof(uint8_t)] = {};
    memset(mv, 0, sizeof(mv)/sizeof(uint8_t));
    spi_write_read_blocking(spi, buf, mv, sizeof(mv)/sizeof(uint8_t));

    spi_write_blocking(spi, &TOKEN_FF, 1);
    spi_write_blocking(spi, &TOKEN_FF, 1);
    gpio_put(cs,1);
    spi_write_blocking(spi, &TOKEN_FF, 1);
}

int SDCard::cmd(int cmd, int arg, uint8_t crc, int final = 0, bool release=true, bool skip1=false){
    gpio_put(cs, 0);
    uint8_t buf[6] = {
        0x40|cmd,
        arg >> 24,
        arg >> 16,
        arg >> 8,
        arg,
        crc
    };
    spi_write_blocking(spi, buf, 6);

    if(skip1){
        spi_write_read_blocking(spi, &TOKEN_FF, tokenbuf, 1);
    }

    for (size_t i = 0; i < CMD_TIMEOUT; i++){
        spi_write_read_blocking(spi, &TOKEN_FF, tokenbuf, 1);
        uint8_t response = tokenbuf[0];
        if (!(response & 0x80)){
            for (size_t j = 0; i < final; i++){
                spi_write_blocking(spi, &TOKEN_FF, 1);
            }
            if (release){
                gpio_put(cs, 1);
                spi_write_blocking(spi, &TOKEN_FF, 1);
            }
            return response;
            
        }
    }

    gpio_put(cs, 1);
    spi_write_blocking(spi, &TOKEN_FF, 1);
    return -1;
}

void SDCard::write(char token, uint8_t buf[], int cycles=0){
    gpio_put(cs,0);

    uint8_t readinto;
    spi_read_blocking(spi, token, &readinto, 1);
    if(!cycles){
        spi_write_blocking(spi, buf, sizeof(buf)/sizeof(uint8_t));
    }
    
    spi_write_blocking(spi, &TOKEN_FF, 1);
    spi_write_blocking(spi, &TOKEN_FF, 1);

    spi_read_blocking(spi, TOKEN_FF, &readinto, 1);
    if(readinto&0x1F != 0x05){
        gpio_put(cs, 1);
        spi_write_blocking(spi, &TOKEN_FF, 1);
        return;
    }

    gpio_put(cs, 1);
    spi_write_blocking(spi, &TOKEN_FF, 1);

}

void SDCard::write_token(char token){
    gpio_put(cs,0);
    uint8_t readinto;
    spi_read_blocking(spi, token, &readinto, 1);
    spi_write_blocking(spi, &TOKEN_FF, 1);
    gpio_put(cs,1);
    spi_write_blocking(spi, &TOKEN_FF, 1);

}

void SDCard::writeblocks(int block_num, uint8_t buf[]){
    nblocks = (sizeof(buf)/sizeof(uint8_t)) /512;
    int err = nblocks = (sizeof(buf)/sizeof(uint8_t)) % 512;
    assert(("Buffer length is invalid", nblocks && !err));

    if(nblocks==1){
        if (cmd(24, block_num*cdv, 0) != 0){
            errno= EIO;
        }
        write(TOKEN_DATA, buf);
    } else{
        if (cmd(25, block_num*cdv, 0) != 0){
            errno= EIO;
        }
        int offset = 0;
        uint8_t mv[sizeof(buf)/sizeof(uint8_t)];
        for(size_t i=0;nblocks != 0;i+=512,nblocks--){
            write(TOKEN_CMD25, &buf[i]);
        }
        memcpy(&mv, &buf, sizeof(buf)/sizeof(uint8_t));
    }
}

void SDCard::readblocks(int block_num, uint8_t buf[]){
    nblocks = sizeof(buf)/sizeof(uint8_t);
    assert(("Invalid buffer length",nblocks && !((sizeof(buf)/sizeof(uint8_t))%512)));
    if(nblocks==1){
        if(cmd(17,block_num*cdv,0,0,false) != 0){
            gpio_put(cs,1);
            errno = EIO;
        }
        spi_read_blocking(spi, TOKEN_00,buf, sizeof(buf)/sizeof(uint8_t));
    } else {
        if(cmd(18,block_num*cdv,0,0,false) != 0){
            gpio_put(cs,1);
            errno = EIO;
        }
        int offset = 0;
        uint8_t mv[sizeof(buf)/sizeof(uint8_t)];
        memcpy(&mv, &buf, sizeof(buf)/sizeof(uint8_t));
        for(size_t i=0;nblocks != 0;i+=512,nblocks--){
            spi_read_blocking(spi,TOKEN_00, &buf[i], 512);
        }
        if (cmd(12, 0, 0xFF,0, true, true)) errno = EIO;
    }
}

int SDCard::ioctl(int op){
    if (op == 4) return sectors;
}