/*
 * FunctionModeManager.cpp
 * Function Mode Manager Implementation
 */

#include "FunctionModeManager.h"
#include "MotorDriverSystem.h"
#include <avr/wdt.h>

/**
 * @brief Initialize Function Mode Manager
 */
void FunctionModeManager::initialize(MotorDriverSystem* motorSystem) {
    m_motorSystem = motorSystem;

    // Initialize Obstacle Avoidance Mode State
    m_obstacleAvoid.isFirstEnter = true;
    
    // Initialize Target Following Mode State
    m_targetFollow.isFirstEnter = true;
}

/**
 * @brief Execute Line Following Function
 * 
 * Rewritten exactly following the simplified version LineFollowing_Simple.ino logic
 * Ensures consistent line following performance with the simplified version
 */
void FunctionModeManager::executeLineFollowing() {
    if (!m_motorSystem) return;
    
    // Static variables to maintain state
    static bool timestamp = true;
    static bool BlindDetection = true;
    static uint32_t MotorRL_time = 0;
    
    // Read sensors in real-time
    bool detL = (digitalRead(A2) == HIGH);
    bool detM = (digitalRead(A1) == HIGH);
    bool detR = (digitalRead(A0) == HIGH);
    
    if (!detL && detM && !detR) {
        // Only middle detected, go straight
        m_motorSystem->move(MovementType::Forward, ModeConfig::LineFollow::NORMAL_SPEED);
        timestamp = true;
        BlindDetection = true;
    }
    else if (detL && detM && !detR) {
        // Left + Middle, turn left 60
        m_motorSystem->move(MovementType::TurnLeft, ModeConfig::LineFollow::TURN_SPEED);
        timestamp = true;
        BlindDetection = true;
    }
    else if (detL && !detM && !detR) {
        // Only Left, turn left 90
        m_motorSystem->move(MovementType::TurnLeft, ModeConfig::LineFollow::SHARP_TURN_SPEED);
        timestamp = true;
        BlindDetection = true;
    }
    else if (!detL && !detM && detR) {
        // Only Right, turn right 90
        m_motorSystem->move(MovementType::TurnRight, ModeConfig::LineFollow::SHARP_TURN_SPEED);
        timestamp = true;
        BlindDetection = true;
    }
    else if (!detL && detM && detR) {
        // Middle + Right, turn right 60
        m_motorSystem->move(MovementType::TurnRight, ModeConfig::LineFollow::TURN_SPEED);
        timestamp = true;
        BlindDetection = true;
    }
    else if (detL && detM && detR) {
        // All detected, stop (Possibly endpoint or intersection)
        m_motorSystem->stop();
        timestamp = true;
        BlindDetection = true;
    }
    else {
        // Completely lost line, execute blind scan
        if (timestamp == true) {
            timestamp = false;
            MotorRL_time = millis();
            m_motorSystem->stop();
        }
        
        // Blind detection scan logic
        uint32_t elapsed = millis() - MotorRL_time;
        
        if ((isInRange(elapsed, 0, 200) || isInRange(elapsed, 1600, 2000)) && BlindDetection == true) {
            // Scan right
            m_motorSystem->move(MovementType::TurnRight, ModeConfig::LineFollow::NORMAL_SPEED);
        }
        else if (isInRange(elapsed, 200, 1600) && BlindDetection == true) {
            // Scan left
            m_motorSystem->move(MovementType::TurnLeft, ModeConfig::LineFollow::NORMAL_SPEED);
        }
        else if (isInRange(elapsed, ModeConfig::LineFollow::SCAN_TIMEOUT_MS,
                           ModeConfig::LineFollow::SCAN_TIMEOUT_MS + 500)) {
            // Scan timeout, stop
            BlindDetection = false;
            m_motorSystem->stop();
        }
    }
}

/**
 * @brief Execute Obstacle Avoidance Function
 */
