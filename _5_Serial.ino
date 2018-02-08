
// Interface is programmed to mimic a BusPirate for easier access to EEPROMS

const uint8_t CMDLIST_LINES = 11;
const char* CMDLIST[CMDLIST_LINES] = {
    "\nCommands:\n",
    "--------------------------\n",
    "?     Show this menu\n",
    "i     Version/status info\n",
    "a/A/@ Panel GPIOx LOW/HIGH/Read\n",
    "c/C   Panel GPIOx assign 0/1\n",
    "e/E   Target EEPROM EDID/epmi\n",
    "f     Print VSYNC frequency\n",
    "m/M   Change mode (I2C print)\n",
    "v     Show volts and states\n",
    "w/W   Power off/ON\n"
};

const uint8_t RESP_ACK = 1;
const uint8_t RESP_NACK = 0;
const uint8_t RESP_SUCCESS = 1;
const uint8_t RESP_FAIL = 0;

const uint8_t BINMODE_HIZ = 1;
const uint8_t BINMODE_I2C = 2;

uint8_t nullcount = 0; // for entering bin mode
uint8_t lastcmd = 0;
uint16_t dataleft = 0;
uint8_t paramindex = 0;

typedef enum {
    NORMAL_IDLE = 0,
    NORMAL_CMD,
    NORMAL_DATA,
    BIN_CMD,
    BIN_PARAMS,
    BIN_DATA
} serState_t;
serState_t serstate = NORMAL_CMD;

