#ifndef BEENHERE
#include "SDT.h"
#endif

/*****
  Purpose: EncoderFilter

  Parameter list:
    void

  Return value;
    void
    Modified AFP21-12-15
*****/
void FilterSetSSB() {
  long filter_change;

  // SSB
  if (filter_pos != last_filter_pos) {
    tft.writeTo(L2);  // Clear layer 2.  KF5N July 31, 2023
    tft.clearMemory();
    if(EEPROMData.xmtMode == CW_MODE) BandInformation(); 
    tft.fillRect((MAX_WATERFALL_WIDTH + SPECTRUM_LEFT_X) / 2 - filterWidth, SPECTRUM_TOP_Y + 17, filterWidth, SPECTRUM_HEIGHT - 20, RA8875_BLACK);  // Erase old filter background
    filter_change = (filter_pos - last_filter_pos);
    if (filter_change >= 1) {
      filterWidth--;
      if (filterWidth < 10)
        filterWidth = 10;
    }
    if (filter_change <= -1) {
      filterWidth++;
      if (filterWidth > 100)
        filterWidth = 50;
    }
    last_filter_pos = filter_pos;
    // =============  AFP 10-27-22
    switch (bands[EEPROMData.currentBand].mode) {
      case DEMOD_LSB:
        if (switchFilterSideband == 0)  // "0" = normal, "1" means change opposite filter
        {
          bands[EEPROMData.currentBand].FLoCut = bands[EEPROMData.currentBand].FLoCut + filter_change * 50 * ENCODER_FACTOR;
          //fHiCutOld= bands[EEPROMData.currentBand].FHiCut;
          FilterBandwidth();
        } else if (switchFilterSideband == 1) {
          //if (abs(bands[EEPROMData.currentBand].FHiCut) < 500) {
          bands[EEPROMData.currentBand].FHiCut = bands[EEPROMData.currentBand].FHiCut + filter_change * 50 * ENCODER_FACTOR;
          fLoCutOld = bands[EEPROMData.currentBand].FLoCut;
        }
        break;
      case DEMOD_USB:
        if (switchFilterSideband == 0) {
          bands[EEPROMData.currentBand].FHiCut = bands[EEPROMData.currentBand].FHiCut - filter_change * 50 * ENCODER_FACTOR;
          //bands[EEPROMData.currentBand].FLoCut= fLoCutOld;
          FilterBandwidth();
        } else if (switchFilterSideband == 1) {
          bands[EEPROMData.currentBand].FLoCut = bands[EEPROMData.currentBand].FLoCut - filter_change * 50 * ENCODER_FACTOR;
          // bands[EEPROMData.currentBand].FHiCut= fHiCutOld;
        }
        break;
      case DEMOD_AM:
        bands[EEPROMData.currentBand].FHiCut = bands[EEPROMData.currentBand].FHiCut - filter_change * 50 * ENCODER_FACTOR;
        bands[EEPROMData.currentBand].FLoCut = -bands[EEPROMData.currentBand].FHiCut;
        FilterBandwidth();
        InitFilterMask();
        break;
      case DEMOD_SAM:  // AFP 11-03-22
        bands[EEPROMData.currentBand].FHiCut = bands[EEPROMData.currentBand].FHiCut - filter_change * 50 * ENCODER_FACTOR;
        bands[EEPROMData.currentBand].FLoCut = -bands[EEPROMData.currentBand].FHiCut;
        FilterBandwidth();
        InitFilterMask();
        break;
    }
    // =============  AFP 10-27-22

    //ControlFilterF();
    Menu2 = MENU_F_LO_CUT;  // set Menu2 to MENU_F_LO_CUT
    FilterBandwidth();
    ShowBandwidth();
    DrawFrequencyBarValue();
    UpdateDecoderField();   // Adjust Morse decoder graphics.
  }
    tft.writeTo(L1);  // Exit function in layer 1.  KF5N August 3, 2023
}

