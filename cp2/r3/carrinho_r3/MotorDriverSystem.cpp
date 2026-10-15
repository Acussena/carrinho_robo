/*
 * MotorDriverSystem.cpp
 * Motor Driver System Implementation
 */

#include "MotorDriverSystem.h"

// Safe delay function (Prevent watchdog reset)
static void safeDelay(uint16_t ms) {
    for (uint16_t i = 0; i < ms; i++) {
        delay(1);
    }
}

/**
 * @brief Initialize motor driver system
 */
void MotorDriverSystem::initialize() {
    // Initialize motor pins (TB6612 Driver)
    pinMode(MotorPin::MOTOR_PWMA, OUTPUT);
    pinMode(MotorPin::MOTOR_PWMB, OUTPUT);
    pinMode(MotorPin::MOTOR_AIN_1, OUTPUT);
    pinMode(MotorPin::MOTOR_BIN_1, OUTPUT);
    pinMode(MotorPin::MOTOR_STBY, OUTPUT);  // Enable pin
    
    // Initialize motors to stop state
    digitalWrite(MotorPin::MOTOR_STBY, LOW);  // Disable motors
    digitalWrite(MotorPin::MOTOR_AIN_1, LOW);
    digitalWrite(MotorPin::MOTOR_BIN_1, LOW);
    analogWrite(MotorPin::MOTOR_PWMA, 0);
    analogWrite(MotorPin::MOTOR_PWMB, 0);
    
    // Initialize line sensors
    pinMode(MotorPin::LINE_SENSOR_LEFT, INPUT);
    pinMode(MotorPin::LINE_SENSOR_MIDDLE, INPUT);
    pinMode(MotorPin::LINE_SENSOR_RIGHT, INPUT);
    
    // Initialize ultrasonic sensor
    pinMode(MotorPin::ULTRASONIC_ECHO, INPUT);
    pinMode(MotorPin::ULTRASONIC_TRIG, OUTPUT);
    digitalWrite(MotorPin::ULTRASONIC_TRIG, LOW);
    
    // Initialize voltage sensor
    pinMode(MotorPin::VOLTAGE_SENSOR, INPUT);
    
    // Initialize servo
    m_servo.attach(MotorPin::SERVO);
    m_servo.write(90); // Initialize to middle position
    
    // Initialize sensor data
    m_sensorData.ultrasonic.distanceCm = 0;
    m_sensorData.ultrasonic.hasObstacle = false;
    m_sensorData.ultrasonic.lastUpdateTime = 0;
    
    m_sensorData.lineTracker.leftValue = 0;
    m_sensorData.lineTracker.middleValue = 0;
    m_sensorData.lineTracker.rightValue = 0;
    m_sensorData.lineTracker.leftDetected = false;
    m_sensorData.lineTracker.middleDetected = false;
    m_sensorData.lineTracker.rightDetected = false;
    
    m_sensorData.power.voltage = 0.0f;
    m_sensorData.power.isLowBattery = false;
}

/**
 * @brief Update all sensor data
 */
void MotorDriverSystem::updateSensors() {
    
    // Update ultrasonic (Appropriate frequency)
    static uint32_t lastUltrasonicUpdate = 0;
    if (millis() - lastUltrasonicUpdate > 60) { // Update every 60ms
        lastUltrasonicUpdate = millis();
        updateUltrasonic();
    }
    
    // Update voltage (Low frequency update)
    static uint32_t lastVoltageUpdate = 0;
    if (millis() - lastVoltageUpdate > 10) { // Update every 10ms
        lastVoltageUpdate = millis();
        updateVoltage();
    }
}

// ========== Motor Control Implementation ==========

/**
 * @brief Movement Control (High Level Interface)
 */
void MotorDriverSystem::move(MovementType type, uint8_t speed) {
    switch (type) {
        case MovementType::Stop:
            stop();
            break;
            
        case MovementType::Forward:
            setLeftMotor(speed, true);
            setRightMotor(speed, true);
            break;
            
        case MovementType::Backward:
            setLeftMotor(speed, false);
            setRightMotor(speed, false);
            break;
            
        case MovementType::TurnLeft:
            // Turn Left: Right motor forward, Left motor backward
            setRightMotor(speed, true);
            setLeftMotor(speed, false);
            break;
            
        case MovementType::TurnRight:
            // Turn Right: Right motor backward, Left motor forward
            setRightMotor(speed, false);
            setLeftMotor(speed, true);
            break;
            
        case MovementType::ForwardLeft:
            // Forward Left: Right motor speed, Left motor speed/2
            setRightMotor(speed, true);
            setLeftMotor(speed / 2, true);
            break;
            
        case MovementType::ForwardRight:
            // Forward Right: Right motor speed/2, Left motor speed
            setRightMotor(speed / 2, true);
            setLeftMotor(speed, true);
            break;
            
        case MovementType::BackwardLeft:
            // Backward Left: Right motor backward speed, Left motor backward speed/2
            setRightMotor(speed, false);
            setLeftMotor(speed / 2, false);
            break;
            
        case MovementType::BackwardRight:
            // Backward Right: Right motor backward speed/2, Left motor backward speed
            setRightMotor(speed / 2, false);
            setLeftMotor(speed, false);
            break;
            
        default:
            stop();
            break;
    }
}

