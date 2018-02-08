
#define TIM1_PRESCALE      ((0<<CS12) | (1<<CS11) | (0<<CS10))

#define CONNECT_PWM_PIN()    TCCR2A |= (1<<COM2B1);
#define DISCONNECT_PWM_PIN() TCCR2A &= ~(1<<COM2B1);

#ifdef BL_WARMUP_NEEDED
bool bl_warmup = false;
uint32_t bl_warmupstart = 0;
#endif

bool bl_active = false;
bool bl_strobing = false;
volatile uint32_t bl_timTicks = 0;
volatile uint8_t bl_timOvfls = 0;

void BL_Init(void)
{
    // Frame timing
    TCCR1B = 0; // stop timer
    TCCR1A = 0; // normal mode, pins not connected
    TIMSK1 = 0; // no interrupts
    TCNT1 = 0;
    TCCR1B = TIM1_PRESCALE; // start frame timer

    // PWM
    TCCR2B = 0; // stop timer
    TCCR2A = ((!BL_HALFFREQ)<<WGM21) | (1<<WGM20); // fast or phase-correct pwm
    TCNT2 = 0;

    pinMode(PIN_BL_ON,  OUTPUT);
    pinMode(PIN_BL_PWM, OUTPUT);
}
void BL_Update(unsigned long curtime)
{
    //if (!bl_active) return;
#ifdef BL_WARMUP_NEEDED
    if (bl_warmup)
    {
        if((curtime-bl_warmupstart) >= BL_WARMUP_DURATION)
        {
            PORTD &= ~(1<<3); // TODO fade
            bl_warmup = false;
            BLStart();
        }
    }
#endif
}

void BLSetMode(uint8_t newMode)
{
    if (!bl_active || (newMode == UserConfig.blMode) )
        return;
#ifdef BL_WARMUP_NEEDED
    if (bl_warmup)
      return;
#endif

    if (newMode == 0)
    {
        bl_strobing = false;
        BLStartPWM();
        Serial.write("BL PWM-ing\n");
    }
    else
    {
        BLStopPWM();
        bl_strobing = true;
        Serial.write("BL Strobing\n");
    }
    UserConfig.blMode = newMode;
}

void BLStartPWM(void)
{
    OCR2B = UserConfig.brightness;
    CONNECT_PWM_PIN();
    TCNT2 = 0;
    TCCR2B = TIM2_PRESCALE; // start PWM timer
}
void BLStopPWM(void)
{
    TCCR2B = 0; // stop PWM timer
    DISCONNECT_PWM_PIN();
}

void BLStart(void)
{
    TIMSK1 = (1<<TOIE1); // overflow interrupt for timeout
    TCCR1B = TIM1_PRESCALE; // start frame timer
    bl_timOvfls = 0;
    if (UserConfig.blMode == 0)
        BLStartPWM();
    else
        bl_strobing = true;
    Serial.write("BL Started\n");
}
void BLStop(void)
{
    TCCR1B = 0; // stop frame timer
    TIMSK1 = 0;
    if (UserConfig.blMode == 0)
        BLStopPWM();
    else
        bl_strobing = false;
    Serial.write("BL Stopped\n");
}

void BL_TurnOn(void)
{
#ifdef BL_WARMUP_NEEDED
    PORTD |= (1<<3); // 100% duty cycle warmup
    bl_warmup = true;
    bl_warmupstart = millis();
#else
    BLStart();
#endif
    bl_active = true;
    digitalWrite(PIN_BL_ON, HIGH);
}
void BL_TurnOff(void)
{
    bl_active = false;
    digitalWrite(PIN_BL_ON, LOW);
#ifdef BL_WARMUP_NEEDED
    if (bl_warmup) // warmup not finished
    {
        PORTD &= ~(1<<3);
        bl_warmup = false;
        return;
    }
    else
    {
#endif
      BLStop();
#ifdef BL_WARMUP_NEEDED
    }
#endif
}

void BL_SetBrightness(uint8_t bright)
{
    UserConfig.brightness = bright;
    if (bl_active)
    {
#ifdef BL_WARMUP_NEEDED
        if (!bl_warmup)
        {
#endif
            if (UserConfig.blMode == 0)
                OCR2B = UserConfig.brightness;
#ifdef BL_WARMUP_NEEDED
        }
#endif
    }
}

inline void BL_VSYNC(void)
{
    uint16_t frametime = TCNT1;
    if (bl_strobing)
    {
        TCCR1B = 0; // stop frame timer
        TCNT1 = 0; // reset it
        TIFR1 = 0xFF; // clear pending interrupts
        OCR1A = UserConfig.strobeOffset; // TODO calculate from percentage
        TIMSK1 |= (1<<OCIE1A); // enable start interrupt
        TCCR1B = TIM1_PRESCALE; // restart timer
    }
    bl_timTicks = frametime;
    bl_timOvfls = 0;
}

ISR(TIMER1_COMPA_vect) // Start backlight strobe
{
    TCCR1B = 0; // pause frame timer
    PORTD |= (1<<3); // lights on
    OCR1B = TCNT1 + UserConfig.strobeLength; // strobe end time
    TIMSK1 |= (1<<OCIE1B);  // enable next interrupt
    TIMSK1 &= ~(1<<OCIE1A); // disable this one
    TCCR1B = TIM1_PRESCALE; // resume
    if (sync3D)
        PORTB |= (1<<4); // end vsync out pulse
}
ISR(TIMER1_COMPB_vect) // End backlight strobe
{
    PORTD &= ~(1<<3); // lights out
    TIMSK1 &= ~(1<<OCIE1B); // disable this interrupt
}
// Strobe timer overflow
// Indicates no vsync pulse received within timeframe
ISR(TIMER1_OVF_vect)
{
    bl_timOvfls++;
    bl_timTicks = 0;
}

