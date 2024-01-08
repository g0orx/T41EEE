/*
 * G0ORX New Front Panel.
 *
 * i2c interface to Raspberry Pi Pico acting as a i2c slave device.
 *
 * Supports 4 Encoders, 4 Encoder Switches, Touch Screen
 *
 */

#ifndef BEENHERE
#include "SDT.h"
#endif

#ifdef G0ORX_FRONTPANEL_2

#include <Wire.h>

// uncomment to print debug messages
//#define DEBUG_MESSAGES

#ifdef DEBUG_MESSAGES
#define Debug(x) Serial.println(x)
#else
#define Debug(x)
#endif

#define I2C_ADDRESS 0x20
bool found_pico = false;
bool configured_pico = false;

#define CONFIG_REG 0x00
#define RESET_REG 0x01
#define READ_INTERRUPT_MASK_REG 0x02
#define READ_ENCODER_REG 0x03
#define READ_SWITCH_REG 0x04
#define READ_TOUCH_REG 0x05
#define SET_LED_REG 0x06

#define EN0_INT 0x01
#define EN1_INT 0x02
#define EN2_INT 0x04
#define EN3_INT 0x08
#define SW0_INT 0x10
#define SW1_INT 0x20
#define SW2_INT 0x40
#define SW3_INT 0x80
#define TS_INT 0x100

#define LED0 0x01
#define LED1 0x02
#define LED_ON 0x04
#define LED_OFF 0x00

#define PICO_INTERRUPT_PIN 15

volatile uint16_t config=0x0F0;  // Active Low Interrupt, Invert SW, Invert SW, Invert SW, Invert SW, FWD, FWD, FWD, FWD

volatile uint16_t interrupt_mask;

#define TOUCH_PRESSED 0x02
#define TOUCH_RELEASED 0x01
volatile uint8_t touch_status;
volatile int16_t touch_X;
volatile int16_t touch_Y;
volatile bool pressed = false;
volatile bool has_moved = false;
volatile long start_freq;
volatile long new_freq;

static long zoomBandwidth[5]= {192000L,96000L,48000L,24000L,12000L};

volatile int16_t last_X;

volatile int16_t encoder_value[4] = {0,0,0,0};
int G0ORXButtonPressed = -1;

//bool keypadHit=false;

static const uint8_t my_keymap[22] = { // note 18 and 19 are not connected
    0,
    0,  4,  8, 12, 16,
    1,  5,  9, 13, 17,
    2,  6, 10, 14, 18,
    3,  7, 11, 15, 19,
    255
};


static void KeypadInit() {
  tft.KeypadInit(true, false, 0, 0, 0, false, false);
  tft.SetKeyMap(my_keymap);
}

// initial processing only to handle frequency change

static void update_frequency(long new_freq) {
  Debug(String(__FUNCTION__)+": "+String(new_freq));
  centerTuneFlag = 1;
  if (T41State == CW_XMIT && EEPROMData.decoderFlag == DECODE_ON) {
    ResetHistograms();
  }
  EEPROMData.centerFreq = new_freq;
  TxRxFreq = EEPROMData.centerFreq + NCOFreq;
  centerTuneFlag = 1;
}

FASTRUN
static void process_touch() {
  Debug(String(__FUNCTION__));
  if(menuStatus==NO_MENUS_ACTIVE) {
    long hzPerPixel = zoomBandwidth[EEPROMData.spectrum_zoom]/SPECTRUM_RES;;
    if(touch_status==TOUCH_PRESSED) {
      if(!pressed) {
        if(touch_X<SPECTRUM_RES) {
          pressed = true;
          has_moved = false;
          last_X = touch_X;
        }
      } else {
        long moved = (long)last_X - (long)touch_X;
        if(has_moved || moved>=10 || moved <=-10) {
          has_moved = true;
          last_X = touch_X;
          if (EEPROMData.activeVFO == VFO_A) {
            start_freq = EEPROMData.currentFreqA;
          } else {
            start_freq = EEPROMData.currentFreqB;
          }
          new_freq = start_freq + (moved * hzPerPixel);
          update_frequency(new_freq);
        }
      }
    } else if(touch_status==TOUCH_RELEASED) {
      if(pressed) {
        if(has_moved) {
          if (EEPROMData.activeVFO == VFO_A) {
            start_freq = EEPROMData.currentFreqA;
          } else {
            start_freq = EEPROMData.currentFreqB;
          }
          new_freq = (start_freq/EEPROMData.freqIncrement)*EEPROMData.freqIncrement;
          update_frequency(new_freq);
        } else {
          int x=last_X-SPECTRUM_LEFT_X;
          if (EEPROMData.activeVFO == VFO_A) {
            start_freq = EEPROMData.currentFreqA - (zoomBandwidth[EEPROMData.spectrum_zoom]/2);
          } else {
            start_freq = EEPROMData.currentFreqB - (zoomBandwidth[EEPROMData.spectrum_zoom]/2);
          }
          new_freq = start_freq + (x * hzPerPixel);
          new_freq = (new_freq/EEPROMData.freqIncrement)*EEPROMData.freqIncrement;
          update_frequency(new_freq);
        }
        pressed = false;
        has_moved = false;
      }
    }
#ifdef G0ORX_TOUCH
  } else {
    if(touch_status==TOUCH_RELEASED) {
      // ignore touch pressed - activate when released
      int i=(touch_Y-115)/25;
      if(touch_X<250) {
        //touchSelectPrimaryMenuIndex(i);
        touchPrimaryMenuIndex=i;
      } else if(touch_X>300) {
        touchSecondaryMenuIndex=i;
      }
    }
#endif
  }
}

