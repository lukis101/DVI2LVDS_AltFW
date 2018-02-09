
volatile uint16_t vsTicks = 0;

bool btn_pwr_last     = false, btn_pwr_ack  = false;
bool btn_blmode_last  = false, btn_blmode_ack  = false;
bool btn_brplus_last  = false, btn_brplus_ack  = false;
bool btn_brminus_last = false, btn_brminus_ack = false;
uint32_t btn_pwr_lastdown     = 0;
uint32_t btn_brplus_lastdown  = 0;
uint32_t btn_brminus_lastdown = 0;
uint32_t btn_blmode_lastdown  = 0;

void setup() // MCU powered through video cable
{
    pinMode(PIN_PWR_1V8,   OUTPUT);
    pinMode(PIN_PWR_3V3,   OUTPUT);
    pinMode(PIN_PWR_PANEL, OUTPUT);
    pinMode(PIN_PWR_PSU,   OUTPUT);

    pinMode(PIN_BTN_PWR, INPUT_PULLUP);
    pinMode(PIN_BTN_A,   INPUT_PULLUP);
    pinMode(PIN_BTN_B,   INPUT_PULLUP);

    pinMode(PIN_EPMI_ORDER, OUTPUT);
#if (PIXEL_ORDER == PIXEL_ORDER_LEFTRIGHT)
    digitalWrite(PIN_EPMI_ORDER, HIGH);
#elif (PIXEL_ORDER == PIXEL_ORDER_SEQUENTIAL)
    //digitalWrite(PIN_EPMI_ORDER, LOW); // Low by default on AVR
#else
    #error Bad PIXEL_ORDER definition
#endif

    pinMode(PIN_EPMI_SWING, OUTPUT);
#if (LVDS_SWING_LEVEL == LVDS_SWING_LEVEL_HIGH)
    digitalWrite(PIN_EPMI_SWING, HIGH);
#elif (LVDS_SWING_LEVEL == LVDS_SWING_LEVEL_LOW)
    //digitalWrite(PIN_EPMI_SWING, LOW); // Low by default on AVR
#else
    #error Bad LVDS_SWING_LEVEL definition
#endif

#ifdef PANEL_GPIO0
    pinMode(PIN_PANEL_GPIO0, OUTPUT);
    #if (PANEL_GPIO0 == HIGH)
        digitalWrite(PIN_PANEL_GPIO0, HIGH);
    #elif (PANEL_GPIO0 == LOW)
        //digitalWrite(PIN_PANEL_GPIO0, LOW);
    #else
        #error Bad PANEL_GPIO0 definition
    #endif
#endif
#ifdef PANEL_GPIO1
    pinMode(PIN_PANEL_GPIO1, OUTPUT);
    #if (PANEL_GPIO1 == HIGH)
        digitalWrite(PIN_PANEL_GPIO1, HIGH);
    #elif (PANEL_GPIO1 == LOW)
        //digitalWrite(PIN_PANEL_GPIO1, LOW);
    #else
        #error Bad PANEL_GPIO1 definition
    #endif
#endif

    Serial.begin(38400);
    Serial.write("Init\n");
    EEP_LoadConfig();
    I2C_Init();
    BL_Init();

    // Start listening for signal
    SetPowerMode(PWRMODE_STANDBY);
}

