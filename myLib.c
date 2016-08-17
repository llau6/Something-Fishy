#include "myLib.h"
u16 *videoBuffer = (u16*) 0x6000000;

void setPixel(int r, int c, u16 color) {
	videoBuffer[OFFSET(r, c, 240)] = color;
}

void drawRect4(int row, int col, int height, int width, u8 index) {
    volatile u16 color = index<<8 | index;
    int r;
    for(r=0; r<height; r++) {
        DMA[3].src = &color;
        DMA[3].dst = &videoBuffer[OFFSET(row+r, col, 240)/2];
        DMA[3].cnt = (width/2) | DMA_DESTINATION_INCREMENT | DMA_SOURCE_FIXED | DMA_ON;
    }
} 

void drawRect(int row, int col, int height, int width, u16 color) {
    int r;
    for(r=0; r<height; r++) {
        DMA[3].src = &color;
        DMA[3].dst = &videoBuffer[OFFSET(row+r, col, 240)];
        DMA[3].cnt = width | DMA_DESTINATION_INCREMENT | DMA_SOURCE_FIXED | DMA_ON;
        
    }
}

void drawImage4(int r, int c, int width, int height, const u16* image) {
	for (int i = 0; i < height; i++) {
	    DMA[3].src = image + OFFSET(i, 0, width)/2;
  	    DMA[3].dst = videoBuffer + OFFSET(r+i, c, 240)/2;
 	    DMA[3].cnt = width/2 | DMA_ON;
	}
}

void waitForVblank() {
    while(SCANLINECOUNTER > 160);
    while(SCANLINECOUNTER < 160);
}

void setPixel4(int row, int col, u8 index) {
    int whichPixel = OFFSET(row, col, 240);
    int whichShort = whichPixel/2;
    if (col & 1) {
        // Column is odd. We change left byte
        videoBuffer[whichShort] = (videoBuffer[whichShort] & 0x00FF) | (index<<8);
        
    } else {
        // Column is even. We change right byte
        videoBuffer[whichShort] = (videoBuffer[whichShort] & 0xFF00) | (index);
    }
}

void FlipPage() {
    if(REG_DISPCNT & BUFFER1FLAG) {
        // we were displaying BUFFER1
        videoBuffer = BUFFER1;
        REG_DISPCNT = REG_DISPCNT & ~BUFFER1FLAG;
    } else {
        // We were displaying BUFFER0
        videoBuffer = BUFFER0;
        REG_DISPCNT = REG_DISPCNT | BUFFER1FLAG;
    }
}