#ifndef G0ORX_FRONTPANEL_2
/*****
  Purpose: EncoderCenterTune.  This is "coarse" tuning.
  Parameter list:
    void
  Return value;
    void
*****/
void EncoderCenterTune() {
  long tuneChange = 0L;
  //  long oldFreq    = EEPROMData.centerFreq;

#ifdef G0ORX_FRONTPANEL
  int result = tuneEncoder.process();  // Read the encoder
#else
  unsigned char result = tuneEncoder.process();  // Read the encoder
#endif

  if (result == 0)  // Nothing read
    return;

  if (EEPROMData.xmtMode == CW_MODE && EEPROMData.decoderFlag == DECODE_ON) {  // No reason to reset if we're not doing decoded CW AFP 09-27-22
    ResetHistograms();
  }
#ifdef G0ORX_FRONTPANEL
  tuneChange = result;
#else
  switch (result) {
    case DIR_CW:  // Turned it clockwise, 16
      tuneChange = 1L;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      tuneChange = -1L;
      break;
  }
#endif
  EEPROMData.centerFreq += ((long)EEPROMData.freqIncrement * tuneChange);  // tune the master vfo
  TxRxFreq = EEPROMData.centerFreq + NCOFreq;
  EEPROMData.lastFrequencies[EEPROMData.currentBand][EEPROMData.activeVFO] = TxRxFreq;
  SetFreq();  //  Change to receiver tuning process.  KF5N July 22, 2023
  //currentFreqA= EEPROMData.centerFreq + NCOFreq;
  DrawBandWidthIndicatorBar();  // AFP 10-20-22
  //FilterOverlay(); // AFP 10-20-22
  ShowFrequency();
  BandInformation();
}


/*****
  Purpose: Encoder volume control

  Parameter list:
    void

  Return value;
    int               0 means encoder didn't move; otherwise it moved
*****/
void EncoderVolume()  //============================== AFP 10-22-22  Begin new
{
#ifdef G0ORX_FRONTPANEL
  int result;
#else
  char result;
#endif 
  int increment [[maybe_unused]] = 0;

  result = volumeEncoder.process();  // Read the encoder

  if (result == 0) {  // Nothing read
    return;
  }
#ifdef G0ORX_FRONTPANEL
  adjustVolEncoder = result;
#else
  switch (result) {
    case DIR_CW:  // Turned it clockwise, 16
      adjustVolEncoder = 1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      adjustVolEncoder = -1;
      break;
  }
#endif
#ifdef G0ORX_FRONTPANEL
  switch(volumeFunction) {
    case AUDIO_VOLUME:
      EEPROMData.audioVolume += adjustVolEncoder;

      if (EEPROMData.audioVolume > 100) {  // In range?
        EEPROMData.audioVolume = 100;
      } else {
        if (EEPROMData.audioVolume < 0) {
          EEPROMData.audioVolume = 0;
        }
      }
      break;
    case AGC_GAIN:
      bands[EEPROMData.currentBand].AGC_thresh += adjustVolEncoder;
      if(bands[EEPROMData.currentBand].AGC_thresh < -20) {
        bands[EEPROMData.currentBand].AGC_thresh = -20;
      } else if(bands[EEPROMData.currentBand].AGC_thresh > 120) {
        bands[EEPROMData.currentBand].AGC_thresh = 120;
      }
      AGCLoadValues();
      break;
    case MIC_GAIN:
      EEPROMData.currentMicGain += adjustVolEncoder;
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
      EEPROMData.sidetoneVolume += adjustVolEncoder;
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
      EEPROMData.currentNoiseFloor[EEPROMData.currentBand] += adjustVolEncoder;
      if(EEPROMData.currentNoiseFloor[EEPROMData.currentBand]<0) {
        EEPROMData.currentNoiseFloor[EEPROMData.currentBand]=0;
      } else if(EEPROMData.currentNoiseFloor[EEPROMData.currentBand]>100) {
        EEPROMData.currentNoiseFloor[EEPROMData.currentBand]=100;
      }
      break;
  }
#else
  EEPROMData.audioVolume += adjustVolEncoder; 
  // simulate log taper.  As we go higher in volume, the increment increases.

//  if (EEPROMData.audioVolume < (MIN_AUDIO_VOLUME + 10)) increment = 2;
//  else if (EEPROMData.audioVolume < (MIN_AUDIO_VOLUME + 20)) increment = 3;
//  else if (EEPROMData.audioVolume < (MIN_AUDIO_VOLUME + 30)) increment = 4;
//  else if (EEPROMData.audioVolume < (MIN_AUDIO_VOLUME + 40)) increment = 5;
//  else if (EEPROMData.audioVolume < (MIN_AUDIO_VOLUME + 50)) increment = 6;
//  else if (EEPROMData.audioVolume < (MIN_AUDIO_VOLUME + 60)) increment = 7;
//  else increment = 8;


  if (EEPROMData.audioVolume > 100) {
    EEPROMData.audioVolume = 100;
  } else  {
    if (EEPROMData.audioVolume < 0) 
      EEPROMData.audioVolume = 0;
  }
#endif

  volumeChangeFlag = true;  // Need this because of unknown timing in display updating.

}  //============================== AFP 10-22-22  End new
#endif

