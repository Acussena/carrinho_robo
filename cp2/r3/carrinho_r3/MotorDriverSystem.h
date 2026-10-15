/*
 * MotorDriverSystem.h
 * Motor Driver System
 * 
 * Responsibilities:
 * - Motor drive control
 * - Sensor data acquisition (Ultrasonic, Line Tracking, Voltage)
 * - Servo control
 * - Movement execution
 * 
 */

#ifndef MOTOR_DRIVER_SYSTEM_H
#define MOTOR_DRIVER_SYSTEM_H

#include <Arduino.h>
#include <Servo.h>

// Movement Type Enumeration
enum class MovementType : uint8_t {
    Stop = 0,           // Stop
    Forward,            // Forward
    Backward,           // Backward
    TurnLeft,           // Turn Left
    TurnRight,          // Turn Right
    ForwardLeft,        // Forward Left
    ForwardRight,       // Forward Right
    BackwardLeft,       // Backward Left
    BackwardRight       // Backward Right
};

// Sensor Data Structure
struct SensorData {
    // Ultrasonic Sensor
    struct {
        uint16_t distanceCm;        // Distance (cm)
        bool hasObstacle;           // Obstacle detected
        uint32_t lastUpdateTime;    // Last update time
    } ultrasonic;
    
    // Line Tracking Sensor
    struct {
        int leftValue;              // Left sensor value
        int middleValue;            // Middle sensor value
        int rightValue;             // Right sensor value
        bool leftDetected;          // Left side detected line
        bool middleDetected;        // Middle detected line
        bool rightDetected;         // Right side detected line
    } lineTracker;
    
    // Voltage Monitoring
    struct {
        float voltage;              // Voltage value (V)
        bool isLowBattery;          // Is low battery
    } power;
};

// Pin Definition Namespace
namespace MotorPin {
    // Motor Pins (TB6612 Driver)
    constexpr uint8_t MOTOR_PWMA = 5;      // Group A Motor PWM (Right)
    constexpr uint8_t MOTOR_PWMB = 6;      // Group B Motor PWM (Left)
    constexpr uint8_t MOTOR_AIN_1 = 7;     // Group A Motor Direction
    constexpr uint8_t MOTOR_BIN_1 = 8;     // Group B Motor Direction
    constexpr uint8_t MOTOR_STBY = 3;      // Motor Enable Pin (Important!)
    
    // Sensor Pins
    constexpr uint8_t LINE_SENSOR_LEFT = A2;    // Line Sensor Left
    constexpr uint8_t LINE_SENSOR_MIDDLE = A1;  // Line Sensor Middle
    constexpr uint8_t LINE_SENSOR_RIGHT = A0;   // Line Sensor Right
    constexpr uint8_t ULTRASONIC_TRIG = 13;     // Ultrasonic Trigger
    constexpr uint8_t ULTRASONIC_ECHO = 12;     // Ultrasonic Echo
    constexpr uint8_t VOLTAGE_SENSOR = A3;      // Voltage Detection
    
    // Servo Pin
    constexpr uint8_t SERVO = 10;               // Servo
}

// Configuration Parameters Namespace
namespace MotorConfig {
    // Voltage Related
    constexpr float LOW_BATTERY_VOLTAGE = 7.0f; // Low battery threshold
    constexpr float VOLTAGE_RATIO = 3.0f;       // Voltage divider ratio
    
    // Ultrasonic Related
    constexpr uint8_t OBSTACLE_DISTANCE = 20;     // Obstacle distance threshold (cm)
    constexpr uint16_t MAX_DISTANCE = 300;        // Max detection distance (cm)
}

/**
 * @brief Motor Driver System Class
 */
class MotorDriverSystem {
public:
    /**
     * @brief Initialize motor driver system
     */
    void initialize();
    
    /**
     * @brief Update all sensor data
     * 
     * [Critical Modification] No longer updates line tracking sensors,
     * they are read directly via IO in executeLineFollowing()
     */
    void updateSensors();
    
    // ========== Motor Control ==========
    
    /**
     * @brief Movement Control (High Level Interface)
     * @param type Movement Type
     * @param speed Speed (0-255)
     */
    void move(MovementType type, uint8_t speed);
    
    /**
     * @brief Stop Motors
     */
    void stop();
    
    /**
     * @brief Direct Motor Control (Low Level Interface)
     * @param leftSpeed Left Motor Speed (0-255)
     * @param rightSpeed Right Motor Speed (0-255)
     * @param leftForward Left Motor Direction (true=Forward)
     * @param rightForward Right Motor Direction (true=Forward)
     */
    void setMotorSpeed(uint8_t leftSpeed, uint8_t rightSpeed, 
                      bool leftForward, bool rightForward);
    
    // ========== Servo Control ==========
    
    /**
     * @brief Set Servo Angle
     * @param angle Angle (0-180)
     */
    void setServoAngle(uint8_t angle);
    
    /**
     * @brief Center Servo (90 degrees)
     */
    void centerServo();
    
    // ========== Sensor Access ==========
    
    /**
     * @brief Get Sensor Data
     * @return Constant reference to sensor data structure
     */
    const SensorData& getSensorData() const { return m_sensorData; }
    
    /**
     * @brief Read Ultrasonic Distance (Single)
     * @return Distance (cm)
     */
    uint16_t readUltrasonicDistance();
    
private:
    // Sensor Data
    SensorData m_sensorData;
    
    // Servo Object
    Servo m_servo;
    
    // ========== Motor Control Internal Methods ==========
    
    /**
     * @brief Set Left Motor
     * @param speed Speed (0-255)
     * @param forward true=Forward, false=Backward
     */
    void setLeftMotor(uint8_t speed, bool forward);
    
    /**
     * @brief Set Right Motor
     * @param speed Speed (0-255)
     * @param forward true=Forward, false=Backward
     */
    void setRightMotor(uint8_t speed, bool forward);
    
    // ========== Sensor Reading Internal Methods ==========
    
    /**
     * @brief Update Ultrasonic Sensor Data
     */
    void updateUltrasonic();
    
    /**
     * @brief Update Line Tracking Sensor Data
     * 
     * [Removed] This function has been removed, no longer updates line tracking sensors
     * Line tracking sensors now use digitalRead() directly in executeLineFollowing()
     */
    // void updateLineTracker(); // Removed
    
    /**
     * @brief Update Voltage Data
     */
    void updateVoltage();
    
    /**
     * @brief Helper Function: Check if value is in range
     */
    bool isInRange(int value, int minVal, int maxVal) {
        return (value >= minVal) && (value <= maxVal);
    }
};

#endif // MOTOR_DRIVER_SYSTEM_H

