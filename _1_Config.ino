
/* Available configurations */
#define MONITOR_VG248 1
#define TV_47LG7000   2

/* Selected configuration */
#define CONFIGURATION MONITOR_VG248

/* Board configuration */
// TODO add proper input voltage monitoring
//const float MCU_VOLTAGE = 3.3;
//const uint16_t ADC_RESOLUTION = 1024;
//const uint16_t PULLUP_RESISTOR = 4700;
//const uint16_t PULLDOWN_RESISTOR = 1000;
const uint16_t VMON_MIN = 220;
const uint16_t VMON_MAX = 700;

/* Defaults */
const uint8_t PWM_DEFAULT = 128;
// TODO: make strobe config percental
const uint16_t STROBE_LENGTH_DEFAULT = 800;
const uint16_t STROBE_OFFSET_DEFAULT = 400;
const uint16_t STROBE_LENGTH_MAX = 1000;
const uint16_t STROBE_OFFSET_MAX = 1500;

/* Configuration parameters */
#if (CONFIGURATION == MONITOR_VG248)
    // M240HW01 V8
    #define PANEL_BOOTTIME     300
    #define PIXEL_ORDER        PIXEL_ORDER_LEFTRIGHT
    #define LVDS_SWING_LEVEL   LVDS_SWING_LEVEL_LOW

    // Divide PWM frequency by 2
    //#define BL_HALFFREQ
    // Sets PWM frequency to (8MHz / 256 / prescaler)
    #define PWM_PRESCALER      PWM_PRESCALER_8

#elif (CONFIGURATION == TV_47LG7000)
    // 120Hz TV with CCFL backlight, LC420WUD-SAA1
    #define PANEL_BOOTTIME     200
    #define PIXEL_ORDER        PIXEL_ORDER_SEQUENTIAL
    #define LVDS_SWING_LEVEL   LVDS_SWING_LEVEL_LOW

    #define CONTROLLED_PSU
    #define PSU_BOOTTIME       200

    #define BL_WARMUP_NEEDED
    #define BL_WARMUP_DURATION 1500

    //#define BL_HALFFREQ
    // 8MHz/256 /128 ~= 244Hz
    #define PWM_PRESCALER      PWM_PRESCALER_128

#else
    #error Unknown panel and backlight configuration!
#endif


// Configuration checks
#ifndef PIXEL_ORDER
    #error PIXEL_ORDER not set in configuration
#endif
#ifndef LVDS_SWING_LEVEL
    #error LVDS_SWING_LEVEL not set in configuration
#endif

#ifndef PWM_PRESCALER
    #error PWM_PRESCALER not set in configuration
#endif

#ifdef CONTROLLED_PSU
    #ifndef PSU_BOOTTIME
        #error PSU_BOOTTIME not set in configuration
    #endif
#endif
#ifdef BL_WARMUP_NEEDED
    #ifndef BL_WARMUP_DURATION
        #error BL_WARMUP_DURATION not set in configuration
    #endif
#endif
#ifndef PANEL_BOOTTIME
    #error PANEL_BOOTTIME not set in configuration
#endif