/*****
  Purpose: Use the encoder to change the value of a number in some other function

  Parameter list:
    int minValue                the lowest value allowed
    int maxValue                the largest value allowed
    int startValue              the numeric value to begin the count
    int increment               the amount by which each increment changes the value
    char prompt[]               the input prompt
  Return value;
    int                         the new value
*****/
float GetEncoderValueLive(float minValue, float maxValue, float startValue, float increment, char prompt[])  //AFP 10-22-22
{
  float currentValue = startValue;
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.fillRect(250, 0, 285, CHAR_HEIGHT, RA8875_BLACK); // Increased rectangle size to full erase value.  KF5N August 12, 2023
  tft.setCursor(257, 1);
  tft.print(prompt);
  tft.setCursor(440, 1);
  if (abs(startValue) > 2) {
    tft.print(startValue, 0);
  } else {
    tft.print(startValue, 3);
  }
  //while (true) {
  if (filterEncoderMove != 0) {
    currentValue += filterEncoderMove * increment;  // Bump up or down...
    if (currentValue < minValue)
      currentValue = minValue;
    else if (currentValue > maxValue)
      currentValue = maxValue;

  //  tft.fillRect(449, 0, 90, CHAR_HEIGHT, RA8875_BLACK);  // This is not required. KF5N August 12, 2023
    tft.setCursor(440, 1);
    if (abs(startValue) > 2) {
      tft.print(startValue, 0);
    } else {
      tft.print(startValue, 3);
    }
    filterEncoderMove = 0;
  }
  //tft.setTextColor(RA8875_WHITE);
  return currentValue;
}



/*****
  Purpose: Use the encoder to change the value of a number in some other function

  Parameter list:
    int minValue                the lowest value allowed
    int maxValue                the largest value allowed
    int startValue              the numeric value to begin the count
    int increment               the amount by which each increment changes the value
    char prompt[]               the input prompt
  Return value;
    int                         the new value
*****/
int GetEncoderValue(int minValue, int maxValue, int startValue, int increment, char prompt[]) {
  int currentValue = startValue;
  int val;

  tft.setFontScale((enum RA8875tsize)1);

  tft.setTextColor(RA8875_WHITE);
  tft.fillRect(250, 0, 280, CHAR_HEIGHT, RA8875_MAGENTA);
  tft.setCursor(257, 1);
  tft.print(prompt);
  tft.setCursor(470, 1);
  tft.print(startValue);

  while (true) {
    if (filterEncoderMove != 0) {
      currentValue += filterEncoderMove * increment;  // Bump up or down...
      if (currentValue < minValue)
        currentValue = minValue;
      else if (currentValue > maxValue)
        currentValue = maxValue;

      tft.fillRect(465, 0, 65, CHAR_HEIGHT, RA8875_MAGENTA);
      tft.setCursor(470, 1);
      tft.print(currentValue);
      filterEncoderMove = 0;
    }

    val = ReadSelectedPushButton();  // Read the ladder value
    //MyDelay(100L); //AFP 09-22-22
    if (val != -1 && val < (EEPROMData.switchValues[0] + WIGGLE_ROOM)) {
      val = ProcessButtonPress(val);    // Use ladder value to get menu choice
      if (val == MENU_OPTION_SELECT) {  // Make a choice??
        return currentValue;
      }
    }
  }
}

