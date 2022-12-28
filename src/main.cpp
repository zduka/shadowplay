#include "platform/platform.h"
#include "peripherals/neopixel.h"


/** Pinout
 
                            VDD     GND
     BTN_WHITE_MODE_PIN (0) PA4     PA3 (10) BTN_RGB_MODE_PIN
       BTN_EFFECT_L_PIN (1) PA5     PA2  (9) BTN_EFFECT_R_PIN
  BTN_BRIGHTNESS_UP_PIN (2) PA6     PA1  (8) VCC_PIN
BTN_BRIGHTNESS_DOWN_PIN (3) PA7    UPDI (11)
                        (4) PB3     PB0  (7) RGB_CONTROL_PIN
          WHITE_PWM_PIN (5) PB2     PB1  (6) RGB_PWR_PIN

- 4 buttons
- RGB led control
- RGB led switch
- white LED PWM
- 
  
    https://github.com/SpenceKonde/megaTinyCore/blob/master/megaavr/extras/ATtiny_x04.md

    6x15R for 3R parallel for 200mA at 4.2V. The LED shoudl not work while charging, makes sense
*/

#define BTN_RGB_MODE_PIN 10
#define BTN_EFFECT_R_PIN 9
#define BTN_BRIGHTNESS_UP_PIN 2
#define BTN_BRIGHTNESS_DOWN_PIN 3
#define BTN_WHITE_MODE_PIN 0
#define BTN_EFFECT_L_PIN 1
#define WHITE_PWM_PIN 5
#define RGB_PWR_PIN 6
#define RGB_CONTROL_PIN 7
#define VCC_PIN 8

// 10 minutes for countdown
#define POWER_OFF_COUNTDOWN 10 * 60 * 100

#define DEFAULT_BRIGHTNESS 8

// 50 ms debounce time
#define DEBOUNCE_TICKS 5

enum class Mode : uint8_t {
    Off,
    White,
    Candle, 
    Strobe,  
    RGB,  
};

// current mode, starts in off mode 
Mode mode = Mode::Off;
// countdown till shutdown in 10ms ticks
uint16_t countdown;
uint8_t ticksDivider;

uint8_t brightness = 8;
uint8_t currentBrightness = 0;


struct Button {
    bool state : 1;
    unsigned debounce : 7;
};

// debounce counters
Button buttons[6];

NeopixelStrip<1> currentRgb(RGB_CONTROL_PIN);
ColorStrip<1> rgb;
uint8_t hue = 0;
bool rainbow;


/** Enters sleep mode */
void sleep() {
    digitalWrite(WHITE_PWM_PIN, 0);
    digitalWrite(RGB_PWR_PIN, HIGH);
    mode = Mode::Off;
    cpu::sleep();
    //countdown = 10 * 60 * 100; // 10 minutes
    // this comes after wakeup
    //rgb.fill(Color::Cyan().withBrightness(32));
    //rgb.update();
}

void powerOn() {
    // nop
}

void powerOff() {
    countdown = DEBOUNCE_TICKS + 1; // TODO maybe number of steps? 
    brightness = 0;
}


/** A 10ms tick that is used for animation and counting purposes.
 * 
 */
void tick() {
    if (++ticksDivider % 5 != 0)
       return;
    // update brightness
    if (currentBrightness < brightness)
        ++currentBrightness;
    else if (currentBrightness > brightness)
        --currentBrightness;
    // perform the mode or effect settings
    switch (mode) {
        case Mode::White:
            analogWrite(WHITE_PWM_PIN, currentBrightness);
            break;
        case Mode::Candle:
            analogWrite(WHITE_PWM_PIN, currentBrightness);
            currentBrightness = random(0, brightness);
            break;
        case Mode::Strobe:
            if (++hue == 32)
                mode = Mode::White;
            else
                analogWrite(WHITE_PWM_PIN, (hue >> 2) & 1 ? currentBrightness : 0);
            break;
        case Mode::RGB:
            rgb.fill(Color::HSV(hue, 255, brightness));
            if (currentRgb.moveTowards(rgb)) {
                currentRgb.update();
                if (rainbow)
                    hue += 1;
            }
            break;
        default:
            // unreachable
            break;
    }
}

