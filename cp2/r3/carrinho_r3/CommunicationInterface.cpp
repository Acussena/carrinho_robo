/*
 * CommunicationInterface.cpp
 * Communication Interface Implementation
 */

#include "CommunicationInterface.h"
#include "RobotController.h"
#include "MotorDriverSystem.h"
#include "FunctionModeManager.h"
#include "ArduinoJson-v6.11.1.h"
#include "IRremote.h"
#include <avr/wdt.h>  // Watchdog timer, used to prevent timeout restarts

// IR Receiver Instance
IRrecv irrecv(IRConfig::PIN_IR);
decode_results ir_results;

// Key Interrupt Handler
// Note: This is a global variable, modified in interrupt
volatile uint8_t g_keyInterruptCounter = 0;
volatile uint32_t g_keyLastInterruptTime = 0;

void keyInterruptHandler() {
    // Debounce: Trigger only once within 500ms
    uint32_t currentTime = millis();
    if ((currentTime - g_keyLastInterruptTime) > 500) {
        g_keyInterruptCounter++;
        if (g_keyInterruptCounter > KeyConfig::KEY_VALUE_MAX) {
            g_keyInterruptCounter = 0;
        }
        g_keyLastInterruptTime = currentTime;
        
        // Do not use Serial print in interrupt (may cause issues)
        // Print in main loop instead
    }
}

/**
 * @brief Initialize Communication Interface
 */
void CommunicationInterface::initialize() {
    // Initialize Serial
    // Serial is already initialized in RobotController
    m_bufferPos = 0;
    memset(m_serialBuffer, 0, sizeof(m_serialBuffer));
    
    // Initialize LEDs
    FastLED.addLeds<NEOPIXEL, LEDConfig::PIN_LED>(leds, LEDConfig::NUM_LEDS);
    FastLED.setBrightness(LEDConfig::DEFAULT_BRIGHTNESS);
    m_ledBrightness = LEDConfig::DEFAULT_BRIGHTNESS;
    clearLEDs();
    
    // Initialize Breathing Effect
    m_breathingEffect.brightness = 0;
    m_breathingEffect.increasing = true;
    m_breathingEffect.lastUpdate = 0;
    
    // Initialize IR Receiver
    irrecv.enableIRIn();
    m_IR_PreMillis = 0;
    m_IRrecv_enabled = false;
    
    // Initialize Manual Control Speed (Default 50% = 128)
    m_manualSpeed = 128;  // 255 * 0.5 = 127.5 approx 128
    
    // Initialize Servo Angle (Default 90 degrees)
    m_servoAngle = 90;
    
    // Initialize Key (Using Interrupt)
    pinMode(KeyConfig::PIN_KEY, INPUT_PULLUP);  // Use internal pull-up
    attachInterrupt(digitalPinToInterrupt(KeyConfig::PIN_KEY), keyInterruptHandler, FALLING);  // Trigger on falling edge
    m_lastKeyValue = 0;
    g_keyInterruptCounter = 0;
    g_keyLastInterruptTime = 0;
    
    Serial.println(F("Key interrupt attached to pin 2"));
}

// ========== Serial Communication Processing ==========

/**
 * @brief Process Serial Commands
 */
void CommunicationInterface::processSerialCommands(RobotController* robot) {
    if (!robot) return;
    
    // Read serial data
    while (Serial.available() > 0) {
        char c = Serial.read();
        
        // Detect frame header
        if (c == '{') {
            m_bufferPos = 0;
            memset(m_serialBuffer, 0, sizeof(m_serialBuffer));
        }
        
        // Add to buffer
        if (m_bufferPos < sizeof(m_serialBuffer) - 1) {
            m_serialBuffer[m_bufferPos++] = c;
        }
        
        // Detect frame footer
        if (c == '}') {
            m_serialBuffer[m_bufferPos] = '\0';
            parseJSONCommand(m_serialBuffer, robot);
            m_bufferPos = 0;
        }
    }
}

/**
 * @brief Parse JSON Command
 */