void loop()
{
    switch (pwrMode)
    {
    case PWRMODE_ON:
        if (!checkPower())
        {
            Serial.write("Power lost!\n");
            SetPowerMode(PWRMODE_STANDBY);
            break;
        }
        if (bl_timOvfls >= 10)
        {
            Serial.write("Signal lost!\n");
            SetPowerMode(PWRMODE_STANDBY);
            break;
        }
        BL_Update(millis());
        break;
    case PWRMODE_STANDBY:
        if (vsTicks >= 10)
        {
            Serial.write("Signal detected...\n");
            SetPowerMode(PWRMODE_ON);
        }
        break;
    case PWRMODE_OFF:
        break;
    default: // Invalid
        pwrMode = PWRMODE_OFF;
        break;
    }

    /* DDC/CI commands */
    ProcessDDC();

    /* Serial input */
      while (Serial.available() > 0)
      {
          byte inbyte = Serial.read();
          if (serMode < BIN_HIZ) // Normal mode
          {
              if (inbyte == 0x30) // STK_GET_SYNC - reset for programming
                  reset();
              else
                  parseCommand(inbyte);
          }
          else
          {
              parseBinMode(inbyte);
          }
      }

    /* Button input */
    // TODO restore defaults with a special combination
    // POWER
    bool btn_pwr = digitalRead(PIN_BTN_PWR) == LOW;
    if (btn_pwr) // is down
    {
        uint32_t curtime = millis(); // TODO check for timer overflow
        if (!btn_pwr_last) // pressed
        {
            btn_pwr_lastdown = curtime;
            btn_pwr_ack = false;
        }
        else if (((curtime-btn_pwr_lastdown) > MEDIUMPRESS) // debounce
         && !btn_pwr_ack)
        {
            btn_pwr_ack = true;
            if (pwrMode == PWRMODE_OFF)
                SetPowerMode(PWRMODE_STANDBY);
            else
                SetPowerMode(PWRMODE_OFF);
        }
    }
    btn_pwr_last = btn_pwr;

    // Only process the rest if the monitor is on
    if (pwrMode == PWRMODE_ON)
    {
        uint16_t abtn_a = analogRead(PIN_BTN_A);
        uint16_t abtn_b = analogRead(PIN_BTN_B);
        bool btn_brplus = abtn_a < 70;
        bool btn_brminus = abtn_b < 70;
        bool btn_blmode = (abtn_b > 120) && (abtn_b < 180);

        // BRI+ Button
        if (btn_brplus) // is down
        {
            uint32_t curtime = millis(); // TODO check for timer overflow
            if (!btn_brplus_last) // pressed
            {
                btn_brplus_lastdown = curtime;
                btn_brplus_ack = false;
            }
            else if (((curtime-btn_brplus_lastdown) > SHORTPRESS) // debounce
             && !btn_brplus_ack)
            {
                btn_brplus_lastdown = curtime;
                //btn_brplus_ack = true;
                if (UserConfig.blMode == 0)
                {
                    if (UserConfig.brightness <= (255 - BTN_BRI_STEP))
                    {
                        BL_SetBrightness(UserConfig.brightness + BTN_BRI_STEP);
                        Serial.write("PWM: ");
                        Serial.println(UserConfig.brightness);
                        EEP_SaveConfig();
                    }
                }
                else
                {
                    if (UserConfig.strobeLength <= (STROBE_LENGTH_MAX - BTN_STROBELEN_STEP))
                    {
                        UserConfig.strobeLength += BTN_STROBELEN_STEP;
                        Serial.write("Strobe: ");
                        Serial.println(UserConfig.strobeLength);
                        EEP_SaveConfig();
                    }
                }
            }
        }
        btn_brplus_last = btn_brplus;
        
        // BRI- Button
        if (btn_brminus) // is down
        {
            uint32_t curtime = millis(); // TODO check for timer overflow
            if (!btn_brminus_last) // pressed
            {
                btn_brminus_lastdown = curtime;
                btn_brminus_ack = false;
            }
            else if (((curtime-btn_brminus_lastdown) > SHORTPRESS) // debounce
             && !btn_brminus_ack)
            {
                btn_brminus_lastdown = curtime;
                //btn_brminus_ack = true;
                if (UserConfig.blMode == 0)
                {
                    if (UserConfig.brightness >= BTN_BRI_STEP)
                    {
                        BL_SetBrightness(UserConfig.brightness - BTN_BRI_STEP);
                        Serial.write("PWM: ");
                        Serial.println(UserConfig.brightness);
                        EEP_SaveConfig();
                    }
                }
                else
                {
                    if (UserConfig.strobeLength > BTN_STROBELEN_STEP) // Prevent 0
                    {
                        UserConfig.strobeLength -= BTN_STROBELEN_STEP;
                        Serial.write("Strobe: ");
                        Serial.println(UserConfig.strobeLength);
                        EEP_SaveConfig();
                    }
                }
            }
        }
        btn_brminus_last = btn_brminus;

        // Backlight Mode (5) Button
        if (btn_blmode) // is down
        {
            uint32_t curtime = millis(); // TODO check for timer overflow
            if (!btn_blmode_last) // pressed
            {
                btn_blmode_lastdown = curtime;
                btn_blmode_ack = false;
            }
            else if (((curtime-btn_blmode_lastdown) > MEDIUMPRESS) // debounce
             && !btn_blmode_ack)
            {
                btn_blmode_ack = true;
                BLSetMode(UserConfig.blMode == 0 ? 1 : 0);
                EEP_SaveConfig();
            }
        }
        btn_blmode_last = btn_blmode;
    }
}

