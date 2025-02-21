/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

/*
 * GccApplication1.c
 *
 * Created: 05.08.2014 13:26:30
 *  Author: hpcwoess
 */

#define F_CPU 12000000L
/*! \brief Pin number of IRQ contact on RFM73 module.*/
#define RFM73_IRQ_PIN DDD3
/*! \brief PORT register to IRQ contact on RFM73 module.*/
#define RFM73_IRQ_PORT PORTD
/*! \brief PIN register of IRQ contact on RFM73 module.*/
#define RFM73_IRQ_IN PIND
/*! \brief DDR register of IRQ contact on RFM73 module.*/
#define RFM73_IRQ_DIR DDRD
/*! \brief Pin number of CE contact on RFM73 module.*/
#define RFM73_CE_PIN DDB2
/*! \brief PORT register to CE contact on RFM73 module.*/
#define RFM73_CE_PORT PORTB
/*! \brief PIN register of CE contact on RFM73 module.*/
#define RFM73_CE_IN PINB
/*! \brief DDR register of CE contact on RFM73 module.*/
#define RFM73_CE_DIR DDRB
/*! \brief Pin number of CSN contact on RFM73 module.*/
#define RFM73_CSN_PIN DDB0
/*! \brief PORT register to CSN contact on RFM73 module.*/
#define RFM73_CSN_PORT PORTB
/*! \brief PIN register of CSN contact on RFM73 module.*/
#define RFM73_CSN_IN PINB
/*! \brief DDR register of CSN contact on RFM73 module.*/
#define RFM73_CSN_DIR DDRB

/*! \brief Setting high level on CE line.*/
#define RFM73_CE_HIGH RFM73_CE_PORT |= (1 << RFM73_CE_PIN)
/*! \brief Setting low level on CE line.*/
#define RFM73_CE_LOW RFM73_CE_PORT &= ~(1 << RFM73_CE_PIN)

#define sbi(port, bit) (port) |= (1 << (bit)) // set bit
#define cbi(port, bit) (port) &= ~(1 << (bit)) // clear bit

#define LED_RED 1 // red led is connected to pin 2 port d receiver only

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h>

#include "usbdrv.h"

#include "spi_init.h"
#include "rfm73.h"

#define USB_LED_OFF 0
#define USB_LED_ON 1

const PROGMEM char usbHidReportDescriptor[52] = { /* USB report descriptor, size must match usbconfig.h */
                                                  0x05, 0x01, // USAGE_PAGE (Generic Desktop)
                                                  0x09, 0x02, // USAGE (Mouse)
                                                  0xa1, 0x01, // COLLECTION (Application)
                                                  0x09, 0x01, //   USAGE (Pointer)
                                                  0xA1, 0x00, //   COLLECTION (Physical)
                                                  0x05, 0x09, //     USAGE_PAGE (Button)
                                                  0x19, 0x01, //     USAGE_MINIMUM
                                                  0x29, 0x03, //     USAGE_MAXIMUM
                                                  0x15, 0x00, //     LOGICAL_MINIMUM (0)
                                                  0x25, 0x01, //     LOGICAL_MAXIMUM (1)
                                                  0x95, 0x05, //     REPORT_COUNT (5)
                                                  0x75, 0x01, //     REPORT_SIZE (1)
                                                  0x81, 0x02, //     INPUT (Data,Var,Abs)
                                                  0x95, 0x01, //     REPORT_COUNT (1)
                                                  0x75, 0x05, //     REPORT_SIZE (5)
                                                  0x81, 0x03, //     INPUT (Const,Var,Abs)
                                                  0x05, 0x01, //     USAGE_PAGE (Generic Desktop)
                                                  0x09, 0x30, //     USAGE (X)
                                                  0x09, 0x31, //     USAGE (Y)
                                                  0x09, 0x38, //     USAGE (Wheel)
                                                  0x15, 0x81, //     LOGICAL_MINIMUM (-127)
                                                  0x25, 0x7F, //     LOGICAL_MAXIMUM (127)
                                                  0x75, 0x08, //     REPORT_SIZE (8)
                                                  0x95, 0x03, //     REPORT_COUNT (3)
                                                  0x81, 0x06, //     INPUT (Data,Var,Rel)
                                                  0xC0, //   END_COLLECTION
                                                  0xC0, // END COLLECTION
};

typedef struct
{
    uchar buttonMask;
    char dx;
    char dy;
    char dWheel;
} report_t;

static report_t reportBuffer;
static uchar idleRate; /* repeat rate for keyboards, never used for mice */

uint8_t oldDX = 0;
uint8_t oldDY = 0;

//----------------------------------------------------------------------------------
// Receive Payload
//----------------------------------------------------------------------------------

