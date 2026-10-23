// M5StickC + Hat 8Servos sample
// https://docs.m5stack.com/en/hat/Hat_8Servos
//
//  BtnA : Run / Pause (pause moves all servos to 90 deg)
//  BtnB : Change motion pattern (Sweep -> Wave -> Center)

#include <M5Unified.h>
#include <Wire.h>

// Hat 8Servos
#define SERVO_ADDR      0x38
#define SERVO_ANGLE_REG 0x00    // 0x00-0x07 : CH1-CH8 angle (0-180)
#define SERVO_PULSE_REG 0x10    // 0x10-0x1F : CH1-CH8 pulse width (2 bytes, high byte first)
#define SERVO_RGB_REG   0x20    // 0x20-0x22 : G, R, B
#define SERVO_NUM       8

// HAT port of M5StickC
#define HAT_SDA 0
#define HAT_SCL 26

enum Pattern { PATTERN_SWEEP, PATTERN_WAVE, PATTERN_CENTER, PATTERN_MAX };
const char *patternName[PATTERN_MAX] = {"Sweep", "Wave", "Center"};

Pattern pattern = PATTERN_SWEEP;
bool running = true;
uint8_t angles[SERVO_NUM];

void writeServoAngle(uint8_t ch, uint8_t angle)
{
    if (ch >= SERVO_NUM) return;
    if (angle > 180) angle = 180;
    Wire.beginTransmission(SERVO_ADDR);
    Wire.write(SERVO_ANGLE_REG + ch);
    Wire.write(angle);
    Wire.endTransmission();
    angles[ch] = angle;
}

void writeServoPulse(uint8_t ch, uint16_t width)
{
    if (ch >= SERVO_NUM) return;
    Wire.beginTransmission(SERVO_ADDR);
    Wire.write(SERVO_PULSE_REG + ch * 2);
    Wire.write(width >> 8);
    Wire.write(width & 0xff);
    Wire.endTransmission();
}

void writeLed(uint8_t r, uint8_t g, uint8_t b)
{
    Wire.beginTransmission(SERVO_ADDR);
    Wire.write(SERVO_RGB_REG);
    Wire.write(g);
    Wire.write(r);
    Wire.write(b);
    Wire.endTransmission();
}

void centerAll()
{
    for (uint8_t ch = 0; ch < SERVO_NUM; ch++) {
        writeServoAngle(ch, 90);
    }
}

void drawStatus()
{
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.println("Hat 8Servos");
    M5.Display.setTextColor(running ? TFT_GREEN : TFT_RED, TFT_BLACK);
    M5.Display.printf("%s %s\n", running ? "RUN " : "STOP", patternName[pattern]);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
}

void drawAngles()
{
    M5.Display.setCursor(0, 20);
    for (uint8_t ch = 0; ch < SERVO_NUM; ch++) {
        M5.Display.printf("CH%d:%3d\n", ch + 1, angles[ch]);
    }
}

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);

    M5.Display.setRotation(0);
    M5.Display.setTextSize(1);

    Wire.begin(HAT_SDA, HAT_SCL, 100000);

    Wire.beginTransmission(SERVO_ADDR);
    if (Wire.endTransmission() != 0) {
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.println("8Servos\nnot found");
        Serial.println("Hat 8Servos not found");
        while (true) delay(100);
    }

    writeLed(0, 32, 0);
    centerAll();
    drawStatus();
}

void loop()
{
    static uint32_t lastUpdate = 0;
    static int step = 0;

    M5.update();

    if (M5.BtnA.wasPressed()) {
        running = !running;
        if (!running) {
            centerAll();
            writeLed(32, 0, 0);
        } else {
            writeLed(0, 32, 0);
        }
        drawStatus();
    }
    if (M5.BtnB.wasPressed()) {
        pattern = (Pattern)((pattern + 1) % PATTERN_MAX);
        step = 0;
        if (pattern == PATTERN_CENTER) centerAll();
        drawStatus();
    }

    if (running && millis() - lastUpdate >= 20) {
        lastUpdate = millis();
        switch (pattern) {
        case PATTERN_SWEEP: {
            // all servos move together 0 -> 180 -> 0
            int a = step % 360;
            if (a > 180) a = 360 - a;
            for (uint8_t ch = 0; ch < SERVO_NUM; ch++) writeServoAngle(ch, a);
            step += 2;
            break;
        }
        case PATTERN_WAVE:
            // each channel shifted by 45 deg in phase
            for (uint8_t ch = 0; ch < SERVO_NUM; ch++) {
                float rad = (step + ch * 45) * DEG_TO_RAD;
                writeServoAngle(ch, 90 + 60 * sinf(rad));
            }
            step += 4;
            break;
        default:
            break;
        }
        drawAngles();
    }
}