FASTRUN
void KeypadInterrupt() {
  //uint8_t status;
  tft.getINTC2();
  G0ORXButtonPressed = tft.KeypadGetKey();
}

FASTRUN
void getKeypad() {
  if(G0ORXButtonPressed==-1) {
    int val=tft.KeypadGetKey();
    if(val!=0xFF) {
      G0ORXButtonPressed=val;
    } else {
      G0ORXButtonPressed=-1;
    }
  }
}

FASTRUN
uint8_t get_byte() {
  uint8_t byte=0;
  while(!Wire1.available()) {
    delay(10);
  }
  byte = Wire1.read();
  return byte;
}

FASTRUN
uint16_t get_interrupt_mask() {

  uint16_t mask;
  uint8_t res;
  
  Debug(String(__FUNCTION__));
  mask=0;
  Wire1.beginTransmission(I2C_ADDRESS);
  Wire1.write(READ_INTERRUPT_MASK_REG);
  res=Wire1.endTransmission(true);
  if(res==0) {
    Wire1.requestFrom(I2C_ADDRESS, 2);
    mask=get_byte()&0xFF;
    mask|=(get_byte()&0xFF)<<8;
  } else {
    Debug(String(__FUNCTION__)+": endTransmission: "+String(res,HEX));
  }
  Debug(String(__FUNCTION__)+": 0x"+String(mask,HEX));
  return mask;
}

FASTRUN
int16_t get_encoder_value(uint8_t encoder) {
  int16_t result=0;
  uint8_t res;

  Debug(String(__FUNCTION__)+": encoder="+String(encoder));
  Wire1.beginTransmission(I2C_ADDRESS);
  Wire1.write(READ_ENCODER_REG);
  Wire1.write(encoder);
  res=Wire1.endTransmission(true);
  if(res==0) {
    Wire1.requestFrom(I2C_ADDRESS, 2);  // request 2 bytes from Pico}
    result=get_byte()&0xFF;
    result|=(get_byte()&0xFF)<<8;

  } else {
    Debug(String(__FUNCTION__)+": endTransmission: "+String(res,HEX));
  }

  Debug(String(__FUNCTION__)+": "+String(result));
  return result;
}

FASTRUN
uint8_t get_switch_state(uint8_t sw) {
  uint8_t state=0;
  uint8_t res;
  Debug(String(__FUNCTION__)+": sw="+String(sw));
  Wire1.beginTransmission(I2C_ADDRESS);
  Wire1.write(READ_SWITCH_REG);
  Wire1.write(sw);
  res=Wire1.endTransmission(true);

  if(res==0) {
    Wire1.requestFrom(I2C_ADDRESS, 1);  // request 1 bytes from Pico
    state = get_byte();
  } else {
    Debug(String(__FUNCTION__)+": endTransmission: "+String(res,HEX));
  }
  return state;
}

FASTRUN
void get_touch() {
  uint8_t res;
  Wire1.beginTransmission(I2C_ADDRESS);
  Wire1.write(READ_TOUCH_REG);
  res=Wire1.endTransmission(true);
  if(res==0) {
    Wire1.requestFrom(I2C_ADDRESS, 5);
    touch_status = get_byte();
    touch_X=get_byte()&0xFF;
    touch_X|=(get_byte()&0xFF)<<8;
    touch_Y=get_byte()&0xFF;
    touch_Y|=(get_byte()&0xFF)<<8;
  }
  Debug(String(__FUNCTION__)+": 0x"+String(touch_status,HEX)+" X="+String(touch_X)+" Y="+String(touch_Y));
}