void CommunicationInterface::parseJSONCommand(const char* jsonString, RobotController* robot) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        Serial.println(F("{error:json}"));
        return;
    }
    
    // Get Command ID
    if (!doc.containsKey("N")) {
        Serial.println(F("{error:no_cmd}"));
        return;
    }
    
    int cmdId = doc["N"];
    const char* cmdHandle = doc["H"] | "";
    robot->setCommandId(cmdHandle);
    
    // Execute Command
    executeCommand(cmdId, robot);
}

/**
 * @brief Execute Specific Command
 * 
 * Core commands are implemented here, complete implementation requires adding all 20+ commands
 */
void CommunicationInterface::executeCommand(int cmdId, RobotController* robot) {
    switch (cmdId) {
        case 100: // Clear all functions, enter standby
            robot->setMode(OperationMode::Standby);
            robot->getMotorSystem()->stop();
            clearLEDs();
            Serial.print(F("{ok}"));
            break;
            
        case 101: // Mode Switching
        {
            StaticJsonDocument<200> doc;
            deserializeJson(doc, m_serialBuffer);
            int mode = doc["D1"];
            
            if (mode == 1) {
                robot->setMode(OperationMode::LineFollowing);
            } else if (mode == 2) {
                robot->setMode(OperationMode::ObstacleAvoidance);
            } else if (mode == 3) {
                robot->setMode(OperationMode::TargetFollowing);
            }
            Serial.print(F("{ok}"));
        }
        break;
        
        case 102: // Joystick Control
        {
            StaticJsonDocument<200> doc;
            deserializeJson(doc, m_serialBuffer);
            uint8_t direction = doc["D1"];
            uint8_t speed = doc["D2"];
            
            if (direction == 9) {
                // Joystick released, switch to standby mode and stop
                robot->setMode(OperationMode::Standby);
                robot->getMotorSystem()->stop();
            } else {
                // Joystick controlling, switch to manual control mode
            robot->setMode(OperationMode::ManualControl);
            robot->getModeManager()->executeManualControl(direction, speed);
            }
        }
        break;
        
        case 5: // Servo Control (Absolute Angle)
        {
            // [Fix Watchdog Timeout] Reset watchdog before potentially long operation
            wdt_reset();
            
            StaticJsonDocument<200> doc;
            deserializeJson(doc, m_serialBuffer);
            uint8_t servoSelect = doc["D1"]; // 1=Left/Right Servo, 2=Up/Down Servo
            uint8_t requestedAngle = doc["D2"]; // Angle requested by App 0-180
            
            // Limit angle range
            uint8_t angle = requestedAngle;
            if (angle > 180) {
                angle = 180;
            }
            
            // Call servo control function (delay removed internally, non-blocking)
            robot->getMotorSystem()->setServoAngle(angle);
            
            // Update internal state (for synchronization with IR remote)
            m_servoAngle = angle;
            
            // [Fix Watchdog Timeout] Reset watchdog again before serial output
            wdt_reset();
            
            // Return success response, including current angle for App sync
            // Format: {ok,angle:90}
            Serial.print(F("{ok,angle:"));
            Serial.print(m_servoAngle);
            Serial.print(F("}"));
        }
        break;
        
        case 6: // Servo Relative Rotation (Based on current angle)
        {
            // [Fix Watchdog Timeout] Reset watchdog before potentially long operation
            wdt_reset();
            
            StaticJsonDocument<200> doc;
            deserializeJson(doc, m_serialBuffer);
            uint8_t servoSelect = doc["D1"]; // 1=Left/Right Servo, 2=Up/Down Servo
            int16_t step = doc["D2"]; // Rotation step, positive=Right(increase), negative=Left(decrease)
            
            // Calculate new angle based on current angle
            int16_t newAngle = m_servoAngle + step;
            
            // Limit angle range 0-180
            if (newAngle < 0) {
                newAngle = 0;
            } else if (newAngle > 180) {
                newAngle = 180;
            }
            
            // Call servo control function (delay removed internally, non-blocking)
            robot->getMotorSystem()->setServoAngle(newAngle);
            
            // Update internal state (for synchronization with IR remote)
            m_servoAngle = newAngle;
            
            // [Fix Watchdog Timeout] Reset watchdog again before serial output
            wdt_reset();
            
            // Return success response, including current angle and step for App sync
            // Format: {ok,angle:90,step:15}
            Serial.print(F("{ok,angle:"));
            Serial.print(m_servoAngle);
            Serial.print(F(",step:"));
            Serial.print(step);
            Serial.print(F("}"));
        }
        break;
        
        // TODO: Add implementation for other commands
        // Command 1: Motor Control
        // Command 2: Car Move (Time Limited)
        // Command 3: Car Move (Unlimited Time)
        // Command 4: Motor Speed Control
        // Command 7: LED Control (Time Limited)
        // Command 8: LED Control (Unlimited Time)
        // Command 21: Ultrasonic Query
        // Command 22: Line Sensor Query
        // Command 23: Ground Detection
        // Etc...
        
        default:
            Serial.print(F("{error:unknown_cmd}"));
            break;
    }
}

