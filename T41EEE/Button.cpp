
#ifndef BEENHERE
#include "SDT.h"
#endif

/*
The button interrupt routine implements a first-order recursive filter, or "leaky integrator,"
as described at:

  https://www.edn.com/a-simple-software-lowpass-filter-suits-embedded-system-applications/

Filter bandwidth is dependent on the sample rate and the "k" parameter, as follows:

                                1 Hz
                          k   Bandwidth   Rise time (samples)
                          1   0.1197      3
                          2   0.0466      8
                          3   0.0217      16
                          4   0.0104      34
                          5   0.0051      69
                          6   0.0026      140
                          7   0.0012      280
                          8   0.0007      561

Thus, the default values below create a filter with 10000 * 0.0217 = 217 Hz bandwidth
*/

#define BUTTON_FILTER_SAMPLERATE 10000  // Hz
#define BUTTON_FILTER_SHIFT 3           // Filter parameter k
#define BUTTON_DEBOUNCE_DELAY 5000      // uSec

#define BUTTON_STATE_UP 0
#define BUTTON_STATE_DEBOUNCE 1
#define BUTTON_STATE_PRESSED 2

#define BUTTON_USEC_PER_ISR (1000000 / BUTTON_FILTER_SAMPLERATE)

#define BUTTON_OUTPUT_UP 1023  // Value to be output when in the UP state

IntervalTimer buttonInterrupts;
bool buttonInterruptsEnabled = false;
static unsigned long buttonFilterRegister;
static int buttonState, buttonADCPressed, buttonElapsed;
static volatile int buttonADCOut;

/*****
  Purpose: ISR to read button ADC and detect button presses

  Parameter list:
    none
  Return value;
    void
*****/

void ButtonISR() {
  int filteredADCValue;

  buttonFilterRegister = buttonFilterRegister - (buttonFilterRegister >> BUTTON_FILTER_SHIFT) + analogRead(BUSY_ANALOG_PIN);
  filteredADCValue = (int)(buttonFilterRegister >> BUTTON_FILTER_SHIFT);

  switch (buttonState) {
    case BUTTON_STATE_UP:
      if (filteredADCValue <= EEPROMData.buttonThresholdPressed) {
        buttonElapsed = 0;
        buttonState = BUTTON_STATE_DEBOUNCE;
      }

      break;
    case BUTTON_STATE_DEBOUNCE:
      if (buttonElapsed < BUTTON_DEBOUNCE_DELAY) {
        buttonElapsed += BUTTON_USEC_PER_ISR;
      } else {
        buttonADCOut = buttonADCPressed = filteredADCValue;
        buttonElapsed = 0;
        buttonState = BUTTON_STATE_PRESSED;
      }

      break;
    case BUTTON_STATE_PRESSED:
      if (filteredADCValue >= EEPROMData.buttonThresholdReleased) {
        buttonState = BUTTON_STATE_UP;
      } else if (EEPROMData.buttonRepeatDelay != 0) {  // buttonRepeatDelay of 0 disables repeat
        if (buttonElapsed < EEPROMData.buttonRepeatDelay) {
          buttonElapsed += BUTTON_USEC_PER_ISR;
        } else {
          buttonADCOut = buttonADCPressed;
          buttonElapsed = 0;
        }
      }

      break;
  }
}

/*****
  Purpose: Starts button IntervalTimer and toggles subsequent button
           functions into interrupt mode.

  Parameter list:
    none
  Return value;
    void
*****/

void EnableButtonInterrupts() {
  buttonADCOut = BUTTON_OUTPUT_UP;
  buttonFilterRegister = buttonADCOut << BUTTON_FILTER_SHIFT;
  buttonState = BUTTON_STATE_UP;
  buttonADCPressed = BUTTON_STATE_UP;
  buttonElapsed = 0;
  buttonInterrupts.begin(ButtonISR, 1000000 / BUTTON_FILTER_SAMPLERATE);
  buttonInterruptsEnabled = true;
}

/*****
  Purpose: Determine which UI button was pressed

  Parameter list:
    int valPin            the ADC value from analogRead()

  Return value;
    int                   -1 if not valid push button, index of push button if valid
*****/
int ProcessButtonPress(int valPin) {
#if defined(G0ORX_FRONTPANEL) || defined(G0ORX_FRONTPANEL_2)
  return valPin;
#else
  int switchIndex;

  if (valPin == BOGUS_PIN_READ) {  // Not valid press
    return -1;
  }

  if (valPin == MENU_OPTION_SELECT && menuStatus == NO_MENUS_ACTIVE) {
    NoActiveMenu();
    return -1;
  }

  for (switchIndex = 0; switchIndex < NUMBER_OF_SWITCHES; switchIndex++) {
    if (abs(valPin - EEPROMData.switchValues[switchIndex]) < WIGGLE_ROOM)  // ...because ADC does return exact values every time
    {
      return switchIndex;
    }
  }

  return -1;  // Really should never do this
#endif
}