void parseBinMode(char inbyte)
{
    switch (serstate)
    {
    case BIN_CMD:
        if (inbyte == 0) // Back to Hi-Z
        {
            if (serMode != BIN_HIZ)
                Serial.write("BBIO1");
            serMode = BIN_HIZ;
            return;
        }
        else if (inbyte == 0x02) // I2C mode
        {
            serMode = BIN_I2C;
            I2C_SetMaster();
            Serial.write("I2C1");
            return;
        }
        else if (inbyte == 0x08) // Write then read
        {
            if (serMode == BIN_I2C)
            {
                serstate = BIN_PARAMS;
                dataleft = 4;
                paramindex = 0;
            }
        }
        else if ((inbyte & 0b11111100) == 0b01100000) // Set I2C speed
        {
            if (serMode == BIN_I2C)
            {
                switch(inbyte & 3)
                {
                    case 0: Wire.setClock(5000); break;
                    case 1: Wire.setClock(50000); break;
                    case 2: Wire.setClock(100000); break;
                    case 3: Wire.setClock(400000); break;
                }
                Serial.write(RESP_SUCCESS);
            }
        }
        else if ((inbyte & 0b11110000) == 0b01000000) // Pin on/off
        {
            i2c_eep2 = (inbyte>>3)&1;
            Serial.write(RESP_SUCCESS); // Always on..
        }
        else if (inbyte == 0b00001111) // HW Reset
        {
           serstate = NORMAL_CMD; // Back to terminal mode
           serMode = NORMAL_HIZ;
           I2C_SetSlave();
        }
        else
        {
            Serial.write(RESP_FAIL);
        }
        lastcmd = inbyte;
        break;
    case BIN_PARAMS:
        i2c_params[paramindex++] = inbyte;
        if (paramindex == dataleft)
        {
            //uint16_t writecount = (uint16_t(i2c_params[0])<<8) | i2c_params[1];
            uint8_t writecount = i2c_params[1];
            if (writecount > 0)
            {
                serstate = BIN_DATA;
                dataleft = writecount;
                i2c_writeamount = 0;
            }
            else // should not happen, read needs device addr write
                serstate = BIN_CMD;
        }
        break;
    case BIN_DATA:
        i2c_txbuff[i2c_writeamount++] = inbyte;
        if (i2c_writeamount == 10) // dev addr + mem addr + 8 bytes
        {
            I2C_TransferBuff(10);
            i2c_writeamount = 0;
        }
        if (--dataleft == 0)
        {
            Serial.write(RESP_SUCCESS);
            if (i2c_writeamount == 1)
            {
                uint16_t readcount = (uint16_t(i2c_params[2])<<8) | i2c_params[3];
                if (readcount > 0)
                    I2C_Read(readcount);
                serstate = BIN_CMD;
            }
            else if (i2c_writeamount > 0)
            {
                I2C_TransferBuff(i2c_writeamount);
            }
            serstate = BIN_CMD;
        }
        break;
    default: // not reachable
        serstate = BIN_CMD;
        break;
    }
}
void parseCommand(char inbyte)
{
    Serial.write(inbyte);
    switch (serstate)
    {
    case NORMAL_CMD:
        switch (inbyte)
        {
        case 0:
            nullcount++;
            if (nullcount >= 20)
            {
                serstate = BIN_CMD;
                serMode = BIN_HIZ;
                Serial.print("BBIO1");
                return;
            }
            break;
        case '?': // Print command list
            for(int i=0; i<CMDLIST_LINES; i++)
                Serial.write(CMDLIST[i], strlen(CMDLIST[i]));
            printModePrompt(0);
            break;
        case 'i': // Show info/status
            Serial.write("\nDVI2LVDS\n");
            Serial.write("Panel = ");
            Serial.write('0'+panel_powered);
            Serial.write("; PWM = ");
            Serial.print(UserConfig.brightness);

            Serial.write("\nBTNS: P = ");
            Serial.print(analogRead(PIN_BTN_PWR));
            Serial.write("; A = ");
            Serial.print(analogRead(PIN_BTN_A));
            Serial.write("; B = ");
            Serial.print(analogRead(PIN_BTN_B));
            printModePrompt(1);
            break;
        case 'c': // select panel GPIO 0
            panelgpio = 0;
            printModePrompt(1);
            break;
        case 'C': // select panel GPIO 1
            panelgpio = 1;
            printModePrompt(1);
            break;
        case 'a': // set panel GPIO to LOW
            if (panelgpio == 0)
            {
                pinMode(PIN_PANEL_GPIO0, OUTPUT);
                digitalWrite(PIN_PANEL_GPIO0, LOW);
            }
            else
            {
                pinMode(PIN_PANEL_GPIO1, OUTPUT);
                digitalWrite(PIN_PANEL_GPIO1, LOW);
            }
            printModePrompt(1);
            break;
        case 'A': // set panel GPIO to HIGH
            if (panelgpio == 0)
            {
                pinMode(PIN_PANEL_GPIO0, OUTPUT);
                digitalWrite(PIN_PANEL_GPIO0, HIGH);
            }
            else
            {
                pinMode(PIN_PANEL_GPIO1, OUTPUT);
                digitalWrite(PIN_PANEL_GPIO1, HIGH);
            }
            printModePrompt(1);
            break;
        case '@': // read panel GPIO
            Serial.write("\nGPIO");
            Serial.write('0'+panelgpio);
            Serial.write(" = ");
            if (panelgpio == 0)
            {
                pinMode(PIN_PANEL_GPIO0, INPUT);
                Serial.write('0'+digitalRead(PIN_PANEL_GPIO0));
            }
            else
            {
                pinMode(PIN_PANEL_GPIO1, INPUT);
                Serial.write('0'+digitalRead(PIN_PANEL_GPIO1));
            }
            printModePrompt(1);
            break;
        case 'e': // Select EDID EEPROM
            i2c_eep2 = false;
            Serial.write("\nEDID EEPROM");
            printModePrompt(1);
            break;
        case 'E': // Select epmi EEPROM
            i2c_eep2 = true;
            Serial.write("\nepmi EEPROM");
            printModePrompt(1);
            break;
        case 'm': // Change mode to HIZ
            serMode = NORMAL_HIZ;
            printModePrompt(1);
            break;
        case 'M': // Change mode to I2C logging
            serMode = NORMAL_I2C;
            printModePrompt(1);
            break;
        case 'f': // Print VSYNC frequency
            Serial.write("\n VSYNC: ");
            Serial.print(bl_timTicks);
            printModePrompt(1);
            break;
        case 'v': // read panel GPIO
            Serial.write("\n3V3GOOD = ");
            Serial.write('0'+digitalRead(PIN_PWRGOOD_3V3));
            Serial.write(", PWRGOOD = ");
            Serial.write('0'+checkPower());
            Serial.write("\nPWRIN = ");
            Serial.print(analogRead(PIN_VMON_PANELREG));
            printModePrompt(1);
            break;
      case 'w':
            TogglePanel(false);
            Serial.write("\nPanel OFF");
            printModePrompt(1);
            break;
      case 'W':
            TogglePanel(true);
            Serial.write("\nPanel ON");
            printModePrompt(1);
            break;
        default: // unsupported command
            Serial.write("\nCMD??");
            printModePrompt(1);
            break;
        } // switch(inbyte)
        break;
    default:
        Serial.write("UNDEFINED MODE\n");
        return;
    }
}
void printModePrompt(bool newline)
{
    if (newline)
        Serial.write('\n');
    switch (serMode)
    {
    case NORMAL_HIZ:
        Serial.write("HiZ>");
        break;
    case NORMAL_I2C:
        Serial.write("I2C>");
        break;
    default: // nothing...
        break;
    }
}