// ========== IR Remote Processing ==========

/**
 * @brief Get IR Button Number
 */
bool CommunicationInterface::getIRButton(uint8_t* buttonNum) {
    if (!buttonNum) return false;
    
    if (irrecv.decode(&ir_results)) {
        m_IR_PreMillis = millis();  // Record timestamp
        
        switch (ir_results.value) {
            // Direction Keys
            case 16736925:  // aRECV_upper
            case 5316027:   // bRECV_upper
                *buttonNum = 1;
                break;
            case 16754775:  // aRECV_lower
            case 2747854299UL: // bRECV_lower
                *buttonNum = 2;
                break;
            case 16720605:  // aRECV_Left
            case 1386468383UL: // bRECV_Left
                *buttonNum = 3;
                break;
            case 16761405:  // aRECV_right
            case 553536955UL: // bRECV_right
                *buttonNum = 4;
                break;
            case 16712445:  // aRECV_ok
            case 3622325019UL: // bRECV_ok
                *buttonNum = 5;
                break;
            
            // Number Keys
            case 16738455:  // aRECV_1
            case 3238126971UL: // bRECV_1
                *buttonNum = 6;
                break;
            case 16750695:  // aRECV_2
            case 2538093563UL: // bRECV_2
                *buttonNum = 7;
                break;
            case 16756815:  // aRECV_3
            case 4039382595UL: // bRECV_3
                *buttonNum = 8;
                break;
            case 16724175:  // aRECV_4
            case 2534850111UL: // bRECV_4
                *buttonNum = 9;
                break;
            case 16718055:  // aRECV_5
            case 1033561079UL: // bRECV_5
                *buttonNum = 10;
                break;
            case 16743045:  // aRECV_6
            case 1635910171UL: // bRECV_6
                *buttonNum = 11;
                break;
            case 16716015:  // aRECV_7
            case 2351064443UL: // bRECV_7
                *buttonNum = 12;
                break;
            case 16726215:  // aRECV_8
            case 1217346747UL: // bRECV_8
                *buttonNum = 13;
                break;
            case 16734885:  // aRECV_9
            case 71952287UL: // bRECV_9
                *buttonNum = 14;
                break;
            
            default:
                irrecv.resume();
                return false;  // No button match
        }
        
        irrecv.resume();
        return true;  // Successfully got button
    }
    
    return false;  // No data received
}

/**
 * @brief Process IR Remote
 */