/*****
  Purpose: Check for UI button press. If pressed, return the ADC value

  Parameter list:
    none

  Return value;
    int                   -1 if not valid push button, ADC value if valid
*****/
int ReadSelectedPushButton() {
#if defined(G0ORX_FRONTPANEL) || defined(G0ORX_FRONTPANEL_2)
  __disable_irq();
#ifdef G0ORX_FRONTPANEL_2
  getKeypad();
#endif
  int i=G0ORXButtonPressed;
  G0ORXButtonPressed=-1;
  __enable_irq();
  return i;
#else
  minPinRead = 0;
  int buttonReadOld = 1023;

  if (buttonInterruptsEnabled) {
    noInterrupts();
    buttonRead = buttonADCOut;

    /*
    Clear the button read.  If the button remains pressed, the ISR will reset the value nearly
    instantly.  Clearing the value here rather than in the ISR provides more consistent button
    press "feel" when calls to ReadSelectedPushButton have variable timing.
    */

    buttonADCOut = BUTTON_OUTPUT_UP;
    interrupts();
  } else {
    while (abs(minPinRead - buttonReadOld) > 3) {  // do averaging to smooth out the button response
      minPinRead = analogRead(BUSY_ANALOG_PIN);

      buttonRead = .1 * minPinRead + (1 - .1) * buttonReadOld;  // See expected values in next function.
      buttonReadOld = buttonRead;
    }
  }

  if (buttonRead > EEPROMData.switchValues[0] + WIGGLE_ROOM) {  //AFP 10-29-22 per Jack Wilson
    return -1;
  }
  minPinRead = buttonRead;
  if (!buttonInterruptsEnabled) {
    MyDelay(100L);
  }
  return minPinRead;
#endif
}

