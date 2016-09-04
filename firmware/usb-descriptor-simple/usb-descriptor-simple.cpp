#include "hal.h"
#include "Usb.h"
#include <stdint.h>
#include <stdlib.h>

USB  Usb;
uint8_t buffer[4096];


int main()
{
    platform_init();

    // Wait for a "go" signal from the host.
    Serial.begin(38400);
    Serial.println("Waiting");
    while (Serial.read() != '\n');

    // Pull the target reset low; actual reset can take a somewhat unpredictable amount of time,
    // both due to the capacitance of the RST line charging, and the CPU starting up from an
    // internal 8 MHz oscillator
    reset_target();

    // Make sure reset has been asserted and I/Os have settled before sync'ing
    _delay_ms(10);

    // Resynchronize with the target using a GPIO signal it asserts after booting
    sync_to_posedge();

    // Initialize peripherals with timers only after sync'ing to the target,
    // to avoid introducing additional jitter any time we access one of these modules.
    timer_init();
    reset_usb();
    Usb.Init();

    Serial.println();

    // Wait until the device has been found and addressed
    do {
        led_error(1);
        Usb.Task();
        if (Usb.getUsbTaskState() == USB_STATE_ERROR ||
            Usb.getUsbTaskState() == USB_STATE_DETACHED) {
            reset_xmega();
        }
        led_error(0);
    } while (Usb.getUsbTaskState() != USB_STATE_RUNNING);
    led_ok(1);

    // Set a small NAK limit for EP0, so we fail faster if/when the device NAKs
    Usb.getEpInfoEntry(1, 0)->bmNakPower = 4;

    // Try to do a ridiculous long descriptor read, and dump whatever we get back.
    memset(buffer, 0, sizeof buffer);
    trigger_high();
    int result = Usb.getConfDescr(1, 0, sizeof buffer, 0, buffer);
    trigger_low();

    // ctrlReq doesn't tell us how many IN packets we got back successfully,
    // so to keep the output concise we'll just dump everything save for trailing zeroes.

    int guessed_length = sizeof buffer;
    while (guessed_length && buffer[guessed_length - 1] == 0) {
        guessed_length--;
    }

    // Result metadata
    Serial.print("code ");
    Serial.print(result);
    Serial.print(" len ");
    Serial.print(guessed_length);

    // Result packet, in hex
    for (int i = 0; i < guessed_length; i++) {
        Serial.print(" ");
        Serial.print(buffer[i], HEX);
    }

    // End of experiment
    Serial.println();

    // Start over with a software reset
    reset_xmega();

    return 0;
}