void FunctionModeManager::executeObstacleAvoidance() {
    if (!m_motorSystem) return;
    
    // First enter mode, reset servo to center
    if (m_obstacleAvoid.isFirstEnter) {
        m_motorSystem->setServoAngle(90);
        m_obstacleAvoid.isFirstEnter = false;
    }
    
    // Get current distance
    const SensorData& sensors = m_motorSystem->getSensorData();
    uint16_t distance = sensors.ultrasonic.distanceCm;
    
    // If obstacle detected
    if (distance > 0 && distance <= ModeConfig::ObstacleAvoid::DETECTION_DISTANCE) {
        // Stop
        m_motorSystem->stop();
        
        // Scan surroundings (30°, 90°, 150°)
        for (uint8_t i = 1; i <= 5; i += 2) { // 1, 3, 5 (30°, 90°, 150°)
            uint8_t angle = 30 * i;
            m_motorSystem->setServoAngle(angle);
            safeDelay(5);
            
            uint16_t dist = m_motorSystem->readUltrasonicDistance();
            
            // If distance > 20, immediately choose this direction
            if (dist > ModeConfig::ObstacleAvoid::DETECTION_DISTANCE) {
                m_motorSystem->stop();
                
                switch (i) {
                    case 1: // 30° Right side clear
                        m_motorSystem->move(MovementType::TurnRight, ModeConfig::ObstacleAvoid::TURN_SPEED);
                        break;
                    case 3: // 90° Front clear
                        m_motorSystem->move(MovementType::Forward, ModeConfig::ObstacleAvoid::MOVE_SPEED);
                        break;
                    case 5: // 150° Left side clear
                        m_motorSystem->move(MovementType::TurnLeft, ModeConfig::ObstacleAvoid::TURN_SPEED);
                        break;
                }
                safeDelay(ModeConfig::ObstacleAvoid::TURN_TIME);
                m_obstacleAvoid.isFirstEnter = true; // Reset state
                return; // Found path, exit
            }
            else {
                // Obstacle in this direction too
                m_motorSystem->stop();
                
                // If last angle (150°) and still blocked
                if (i == 5) {
                    // Back up and turn using the configured obstacle-avoid turn speed.
                    m_motorSystem->move(MovementType::Backward, ModeConfig::ObstacleAvoid::MOVE_SPEED);
                    safeDelay(ModeConfig::ObstacleAvoid::BACKUP_TIME);
                    m_motorSystem->move(MovementType::TurnRight, ModeConfig::ObstacleAvoid::TURN_SPEED);
                    safeDelay(ModeConfig::ObstacleAvoid::TURN_TIME);
                    m_obstacleAvoid.isFirstEnter = true; // Reset state
                    return;
                }
            }
        }
    }
    else {
        // No obstacle ahead, move forward
        m_motorSystem->move(MovementType::Forward, ModeConfig::ObstacleAvoid::MOVE_SPEED);
    }
}

/**
 * @brief Execute Target Following Function
 */
void FunctionModeManager::executeTargetFollowing() {
    if (!m_motorSystem) return;
    
    // First enter mode, reset servo to center
    if (m_targetFollow.isFirstEnter) {
        m_motorSystem->setServoAngle(90);
        m_targetFollow.isFirstEnter = false;
    }
    
    // Get current distance
    const SensorData& sensors = m_motorSystem->getSensorData();
    uint16_t distance = sensors.ultrasonic.distanceCm;
    
    // Decide action based on distance
    if (isInRange(distance, 0, ModeConfig::TargetFollow::MIN_DISTANCE)) {
        // Too close, move backward
        m_motorSystem->move(MovementType::Backward, ModeConfig::TargetFollow::BACKWARD_SPEED);
    }
    else if (isInRange(distance, ModeConfig::TargetFollow::MIN_DISTANCE + 1, 
                               ModeConfig::TargetFollow::STOP_DISTANCE)) {
        // Good distance, stop
        m_motorSystem->stop();
    }
    else if (isInRange(distance, ModeConfig::TargetFollow::STOP_DISTANCE + 1, 
                               ModeConfig::TargetFollow::FOLLOW_DISTANCE)) {
        // A bit far, move forward
        m_motorSystem->move(MovementType::Forward, ModeConfig::TargetFollow::FORWARD_SPEED);
    }
    else {
        // Too far or lost, stop
        m_motorSystem->stop();
    }
}

/**
 * @brief Execute Manual Control
 */
void FunctionModeManager::executeManualControl(uint8_t direction, uint8_t speed) {
    if (!m_motorSystem) return;
    
    switch (direction) {
        case 1: // Forward
            m_motorSystem->move(MovementType::Forward, speed);
            break;
        case 2: // Backward
            m_motorSystem->move(MovementType::Backward, speed);
            break;
        case 3: // Turn Left
            m_motorSystem->move(MovementType::TurnLeft, speed);
            break;
        case 4: // Turn Right
            m_motorSystem->move(MovementType::TurnRight, speed);
            break;
        case 5: // Forward Left
            m_motorSystem->move(MovementType::ForwardLeft, speed);
            break;
        case 6: // Backward Left
            m_motorSystem->move(MovementType::BackwardLeft, speed);
            break;
        case 7: // Forward Right
            m_motorSystem->move(MovementType::ForwardRight, speed);
            break;
        case 8: // Backward Right
            m_motorSystem->move(MovementType::BackwardRight, speed);
            break;
        case 9: // Stop
            m_motorSystem->stop();
            break;
        default:
            break;
    }
}

/**
 * @brief Execute Standby Mode
 */
void FunctionModeManager::executeStandby() {
    if (!m_motorSystem) return;
    m_motorSystem->stop();
}

/**
 * @brief Reset Line Following State
 * 
 * Called when exiting line following mode to reset scan state
 */
void FunctionModeManager::resetLineFollowingState() {
    // Line following uses local static state inside executeLineFollowing().
    // There is no external mutable state to clear here.
}

// ========== Helper Functions ==========

void FunctionModeManager::safeDelay(uint16_t ms) {
    wdt_reset();
    for (uint16_t i = 0; i < ms; i++) {
        delay(1);
        wdt_reset();
    }
}
