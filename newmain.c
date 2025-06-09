/*
* File:   newmain.c
* Author: ext_mmt
*
* Created on 2025/06/09, 8:58
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

// Function prototypes
void USART0_init(void);
void USART0_sendChar(char c);
void USART0_sendString(char *str);
char USART0_readChar(void);
void initializeEEPROM(void);
void readEEPROMAndSend(void);
void writeToEEPROM(char *input);
void setContinueSendingState(bool state);
bool getContinueSendingState(void);
void sendNumber(uint8_t number);
void logEEPROMOperation(uint8_t address, char value);
void executeCallsign(char *input);
void SleepMode(void);

// Function to initialize USART0
void USART0_init(void)
{
    PORTB.DIR &= ~PIN3_bm;  // Set RXD pin (PIN3) as input
    PORTB.DIR |= PIN2_bm;   // Set TXD pin (PIN2) as output
    USART0.BAUD = (uint16_t)USART0_BAUD_RATE(9600); // Set baud rate to 9600
    USART0.CTRLB |= USART_TXEN_bm | USART_RXEN_bm; // Enable transmitter and receiver
    USART0.CTRLA |= USART_RXSIF_bm; // Enable RX start of frame interrupt
    USART0.CTRLB |= USART_SFDEN_bm;
}

// Function to send a single character via USART
void USART0_sendChar(char c)
{
    while (!(USART0.STATUS & USART_DREIF_bm)) // Wait until data register is empty
    {
        ; 
    }        
    USART0.TXDATAL = c; 
}

// Function to send a string via USART
void USART0_sendString(char *str)
{
    for(size_t i = 0; i < strlen(str); i++)
    {
        USART0_sendChar(str[i]);
    }
}

// Function to read a single character from USART
char USART0_readChar(void)
{
    while (!(USART0.STATUS & USART_RXCIF_bm)) // Wait until a character is received
    {
        ;
    }
    return USART0.RXDATAL;
}

// Function to read data from EEPROM and send it via USART
void readEEPROMAndSend(void)
{
    char eepromData[EEPROM_SIZE]; // Buffer to hold EEPROM data
    eeprom_read_block(eepromData, (const void *)EEPROM_DATA_START_ADDR, EEPROM_SIZE - EEPROM_DATA_START_ADDR); // Read from EEPROM
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

// Function to read data from EEPROM and log it
void readEEPROMAndSendLOG(void)
{
    char eepromData[EEPROM_SIZE]; // Buffer to hold EEPROM data
    eeprom_read_block(eepromData, (const void *)EEPROM_DATA_START_ADDR, EEPROM_SIZE - EEPROM_DATA_START_ADDR); // Read from EEPROM
    for (uint8_t i = 0; i < EEPROM_SIZE - EEPROM_DATA_START_ADDR; i++) 
    {
        if (eepromData[i] == '\0') 
        {
            break; 
        }
        logEEPROMOperation(EEPROM_DATA_START_ADDR + i, eepromData[i]); // Log each character
    }
    USART0_sendChar('\n'); 
}

// Function to initialize EEPROM with empty data
void initializeEEPROM(void)
{
    char emptyData[EEPROM_SIZE] = {0}; // Initialize with null characters
    eeprom_write_block(emptyData, (void *)0, EEPROM_SIZE); // Clear EEPROM
    setContinueSendingState(false); // Reset sending state
}

// Function to write input data to EEPROM
void writeToEEPROM(char *input)
{
    eeprom_write_block(input, (void *)EEPROM_DATA_START_ADDR, strlen(input) + 1); // Write input to EEPROM
}

// Function to set the continue sending state in EEPROM
void setContinueSendingState(bool state)
{
    eeprom_write_byte((uint8_t *)CONTINUE_SENDING_FLAG_ADDR, state ? 1 : 0); // Write state to EEPROM
}

// Function to get the continue sending state from EEPROM
bool getContinueSendingState(void)
{
    return eeprom_read_byte((const uint8_t *)CONTINUE_SENDING_FLAG_ADDR) == 1; // Read state from EEPROM
}

// Function to send a number as a character via USART
void sendNumber(uint8_t number)
{
    if (number >= 10) // If number is 10 or greater
    {
        sendNumber(number / 10); // Recursively send the higher digit
    }
    USART0_sendChar((number % 10) + '0'); // Send the last digit
}

// Function to log EEPROM operations
void logEEPROMOperation(uint8_t address, char value)
{
    USART0_sendString("EEPROM["); // Start logging
    sendNumber(address); // Log the address
    USART0_sendString("]; "); // Log the separator
    USART0_sendChar(value); // Log the value
    USART0_sendChar('\n'); // Newline after logging
}

// Function to handle received commands
void sendChar(char *input)
{
    if (strcmp(input, "info") != 0 && strcmp(input, "RESET") != 0 && strcmp(input, "LOG") != 0 && strcmp(input, "MODE") != 0) // Check for valid commands
    {
        USART0_sendString("Error\n"); // Send error message for invalid commands
        return;
    }
    if (strcmp(input, "info") == 0) // If command is "info"
    {
        readEEPROMAndSend(); // Read and send EEPROM data
    } 
    else if (strcmp(input, "RESET") == 0) // If command is "RESET"
    {
        setContinueSendingState(false); // Reset sending state
        USART0_sendString("Reset to factory mode. Disconnect and reconnect, then refer to documentation for instructions. \n"); // Send reset message
    }
    else if (strcmp(input, "LOG") == 0) // If command is "LOG"
    {
        USART0_sendString("Current bytes & respective positions: \n"); // Log message
        readEEPROMAndSendLOG(); // Read and log EEPROM data
    }
    else if (strcmp(input, "MODE") == 0)
    {
        USART0_sendString("Customer Mode\n");
    }
    
    SleepMode();
}

// Function to execute commands based on input
void executeCallsign(char *input)
{
    if (strlen(input) != 56 && strcmp(input, "INIT") != 0 && strcmp(input, "FINAL") != 0 && strcmp(input, "LOG") != 0 && strcmp(input, "MODE") != 0) // Check for valid input length and commands
    {
        USART0_sendString("Error\n"); // Send error message for invalid input
        return; // Exit function
    }
    if(strcmp(input, "INIT") == 0) // If command is "INIT"
    {
        for (uint8_t i = 0; i < 4; i++) // Loop to send EEPROM data multiple times
        {
            readEEPROMAndSend(); // Read and send EEPROM data
            _delay_ms(500); // Delay between sends
        }
    }
    else if (strcmp(input, "FINAL") == 0) // If command is "FINAL"
    {
        setContinueSendingState(true); // Set continue sending state
        USART0_sendString("Entered customer mode. Disconnect and reconnect, then input 'info' for shunt characteristics. \n"); // Send confirmation message
    }
    else if (strcmp(input, "LOG") == 0) // If command is "LOG"
    {
        USART0_sendString("Current bytes & respective positions: \n"); // Log message
        readEEPROMAndSendLOG(); // Read and log EEPROM data
    }
    else if (strcmp(input, "MODE") == 0)
    {
        USART0_sendString("Factory Mode\n");
    }
    else // If input is a custom command
    {
        USART0_sendString(input); // Echo the input
        USART0_sendChar('\n'); // Newline after echo
        writeToEEPROM(input); // Write input to EEPROM
    }
    
    SleepMode();
}

// Function to enter sleep mode
void SleepMode(void)
{
    set_sleep_mode(SLEEP_MODE_STANDBY); 
    sleep_enable(); 
    sleep_cpu();
    PORTB.DIR &= ~PIN3_bm;  // Set RXD pin (PIN3) as input
    PORTB.DIR |= PIN2_bm;   // Set TXD pin (PIN2) as output
    sleep_disable(); 
}

// USART RX interrupt service routine
ISR(USART0_RXC_vect) 
{
    char c; // Character variable
    char input[MAX_INPUT_LEN]; // Buffer to hold received input
    uint8_t index = 0; // Index for input buffer

    if (eeprom_read_byte((const uint8_t *)CONTINUE_SENDING_FLAG_ADDR) == 0xFF) 
    {
        initializeEEPROM(); 
    }

    if (getContinueSendingState())
    {
        while (1)
        {
            c = USART0_readChar();
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
            if(c == '\n')
            {
                input[index] = '\0'; 
                index = 0; 
                sendChar(input);
            }
        }
    }
    
    while (1) 
    {
        c = USART0_readChar();
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
        if(c == '\n')
        {
            input[index] = '\0'; 
            index = 0; 
            executeCallsign(input);
        }
    }
}

// Main function
int main(void)
{
    USART0_init(); // Initialize USART
    sei(); // Enable global interrupts

    while (1) // Main loop
    {
        SleepMode(); // Enter sleep mode when not processing commands
    }
    return 0; // Return from main (not reached)
}