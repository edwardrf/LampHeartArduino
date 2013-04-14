#include "animation.h"
#define COLOR_DEPTH 16
#define INITIAL_WAIT 3000       // in ms, initial waiting time before start animation
#define SILENT_WAIT 10000

volatile uint8_t zc_flag = 0;   // zc_flat zero crossing flag
uint8_t buffer[8][8];           // the rendering frame
int kfc = 0, sfc = 1;           // kfc: Key frame counter, sfc: subframe counter
long t0, tAction;               // System start time, use to check if it should play the default animation
uint8_t auto_flag = 1;          // flag to track if the system is in auto playing mode.
uint8_t frp = 255;              // Frame reading pointer. 255 is special stop


// Pre-calculated sine wave integral to have the same amount of work.
int delays[] = {1609 , 2301 , 2851 , 3333 , 3776 , 4196 , 4601 , 5000 , 5399 , 5804 , 6224 , 6667 , 7149 , 7699 , 8391, 8500};

void setup(void)
{
  delay(1000);
  init_shift_register();
  Serial.begin(57600);
  attachInterrupt(0, zero_crossing, FALLING);
  t0 = millis();
  memcpy(buffer, key_frames[0], 64);
}

void get_frame(uint8_t from[][8], uint8_t to[][8], int frame_number, int frame_count, uint8_t frame[][8]){
  uint8_t i, j;
  for(i = 0; i < 8; i++){
    for(j = 0; j < 8; j++){
      frame[i][j] = ((to[i][j] - from[i][j]) * frame_number + from[i][j] * frame_number) / frame_count;
    }
  }
}

void print_frame(uint8_t frame[][8]){
  Serial.print("\r\n===================\r\n");
  uint8_t i, j;
  for(i = 0; i < 8; i++){
    for(j = 0; j < 8; j++){
      Serial.print(frame[i][j]);
    }
    Serial.print("\r\n");
  }
  Serial.print("\r\n");
}

void loop(){
  if(zc_flag) {
    delayMicroseconds(900);
    long start = micros();
    uint8_t disp[8][8];
    uint8_t row, col;
    uint8_t *fkfp, *tkfp, *bufp;
    uint8_t tkfc;
    
    memcpy(disp, buffer, 64);

    if(auto_flag == 2){
      tkfc = kfc + 1;
      if(tkfc >= key_frame_count) tkfc = 0;
      fkfp = &key_frames[kfc][0][0];
      tkfp = &key_frames[tkfc][0][0];
      bufp = &buffer[0][0];
    }

    // One frame ==> 1sec/50hz/2 = 10ms
    for(uint8_t i = COLOR_DEPTH; i > 0; i --){
      // One colour depth, each color depth takes as much as time defined in the sine array above insead of equal amount of time
      // to give roughly equal amount of energy to the lamp for each gray scale.
      col = 1;
      for(int8_t j = 7; j >= 0; j--){
        row = 0;
        int v = i - 1;
        if(disp[j][0] == v){row |= 0b00000001;}
        if(disp[j][1] == v){row |= 0b00000010;}
        if(disp[j][2] == v){row |= 0b00000100;}
        if(disp[j][3] == v){row |= 0b00001000;}
        if(disp[j][4] == v){row |= 0b00010000;}
        if(disp[j][5] == v){row |= 0b00100000;}
        if(disp[j][6] == v){row |= 0b01000000;}
        if(disp[j][7] == v){row |= 0b10000000;}

        output(~col, row);
        col <<= 1;

        // Optocoupler requires 20us to be turned on
        delayMicroseconds(20);
      }
      output(0b11111111, 0b00000000);// Turn off
      
      //Frame calculation between colour depth timing
      if(auto_flag == 2){
        // 4 number calculated, and in 16 rounds, 64 is calculated.
        *bufp++ = (*tkfp++ * sfc + *fkfp * key_frame_timing[kfc] - *fkfp++ * sfc) / key_frame_timing[kfc];
        *bufp++ = (*tkfp++ * sfc + *fkfp * key_frame_timing[kfc] - *fkfp++ * sfc) / key_frame_timing[kfc];
        *bufp++ = (*tkfp++ * sfc + *fkfp * key_frame_timing[kfc] - *fkfp++ * sfc) / key_frame_timing[kfc];
        *bufp++ = (*tkfp++ * sfc + *fkfp * key_frame_timing[kfc] - *fkfp++ * sfc) / key_frame_timing[kfc];
      }
      
      //Delay between colour depth timing
      while(i > 0 && micros() - start < delays[16 - i]){
        //Process any possible serial command
        if(Serial.available()){
          uint8_t cmd = Serial.read();
          auto_flag = 0;
          tAction = millis();
          // Check if it is in the middle of reading a frame
          if(frp != 255) {
            *(bufp + frp++) = cmd >= 'a' ? cmd - 'a' + 10 : cmd >= 'A' ? cmd - 'A' + 10 : cmd - '0';
            if(frp >= 64) frp = 255;
          }else {
            if(cmd == 'f'){
              frp = 0; // Start the frame reading process
            }else if(cmd == 'c'){
//              Serial.println("c received");
              memset(buffer, 0, 64);
            }else if(cmd == 'a'){
//              Serial.println("a received");
              memset(buffer, 15, 64);
            }else if(cmd == 'p'){
              while(!Serial.available()); uint8_t x = Serial.read();
              while(!Serial.available()); uint8_t y = Serial.read();
              while(!Serial.available()); uint8_t bb = Serial.read();
              uint8_t b = bb >= 'a' ? bb - 'a' + 10 : bb >= 'A' ? bb - 'A' + 10 : bb - '0';
              buffer[x - '0'][y - '0'] = b;
            }
          }
        }
      }
    }
    if(auto_flag == 2){
      ++sfc;
      if(sfc > key_frame_timing[kfc]){
        kfc = tkfc;
        sfc = 1;
      }
    }
    zc_flag = 0;
  }
  
  if(auto_flag == 1 && millis() - t0 > INITIAL_WAIT){
    auto_flag = 2;
  }
  
  if(auto_flag == 0 && millis() - tAction > SILENT_WAIT){
    auto_flag = 2;
  }
  
}

void zero_crossing(){
  zc_flag = 1;
}
