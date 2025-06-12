/*
 * File:   main.c
 * Author: ext_mmt
 * 
 * Sends the information exactly how it is written: 2024032600001R24680n-0.000000349+0.000124909+0.996664588
 *
 * Created on 2025/06/11, 14:38
 */

#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#define F_CPU 3333333UL
#define USART0_BAUD_RATE(BAUD_RATE) ((float)(F_CPU * 64 / (16 * (float)BAUD_RATE)) + 0.5)
#define MAX_INPUT_LEN 57
#define CONTINUE_SENDING_FLAG_ADDR 0
#define EEPROM_DATA_START_ADDR 1 

void USART0_init(void);
void USART0_sendChar(char c);
void USART0_sendString(char *str);
char USART0_readCharLow(void);
char USART0_readCharHigh(void);
void initializeEEPROM(void);
void readEEPROMAndSend(void);
void writeToEEPROM(char *input);
void setContinueSendingState(bool state);
bool getContinueSendingState(void);
void sendNumber(uint8_t number);
void logEEPROMOperation(uint8_t address, char value);
void executeCallsign(char *input);
void enterSleepMode(void);

void USART0_init(void)
{
    cli();
    PORTB.OUT |= PIN2_bm;
    PORTB.DIR &= ~PIN3_bm; // RX
    PORTB.DIR |= PIN2_bm;  // TX 
    USART0.BAUD = (uint16_t)USART0_BAUD_RATE(9600);
    USART0.CTRLC |= USART_PMODE_ODD_gc;
    USART0.CTRLB |= USART_TXEN_bm | USART_RXEN_bm; 
    USART0.CTRLB |= USART_SFDEN_bm; 
    USART0.CTRLA |= USART_RXCIE_bm;
    sei();
}

void USART0_sendChar(char c)
{
    while (!(USART0.STATUS & USART_DREIF_bm))
    {
        ;
    }        
    USART0.TXDATAL = c; 
}

void USART0_sendString(char *str)
{
    for(size_t i = 0; i < strlen(str); i++)
    {
        USART0_sendChar(str[i]);
    }
}

char USART0_readCharLow(void)
{
    while (!(USART0.STATUS & USART_RXCIF_bm))
    {
        ;
    }
    return USART0.RXDATAL; 
}

char USART0_readCharHigh(void)
{
    while (!(USART0.STATUS & USART_RXCIF_bm))
    {
        ;
    }
    return USART0.RXDATAH;
}

void readEEPROMAndSend(void)
{
    char eepromData[EEPROM_SIZE];
    eeprom_read_block(eepromData, (const void *)EEPROM_DATA_START_ADDR, EEPROM_SIZE - EEPROM_DATA_START_ADDR);
    for (uint8_t i = 0; i < EEPROM_SIZE - EEPROM_DATA_START_ADDR; i++)
    {
        if (eepromData[i] == '\0') 
        {
            break;
        }
        USART0_sendChar(eepromData[i]);
    }
    USART0_sendChar('\n'); 
}

void readEEPROMAndSendLOG(void)
{
    char eepromData[EEPROM_SIZE];
    eeprom_read_block(eepromData, (const void *)EEPROM_DATA_START_ADDR, EEPROM_SIZE - EEPROM_DATA_START_ADDR);
    for (uint8_t i = 0; i < EEPROM_SIZE - EEPROM_DATA_START_ADDR; i++)
    {
        if (eepromData[i] == '\0') 
        {
            break;
        }
        logEEPROMOperation(EEPROM_DATA_START_ADDR + i, eepromData[i]);
    }
    USART0_sendChar('\n'); 
}

void initializeEEPROM(void)
{
    char emptyData[EEPROM_SIZE] = {0}; 
    eeprom_write_block(emptyData, (void *)0, EEPROM_SIZE); 
    setContinueSendingState(false); 
}

void writeToEEPROM(char *input)
{
    eeprom_write_block(input, (void *)EEPROM_DATA_START_ADDR, strlen(input) + 1); 
}

void setContinueSendingState(bool state)
{
    eeprom_write_byte((uint8_t *)CONTINUE_SENDING_FLAG_ADDR, state ? 1 : 0); 
}

bool getContinueSendingState(void)
{
    return eeprom_read_byte((const uint8_t *)CONTINUE_SENDING_FLAG_ADDR) == 1; 
}