uint8_t rfm70ReceivePayload()
{
    uint8_t len;
    uint8_t status;
    //uint8_t detect;
    uint8_t fifo_status;
    uint8_t rx_buf[32];

    status = rfm70ReadRegValue(RFM70_REG_STATUS);

    // check if receive data ready (RX_DR) interrupt
    if (status & RFM70_IRQ_STATUS_RX_DR)
    {

        do
        {
            // read length of playload packet
            len = rfm70ReadRegValue(RFM70_CMD_RX_PL_WID);

            if (len >= 5 && len <= 32) // 32 = max packet length
            {
                // read data from FIFO Buffer
                rfm70ReadRegPgmBuf(RFM70_CMD_RD_RX_PLOAD, rx_buf, len);

                reportBuffer.buttonMask = rx_buf[0];
                reportBuffer.dx = rx_buf[1] - oldDX;
                reportBuffer.dy = rx_buf[2] - oldDY;
                oldDX = rx_buf[1];
                oldDY = rx_buf[2];
                sbi(PORTD, LED_RED);

                _delay_ms(10);
                cbi(PORTD, LED_RED);
            }
            else
            {
                // flush RX FIFO
                rfm70WriteRegPgmBuf((uint8_t *)RFM70_CMD_FLUSH_RX, sizeof(RFM70_CMD_FLUSH_RX));
            }

            fifo_status = rfm70ReadRegValue(RFM70_REG_FIFO_STATUS);
        } while ((fifo_status & RFM70_FIFO_STATUS_RX_EMPTY) == 0);

        if ((rx_buf[0] == 0xAA) && (rx_buf[1] == 0x80))
        {
            sbi(PORTD, LED_RED);
            _delay_ms(10);
            cbi(PORTD, LED_RED);

            rfm70SetModeRX();
        }
    }
    rfm70WriteRegValue(RFM70_CMD_WRITE_REG | RFM70_REG_STATUS, status);

    return 0;
}

int main()
{
    uchar i;

    wdt_enable(WDTO_1S); // enable 1s watchdog timer

    //all unused ports as input with pullups

    DDRC &= ~((1 << DDC0) | (1 << DDC1) | (1 << DDC2) | (1 << DDC3) | (1 << DDC4) | (1 << DDC5));
    PORTC |= ((1 << DDC0) | (1 << DDC1) | (1 << DDC2) | (1 << DDC3) | (1 << DDC4) | (1 << DDC5));
    DDRB &= ~((1 << DDB1));
    PORTB |= ((1 << DDB1));
    DDRD &= ~((1 << DDD4) | (1 << DDD5) | (1 << DDD6) | (1 << DDD7));
    PORTD |= ((1 << DDD4) | (1 << DDD5) | (1 << DDD6) | (1 << DDD7));

    // leds
    // DDD2 - red led
    DDRD |= (1 << DDD1);
    PORTD &= ~((1 << DDD1));

    sbi(PORTD, LED_RED);

    usbInit();
    cli();
    usbDeviceDisconnect(); // enforce re-enumeration
    for (i = 0; i < 250; i++)
    { // wait 500 ms
        wdt_reset(); // keep the watchdog happy
        _delay_ms(2);
    }
    usbDeviceConnect();
    sei(); // Enable interrupts after re-enumeration
    // do SPI init after seting CE to LOW, this is important, otherwise the RFM73 module does not always answer to SPI requests.

    //out
    RFM73_CSN_DIR |= (1 << RFM73_CSN_PIN);
    RFM73_CE_DIR |= (1 << RFM73_CE_PIN);
    //in
    RFM73_IRQ_DIR &= ~(1 << RFM73_IRQ_PIN);
    RFM73_CE_LOW;
    // low level init
    // IO-pins and pullups
    spiInit();
    spiSelect(csNONE);
    sbi(PORTD, LED_RED);

    // RFM70
    // write registers
    // light green led if rfm70 found
    // light red led if rfm70 not found
    if (rfm70InitRegisters())
    {
        cbi(PORTD, LED_RED);
    }
    else
    {
        sbi(PORTD, LED_RED);
    }

    _delay_ms(50);
    // init and power up modules
    // goto RX mode
    wdt_reset(); // keep the watchdog happy
    rfm70SetModeRX();
    wdt_reset(); // keep the watchdog happy
    _delay_ms(50);

    int rand = 1234;
    while (1)
    {
        wdt_reset(); // keep the watchdog happy
        usbPoll();

        rfm70ReceivePayload();

        if (usbInterruptIsReady())
        { // if the interrupt is ready, feed data
            // pseudo-random sequence generator, thanks to Dan Frederiksen @AVRfreaks
            // http://en.wikipedia.org/wiki/Linear_congruential_generator
            rand = (rand * 109 + 89) % 251;

            // move to a random direction
            //reportBuffer.dx = (rand&0xf)-8;
            //reportBuffer.dy = ((rand&0xf0)>>4)-8;

            usbSetInterrupt((void *)&reportBuffer, sizeof(reportBuffer));

            reportBuffer.dx = 0;
            reportBuffer.dy = 0;
        }
    }

    return 0;
}

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
    usbRequest_t *rq = (void *)data;

    // The following requests are never used. But since they are required by
    // the specification, we implement them in this example.
    if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS)
    {
        if (rq->bRequest == USBRQ_HID_GET_REPORT)
        {
            // wValue: ReportType (highbyte), ReportID (lowbyte)
            usbMsgPtr = (void *)&reportBuffer; // we only have this one
            return sizeof(reportBuffer);
        }
        else if (rq->bRequest == USBRQ_HID_GET_IDLE)
        {
            usbMsgPtr = &idleRate;
            return 1;
        }
        else if (rq->bRequest == USBRQ_HID_SET_IDLE)
        {
            idleRate = rq->wValue.bytes[1];
        }
    }

    return 0; // by default don't return any data
}