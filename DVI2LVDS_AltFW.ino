/*
 *  Alternaltive firmware for ZisWorks DVI2LVDS boards
 *  
 *  !!! USE AT YOUR OWN RISK !!!
 *  Only tested on boards without panel DC/DC converters!
 *  
 *  HARDWARE MODIFICATIONS NEEDED FOR BACKLIGHT STROBING:
 *  Pin PD2 needs to be rewired to VSYNC Instead of DE
 *  
 *  Make modifications in "Config" tab for your setup
 *  Boards run on 8MHz internal oscillator with modified Optiboot
 *  To compile and upload, use config in provided boards.txt
 *  
 *  Uses SoftI2CMaster library available at https://github.com/felias-fogg/SoftI2CMaster
*/

/// --- PINS --- ///
#define PIN_PWR_1V8         7
#define PIN_PWR_3V3        10
#define PIN_PWRGOOD_3V3     8
#define PIN_PWR_PANEL       4 // CONTROL_VREG_VPANEL
#define PIN_VSYNC           2 // MODIFIED: normally DE
#define PIN_BL_ON           6
#define PIN_BL_PWM          3 // PWM_D on boards, hardcoded
#define PIN_PWR_PSU        11 // PWM_A on boards
#define PIN_PANEL_GPIO0    12 // outer lvds header
#define PIN_PANEL_GPIO1    13 // inner lvds header
#define PIN_EPMI_SWING      5
#define PIN_EPMI_ORDER      9
#define PIN_EPMI_SCL       20 // B6 } Not defined by arduino
#define PIN_EPMI_SDA       21 // B7 }
#define PIN_DDC_SDA        A4
#define PIN_DDC_SCL        A5
#define PIN_BTN_PWR        A0
#define PIN_BTN_A          A1
#define PIN_BTN_B          A2
// This is a dual-purpose pin. If using the panel DC/DC converter, this pin sets ouptut voltage, high=12v, low=5v.
// If not using the panel DC/DC converter, this is an analog input. Multiply by the scaling value to get VIN.
#define PIN_VMON_PANELREG  A3
// A4 - DDC SDA
// A5 - DDC SCL

#include <avr/eeprom.h>
#include <avr/wdt.h>

/// --- BUTTON DEBOUNCE --- ///
const uint16_t SHORTPRESS  = 100;
const uint16_t MEDIUMPRESS = 200;
const uint16_t LONGPRESS   = 1000;

const uint8_t BTN_BRI_STEP = 5;
const uint8_t BTN_STROBELEN_STEP = 12;

/// --- GLOBALS --- ///
bool panel_powered = false;
bool bridge_powered = false;
bool psu_enabled = false;
bool eepromSelect = false; // 0 = ddc, 1 = epmi
uint8_t panelgpio = 0;
// for syncing with shutter glasses
// TODO set via VCP or button
bool sync3D = false;

typedef struct {
    uint8_t brightness;
    uint8_t blMode;
    uint16_t strobeLength;
    uint16_t strobeOffset;
} UserConfig_t;
UserConfig_t UserConfig;

// Values are based on VCP feature 0xD6
typedef enum {
    PWRMODE_ON = 1,
    PWRMODE_STANDBY = 2, // waiting for signal
    PWRMODE_OFF = 4, // fully off by user
} PowerMode_t;
PowerMode_t pwrMode = PWRMODE_OFF;

typedef enum {
    NORMAL_HIZ = 0,
    NORMAL_I2C,
    BIN_HIZ,
    BIN_I2C
} serMode_t;
serMode_t serMode = NORMAL_HIZ;

#define PIXEL_ORDER_SEQUENTIAL 0
#define PIXEL_ORDER_LEFTRIGHT  1

#define LVDS_SWING_LEVEL_LOW   0
#define LVDS_SWING_LEVEL_HIGH  1

/// --- UTILS --- ///
bool checkPower(void);
void SetDefaultConfig(void);

void EEP_LoadConfig(void)
{
    // Magic byte to check if EEPROM was erased
    if (eeprom_read_byte(0) != 0x5A)
    {
        SetDefaultConfig();
        eeprom_write_byte(0, 0x5A);
        EEP_SaveConfig();
    }
    else
    {
        eeprom_read_block((void*)&UserConfig, 1, sizeof(UserConfig_t));
    }
}
void EEP_SaveConfig(void)
{
    eeprom_update_block((void*)&UserConfig, 1, sizeof(UserConfig_t));
}

void reset()
{
    wdt_enable( WDTO_15MS );
    while (1); // Wait for watchdog to reset
}

