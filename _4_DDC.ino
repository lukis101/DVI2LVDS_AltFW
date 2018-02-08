
const byte FRAGLEN = 20;
uint8_t capsOffset = 0;
const char capsString[] = "(prot(monitor)type(lcd)model(vg248)cmds(01 02 03 0c f3)vcp(02 04 10 40 41 42 b6 d6(01 02 04) df)mccs_ver(2.2))";
// Can use "model(ZOWIE XL)" to spoof for BlurBusters strobe utility but value ranges are incompatible
// VG248QE example: "(prot(monitor)type(lcd)model(vg248)cmds(01 02 03 07 0c f3)vcp(02 04 05 08 0b 0c 10 12 14(05 06 08 0b) 16 18 1a 60(03 11 0f) 62 a8 ac ae b6 c6 c8 c9 cc(01 02 03 04 05 06 07 08 09 0a 0c 0d 11 12 14 1a 1e 1f 23 72 73 ) d6(01 04) df)mccs_ver(2.2)asset_eep(32)mpu(01)mswhql(1))";
// U2879G6 example: "(prot(monitor)type(lcd)model(mstar)cmds(01 02 03 07 0c e3 f3)vcp(02 04 05 08 0b 0c 10 12 14(01 05 08 0b) 16 18 1a 52 60(01 03 11 0f) 62 6c 6e 70 8d(01 02) aa(01 02) ac ae b6 c0 c6 c8 c9 ca(01 02) cc(02 03 04 05 07 08 09 0a 0d 01 06 0b 12 14 16 1e) d6(01 05) df e9(00 02) f0(00 01) ff)mswhql(1)asset_eep(40)mccs_ver(2.2))"
// "softMCCS" seems to ignore renaming with "vcpname(41(Strobe length) 42(Strobe phase))". Not tested with vendor-speciffic addresses

typedef enum
{
    VCP_CMD_VCPRequest      = 0x01,
    VCP_CMD_VCPReply        = 0x02,
    VCP_CMD_VCPSet          = 0x03,
    // 04-06 is for button control
    VCP_CMD_TimingRequest   = 0x07,
    VCP_CMD_TimingReport    = 0x4E,
    VCP_CMD_VCPReset        = 0x09,
    VCP_CMD_SaveSettings    = 0x0C,
    VCP_CMD_SelfTest        = 0xB1,
    VCP_CMD_TableRead       = 0xE2,
    VCP_CMD_TableWrite      = 0xE7,
    VCP_CMD_IDRequest       = 0xF1,
    VCP_CMD_CapsRequest     = 0xF3,
    VCP_CMD_CapsReply       = 0xE3,
    VCP_CMD_EnableAppReport = 0xF5,
    VCP_CMD_NV3DRequest     = 0x59,
    VCP_CMD_NV3DReply       = 0x58,
} VCP_CMD_t;
typedef enum
{
    VCP_NewCtrlValue     = 0x02, // To update value shown on host
    VCP_FactoryRestore   = 0x04,
    VCP_RestoreLuminance = 0x05,
    VCP_Luminance        = 0x10,
    VCP_Contrast         = 0x12,
    VCP_ColorPreset      = 0x14,
    VCP_RedGain          = 0x16,
    VCP_GreenGain        = 0x18,
    VCP_BlueGain         = 0x1a,
    VCP_StrobeEnable     = 0x40,
    VCP_StrobeLength     = 0x41,
    VCP_StrobePhase      = 0x42,
    VCP_HorizontalFreq   = 0xAC,
    VCP_VerticalFreq     = 0xAE,
    VCP_SubPixelLayout   = 0xB2,
    VCP_TechnologyType   = 0xB6,
    VCP_OSD              = 0xC0,
    VCP_AppEnableKey     = 0xC6,
    VCP_DispController   = 0xC8,
    VCP_DispFirmware     = 0xC9,
    VCP_StereoMode       = 0xD4,
    VCP_PowerMode        = 0xD6,
    VCP_VCPVer           = 0xDF,
} VCP_Code_t;

