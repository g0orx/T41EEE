#ifndef BEENHERE
#include "SDT.h"
#endif

/*****
  Purpose: A special variant of SetFreq() used only for calibration.

  Parameter list:
    void

  Return value;
    void

  CAUTION: SI5351_FREQ_MULT is set in the si5253.h header file and is 100UL
*****/
void SetFreqCal(int calFreqShift) 
{  // July 7 2023 KF5N
  // NEVER USE AUDIONOINTERRUPTS HERE: that introduces annoying clicking noise with every frequency change
  // SI5351_FREQ_MULT is 100ULL, MASTER_CLK_MULT is 4;
  int cwFreqOffset = (EEPROMData.CWOffset + 6) * 24000 / 256;  // Calculate the CW offset based on user selected CW offset frequency.
  // The SSB LO frequency is always the same as the displayed transmit frequency.
  // The CW LOT frequency must be shifted by 750 Hz due to the way the CW carrier is generated by a quadrature tone.
  if (bands[EEPROMData.currentBand].mode == DEMOD_LSB)
    Clk1SetFreq = (((TxRxFreq + cwFreqOffset + calFreqShift) * SI5351_FREQ_MULT)) * MASTER_CLK_MULT_TX;  // AFP 09-27-22;  KF5N flip CWFreqShift, sign originally minus
  if (bands[EEPROMData.currentBand].mode == DEMOD_USB)
    Clk1SetFreq = (((TxRxFreq - cwFreqOffset - calFreqShift) * SI5351_FREQ_MULT)) * MASTER_CLK_MULT_TX;  // AFP 10-01-22; KF5N flip CWFreqShift, sign originally plus

  //  The receive LO frequency is not dependent on mode or sideband.  CW frequency shift is done in DSP code.
  Clk2SetFreq = ((EEPROMData.centerFreq * SI5351_FREQ_MULT) + IFFreq * SI5351_FREQ_MULT) * MASTER_CLK_MULT_RX;

  //  Set and enable both RX and TX local oscillator outputs.
  si5351.set_freq(Clk2SetFreq, SI5351_CLK2);
  si5351.set_freq(Clk1SetFreq, SI5351_CLK1);
  si5351.output_enable(SI5351_CLK2, 1);
  si5351.output_enable(SI5351_CLK1, 1);

}

/***** //AFP 10-11-22 all new
  Purpose:  Reset tuning to center

  Parameter list:
  void

  Return value;
  void
*****/
void ResetTuning()
{
  currentFreq = EEPROMData.centerFreq + NCOFreq;  // currentFreqA changed to currentFreq.  KF5N August 7, 2023
  NCOFreq = 0L;
  EEPROMData.centerFreq = TxRxFreq = currentFreq ;  //AFP 10-28-22  // currentFreqA changed to currentFreq.  KF5N August 7, 2023
  tft.writeTo(L2);  // Clear layer 2.  KF5N July 31, 2023
  tft.clearMemory();
  SetFreq();  // For new tuning scheme.  KF5N July 22, 2023
  DrawBandWidthIndicatorBar();
  BandInformation();
  ShowFrequency();
  UpdateDecoderField();  // Update Morse decoder if used.
}
// ===== End AFP 10-11-22

/*****
  Purpose: SetFrequency

  Parameter list:
    void

  Return value;
    void

  CAUTION: SI5351_FREQ_MULT is set in the si5253.h header file and is 100UL
*****/
void SetFreq() {  //AFP 09-22-22   Revised July 7 KF5N

  // NEVER USE AUDIONOINTERRUPTS HERE: that introduces annoying clicking noise with every frequency change
  // SI5351_FREQ_MULT is 100ULL, MASTER_CLK_MULT is 4;
  int cwFreqOffset = (EEPROMData.CWOffset + 6) * 24000 / 256;  // Calculate the CW offset based on user selected CW offset frequency.
  // The SSB LO frequency is always the same as the displayed transmit frequency.
  // The CW LOT frequency must be shifted by 750 Hz due to the way the CW carrier is generated by a quadrature tone.
  if (radioState == SSB_TRANSMIT_STATE) {
    Clk1SetFreq = (TxRxFreq * SI5351_FREQ_MULT) * MASTER_CLK_MULT_TX;  // AFP 09-27-22
  } else if (radioState == CW_TRANSMIT_STRAIGHT_STATE || radioState == CW_TRANSMIT_KEYER_STATE) {
    if (bands[EEPROMData.currentBand].mode == DEMOD_LSB) {
      Clk1SetFreq = (((TxRxFreq + cwFreqOffset) * SI5351_FREQ_MULT)) * MASTER_CLK_MULT_TX;  // AFP 09-27-22;  KF5N flip CWFreqShift, sign originally minus
    } else {
      if (bands[EEPROMData.currentBand].mode == DEMOD_USB) {
        Clk1SetFreq = (((TxRxFreq - cwFreqOffset) * SI5351_FREQ_MULT)) * MASTER_CLK_MULT_TX;  // AFP 10-01-22; KF5N flip CWFreqShift, sign originally plus
      }
    }
  }

  //  The receive LO frequency is not dependent on mode or sideband.  CW frequency shift is done in DSP code.
  Clk2SetFreq = ((EEPROMData.centerFreq * SI5351_FREQ_MULT) + IFFreq * SI5351_FREQ_MULT) * MASTER_CLK_MULT_RX;

  if (radioState == SSB_RECEIVE_STATE || radioState == CW_RECEIVE_STATE) {   //  Receive state
    si5351.set_freq(Clk2SetFreq, SI5351_CLK2);
    si5351.output_enable(SI5351_CLK1, 0);  // CLK1 (transmit) off during receive to prevent birdies
    si5351.output_enable(SI5351_CLK2, 1);
  }

  if (radioState == SSB_TRANSMIT_STATE || radioState == CW_TRANSMIT_STRAIGHT_STATE || radioState == CW_TRANSMIT_KEYER_STATE) {  // Transmit state
    si5351.set_freq(Clk1SetFreq, SI5351_CLK1);
    si5351.output_enable(SI5351_CLK2, 0);  // CLK2 (receive) off during transmit to prevent spurious outputs
    si5351.output_enable(SI5351_CLK1, 1);
  }
  //=====================  AFP 10-03-22 =================
  DrawFrequencyBarValue();
}