/*****
  Purpose: Function is designed to route program control to the proper execution point in response to
           a button press.

  Parameter list:
    int vsl               the value from analogRead in loop()

  Return value;
    void
*****/
void ExecuteButtonPress(int val) {
  if (val == MENU_OPTION_SELECT && menuStatus == NO_MENUS_ACTIVE) {  // Pressed Select with no primary/secondary menu selected
    NoActiveMenu();
    return;
  } else {
    menuStatus = PRIMARY_MENU_ACTIVE;
  }
  //Serial.print("val = ");
  //Serial.println(val);
  switch (val) {
    case MENU_OPTION_SELECT:  // 0

      if (menuStatus == PRIMARY_MENU_ACTIVE) {  // Doing primary menu
        ErasePrimaryMenu();
        secondaryMenuChoiceMade = functionPtr[mainMenuIndex]();  // These are processed in MenuProcessing.cpp
        menuStatus = SECONDARY_MENU_ACTIVE;
        secondaryMenuIndex = -1;  // Reset secondary menu
      } else {
        if (menuStatus == SECONDARY_MENU_ACTIVE) {  // Doing primary menu
          menuStatus = PRIMARY_MENU_ACTIVE;
          mainMenuIndex = 0;
        }
      }
      EraseMenus();
      break;

    case MAIN_MENU_UP:                      // 1
      ButtonMenuIncrease();                 // This makes sure the increment does go outta range
      if (menuStatus != NO_MENUS_ACTIVE) {  // Doing primary menu
        ShowMenu(&topMenus[mainMenuIndex], PRIMARY_MENU);
      }
      break;

    case BAND_UP:  // 2 Now calls ProcessIQData and Encoders calls
      EraseMenus();
      if (EEPROMData.currentBand < 5) digitalWrite(bandswitchPins[EEPROMData.currentBand], LOW);  // Added if so unused GPOs will not be touched.  KF5N October 16, 2023.
      ButtonBandIncrease();
      if (EEPROMData.currentBand < 5) digitalWrite(bandswitchPins[EEPROMData.currentBand], HIGH);
      BandInformation();
      NCOFreq = 0L;
      DrawBandWidthIndicatorBar();  // AFP 10-20-22
      //FilterOverlay();   // AFP 10-20-22
      SetFreq();
      ShowSpectrum();
      break;

    case ZOOM:  // 3
      menuStatus = PRIMARY_MENU_ACTIVE;
      EraseMenus();
      ButtonZoom();
      break;

    case MAIN_MENU_DN:  // 4
      ButtonMenuDecrease();
      if (menuStatus != NO_MENUS_ACTIVE) {  // Doing primary menu
        ShowMenu(&topMenus[mainMenuIndex], PRIMARY_MENU);
      }
      break;

    case BAND_DN:  // 5
      EraseMenus();
      ShowSpectrum();  //Now calls ProcessIQData and Encoders calls
      if (EEPROMData.currentBand < 5) digitalWrite(bandswitchPins[EEPROMData.currentBand], LOW);
      ButtonBandDecrease();
      if (EEPROMData.currentBand < 5) digitalWrite(bandswitchPins[EEPROMData.currentBand], HIGH);
      BandInformation();
      NCOFreq = 0L;
      DrawBandWidthIndicatorBar();  //AFP 10-20-22
      //FilterOverlay();            // AFP 10-20-22
      break;

    case FILTER:  // 6
      EraseMenus();
      ButtonFilter();
      break;

    case DEMODULATION:  // 7
      EraseMenus();
      ButtonDemodMode();
      break;

    case SET_MODE:  // 8
      ButtonMode();
      ShowSpectrumdBScale();
      break;

    case NOISE_REDUCTION:  // 9
      ButtonNR();
      break;

    case NOTCH_FILTER:  // 10
      ButtonNotchFilter();
      UpdateNotchField();
      break;

    case NOISE_FLOOR:  // 11
      ButtonSetNoiseFloor();
      break;

    case FINE_TUNE_INCREMENT:  // 12
      UpdateIncrementField();
      break;

    case DECODER_TOGGLE:  // 13
      EEPROMData.decoderFlag = !EEPROMData.decoderFlag;
      UpdateDecoderField();
      break;

    case MAIN_TUNE_INCREMENT:  // 14
      ButtonFreqIncrement();
      break;

    case RESET_TUNING:  // 15   AFP 10-11-22
      ResetTuning();    // AFP 10-11-22
      break;            // AFP 10-11-22

    case UNUSED_1:  // 16
#ifdef G0ORX_VFO
      // Copy VFOA to VFOB
      EEPROMData.currentFreqB = EEPROMData.currentFreqA;
      EEPROMData.currentBandB = EEPROMData.currentBandA;
      if(EEPROMData.activeVFO == VFO_A) {
        ShowFrequency();
      } else {
        NCOFreq = 0L;
        EEPROMData.centerFreq = TxRxFreq = EEPROMData.currentFreqB;
        SetBand();           // KF5N July 12, 2023
        SetBandRelay(HIGH);  // Required when switching VFOs. KF5N July 12, 2023
        SetFreq();
        RedrawDisplayScreen();
        BandInformation();
        ShowBandwidth();
        FilterBandwidth();

        tft.fillRect(FREQUENCY_X_SPLIT, FREQUENCY_Y - 12, VFOB_PIXEL_LENGTH, FREQUENCY_PIXEL_HI, RA8875_BLACK);  // delete old digit
        tft.fillRect(FREQUENCY_X, FREQUENCY_Y - 12, VFOA_PIXEL_LENGTH, FREQUENCY_PIXEL_HI, RA8875_BLACK);        // delete old digit  tft.setFontScale( (enum RA8875tsize) 0);
        ShowFrequency();
        // Draw or not draw CW filter graphics to audio spectrum area.  KF5N July 30, 2023
        tft.writeTo(L2);
        tft.clearMemory();
        if (EEPROMData.xmtMode == CW_MODE) BandInformation();
        DrawBandWidthIndicatorBar();
        DrawFrequencyBarValue();
      }
#else
      if (calOnFlag == 0) {
        ButtonFrequencyEntry();
      }
#endif
      break;

    case BEARING:  // 17  // AFP 10-11-22#ifdef G0ORX_VFO
#ifdef G0ORX_VFO
      // Copy VFOB to VFOA
      EEPROMData.currentFreqA = EEPROMData.currentFreqB;
      EEPROMData.currentBandA = EEPROMData.currentBandB;
      if(EEPROMData.activeVFO == VFO_B) {
        // just need to update display
        ShowFrequency();
      } else {
        NCOFreq = 0L;
        EEPROMData.centerFreq = TxRxFreq = EEPROMData.currentFreqA;
        SetBand();           // KF5N July 12, 2023
        SetBandRelay(HIGH);  // Required when switching VFOs. KF5N July 12, 2023
        SetFreq();
        RedrawDisplayScreen();
        BandInformation();
        ShowBandwidth();
        FilterBandwidth();

        tft.fillRect(FREQUENCY_X_SPLIT, FREQUENCY_Y - 12, VFOB_PIXEL_LENGTH, FREQUENCY_PIXEL_HI, RA8875_BLACK);  // delete old digit
        tft.fillRect(FREQUENCY_X, FREQUENCY_Y - 12, VFOA_PIXEL_LENGTH, FREQUENCY_PIXEL_HI, RA8875_BLACK);        // delete old digit  tft.setFontScale( (enum RA8875tsize) 0);
        ShowFrequency();
        // Draw or not draw CW filter graphics to audio spectrum area.  KF5N July 30, 2023
        tft.writeTo(L2);
        tft.clearMemory();
        if (EEPROMData.xmtMode == CW_MODE) BandInformation();
        DrawBandWidthIndicatorBar();
        DrawFrequencyBarValue();
      }
#else
      int buttonIndex, doneViewing, valPin;
      float retVal;

      tft.clearScreen(RA8875_BLACK);

      DrawKeyboard();
      CaptureKeystrokes();
      retVal = BearingHeading(keyboardBuffer);


      if (retVal != -1.0) {  // We have valid country
        bmpDraw((char *)myMapFiles[selectedMapIndex].mapNames, IMAGE_CORNER_X, IMAGE_CORNER_Y);
        doneViewing = false;
      } else {
        tft.setTextColor(RA8875_RED);
        tft.setCursor(380 - (17 * tft.getFontWidth(0)) / 2, 240);  // Center message
        tft.print("Country not found");
        tft.setTextColor(RA8875_WHITE);
      }
      while (true) {
        valPin = ReadSelectedPushButton();  // Poll UI push buttons
        MyDelay(100L);
        if (valPin != BOGUS_PIN_READ) {              // If a button was pushed...
          buttonIndex = ProcessButtonPress(valPin);  // Winner, winner...chicken dinner!
          switch (buttonIndex) {
            case BEARING:  // Pressed puchbutton 18
              doneViewing = true;
              break;
            default:
              break;
          }
        }

        if (doneViewing == true) {
          //tft.clearMemory();          // Need to clear overlay too
          //tft.writeTo(L2);
          //tft.fillWindow();
          break;
        }
      }
      RedrawDisplayScreen();
      ShowFrequency();
      DrawFrequencyBarValue();
#endif
      break;
#if (defined(G0ORX_FRONTPANEL) || defined(G0ORX_FRONTPANEL_2))
    case 18:                                                      // 18 - Encoder 1 SW (Volume)
      switch(volumeFunction) {
        case AUDIO_VOLUME:
          volumeFunction=AGC_GAIN;
          break;
        case AGC_GAIN:
          volumeFunction=MIC_GAIN;
          break;
        case MIC_GAIN:
          volumeFunction=SIDETONE_VOLUME;
          break;
        case SIDETONE_VOLUME:
          volumeFunction=NOISE_FLOOR_LEVEL;
          break;
        case NOISE_FLOOR_LEVEL:
          volumeFunction=AUDIO_VOLUME;
          break;
      }
      volumeChangeFlag = true;
      break;

    case 19:                                                      // 19 - Encoder 2 SW (Filter/Menu)
      // Temp use to step through AGC
      EEPROMData.AGCMode++;
      if(EEPROMData.AGCMode>4) {
        EEPROMData.AGCMode = 0;
      }

      AGCLoadValues();

      //EEPROMData.AGCMode = AGCMode;                               // Store in EEPROM and...
      EEPROM.put(EEPROM_BASE_ADDRESS, EEPROMData);                // ...save it
      UpdateAGCField();
      break;

    case 20:                                                      // 20 - Encoder 3 SW (Center Tune)
      // switch between VFO-A and VFO-B
      if (EEPROMData.activeVFO == VFO_A) {
        EEPROMData.centerFreq = TxRxFreq = EEPROMData.currentFreqB;
        EEPROMData.activeVFO = VFO_B;
        EEPROMData.currentBand = EEPROMData.currentBandB;
        tft.fillRect(FILTER_PARAMETERS_X + 180, FILTER_PARAMETERS_Y, 150, 20, RA8875_BLACK);      // Erase split message
        splitOn = 0;
      } else { // Must be VFO-B
        EEPROMData.centerFreq = TxRxFreq = EEPROMData.currentFreqA;
        EEPROMData.activeVFO = VFO_A;
        EEPROMData.currentBand = EEPROMData.currentBandA;
        tft.fillRect(FILTER_PARAMETERS_X + 180, FILTER_PARAMETERS_Y, 150, 20, RA8875_BLACK);      // Erase split message
        splitOn = 0;
      }
      bands[EEPROMData.currentBand].freq = TxRxFreq;
      SetBand();  // KF5N July 12, 2023
      SetBandRelay(HIGH);  // Required when switching VFOs. KF5N July 12, 2023
      SetFreq();
      RedrawDisplayScreen();
      BandInformation();
      ShowBandwidth();
      FilterBandwidth();
      //EEPROMData.activeVFO = activeVFO;

      tft.fillRect(FREQUENCY_X_SPLIT, FREQUENCY_Y - 12, VFOB_PIXEL_LENGTH, FREQUENCY_PIXEL_HI, RA8875_BLACK); // delete old digit
      tft.fillRect(FREQUENCY_X,       FREQUENCY_Y - 12, VFOA_PIXEL_LENGTH, FREQUENCY_PIXEL_HI, RA8875_BLACK); // delete old digit  tft.setFontScale( (enum RA8875tsize) 0);
      ShowFrequency();
      // Draw or not draw CW filter graphics to audio spectrum area.  KF5N July 30, 2023
      tft.writeTo(L2);
      tft.clearMemory();
      if(EEPROMData.xmtMode == CW_MODE) BandInformation();
      DrawBandWidthIndicatorBar();
      DrawFrequencyBarValue();
      break;

    case 21:                                                      // 21 - Encoder 4 SW (Fine Tune)
      // swap VFO A and B
      long tempFreq = EEPROMData.currentFreqA;
      int tempBand = EEPROMData.currentBandA;
      EEPROMData.currentFreqA = EEPROMData.currentFreqB;
      EEPROMData.currentBandA = EEPROMData.currentBandB;
      EEPROMData.currentFreqB = tempFreq;
      EEPROMData.currentBandB = tempBand;
      if (EEPROMData.activeVFO == VFO_A) {
        EEPROMData.centerFreq = TxRxFreq = EEPROMData.currentFreqA;
        EEPROMData.currentBand = EEPROMData.currentBandA;
      } else { // must be VFO-B
        EEPROMData.centerFreq = TxRxFreq = EEPROMData.currentFreqB;
        EEPROMData.currentBand = EEPROMData.currentBandB;
      }
      bands[EEPROMData.currentBand].freq = TxRxFreq;
      SetBand();  // KF5N July 12, 2023
      SetBandRelay(HIGH);  // Required when switching VFOs. KF5N July 12, 2023
      SetFreq();
      RedrawDisplayScreen();
      BandInformation();
      ShowBandwidth();
      FilterBandwidth();
      //EEPROMData.activeVFO = activeVFO;

      tft.fillRect(FREQUENCY_X_SPLIT, FREQUENCY_Y - 12, VFOB_PIXEL_LENGTH, FREQUENCY_PIXEL_HI, RA8875_BLACK); // delete old digit
      tft.fillRect(FREQUENCY_X,       FREQUENCY_Y - 12, VFOA_PIXEL_LENGTH, FREQUENCY_PIXEL_HI, RA8875_BLACK); // delete old digit  tft.setFontScale( (enum RA8875tsize) 0);
      ShowFrequency();
      // Draw or not draw CW filter graphics to audio spectrum area.  KF5N July 30, 2023
      tft.writeTo(L2);
      tft.clearMemory();
      if(EEPROMData.xmtMode == CW_MODE) BandInformation();
      DrawBandWidthIndicatorBar();
      DrawFrequencyBarValue();
      break;
#endif
  }
}


/*****
  Purpose: To process a tuning increment button push

  Parameter list:
    void

  Return value:
    void
*****/
void ButtonFreqIncrement() {
  EEPROMData.tuneIndex--;
  if (EEPROMData.tuneIndex < 0)
    EEPROMData.tuneIndex = MAX_FREQ_INDEX - 1;
  EEPROMData.freqIncrement = incrementValues[EEPROMData.tuneIndex];
  DisplayIncrementField();
}


/*****
  Purpose: Error message if Select button pressed with no Menu active

  Parameter list:
    void

  Return value;
    void
*****/
void NoActiveMenu() {
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_RED);
  tft.setCursor(10, 0);
  tft.print("No menu selected");

  menuStatus = NO_MENUS_ACTIVE;
  mainMenuIndex = 0;
  secondaryMenuIndex = 0;
}