/*****
  Purpose: Allows quick setting of WPM without going through a menu

  Parameter list:
    void

  Return value;
    int           the current WPM
*****/
int SetWPM() {
  int val;
  long lastWPM = EEPROMData.currentWPM;

  tft.setFontScale((enum RA8875tsize)1);

  tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_MAGENTA);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(SECONDARY_MENU_X + 1, MENUS_Y + 1);
  tft.print("current WPM:");
  tft.setCursor(SECONDARY_MENU_X + 200, MENUS_Y + 1);
  tft.print(EEPROMData.currentWPM);
#ifdef G0ORX_FRONTPANEL_2
  calibrateFlag = 1; 
#endif
  while (true) {
    if (filterEncoderMove != 0) {       // Changed encoder?
      EEPROMData.currentWPM += filterEncoderMove;  // Yep
      lastWPM = EEPROMData.currentWPM;
      if (lastWPM < 5)    // Set minimum keyer speed to 5 wpm.  KF5N August 20, 2023
        lastWPM = 5;
      else if (lastWPM > MAX_WPM)
        lastWPM = MAX_WPM;

      tft.fillRect(SECONDARY_MENU_X + 200, MENUS_Y + 1, 50, CHAR_HEIGHT, RA8875_MAGENTA);
      tft.setCursor(SECONDARY_MENU_X + 200, MENUS_Y + 1);
      tft.print(lastWPM);
      filterEncoderMove = 0;
    }

    val = ReadSelectedPushButton();  // Read pin that controls all switches
    val = ProcessButtonPress(val);
    if (val == MENU_OPTION_SELECT) {  // Make a choice??
      EEPROMData.currentWPM = lastWPM;
      //EEPROMData.EEPROMData.currentWPM = EEPROMData.currentWPM;
      UpdateWPMField();
      break;
    }
  }
  tft.setTextColor(RA8875_WHITE);
  EraseMenus();
#ifdef G0ORX_FRONTPANEL_2
  calibrateFlag = 0;
#endif
  return EEPROMData.currentWPM;
}

/*****
  Purpose: Determines how long the transmit relay remains on after last CW atom is sent.

  Parameter list:
    void

  Return value;
    long            the delay length in milliseconds
*****/
long SetTransmitDelay()  // new function JJP 9/1/22
{
  int val;
  long lastDelay = EEPROMData.cwTransmitDelay;
  long increment = 250;  // Means a quarter second change per detent

  tft.setFontScale((enum RA8875tsize)1);

  tft.fillRect(SECONDARY_MENU_X - 150, MENUS_Y, EACH_MENU_WIDTH + 150, CHAR_HEIGHT, RA8875_MAGENTA);  // scoot left cuz prompt is long
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(SECONDARY_MENU_X - 149, MENUS_Y + 1);
  tft.print("current delay:");
  tft.setCursor(SECONDARY_MENU_X + 79, MENUS_Y + 1);
  tft.print(EEPROMData.cwTransmitDelay);
#ifdef G0ORX_FRONTPANEL_2
  calibrateFlag = 1;
#endif
  while (true) {
    if (filterEncoderMove != 0) {                  // Changed encoder?
      lastDelay += filterEncoderMove * increment;  // Yep
      if (lastDelay < 0L)
        lastDelay = 250L;

      tft.fillRect(SECONDARY_MENU_X + 80, MENUS_Y + 1, 200, CHAR_HEIGHT, RA8875_MAGENTA);
      tft.setCursor(SECONDARY_MENU_X + 79, MENUS_Y + 1);
      tft.print(lastDelay);
      filterEncoderMove = 0;
    }

    val = ReadSelectedPushButton();  // Read pin that controls all switches
    val = ProcessButtonPress(val);
    //MyDelay(150L);  //ALF 09-22-22
    if (val == MENU_OPTION_SELECT) {  // Make a choice??
      EEPROMData.cwTransmitDelay = lastDelay;
      //EEPROMData.EEPROMData.cwTransmitDelay = EEPROMData.cwTransmitDelay;
      EEPROMWrite();
      break;
    }
  }
  tft.setTextColor(RA8875_WHITE);
  EraseMenus();
#ifdef G0ORX_FRONTPANEL_2
  calibrateFlag = 0;
#endif
  return EEPROMData.cwTransmitDelay;
}