void sendNumber(uint8_t number)
{
    if (number >= 10)
    {
        sendNumber(number / 10);
    }
    USART0_sendChar((number % 10) + '0');
}

void logEEPROMOperation(uint8_t address, char value)
{
    USART0_sendString("EEPROM[");
    sendNumber(address);
    USART0_sendString("]; ");
    USART0_sendChar(value);
    USART0_sendChar('\n');
}

void sendChar(char *input)
{
    if (strcmp(input, "info") != 0 && strcmp(input, "RESET") != 0 && strcmp(input, "LOG") != 0)
    {
        USART0_sendString("Error\n");
        _delay_ms(500);
    }
    if (strcmp(input, "info") == 0)
    {
        readEEPROMAndSend();
        _delay_ms(500);
    } 
    else if (strcmp(input, "RESET") == 0)
    {
        setContinueSendingState(false); 
        USART0_sendString("Reset to factory mode. Disconnect and reconnect, then refer to documentation for instructions. \n");
        _delay_ms(500);
    }
    else if (strcmp(input, "LOG") == 0)
    {
        USART0_sendString("Current bytes & respective positions: \n");
        readEEPROMAndSendLOG();
        _delay_ms(500);
    }
}

void executeCallsign(char *input)
{
    if (strlen(input) != 56 && strcmp(input, "INIT") != 0 && strcmp(input, "FINAL") != 0 && strcmp(input, "LOG") != 0)
    {
        USART0_sendString("Error\n");
        _delay_ms(500);
        return;
    }
    if(strcmp(input, "INIT") == 0)
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            readEEPROMAndSend();
            _delay_ms(500);
        }
    }
    else if (strcmp(input, "FINAL") == 0)
    {
        setContinueSendingState(true);
        USART0_sendString("Entered customer mode. Disconnect and reconnect, then input 'info' for shunt characteristics. \n");
        _delay_ms(500);
    }
    else if (strcmp(input, "LOG") == 0)
    {
        USART0_sendString("Current bytes & respective positions: \n");
        readEEPROMAndSendLOG();
        _delay_ms(500);
    }
    else 
    {
        USART0_sendString(input);
        USART0_sendChar('\n');
        writeToEEPROM(input);
        _delay_ms(500);
    }
}

ISR(USART0_RXC_vect)
{ 
    ;
}

void enterSleepMode(void)
{
    set_sleep_mode(SLEEP_MODE_STANDBY);
    sleep_enable();
    sleep_cpu();
    sleep_disable();
}

int main(void)
{
    char input[MAX_INPUT_LEN];
    uint8_t index = 0;
    char c;
    USART0_init();
    
    if (eeprom_read_byte((const uint8_t *)CONTINUE_SENDING_FLAG_ADDR) == 0xFF) 
    {
        initializeEEPROM(); 
    }
    
    if (getContinueSendingState())
    {
        while (1)
        {
            enterSleepMode();
            
            uint8_t meta = USART0_readCharHigh();
            c = USART0_readCharLow();
            
            
            if (meta & USART_PERR_bm)
            {
                USART0_sendString("USART Error: Parity\n");
                break;
            }
            
            if (meta & USART_FERR_bm)
            {
                USART0_sendString("USART Error: Frame\n");
                break;
            }
            
            
            if (c != '\n' && c != '\r')
            {
                if (index < MAX_INPUT_LEN - 1)
                {
                    input[index++] = c;
                }
                else 
                {
                    index = 0; 
                }
            }
            if (c == '\n')
            {
                input[index] = '\0'; 
                index = 0; 
                sendChar(input);
            }
        }
    }
    
    while (1) 
    {
        enterSleepMode();
        
        uint8_t meta = USART0_readCharHigh();
        c = USART0_readCharLow();
        
        if (meta & USART_PERR_bm)
        {
            USART0_sendString("USART Error: Parity\n");
            break;
        }
         
        if (meta & USART_FERR_bm)
        {
            USART0_sendString("USART Error: Frame\n");
            break;
        }
        
        if (c != '\n' && c != '\r')
        {
            if (index < MAX_INPUT_LEN - 1)
            {
                input[index++] = c;
            }
            else 
            {
                index = 0; 
            }
        }
        if (c == '\n')
        {
            input[index] = '\0'; 
            index = 0; 
            executeCallsign(input);
        }
    }
    
    return 0;
}