/**
 * @brief Stop Motors
 */
void MotorDriverSystem::stop() {
    analogWrite(MotorPin::MOTOR_PWMA, 0);
    analogWrite(MotorPin::MOTOR_PWMB, 0);
    digitalWrite(MotorPin::MOTOR_STBY, LOW);  // Disable motor driver
}

/**
 * @brief Direct Motor Control
 */
void MotorDriverSystem::setMotorSpeed(uint8_t leftSpeed, uint8_t rightSpeed, 
                                      bool leftForward, bool rightForward) {
    setLeftMotor(leftSpeed, leftForward);
    setRightMotor(rightSpeed, rightForward);
}

/**
 * @brief Set Left Motor
 * 
 * TB6612 Driver: Single direction pin + PWM + Enable
 */
void MotorDriverSystem::setLeftMotor(uint8_t speed, bool forward) {
    if (speed > 0) {
        digitalWrite(MotorPin::MOTOR_STBY, HIGH);  // Enable motor driver
        digitalWrite(MotorPin::MOTOR_BIN_1, forward ? HIGH : LOW);  // Direction control
        analogWrite(MotorPin::MOTOR_PWMB, speed);   // Speed control
    } else {
        analogWrite(MotorPin::MOTOR_PWMB, 0);
    }
}

/**
 * @brief Set Right Motor
 * 
 * TB6612 Driver: Single direction pin + PWM + Enable
 */
void MotorDriverSystem::setRightMotor(uint8_t speed, bool forward) {
    if (speed > 0) {
        digitalWrite(MotorPin::MOTOR_STBY, HIGH);  // Enable motor driver
        digitalWrite(MotorPin::MOTOR_AIN_1, forward ? HIGH : LOW);  // Direction control
        analogWrite(MotorPin::MOTOR_PWMA, speed);   // Speed control
    } else {
        analogWrite(MotorPin::MOTOR_PWMA, 0);
    }
}

// ========== Servo Control Implementation ==========

/**
 * @brief Set Servo Angle
 */
void MotorDriverSystem::setServoAngle(uint8_t angle) {
    // Limit angle range
    if (angle > 180) angle = 180;
    m_servo.write(angle);
    delay(500); // Give servo time to reach position
}

/**
 * @brief Center Servo
 */
void MotorDriverSystem::centerServo() {
    setServoAngle(90);
}

// ========== Sensor Reading Implementation ==========

/**
 * @brief Read Ultrasonic Distance
 */
uint16_t MotorDriverSystem::readUltrasonicDistance() {
    unsigned long timeout = MotorConfig::MAX_DISTANCE * 58 + 2000;
    
    // Trigger ultrasonic
    digitalWrite(MotorPin::ULTRASONIC_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(MotorPin::ULTRASONIC_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(MotorPin::ULTRASONIC_TRIG, LOW);
    
    // Read echo time and convert to distance
    unsigned long duration = pulseIn(MotorPin::ULTRASONIC_ECHO, HIGH, timeout);
    uint16_t distance = duration / 58;
    
    // Limit max distance
    if (distance > 150) {
        distance = 150;
    }
    
    delay(60);
    
    return distance;
}

/**
 * @brief Update Ultrasonic Sensor Data
 */
void MotorDriverSystem::updateUltrasonic() {
    m_sensorData.ultrasonic.distanceCm = readUltrasonicDistance();
    m_sensorData.ultrasonic.hasObstacle = 
        (m_sensorData.ultrasonic.distanceCm > 0) && 
        (m_sensorData.ultrasonic.distanceCm <= MotorConfig::OBSTACLE_DISTANCE);
    m_sensorData.ultrasonic.lastUpdateTime = millis();
}

/**
 * @brief Update Line Tracking Sensor Data
 */

/**
 * @brief Update Voltage Data
 * 
 * Note: Called every 10ms by updateSensors()
 */
void MotorDriverSystem::updateVoltage() {
    int analogValue = analogRead(MotorPin::VOLTAGE_SENSOR);
    m_sensorData.power.voltage = analogValue * 0.0375f;
    m_sensorData.power.voltage = m_sensorData.power.voltage + (m_sensorData.power.voltage * 0.08f); // 8% compensation
    
    // Check for low battery
    // Called every 10ms, requires 500 consecutive checks (5 seconds) to determine low voltage
    static int VoltageData_number = 1;
    
    if (m_sensorData.power.voltage < MotorConfig::LOW_BATTERY_VOLTAGE) {
        VoltageData_number++;
        if (VoltageData_number == 500) { // 500 consecutive times (5 seconds)
            m_sensorData.power.isLowBattery = true;
            VoltageData_number = 0;
        }
    } else {
        m_sensorData.power.isLowBattery = false;
        VoltageData_number = 1; // Reset counter
    }
}