void CommunicationInterface::processIRRemote(RobotController* robot) {
    if (!robot) return;
    
    uint8_t IRrecv_button;
    
    if (getIRButton(&IRrecv_button)) {
        m_IRrecv_enabled = true;
    }
    
    if (m_IRrecv_enabled) {
        // Process Button
        switch (IRrecv_button) {
            case 1: // Forward
                robot->setMode(OperationMode::ManualControl);
                robot->getModeManager()->executeManualControl(1, m_manualSpeed);
                break;
            case 2: // Backward
                robot->setMode(OperationMode::ManualControl);
                robot->getModeManager()->executeManualControl(2, m_manualSpeed);
                break;
            case 3: // Turn Left
                robot->setMode(OperationMode::ManualControl);
                robot->getModeManager()->executeManualControl(3, m_manualSpeed);
                break;
            case 4: // Turn Right
                robot->setMode(OperationMode::ManualControl);
                robot->getModeManager()->executeManualControl(4, m_manualSpeed);
                break;
            case 5: // Standby
                robot->setMode(OperationMode::Standby);
                robot->getMotorSystem()->stop();
                break;
            case 6: // Number 1 - Line Following Mode
                robot->setMode(OperationMode::LineFollowing);
                robot->getMotorSystem()->stop();
                {
                    const SensorData& sensorData = robot->getMotorSystem()->getSensorData();
                    updateLEDStatus(static_cast<uint8_t>(OperationMode::LineFollowing), 
                                  sensorData.power.isLowBattery);
                }
                break;
            case 7: // Number 2 - Obstacle Avoidance Mode
                robot->setMode(OperationMode::ObstacleAvoidance);
                robot->getMotorSystem()->stop();
                {
                    const SensorData& sensorData = robot->getMotorSystem()->getSensorData();
                    updateLEDStatus(static_cast<uint8_t>(OperationMode::ObstacleAvoidance), 
                                  sensorData.power.isLowBattery);
                }
                break;
            case 8: // Number 3 - Target Following Mode
                robot->setMode(OperationMode::TargetFollowing);
                robot->getMotorSystem()->stop();
                {
                    const SensorData& sensorData = robot->getMotorSystem()->getSensorData();
                    updateLEDStatus(static_cast<uint8_t>(OperationMode::TargetFollowing), 
                                  sensorData.power.isLowBattery);
                }
                break;
            case 9:  // Number 4 - Servo Right 15 deg (Swapped)
            {
                // Right Rotate: Increase 15 degrees
                // Limit Max: Cannot exceed 180 degrees
                if (m_servoAngle <= 180 - 15) {
                    m_servoAngle += 15;
                } else {
                    m_servoAngle = 180;  // Reached max
                }
                robot->getMotorSystem()->setServoAngle(m_servoAngle);
            }
            break;
            case 10: // Number 5 - Servo Reset 90 deg
            {
                // Reset to 90 degrees
                m_servoAngle = 90;
                robot->getMotorSystem()->setServoAngle(m_servoAngle);
            }
            break;
            case 11: // Number 6 - Servo Left 15 deg (Swapped)
            {
                // Left Rotate: Decrease 15 degrees
                // Limit Min: Cannot be less than 0 degrees
                if (m_servoAngle >= 15) {
                    m_servoAngle -= 15;
                } else {
                    m_servoAngle = 0;  // Reached min
                }
                robot->getMotorSystem()->setServoAngle(m_servoAngle);
            }
            break;
            case 12: // Number 7 - Decrease Speed (Reduce 10%)
            {
                // Reduce 10%: 255 * 0.1 = 25.5 approx 26
                // Limit Min: Cannot be lower than 10% (approx 26)
                if (m_manualSpeed >= 26) {
                    m_manualSpeed -= 26;
                }
            }
            break;
            case 13: // Number 8 - Reset Default Speed (50%)
            {
                // Reset to default speed 50% = 128
                m_manualSpeed = 128;
            }
            break;
            case 14: // Number 9 - Increase Speed (Increase 10%)
            {
                // Increase 10%: 255 * 0.1 = 25.5 approx 26
                // Limit Max: Cannot exceed 255
                if (m_manualSpeed <= 255 - 26) {
                    m_manualSpeed += 26;
                } else {
                    m_manualSpeed = 255;  // Reached max
                }
            }
                break;
            default:
                robot->setMode(OperationMode::Standby);
                break;
        }
        
        if (IRrecv_button < 5) {
            // Direction Keys: Enter manual control mode, auto return to standby after 300ms
            
            // Check if 300ms passed
            if (millis() - m_IR_PreMillis > 300) {
                m_IRrecv_enabled = false;
                robot->setMode(OperationMode::Standby);
                robot->getMotorSystem()->stop();
                m_IR_PreMillis = millis();
            }
        }
        else {
            // Other keys: Disable immediately, reset timestamp
            m_IRrecv_enabled = false;
            m_IR_PreMillis = millis();
        }
    }
}

/**
 * @brief Check Direction Key Timeout
 */
void CommunicationInterface::checkDirectionKeyTimeout(RobotController* robot) {
    // Timeout check already done in processIRRemote()
    (void)robot; // Avoid unused parameter warning
}

// ========== Key Processing ==========