void ProcessDDC(void) // part of main loop
{
    // I2C input
    if (i2c_readamount >= 4) // source_addr+len+cmd+chksum
    {
        if (i2c_rxbuff[0] != 0x51) // destination (sub-device)
        {
            i2c_readamount = 0;
            return;
        }

        byte resplen = 0;
        byte i=0;
        byte length = i2c_rxbuff[1] & 0x7f;
        if ((length>0) && ((length+3) == i2c_readamount))
        {
            byte* ptxbuff = i2c_txbuff+2;
            switch (i2c_rxbuff[2])
            {
            case VCP_CMD_IDRequest: // TODO
                break;

            case VCP_CMD_CapsRequest:
                if (i2c_rxbuff[4] == 0)
                    capsOffset = 0;
                else if (i2c_rxbuff[4] == capsOffset) {} // resend
                else if (i2c_rxbuff[4] == (capsOffset+FRAGLEN)) // next fragment
                    capsOffset += FRAGLEN;
                else if (i2c_rxbuff[4] >= sizeof(capsString)) // next fragment
                    capsOffset = i2c_rxbuff[4];
                else // back to 0
                    capsOffset = 0;
                i2c_txbuff[2+resplen++] = VCP_CMD_CapsReply;
                i2c_txbuff[2+resplen++] = 0;          // offset high byte
                i2c_txbuff[2+resplen++] = capsOffset; // offset low byte
                for (byte i=0; ((capsOffset+i) < sizeof(capsString)) && (i < FRAGLEN); i++)
                    i2c_txbuff[2+resplen++] = capsString[capsOffset+i];
                break;

            case VCP_CMD_VCPRequest:
                i2c_txbuff[2+resplen++] = VCP_CMD_VCPReply;
                if (i2c_rxbuff[3] == VCP_Luminance)
                {
                    i2c_txbuff[2+resplen++] = 0;    // result code 0:1 = NoError:Unsupported vcp code
                    i2c_txbuff[2+resplen++] = i2c_rxbuff[3]; // same opcode
                    i2c_txbuff[2+resplen++] = 0;    // VCP type code 0:1 = set parameter:momentary
                    i2c_txbuff[2+resplen++] = 0;    // max value high byte
                    i2c_txbuff[2+resplen++] = 255;  // max value low byte
                    i2c_txbuff[2+resplen++] = 0;    // current value high byte
                    i2c_txbuff[2+resplen++] = UserConfig.brightness; // current value low byte
                }
                else if (i2c_rxbuff[3] == VCP_StrobeEnable)
                {
                    i2c_txbuff[2+resplen++] = 0;    // result code 0:1 = NoError:Unsupported vcp code
                    i2c_txbuff[2+resplen++] = i2c_rxbuff[3]; // same opcode
                    i2c_txbuff[2+resplen++] = 0;    // VCP type code 0:1 = set parameter:momentary
                    i2c_txbuff[2+resplen++] = 0;    // max value high byte
                    i2c_txbuff[2+resplen++] = 1;    // max value low byte
                    i2c_txbuff[2+resplen++] = 0;    // current value high byte
                    i2c_txbuff[2+resplen++] = UserConfig.blMode; // current value low byte
                }
                else if (i2c_rxbuff[3] == VCP_StrobeLength)
                {
                    i2c_txbuff[2+resplen++] = 0;          // result code 0:1 = NoError:Unsupported vcp code
                    i2c_txbuff[2+resplen++] = i2c_rxbuff[3]; // same opcode
                    i2c_txbuff[2+resplen++] = 0;          // VCP type code 0:1 = set parameter:momentary
                    i2c_txbuff[2+resplen++] = STROBE_LENGTH_MAX>>8;   // max value high byte
                    i2c_txbuff[2+resplen++] = STROBE_LENGTH_MAX&0xFF; // max value low byte
                    i2c_txbuff[2+resplen++] = UserConfig.strobeLength>>8;   // current value high byte
                    i2c_txbuff[2+resplen++] = UserConfig.strobeLength&0xFF; // current value low byte
                }
                else if (i2c_rxbuff[3] == VCP_StrobePhase)
                {
                    i2c_txbuff[2+resplen++] = 0;          // result code 0:1 = NoError:Unsupported vcp code
                    i2c_txbuff[2+resplen++] = i2c_rxbuff[3]; // same opcode
                    i2c_txbuff[2+resplen++] = 0;          // VCP type code 0:1 = set parameter:momentary
                    i2c_txbuff[2+resplen++] = STROBE_OFFSET_MAX>>8;   // max value high byte
                    i2c_txbuff[2+resplen++] = STROBE_OFFSET_MAX&0xFF; // max value low byte
                    i2c_txbuff[2+resplen++] = UserConfig.strobeOffset>>8;   // current value high byte
                    i2c_txbuff[2+resplen++] = UserConfig.strobeOffset&0xFF; // current value low byte
                }
                else if (i2c_rxbuff[3] == VCP_PowerMode)
                {
                    i2c_txbuff[2+resplen++] = 0;    // result code 0:1 = NoError:Unsupported vcp code
                    i2c_txbuff[2+resplen++] = i2c_rxbuff[3]; // same opcode
                    i2c_txbuff[2+resplen++] = 0;    // VCP type code 0:1 = set parameter:momentary
                    i2c_txbuff[2+resplen++] = 0;    // max value high byte
                    i2c_txbuff[2+resplen++] = 4;    // max value low byte
                    i2c_txbuff[2+resplen++] = 0;    // current value high byte
                    i2c_txbuff[2+resplen++] = pwrMode; // current value low byte
                }
                else if (i2c_rxbuff[3] == VCP_VCPVer)
                {
                    i2c_txbuff[2+resplen++] = 0;    // result code 0:1 = NoError:Unsupported vcp code
                    i2c_txbuff[2+resplen++] = i2c_rxbuff[3]; // same opcode
                    i2c_txbuff[2+resplen++] = 0;    // VCP type code 0:1 = set parameter:momentary
                    i2c_txbuff[2+resplen++] = 0xFF; // max value high byte
                    i2c_txbuff[2+resplen++] = 0xFF; // max value low byte
                    i2c_txbuff[2+resplen++] = 2;    // current value high byte
                    i2c_txbuff[2+resplen++] = 1;    // current value low byte
                }
                else if (i2c_rxbuff[3] == VCP_TechnologyType)
                {
                    i2c_txbuff[2+resplen++] = 0;    // result code 0:1 = NoError:Unsupported vcp code
                    i2c_txbuff[2+resplen++] = i2c_rxbuff[3]; // same opcode
                    i2c_txbuff[2+resplen++] = 0;    // VCP type code 0:1 = set parameter:momentary
                    i2c_txbuff[2+resplen++] = 0;    // ignored
                    i2c_txbuff[2+resplen++] = 0;    // ignored
                    i2c_txbuff[2+resplen++] = 2;    // Direct View Flat Panel
                    i2c_txbuff[2+resplen++] = 3;    // LCD (active matrix)
                }
                else // unsupported feature
                {
                    i2c_txbuff[2+resplen++] = 1;    // result code: Unsupported 
                    i2c_txbuff[2+resplen++] = i2c_rxbuff[3]; // same opcode
                    i2c_txbuff[2+resplen++] = 0;    // VCP type code 0:1 = set parameter:momentary
                    i2c_txbuff[2+resplen++] = 0;    // max value high byte
                    i2c_txbuff[2+resplen++] = 0;    // max value low byte
                    i2c_txbuff[2+resplen++] = 0;    // current value high byte
                    i2c_txbuff[2+resplen++] = 0;    // current value low byte
                }
                break;

            case VCP_CMD_VCPSet:
                switch (i2c_rxbuff[3])
                {
                case VCP_Luminance:
                    BL_SetBrightness(i2c_rxbuff[5]);
                    break;
                case VCP_StrobeEnable:
                    BLSetMode(i2c_rxbuff[5]);
                    break;
                case VCP_StrobeLength:
                    UserConfig.strobeLength = (i2c_rxbuff[4]<<8)|(i2c_rxbuff[5]);
                    break;
                case VCP_StrobePhase:
                    UserConfig.strobeOffset = (i2c_rxbuff[4]<<8)|(i2c_rxbuff[5]);
                    break;
                case VCP_PowerMode:
                    SetPowerMode(i2c_rxbuff[5]);
                    break;
                case VCP_FactoryRestore:
                    SetDefaultConfig();
                    BL_SetBrightness(UserConfig.brightness); // Hack-ish
                    EEP_SaveConfig();
                    break;
                default: // unsupported
                    break;
                }
                break;
            case VCP_CMD_VCPReset:
                break;

            /*case VCP_CMD_TimingRequest: // Get timing report // NOT WORKING
                Serial.write("TREQ\n");
                //i2c_txbuff[1] = 0x06; // data stream message length
                //i2c_txbuff[2] = VCP_CMD_TimingReport;
                i2c_txbuff[2+resplen++] = VCP_CMD_TimingReport;
                // Timing status: bit0-v_pol; bit1-h_pol; bit6-unstable count; bit7-out of range
                i2c_txbuff[2+resplen++] = 3; // timing status byte
                // TODO H freq, 10Hz(0.01kHz) increments
                i2c_txbuff[2+resplen++] = 0x34; // high byte
                i2c_txbuff[2+resplen++] = 0x12; // low byte
                // TODO V freq, 0.1Hz increments
                i2c_txbuff[2+resplen++] = 0x17; // high byte
                i2c_txbuff[2+resplen++] = 0x6F; // low byte
                break;*/

            case VCP_CMD_NV3DRequest:
                i2c_txbuff[2+resplen++] = VCP_CMD_NV3DReply;
                i2c_txbuff[2+resplen++] = 1; // 3D is unlocked
                break;

            // Maybe try writing to EDID with these?
            case VCP_CMD_TableRead:
                break;
            case VCP_CMD_TableWrite:
                break;

            case VCP_CMD_EnableAppReport:
                break;
            case VCP_CMD_SaveSettings:
                EEP_SaveConfig();
                break;
            default:
                break;
            }
            if (resplen > 0)
            {
                i2c_txbuff[0] = 0x6E; // source
                if (i2c_txbuff[2] != VCP_CMD_TimingReport)
                    i2c_txbuff[1] = 0x80 | resplen;
                i2c_txbuff[2+resplen] = 0x6F^0x51^i2c_txbuff[1]; // Checksum
                for (byte i=0; i<resplen; i++)
                    i2c_txbuff[2+resplen] ^= i2c_txbuff[2+i];
                i2c_writeamount = resplen+3;
            } // return NULL message otherwise (Done in handler)
        }
        else
        {
            Serial.write("Wrong len");
        }
        i2c_readamount = 0;
    }
}

