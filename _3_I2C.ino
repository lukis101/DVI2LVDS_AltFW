
#define SCL_PORT PORTB
#define SCL_PIN  6
#define SDA_PORT PORTB
#define SDA_PIN  7

#define I2C_FASTMODE 1
//#define I2C_TIMEOUT 10 // timeout after 10 msec
#define I1C_NOINTERRUPT 0 // don't stop interrupts
//#define I2C_CPUFREQ (F_CPU/8) // slow down CPU frequency
#include <SoftWire.h>
SoftWire Wire2 = SoftWire();

#include <Wire.h>

bool i2c_eep2 = false;

volatile uint16_t i2c_writeamount = 0;
volatile uint16_t i2c_readamount = 0;
volatile uint8_t i2c_rxbuff[32];
uint8_t i2c_txbuff[32];
uint8_t i2c_params[4];

void I2C_Init(void)
{
    I2C_SetSlave(); // join i2c bus
    Wire.setClock(50000); // standard 100kHz
}

void I2C_SetSlave(void)
{
    Wire.begin(0x37); // DDC/CI (w:6E,r:6F)
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);
}
void I2C_SetMaster(void)
{
    Wire.begin();
}
void I2C_Disable(void)
{
}
void I2C_TransferBuff(byte amount)
{
    if (i2c_eep2) // epmi config eeprom
    {
        Wire2.beginTransmission(i2c_txbuff[0]>>1);
        for (byte i=1; i<amount; i++)
            Wire2.write(i2c_txbuff[i]);
        Wire2.endTransmission();
    }
    else // edid, ddc/ci
    {
        Wire.beginTransmission(i2c_txbuff[0]>>1);
        for (byte i=1; i<amount; i++)
            Wire.write(i2c_txbuff[i]);
        Wire.endTransmission();
    }
}
void I2C_Read(uint16_t amount)
{
    if (i2c_eep2) // epmi internal config eeprom
    {
        while(amount > 0)
        {
            uint8_t reqsize = (amount > BUFFER_LENGTH) ? BUFFER_LENGTH : amount;
            amount -= reqsize;
            Wire2.requestFrom(i2c_txbuff[0]>>1, reqsize);
            for (uint8_t i=0; i<reqsize; i++)
                Serial.write(Wire2.read());
        }
    }
    else // edid, ddc/ci
    {
        while(amount > 0)
        {
            uint8_t reqsize = (amount > BUFFER_LENGTH) ? BUFFER_LENGTH : amount;
            amount -= reqsize;
            Wire.requestFrom(i2c_txbuff[0]>>1, reqsize);
            for (uint8_t i=0; i<reqsize; i++)
                Serial.write(Wire.read());
        }
    }
}

void receiveEvent(int amount)
{
    i2c_readamount = amount;
    for(int i=0; i<amount; i++)
        i2c_rxbuff[i] = Wire.read();
    if(serMode == NORMAL_I2C) // Echo to uart
    {
        Serial.write('\n');
        for(int i=0; i<amount; i++)
        {
            Serial.print(i2c_rxbuff[i], HEX);
            Serial.write(' ');
        }
    }
}
void requestEvent(void)
{
    if (i2c_writeamount > 0)
    {
        for (byte i=0; i<i2c_writeamount; i++)
            Wire.write(i2c_txbuff[i]);
        if(serMode == NORMAL_I2C) // Echo to uart
        {
            Serial.write(" -> ");
            Serial.print(i2c_writeamount);
        }
        i2c_writeamount = 0;
    }
    else // Send "null message"
    {
        Wire.write(0x6E); // source addr (read)
        Wire.write(0x80); // 0 length
        Wire.write(0xBE); // chksum: 6F^51^80
        if(serMode == NORMAL_I2C) // Echo to uart
            Serial.write(" -> NULL");
    }
}

