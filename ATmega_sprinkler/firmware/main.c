/*
 * refactored on Apr 28, [LY]
 * Main Source Code for ATMega
 */

#include <avr/io.h>
#include <util/delay.h>
#include <math.h>
#include <stdio.h>

#define CtoF(x)  ( lround(1.8 * x) + 32 )

/* ******************
 * TEMPERATURE SENSOR
 * ******************/
//Function: Takes in a 40 bit value from microcontroller (16 bits are tmemp)
//Converts bit value into decimal, returns a double
int 
wait_for_high_temp()
{
        int time = 0;
        while ((PIND & (1 << PD7)) == 0) {
                _delay_us(1);
                time++;
        }
        return time;
}

int 
wait_for_low_temp()
{
        int time = 0;
        while ((PIND & (1 << PD7)) == 0x80) {
                _delay_us(1);
                time++;
        }
        return time;
}

//Temp sensor is connected to PD7
double 
get_temperature()
{
        // 0-15 are RH data, 16-31 are temp, 32-39 are checksum
        int values[40];
        int counter = 0;

        // send start signal
        DDRD |= (1 << PD7); // makes line low
        _delay_ms(1);       // wait 1 ms

        DDRD &= ~(1 << PD7); // makes line high
        _delay_us(1);        // TODO is this correct? different units

        // wait for sensor to respond
        wait_for_low_temp();

        // Sensor will respond with low for 80us then high 80 us
        wait_for_high_temp();
        wait_for_low_temp();

        // receive data
        while (counter < 40) {
                wait_for_high_temp();

                // count how long before signal goes low
                int time = wait_for_low_temp();

                // it's a zero
                if (time < 30) {
                        values[counter] = 0; 
                } else { // it's a one
                        values[counter] = 1;
                }
                counter++;
        }

        double temperature = 0;
        counter = 31;
        int multiplier = 0;
        while (counter > 16) {
                temperature += values[counter] * (1 << multiplier);
                counter--;
                multiplier++;
        }

        // temperature is below 0 degrees celsius
        if (values[16] == 1) {
                temperature *= -1;
        }

        return (temperature / 10);
}

/* ***************
 * MOISTURE SENSOR
 * ***************/
//Function: Gets moisture value from moisture sensor
//No Input, but returns a double
double 
get_moisture()
{
        double moisture;

        ADMUX  |=  (1 << REFS0); // Set reference to AVCC
        ADMUX  &= ~(1 << REFS1);
        ADMUX  |=  (1 << ADLAR); // Left adjust the output
        ADMUX  |=  (1 << MUX0); // Select the channel
        ADMUX  &=  (0xf0 | (1 << MUX0));
        ADCSRA |=  (7 << ADPS0); // Set the prescalar to 128
        ADCSRA |=  (1 << ADEN); // Enable the ADC

        ADCSRA |=  (1 << ADSC); // Start a conversion

        while (ADCSRA & (1 << ADSC)) {
        } // wait for conversion complete
        moisture = ADCH; // Get converted value

        return moisture;
}

/* **************
 * shift register
 * **************/
void 
shift_init()
{
        DDRB |= ((1 << PB0) | (1 << PB1) | (1 << PB2));
}

void 
shift_pulse()
{
        PORTB |=   (1 << PB2);
        PORTB &= (~(1 << PB2));
}

void 
shift_latch()
{
        PORTB |=   (1 << PB1);
        _delay_loop_1(1);
        PORTB &= (~(1 << PB1));
        _delay_loop_1(1);
}

void 
shift_write(uint8_t data)
{
        uint8_t i;
        for (i = 0; i < 8; i++) {
                if (data & 0b10000000) {
                        PORTB |=   (1 << PB0);
                } else {
                        PORTB &= (~(1 << PB0));
                }
                shift_pulse();
                data = data << 1;
        }
        shift_latch();
}

/* ******************
 * usart port methods
 * ******************/
/* usart_init - initializes the USART port */
void 
usart_init(unsigned short ubrr) 
{
        UBRR0   = ubrr;           // Set baud rate
        UCSR0B |= (1 << TXEN0);   // Turn on transmitter
        UCSR0B |= (1 << RXEN0);   // Turn on receiver
        UCSR0C  = (3 << UCSZ00);  // Set for async . operation , no parity ,
        // one stop bit, 8 data bits
}

/* usart_putchar - prints a byte */
void 
usart_putchar(char ch)
{
        while ((UCSR0A & (1 << UDRE0)) == 0) {
        }
        UDR0 = ch;
}

/* usart_putint - prints a one-byte integer */
void 
usart_putint(uint8_t data)
{
        while ((UCSR0A & (1 << UDRE0)) == 0) {
        }
        UDR0 = data;
}


/* usart_in  - reads a byte from the USART0 and return it */
char 
usart_in()
{
        while (!( UCSR0A & (1 << RXC0))) {
        }
        return UDR0;
}

/* usart_out - prints out a C-string until it sees a null byte */
void 
usart_out(char* str)
{
        while (*str) {
                usart_putchar(*str);
                str++;
        }
}

int 
main()
{
        /*
         * used by rotary encoder
        int rot_a = 0;
        int rot_b = 0;
        int prev_val = 0;
        */

        /* Initialize Clock Rate */
        usart_init(47);
        shift_init();

        DDRD  &= (1 << PD5);   // Set PORTD bit 5 for input, take in rotary bit A
        DDRD  &= (1 << PD6);   // Set PORTD bit 6 for input, take in rotary bit B
        PORTD |= (1 << PD5);   // Activate pull-ups in PORTD pi
        PORTD |= (1 << PD6);   // Activate pull-ups in PORTD pi

        /* main loop
         * every cycle, read at the beginning, write at the end
         * */
        uint8_t zone_reg = 0;
        while (1) {
                int temperature = get_temperature();
                int moisture = get_moisture();
                char zone_char = usart_in();
                usart_putchar('\n');

                char temp_write_buf[36];
                snprintf(temp_write_buf, 
                                36, 
                                "Temp: %ld F, Moisture: %d, ",
                                CtoF(temperature),
                                moisture);
                usart_out(temp_write_buf);

                _delay_ms(200);

                /**
                 * Rotary Encoder - Deprecated
                 * Read inputs
                rot_a = (PIND & (1 << PD5));  // Read value on PD5
                rot_b = (PIND & (1 << PD6));  // Read value on PD6

                if ((prev_val == 0) && (rot_a == 32)) {
                        if (rot_b == 64) {
                                zone_char--; 
                        } else {
                                zone_char++; 
                        }
                }

                prev_val = rot_a;
                */

                /* we only accept zones 1 through 8 */
                char zone_write_buf[17];
                if (zone_char >= '1' && zone_char <= '8') {
                        snprintf(zone_write_buf, 
                                        17, 
                                        "Current Zone: %c\r\n", 
                                        zone_char);
                        usart_out(zone_write_buf);
                        _delay_ms(200);
                        uint8_t zone_onehot 
                                = (uint8_t) (1 << (zone_char - '1'));
                        shift_write(zone_onehot);
                        zone_reg = (uint8_t) (zone_char - '0');
                } else {
                        snprintf(zone_write_buf,
                                        17,
                                        "Current Zone: %c\r\n",
                                        (char) (zone_reg + '0'));
                        usart_out(zone_write_buf);
                        _delay_ms(200);
                }
        }

        return 0;
}
