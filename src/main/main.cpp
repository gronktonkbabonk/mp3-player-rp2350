#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "pinconf.h"

#include "../fatfs/ff.h"
#include "../sd-driver/driver.h"


bool debug = false;
bool cmdDebug = false;
bool initialised = false;
spi_inst_t *spi = SPI_PORT;
int cs = PIN_CS;
int cd = PIN_CD;

const char* getfres(FRESULT res) {
    switch (res) {
        case FR_OK: return "FR_OK";
        case FR_DISK_ERR: return "FR_DISK_ERR";
        case FR_INT_ERR: return "FR_INT_ERR";
        case FR_NOT_READY: return "FR_NOT_READY";
        case FR_NO_FILE: return "FR_NO_FILE";
        case FR_NO_PATH: return "FR_NO_PATH";
        case FR_INVALID_NAME: return "FR_INVALID_NAME";
        case FR_DENIED: return "FR_DENIED";
        case FR_EXIST: return "FR_EXIST";
        case FR_INVALID_OBJECT: return "FR_INVALID_OBJECT";
        case FR_WRITE_PROTECTED: return "FR_WRITE_PROTECTED";
        case FR_INVALID_DRIVE: return "FR_INVALID_DRIVE";
        case FR_NOT_ENABLED: return "FR_NOT_ENABLED";
        case FR_NO_FILESYSTEM: return "FR_NO_FILESYSTEM";
        case FR_MKFS_ABORTED: return "FR_MKFS_ABORTED";
        case FR_TIMEOUT: return "FR_TIMEOUT";
        case FR_LOCKED: return "FR_LOCKED";
        case FR_NOT_ENOUGH_CORE: return "FR_NOT_ENOUGH_CORE";
        case FR_TOO_MANY_OPEN_FILES: return "FR_TOO_MANY_OPEN_FILES";
        case FR_INVALID_PARAMETER: return "FR_INVALID_PARAMETER";
        default: return "UNKNOWN FRESULT";
    }
}

void SDDriverInit(){   

    gpio_set_dir(cs, GPIO_OUT);
    gpio_put(cs,1);

    printf("Card inserted. Initialising...\n");
    char ch;
    printf("debug on? (y/enter)\n");
    ch = getchar();
    if (ch == 'y'){
        debug = true;
        printf("cmd debug on? (y/enter)\n");
        ch = getchar();
        if (ch == 'y'){
            cmdDebug = true;
        }
    }
    return;
}

void init(){
    stdio_init_all();

    gpio_init(PIN_CD);
    gpio_set_dir(PIN_CD,GPIO_IN);
    gpio_pull_up(PIN_CD);
    
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_init(ONBLED);
    gpio_set_dir(ONBLED,GPIO_OUT);
    
    gpio_put(ONBLED,1);

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    
    printf("\n\n=====================START SESSION=====================\n\n\nConsole connected. Waiting for card insertion.\n");

    while(!(gpio_get(PIN_CD)==0)){
        sleep_ms(100);
    }

    SDDriverInit();
}

int main(){
    init();

    // bool debug = true;

    FATFS fat;
    DIR dir;
    FILINFO fno;
    FIL fil;
    FRESULT fr = FR_NOT_READY;
    UINT bw;

    fr = f_mount(&fat, "", 0);
    if (fr == FR_OK){
        printf("Mount OK!\n");
        
        
        fr = f_opendir(&dir, "/");
        if(fr != FR_OK){
            printf("opendir error: %s\n",getfres(fr));
            return -1;
        } 
        while (fr == FR_OK && fno.fname[0]) {
            fr = f_readdir(&dir, &fno);
            if(fr != FR_OK){
                printf("readdir error: %s\n",getfres(fr));
            } 
            printf("Found: %s\n", fno.fname);
        }
        fr = f_closedir(&dir);
        if(fr != FR_OK){
            printf("opendir error: %s\n",getfres(fr));
            return -1;
        }
        
        fr = f_open(&fil, "test.txt", FA_WRITE | FA_CREATE_ALWAYS);
        if(fr != FR_OK){
            printf("open file error: %s\n",getfres(fr));
            return -1;
        }
        fr = f_write(&fil, "Hello, World!\r\n", 15, &bw);
        if(fr != FR_OK){
            printf("write error: %s\n",getfres(fr));
            return -1;
        }
        sleep_ms(10);
        fr = f_close(&fil);
        if(fr != FR_OK){
            printf("close file error: %s\n",getfres(fr));
            return -1;
        }
        printf("file closed\n");  
        sleep_ms(10);
        fr = f_unmount("");
        if (fr == FR_OK) printf("Unmount successful!\n");
        
        else printf("Unmount error: %s\n", getfres(fr));
    } else {
        printf("Mount failed: %s\n", getfres(fr));
        return -1;
    }
    return 0;
}