/*****
  Purpose: Places the Fast Tune cursor in the center of the spectrum display

  Parameter list:

  Return value;
    void
*****/
void CenterFastTune()
{
  tft.drawFastVLine(oldCursorPosition, SPECTRUM_TOP_Y + 20, SPECTRUM_HEIGHT - 27, RA8875_BLACK);
  tft.drawFastVLine(newCursorPosition , SPECTRUM_TOP_Y + 20, SPECTRUM_HEIGHT - 27, RA8875_RED);
}

/*****
  Purpose: Purpose is to sety VFOa to receive frequency and VFOb to the transmit frequency

  Parameter list:
    void

  Return value;
    int           the offset as an int

  CAUTION: SI5351_FREQ_MULT is set in the si5253.h header file and is 100UL
*****/
int DoSplitVFO()
{
  char freqBuffer[15];
  int val;
  long chunk = SPLIT_INCREMENT;
  long splitOffset;

  tft.drawRect(INFORMATION_WINDOW_X - 10, INFORMATION_WINDOW_Y - 2, 260, 200, RA8875_MAGENTA);
  tft.fillRect(INFORMATION_WINDOW_X - 8, INFORMATION_WINDOW_Y, 250, 185, RA8875_BLACK);  // Clear volume field
  tft.setFontScale( (enum RA8875tsize) 1);
  tft.setCursor(INFORMATION_WINDOW_X + 10, INFORMATION_WINDOW_Y + 5);
  tft.print("xmit offset: ");

  splitOffset = chunk;                                                    // Set starting offset to 500Hz
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(INFORMATION_WINDOW_X + 60, INFORMATION_WINDOW_Y + 90);
  tft.print(splitOffset);
  tft.print("Hz  ");
#ifdef G0ORX_FRONTPANEL_2 
  calibrateFlag=1;
#endif
  while (true) {
    if (filterEncoderMove != 0) {                     // Changed encoder?
      splitOffset += filterEncoderMove * chunk;
      tft.fillRect(INFORMATION_WINDOW_X + 60, INFORMATION_WINDOW_Y + 90, 150, 50, RA8875_BLACK);
      tft.setCursor(INFORMATION_WINDOW_X + 60, INFORMATION_WINDOW_Y + 90);
      tft.print(splitOffset);
      tft.print("Hz  ");
    }
    filterEncoderMove = 0L;

    val = ReadSelectedPushButton();                                  // Read pin that controls all switches
    val = ProcessButtonPress(val);
    MyDelay(150L);
    if (val == MENU_OPTION_SELECT) {                              // Make a choice??
      Clk1SetFreq += splitOffset;                                    // New transmit frequency // AFP 09-27-22
      UpdateInfoWindow();
      filterEncoderMove = 0L;
      break;
    }
  }
#ifdef G0ORX_FRONTPANEL_2
  calibrateFlag=0;
#endif
  EEPROMData.currentFreqB = EEPROMData.currentFreqA + splitOffset;
  FormatFrequency(EEPROMData.currentFreqB, freqBuffer);
  tft.fillRect(FREQUENCY_X_SPLIT, FREQUENCY_Y - 12, VFOB_PIXEL_LENGTH, FREQUENCY_PIXEL_HI, RA8875_BLACK);
  tft.setCursor(FREQUENCY_X_SPLIT, FREQUENCY_Y);
  tft.setFont(&FreeMonoBold24pt7b);
  tft.setTextColor(RA8875_GREEN);
  tft.print(freqBuffer);                                          // Show VFO_A

  tft.setFont(&FreeMonoBold18pt7b);
  FormatFrequency(EEPROMData.currentFreqA, freqBuffer);
  tft.setTextColor(RA8875_LIGHT_GREY);
  tft.setCursor(FREQUENCY_X, FREQUENCY_Y + 6);
  tft.print(freqBuffer);                                          // Show VFO_A

  tft.useLayers(1);                 //mainly used to turn on layers!
  tft.layerEffect(OR);
  tft.writeTo(L2);
  tft.clearMemory();
  tft.writeTo(L1);

  tft.setFont(&FreeMono9pt7b);
  tft.setTextColor(RA8875_RED);
  tft.setCursor(FILTER_PARAMETERS_X + 180, FILTER_PARAMETERS_Y + 6);
  tft.print("Split Active");

  tft.setFontDefault();
  return (int) splitOffset;                                       // Can be +/-
}


/*****
  Purpose: Purpose is to sety VFOa to receive frequency and VFOb to the transmit frequency

  Parameter list:
    void

  Return value;
    int           the offset as an int

  CAUTION: SI5351_FREQ_MULT is set in the si5253.h header file and is 100UL
*****/
void ResetFlipFlops() {
  // Toggle GPO1 high momentarily to reset the divide-by-2 flop-flops.
  si5351.output_enable(SI5351_CLK2, 0);
  si5351.output_enable(SI5351_CLK1, 0);
  digitalWrite(0, LOW);
  delay(500);
  digitalWrite(0, HIGH);
  delay(500);
}