/**
 * @brief Process Key Press
 * 
 * Note: Use interrupt counter to read key value (not analog read)
 */
void CommunicationInterface::processKeyPress(RobotController* robot) {
    if (!robot) return;
    
    // Use interrupt counter as key value
    uint8_t keyValue = g_keyInterruptCounter;
    
    if (keyValue != m_lastKeyValue && keyValue > 0) {
        m_lastKeyValue = keyValue;
        
        Serial.print(F("Key mode switch: "));
        Serial.println(keyValue);
        
        switch (keyValue) {
            case 1: // Key 1 - Line Following Mode
                robot->setMode(OperationMode::LineFollowing);
                Serial.println(F("Mode: Line Following"));
                break;
            case 2: // Key 2 - Obstacle Avoidance Mode
                robot->setMode(OperationMode::ObstacleAvoidance);
                Serial.println(F("Mode: Obstacle Avoidance"));
                break;
            case 3: // Key 3 - Target Following Mode
                robot->setMode(OperationMode::TargetFollowing);
                Serial.println(F("Mode: Target Following"));
                break;
            case 4: // Key 4 - Standby Mode
                robot->setMode(OperationMode::Standby);
                Serial.println(F("Mode: Standby"));
                // Reset counter
                g_keyInterruptCounter = 0;
                m_lastKeyValue = 0;
                break;
            default:
                break;
        }
    }
}

/**
 * @brief Read Key Value
 * 
 * Note: Uses interrupt counting, not analog read
 * This function is kept but no longer used
 */
uint8_t CommunicationInterface::readKeyValue() {
    // Return interrupt counter value directly
    return g_keyInterruptCounter;
}

// ========== LED Display Control ==========

/**
 * @brief Update LED Status
 */
void CommunicationInterface::updateLEDStatus(uint8_t mode, bool lowBattery) {
    static unsigned long getAnalogue_time = 0;
    
    FastLED.clear(true);
    
    // Low battery warning time management (reset every 3000ms)
    if (lowBattery) {
        if ((millis() - getAnalogue_time) > 3000) {
            getAnalogue_time = millis();
        }
    }
    
    unsigned long temp = millis() - getAnalogue_time;
    
    // Low battery warning: Flash red quickly for first 500ms
    if (isInRange(temp, 0, 500) && lowBattery) {
        // Toggle every 50ms: 0-49 On, 50-99 Off, 100-149 On, 150-199 Off...
        if (isInRange(temp % 100, 0, 49)) {
            // Red On
            leds[0] = CRGB::Red;
            FastLED.show();
        } else {
            // Off (Black)
            leds[0] = CRGB::Black;
            FastLED.show();
        }
        return;
    }
    
    // Normal display mode color (500ms-3000ms during low battery, or normal voltage)
    if ((isInRange(temp, 500, 3000) && lowBattery) || !lowBattery) {
        switch (mode) {
            case 0: // Standby_mode
                if (lowBattery) {
                    // Low Battery: Red Flash (30ms On, 30ms Off)
                    static uint32_t standbyBlinkTime = 0;
                    static bool standbyBlinkState = false;
                    
                    if (millis() - standbyBlinkTime > 30) {
                        standbyBlinkTime = millis();
                        standbyBlinkState = !standbyBlinkState;
                    }
                    
                    if (standbyBlinkState) {
                        leds[0] = CRGB::Red;
                    } else {
                        leds[0] = CRGB::Black;
                    }
                    FastLED.show();
                } else {
                    // Normal: Violet Breathing Light (Brightness 0-100 cycle, change 1 every 10ms)
                    static uint8_t setBrightness = 0;
                    static boolean et = false;
                    static unsigned long time = 0;
                    
                    if ((millis() - time) > 10) {
                        time = millis();
                        if (et == false) {
                            setBrightness += 1;
                            if (setBrightness == 100) {
                                et = true;
                            }
                        } else if (et == true) {
                            setBrightness -= 1;
                            if (setBrightness == 0) {
                                et = false;
                            }
                        }
                    }
                    leds[0] = CRGB::Violet;
                    FastLED.setBrightness(setBrightness);
                    FastLED.show();
                }
                break;
                
            case 1: // LineFollowing
                leds[0] = CRGB::Green;
                FastLED.setBrightness(LEDConfig::DEFAULT_BRIGHTNESS);
                FastLED.show();
                break;
                
            case 2: // ObstacleAvoidance
                leds[0] = CRGB::Yellow;
                FastLED.setBrightness(LEDConfig::DEFAULT_BRIGHTNESS);
                FastLED.show();
                break;
                
            case 3: // TargetFollowing
                leds[0] = CRGB::Blue;
                FastLED.setBrightness(LEDConfig::DEFAULT_BRIGHTNESS);
                FastLED.show();
                break;
                
            case 4: // ManualControl
                leds[0] = CRGB::Violet;
                FastLED.setBrightness(LEDConfig::DEFAULT_BRIGHTNESS);
                FastLED.show();
                break;
                
            default:
                leds[0] = CRGB::White;
                FastLED.setBrightness(LEDConfig::DEFAULT_BRIGHTNESS);
                FastLED.show();
                break;
        }
    }
}