bool checkButton(uint8_t index, uint8_t pin) {
    if (buttons[index].debounce > 0) {
        --buttons[index].debounce;
        return false;
    } else if (buttons[index].state != digitalRead(pin)) {
        buttons[index].state = ! buttons[index].state;
        buttons[index].debounce = DEBOUNCE_TICKS; 
        if (buttons[index].state) {
            return false;
        } else {
            // reset the countdown and return true  
            countdown = POWER_OFF_COUNTDOWN;
            return true;
        }
    } else {
        return false;
    }
}

void enterRGBMode() {
    digitalWrite(RGB_PWR_PIN, LOW); // on 
    mode = Mode::RGB;
    hue = 0;
    rainbow = true;
    rgb.fill(Color::HSV(hue * 255, 255, brightness));
}

/** Checks whether a button was pressed and performs a very simple debouncing.  
 */
void checkButtons() {
    if (checkButton(0, BTN_WHITE_MODE_PIN)) {
        if (mode == Mode::Off || mode == Mode::RGB) {
            mode = Mode::White;
            currentBrightness = 0;
            brightness = DEFAULT_BRIGHTNESS;
            digitalWrite(RGB_PWR_PIN, HIGH);
        } else {
            powerOff(); 
        }
    }
    if (checkButton(1, BTN_RGB_MODE_PIN)) {
        if (mode != Mode::RGB) {
            enterRGBMode();
        } else {
            powerOff();
        }
    }
    if (checkButton(2, BTN_EFFECT_L_PIN)) {
        if (mode == Mode::RGB) {
            if (hue == 0) {
                rainbow = true;
            } else {
                if (hue >= 16)
                    hue -= 16;
                else
                    hue = 0;
            }
        } else {
            mode = Mode::Candle;
        }
    }
    if (checkButton(3, BTN_EFFECT_R_PIN)) {
        if (mode == Mode::RGB) {
            if (rainbow) {
                rainbow = false;
                hue = 0;
            } else if (hue <= 232) {
                hue += 16;
            } else {
                hue = 248;
            }
        } else {
            mode = Mode::Strobe;
            hue = 0;
        }
    }
    if (checkButton(4, BTN_BRIGHTNESS_DOWN_PIN)) {
        if (brightness >= 16)
            brightness -= 8;
        else
            brightness = 8; 
    }

    if (checkButton(5, BTN_BRIGHTNESS_UP_PIN)) {
        if (brightness <= 240)
            brightness += 8;
        else 
            brightness = 248;
    }
}

void setup() {
    pinMode(BTN_BRIGHTNESS_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_BRIGHTNESS_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_EFFECT_L_PIN, INPUT_PULLUP);
    pinMode(BTN_EFFECT_R_PIN, INPUT_PULLUP);
    pinMode(BTN_WHITE_MODE_PIN, INPUT_PULLUP);
    pinMode(BTN_RGB_MODE_PIN, INPUT_PULLUP);
    pinMode(WHITE_PWM_PIN, OUTPUT);
    pinMode(RGB_PWR_PIN, OUTPUT);
    pinMode(RGB_CONTROL_PIN, OUTPUT);
    pinMode(VCC_PIN, INPUT);
    digitalWrite(WHITE_PWM_PIN, LOW);
    digitalWrite(RGB_PWR_PIN, HIGH); // off
    attachInterrupt(digitalPinToInterrupt(BTN_WHITE_MODE_PIN), powerOn, FALLING);
    attachInterrupt(digitalPinToInterrupt(BTN_RGB_MODE_PIN), powerOn, FALLING);
    // enter RGB Mode
    countdown = POWER_OFF_COUNTDOWN;
    enterRGBMode();
}

void loop() {
    // if we have reached the power off mode, turn off
    if (--countdown == 0)
        sleep();
    checkButtons();
    tick();
    cpu::delay_ms(10);
}