#ifndef G0ORX_FRONTPANEL_2
/*****
  Purpose: Fine tune control.

  Parameter list:
    void

  Return value;
    void               cannot return value from interrupt
*****/
FASTRUN  // Causes function to be allocated in RAM1 at startup for fastest performance.
  void
  EncoderFineTune() {
#ifdef G0ORX_FRONTPANEL
  int result;
#else
  char result;
#endif

  result = fineTuneEncoder.process();  // Read the encoder
  if (result == 0) {                   // Nothing read
    fineTuneEncoderMove = 0L;
    return;
  } else {
#ifdef G0ORX_FRONTPANEL
    fineTuneEncoderMove=result;
#else
    if (result == DIR_CW) {  // 16 = CW, 32 = CCW
      fineTuneEncoderMove = 1L;
    } else {
      fineTuneEncoderMove = -1L;
    }
#endif
  }
  NCOFreq += EEPROMData.stepFineTune * fineTuneEncoderMove;  //AFP 11-01-22
  centerTuneFlag = 1;
  // ============  AFP 10-28-22
  if (EEPROMData.activeVFO == VFO_A) {
    EEPROMData.currentFreqA = EEPROMData.centerFreq + NCOFreq;  //AFP 10-05-22
    EEPROMData.lastFrequencies[EEPROMData.currentBand][0] = EEPROMData.currentFreqA;
  } else {
    EEPROMData.currentFreqB = EEPROMData.centerFreq + NCOFreq;  //AFP 10-05-22
    EEPROMData.lastFrequencies[EEPROMData.currentBand][1] = EEPROMData.currentFreqB;
  }
  // ===============  Recentering at band edges ==========
  if (EEPROMData.spectrum_zoom != 0) {
    if (NCOFreq >= (95000 / (1 << EEPROMData.spectrum_zoom)) || NCOFreq < (-93000 / (1 << EEPROMData.spectrum_zoom))) {  // 47500 with 2x zoom.
      centerTuneFlag = 0;
      resetTuningFlag = 1;
      return;
    }
  } else {
    if (NCOFreq > 142000 || NCOFreq < -43000) {  // Offset tuning window in zoom 1x
      centerTuneFlag = 0;
      resetTuningFlag = 1;
      return;
    }
  }
  fineTuneEncoderMove = 0L;
  TxRxFreq = EEPROMData.centerFreq + NCOFreq;  // KF5N
}


FASTRUN  // Causes function to be allocated in RAM1 at startup for fastest performance.
  void EncoderFilter() {
#ifdef G0ORX_FRONTPANEL
  int result;
#else
  char result;
#endif
  result = filterEncoder.process();  // Read the encoder

  if (result == 0) {
    //    filterEncoderMove = 0;// Nothing read
    return;
  }
#ifdef G0ORX_FRONTPANEL
  filterEncoderMove = result;
#else
  switch (result) {
    case DIR_CW:  // Turned it clockwise, 16
      filterEncoderMove = 1;
      //filter_pos = last_filter_pos - 5 * filterEncoderMove;  // AFP 10-22-22
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      filterEncoderMove = -1;
      // filter_pos = last_filter_pos - 5 * filterEncoderMove;   // AFP 10-22-22
      break;
  }
#endif
  if (calibrateFlag == 0) {                                // AFP 10-22-22
    filter_pos = last_filter_pos - 5 * filterEncoderMove;  // AFP 10-22-22
  }                                                        // AFP 10-22-22
}
#endif