FASTRUN
void FrontPanelSetLed(int led,bool state) {
  uint8_t res;
  Debug(String(__FUNCTION__)+": led+"+String(led)+" state="+String(state));

  Wire1.beginTransmission(I2C_ADDRESS);
  Wire1.write(SET_LED_REG);
  Wire1.write((uint8_t)led);
  Wire1.write((uint8_t)state);
  res=Wire1.endTransmission(true);
  if(res!=0) {
    Debug(String(__FUNCTION__)+": endTransmission: "+String(res,HEX));
  }
}

FASTRUN
static void send_config() {
  uint8_t res;

  Debug("Configure Pico config=0x"+String(config,HEX));
  configured_pico=false;
  while(!configured_pico) {
    Wire1.beginTransmission(I2C_ADDRESS);
    Wire1.write(CONFIG_REG);
    Wire1.write(config&0xFF);
    Wire1.write((config>>8)&0xFF);
    res=Wire1.endTransmission(true);
    if(res!=0) {
      Debug(String(__FUNCTION__)+": endTransmission: "+String(res,HEX));
    } else {
      configured_pico = true;
    }
    delay(100);
  }
}

FASTRUN
void front_panel_interrupt() {
  int16_t encoder_val;
  long tuneChange = 0L;
  uint8_t switch_state;

  Debug(String(__FUNCTION__));
  __disable_irq();
  uint16_t interrupt_mask=get_interrupt_mask();
  Debug(String(__FUNCTION__)+": mask=0x"+String(interrupt_mask,HEX));
  switch(interrupt_mask) {
    case 0: // nothing
      break;
    case 0x8000: // ready
      //send_config();
      break;
    case 0x01: // Volume
      encoder_value[0]=get_encoder_value(0);
      switch(volumeFunction) {
        case AUDIO_VOLUME:
          EEPROMData.audioVolume += encoder_value[0];

          if (EEPROMData.audioVolume > 100) {  // In range?
            EEPROMData.audioVolume = 100;
          } else {
           if (EEPROMData.audioVolume < 0) {
              EEPROMData.audioVolume = 0;
            }
          }
          break;
        case AGC_GAIN:
          bands[EEPROMData.currentBand].AGC_thresh += encoder_value[0];
          if(bands[EEPROMData.currentBand].AGC_thresh < -20) {
            bands[EEPROMData.currentBand].AGC_thresh = -20;
          } else if(bands[EEPROMData.currentBand].AGC_thresh > 120) {
            bands[EEPROMData.currentBand].AGC_thresh = 120;
          }
          AGCLoadValues();
          break;
        case MIC_GAIN:
          EEPROMData.currentMicGain += encoder_value[0];
          if(EEPROMData.currentMicGain < -40) {
            EEPROMData.currentMicGain = -40;
          } else if(EEPROMData.currentMicGain > 30) {
            EEPROMData.currentMicGain = 30;
          }
          if(radioState == SSB_TRANSMIT_STATE ) {
            comp1.setPreGain_dB(EEPROMData.currentMicGain);
            comp2.setPreGain_dB(EEPROMData.currentMicGain);
          }
          break;
        case SIDETONE_VOLUME:
          EEPROMData.sidetoneVolume += encoder_value[0];
          if(EEPROMData.sidetoneVolume < 0.0 ) {
            EEPROMData.sidetoneVolume = 0.0;
          } else if(EEPROMData.sidetoneVolume > 100.0) {
            EEPROMData.sidetoneVolume = 100.0;
          }
          if(radioState == CW_TRANSMIT_STRAIGHT_STATE || radioState == CW_TRANSMIT_KEYER_STATE) {
            modeSelectOutL.gain(1, EEPROMData.sidetoneVolume/100.0);
            modeSelectOutR.gain(1, EEPROMData.sidetoneVolume/100.0);
          }
          break;
        case NOISE_FLOOR_LEVEL:
          EEPROMData.currentNoiseFloor[EEPROMData.currentBand] += encoder_value[0];
          if(EEPROMData.currentNoiseFloor[EEPROMData.currentBand]<0) {
            EEPROMData.currentNoiseFloor[EEPROMData.currentBand]=0;
          } else if(EEPROMData.currentNoiseFloor[EEPROMData.currentBand]>100) {
            EEPROMData.currentNoiseFloor[EEPROMData.currentBand]=100;
          }
          break;
      }
      volumeChangeFlag = true;
      break;
    case 0x02: // Filter/Menu
      encoder_value[1]=get_encoder_value(1);
      if (calibrateFlag == 0) {
        filter_pos = last_filter_pos - 5 * encoder_value[1];
      } else {
        filterEncoderMove=encoder_value[1];
      }
      break;
    case 0x04: // Center Freq
      encoder_val=get_encoder_value(2);
      if (encoder_val == 0)                                // Nothing read
        break;
      //centerTuneFlag = 1; //AFP 10-03-22
      if (T41State == CW_XMIT && EEPROMData.decoderFlag == DECODE_ON) {        // No reason to reset if we're not doing decoded CW AFP 09-27-22
        ResetHistograms();
      }
      tuneChange=(long)encoder_val;
      EEPROMData.centerFreq += ((long)EEPROMData.freqIncrement * tuneChange);
      TxRxFreq = EEPROMData.centerFreq + NCOFreq;
      centerTuneFlag = 1;
      break;
    case 0x08: // Fine Tune
      encoder_value[3]=get_encoder_value(3);
      fineTuneEncoderMove=encoder_value[3];
      NCOFreq += EEPROMData.stepFineTune * fineTuneEncoderMove; //AFP 11-01-22
      centerTuneFlag = 1;

      // ============  AFP 10-28-22
      if (EEPROMData.activeVFO == VFO_A) {
        EEPROMData.currentFreqA = EEPROMData.centerFreq + NCOFreq;   //AFP 10-05-22
      } else {
        EEPROMData.currentFreqB = EEPROMData.centerFreq + NCOFreq;  //AFP 10-05-22
      }
      // ===============  Recentering at band edges ==========
      if (EEPROMData.spectrum_zoom != 0) {
        if (NCOFreq > (95000 / (1 << EEPROMData.spectrum_zoom)) || NCOFreq < (-93000 / (1 << EEPROMData.spectrum_zoom))) {
          NCOFreq    = 0L;
          EEPROMData.centerFreq = TxRxFreq = EEPROMData.currentFreqA;
        }
      } else {
        if (NCOFreq > (142000) || NCOFreq < (-43000)) {  // Offset tuning window in zoom 1x
          NCOFreq    = 0L;
          EEPROMData.centerFreq = TxRxFreq = EEPROMData.currentFreqA;  //AFP 10-28-22
        }
        centerTuneFlag = 1;
      }
      fineTuneEncoderMove = 0L;
      TxRxFreq = EEPROMData.centerFreq + NCOFreq;  // KF5N
      break;
    case 0x10:
      switch_state=get_switch_state(0);
      if(switch_state==1) {
        G0ORXButtonPressed = NUMBER_OF_SWITCHES+0;
      }
      break;
    case 0x20:
      switch_state=get_switch_state(1);
      if(switch_state==1) {
        G0ORXButtonPressed = NUMBER_OF_SWITCHES+1;
      }
      break;
    case 0x40:
      switch_state=get_switch_state(2);
      if(switch_state==1) {
        G0ORXButtonPressed = NUMBER_OF_SWITCHES+2;
      }
      break;
    case 0x80:
      switch_state=get_switch_state(3);
      if(switch_state==1) {
        G0ORXButtonPressed = NUMBER_OF_SWITCHES+3;
      }
      break;
    case 0x100:
      get_touch();
      process_touch();
      break;
    default:
      Debug(String(__FUNCTION__)+": "+String(interrupt_mask,HEX)+"!");
      break;
  }
  __enable_irq();
}