/**
 * @brief Display Mode Color
 */
void CommunicationInterface::displayModeColor(uint8_t mode) {
    CRGB color;
    
    switch (mode) {
        case 0: // Standby Mode
            displayBreathingEffect();
            return;
        case 1: // Line Following Mode
            color = CRGB::Green;
            break;
        case 2: // Obstacle Avoidance Mode
            color = CRGB::Yellow;
            break;
        case 3: // Target Following Mode
            color = CRGB::Blue;
            break;
        case 4: // Manual Control
            color = CRGB::Violet;
            break;
        default:
            color = CRGB::White;
            break;
    }
    
    // Set all LEDs to same color
    for (uint8_t i = 0; i < LEDConfig::NUM_LEDS; i++) {
        leds[i] = color;
    }
}

/**
 * @brief Display Low Battery Warning (Flashing Red)
 */
void CommunicationInterface::displayLowBatteryWarning() {
    static uint32_t lastBlink = 0;
    static bool blinkState = false;
    
    if (millis() - lastBlink > 500) {
        lastBlink = millis();
        blinkState = !blinkState;
    }
    
    if (blinkState) {
        for (uint8_t i = 0; i < LEDConfig::NUM_LEDS; i++) {
            leds[i] = CRGB::Red;
        }
    }
}

/**
 * @brief Display Breathing Effect (Standby Mode)
 */
void CommunicationInterface::displayBreathingEffect() {
    if (millis() - m_breathingEffect.lastUpdate > 10) {
        m_breathingEffect.lastUpdate = millis();
        
        if (m_breathingEffect.increasing) {
            m_breathingEffect.brightness++;
            if (m_breathingEffect.brightness >= 100) {
                m_breathingEffect.increasing = false;
            }
        } else {
            m_breathingEffect.brightness--;
            if (m_breathingEffect.brightness == 0) {
                m_breathingEffect.increasing = true;
            }
        }
    }
    
    leds[0] = CRGB::Violet;
    FastLED.setBrightness(m_breathingEffect.brightness);
}

/**
 * @brief Helper Function: Check if value is in range
 */
bool CommunicationInterface::isInRange(long value, long min, long max) {
    return (value >= min) && (value <= max);
}

/**
 * @brief Set Single LED Color
 */
void CommunicationInterface::setLEDColor(uint8_t ledIndex, uint8_t r, uint8_t g, uint8_t b) {
    if (ledIndex < LEDConfig::NUM_LEDS) {
        leds[ledIndex] = CRGB(r, g, b);
        FastLED.show();
    }
}

/**
 * @brief Set All LEDs Color
 */
void CommunicationInterface::setAllLEDs(uint8_t r, uint8_t g, uint8_t b) {
    for (uint8_t i = 0; i < LEDConfig::NUM_LEDS; i++) {
        leds[i] = CRGB(r, g, b);
    }
    FastLED.show();
}

/**
 * @brief Set LED Brightness
 */
void CommunicationInterface::setLEDBrightness(uint8_t brightness) {
    m_ledBrightness = brightness;
    FastLED.setBrightness(brightness);
    FastLED.show();
}

/**
 * @brief Clear All LEDs
 */
void CommunicationInterface::clearLEDs() {
    FastLED.clear(true);
}
