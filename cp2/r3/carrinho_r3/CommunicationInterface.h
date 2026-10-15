/*
 * CommunicationInterface.h
 * Communication Interface
 * 
 * Responsibilities:
 * - Serial communication and command parsing
 * - IR remote control processing
 * - Button processing
 * - LED display control
 */

#ifndef COMMUNICATION_INTERFACE_H
#define COMMUNICATION_INTERFACE_H

#include <Arduino.h>
#include "FastLED.h"

// Forward Declaration
class RobotController;

// LED Configuration
namespace LEDConfig {
    constexpr uint8_t PIN_LED = 4;              // LED Data Pin
    constexpr uint8_t NUM_LEDS = 1;             // Number of LEDs
    constexpr uint8_t DEFAULT_BRIGHTNESS = 20;  // Default Brightness
}

// Button Configuration
namespace KeyConfig {
    constexpr uint8_t PIN_KEY = 2;              // Button Pin
    constexpr uint8_t KEY_VALUE_MAX = 4;
}

// IR Configuration
namespace IRConfig {
    constexpr uint8_t PIN_IR = 9;
    constexpr uint16_t TIMEOUT_MS = 300;
}

/**
 * @brief Communication Interface Class
 * 
 * Integrates all input/output communication:
 * - Serial Communication (JSON Commands)
 * - IR Remote Control
 * - Button Detection
 * - LED Display
 */
class CommunicationInterface {
public:
    /**
     * @brief Initialize Communication Interface
     */
    void initialize();
    
    // ========== Input Processing ==========
    
    /**
     * @brief Process Serial Commands
     * @param robot Pointer to Robot Controller
     */
    void processSerialCommands(RobotController* robot);
    
    /**
     * @brief Process IR Remote
     * @param robot Pointer to Robot Controller
     */
    void processIRRemote(RobotController* robot);
    
    /**
     * @brief Process Button Press
     * @param robot Pointer to Robot Controller
     */
    void processKeyPress(RobotController* robot);
    
    /**
     * @brief Check Direction Key Timeout (Auto stop after 300ms)
     * @param robot Pointer to Robot Controller
     */
    void checkDirectionKeyTimeout(RobotController* robot);
    
    // ========== Output Display ==========
    
    /**
     * @brief Update LED Status Display
     * @param mode Current Mode
     * @param lowBattery Is Low Battery
     */
    void updateLEDStatus(uint8_t mode, bool lowBattery);
    
    /**
     * @brief Set Single LED Color
     * @param ledIndex LED Index (0-4)
     * @param r Red (0-255)
     * @param g Green (0-255)
     * @param b Blue (0-255)
     */
    void setLEDColor(uint8_t ledIndex, uint8_t r, uint8_t g, uint8_t b);
    
    /**
     * @brief Set All LEDs Color
     */
    void setAllLEDs(uint8_t r, uint8_t g, uint8_t b);
    
    /**
     * @brief Set LED Brightness
     * @param brightness Brightness (0-255)
     */
    void setLEDBrightness(uint8_t brightness);
    
    /**
     * @brief Clear All LEDs
     */
    void clearLEDs();
    
    // LED Array (Public for special control)
    CRGB leds[LEDConfig::NUM_LEDS];
    
private:
    // ========== Serial Communication Private Members ==========
    char m_serialBuffer[256];       // Serial Buffer
    uint16_t m_bufferPos;           // Buffer Position
    
    /**
     * @brief Parse JSON Command
     * @param jsonString JSON String
     * @param robot Pointer to Robot Controller
     */
    void parseJSONCommand(const char* jsonString, RobotController* robot);
    
    /**
     * @brief Execute Specific Command
     * @param cmdId Command ID
     * @param robot Pointer to Robot Controller
     */
    void executeCommand(int cmdId, RobotController* robot);
    
    // ========== IR Remote Private Members ==========
    uint32_t m_IR_PreMillis;        // IR Key Timestamp
    bool m_IRrecv_enabled;          // IR Receive Enabled Flag
    uint8_t m_manualSpeed;          // Manual Control Speed
    uint8_t m_servoAngle;           // Current Servo Angle (0-180, default 90)
    
    /**
     * @brief Get IR Button Number
     * @param buttonNum Output Button Number (1-14)
     * @return true=Success, false=No Button
     */
    bool getIRButton(uint8_t* buttonNum);
    
    // ========== Button Detection Private Members ==========
    uint8_t m_lastKeyValue;         // Last Key Value
    
    /**
     * @brief Read Key Value
     * @return Key Value (0-4, 0 means no key)
     */
    uint8_t readKeyValue();
    
    // ========== LED Control Private Members ==========
    uint8_t m_ledBrightness;        // Current Brightness
    
    /**
     * @brief Display Mode Color
     * @param mode Mode Number
     */
    void displayModeColor(uint8_t mode);
    
    /**
     * @brief Display Low Battery Warning
     */
    void displayLowBatteryWarning();
    
    /**
     * @brief Display Breathing Effect (Standby Mode)
     */
    void displayBreathingEffect();
    
    /**
     * @brief Helper Function: Check if value is in range
     * @param value Value to check
     * @param min Minimum value
     * @param max Maximum value
     * @return true=In range, false=Out of range
     */
    bool isInRange(long value, long min, long max);
    
    // Breathing Effect State
    struct {
        uint8_t brightness;
        bool increasing;
        uint32_t lastUpdate;
    } m_breathingEffect;
};

#endif // COMMUNICATION_INTERFACE_H