void FrontPanel2Init() {
  uint8_t res;
  int count=0;

#ifdef DEBUG_MESSAGES
  Serial.begin(115000);
#endif


  KeypadInit();

  Debug(String(__FUNCTION__));

  Debug("Setup Wire1");
  Wire1.setClock(100000);
  Wire1.begin();

  Debug("Finding Pico...");
  found_pico=false;
  while(!found_pico && count<10) {
    Wire1.beginTransmission(I2C_ADDRESS);
    res=Wire1.endTransmission(true);
    if(res!=0) {
      Debug("endTransmission: "+String(res,HEX));
      ++count;
    } else {
      found_pico = true;
    }
    delay(50);
  }
  if(found_pico) {
    Debug("Found Pico @0x"+String(I2C_ADDRESS,HEX));
    delay(100);
    send_config();
    // Setup Pico interrupt
    pinMode(PICO_INTERRUPT_PIN,INPUT);
    attachInterrupt(digitalPinToInterrupt(PICO_INTERRUPT_PIN), front_panel_interrupt, FALLING);
  } else {
    Debug("Pico Not Found @0x"+String(I2C_ADDRESS,HEX));
    ShowMessageOnWaterfall("Pico Not Found at 0x"+String(I2C_ADDRESS,HEX));
  }


}

#endif