void SetPowerMode(PowerMode_t newMode)
{
    if (pwrMode == newMode) return;
    switch (newMode)
    {
    case PWRMODE_ON:
        if (pwrMode == PWRMODE_OFF)
        {
            SetPowerMode(PWRMODE_STANDBY);
            break;
        }
        if (checkPower())
        {
            TogglePanel(true);
            pwrMode = PWRMODE_ON;
            Serial.write("ON\n");
        }
        else
        {
            Serial.write("No PWR, STBY\n");
        }
        break;
    case PWRMODE_STANDBY:
        if (pwrMode == PWRMODE_OFF)
        {
            TogglePSU(true);
            ToggleBridge(true);
        }
        else// if (pwrMode == PWRMODE_ON)
        {
            TogglePanel(false);
            TogglePSU(false);
        }
        vsTicks = 0;
        pwrMode = PWRMODE_STANDBY;
        Serial.write("STBY\n");
        break;
    case PWRMODE_OFF:
        if (pwrMode == PWRMODE_ON)
        {
            TogglePanel(false);
            TogglePSU(false);
        }
        ToggleBridge(false);
        pwrMode = PWRMODE_OFF;
        Serial.write("OFF\n");
        break;
    default: // Programmers' fault
        break;
    }
}

bool TogglePSU(bool newstate)
{
    if (newstate == psu_enabled)
        return;
#ifdef CONTROLLED_PSU
    if (newstate)
    {
        digitalWrite(PIN_PWR_PSU, HIGH);
        delay(PSU_BOOTTIME); // Don't push psu too early
        psu_enabled = true;
        Serial.write("PSU ON\n");
    }
    else
    {
        digitalWrite(PIN_PWR_PSU, LOW);
        psu_enabled = false;
        Serial.write("PSU OFF\n");
    }
#else
    psu_enabled = newstate;
#endif
}
bool TogglePanel(bool newstate)
{
    if (newstate == panel_powered)
        return;
    if (newstate)
    {
        digitalWrite(PIN_PWR_PANEL, HIGH);
        panel_powered = true;
        Serial.write("Panel ON\n");
        delay(PANEL_BOOTTIME);
        BL_TurnOn();
    }
    else
    {
        BL_TurnOff();
        digitalWrite(PIN_PWR_PANEL, LOW);
        panel_powered = false;
        Serial.write("Panel OFF\n");
    }
}
bool ToggleBridge(bool newstate)
{
    if (newstate == bridge_powered)
        return;
    if (newstate)
    {
        digitalWrite(PIN_PWR_3V3, HIGH);
        delay(6); // should be < 10ms
        if(digitalRead(PIN_PWRGOOD_3V3) == LOW)
        {
            digitalWrite(PIN_PWR_3V3, LOW);
            Serial.write("3.3V start failed\n");
            return;
        }
        digitalWrite(PIN_PWR_1V8, HIGH);
        bridge_powered = true;
        attachInterrupt(digitalPinToInterrupt(PIN_VSYNC), vsPulse, FALLING);
        Serial.write("EP262 ON\n");
    }
    else
    {
        detachInterrupt(digitalPinToInterrupt(PIN_VSYNC));
        digitalWrite(PIN_PWR_1V8, LOW);
        delay(2); // should be < 10ms
        digitalWrite(PIN_PWR_3V3, LOW);
        bridge_powered = false;
        Serial.write("EP262 OFF\n");
    }
}

// External interrupt
void vsPulse(void) // VSync interrupt
{
    if (pwrMode == PWRMODE_STANDBY)
    {
        // TODO save timestamp to prevent small bursts to accumulate
        vsTicks++;
    }
    else
    {
        BL_VSYNC();
        if (sync3D)
            PORTB &= ~(1<<4); // vsync out pulse start
    }
}

/// --- UTILS --- ///
void SetDefaultConfig(void)
{
    UserConfig.brightness = PWM_DEFAULT;
    UserConfig.blMode = 0; // PWM
    UserConfig.strobeLength = STROBE_LENGTH_DEFAULT;
    UserConfig.strobeOffset = STROBE_OFFSET_DEFAULT;
}

bool checkPower(void)
{
    uint16_t vmon = analogRead(PIN_VMON_PANELREG);
    return (VMON_MIN < vmon) && (vmon < VMON_MAX);
}

