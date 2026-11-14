/*
 * FunctionModeManager.h
 * Function Mode Manager
 * 
 * Responsibilities:
 * - Line Following Mode
 * - Obstacle Avoidance Mode
 * - Target Following Mode
 * - Manual Control Mode
 * - Standby Mode
 * 
 */

#ifndef FUNCTION_MODE_MANAGER_H
#define FUNCTION_MODE_MANAGER_H

#include <Arduino.h>

// Forward Declaration
class MotorDriverSystem;

// Function Mode Configuration
namespace ModeConfig {
    // Line Follow Mode Configuration
    namespace LineFollow {
        constexpr uint8_t NORMAL_SPEED = 80;      // Normal Speed
        constexpr uint8_t TURN_SPEED = 60;         // Turn Speed
        constexpr uint8_t SHARP_TURN_SPEED = 90;   // Sharp Turn Speed
        constexpr uint16_t SCAN_TIMEOUT_MS = 3000; // Scan Timeout
    }
    
    // Obstacle Avoidance Mode Configuration
    namespace ObstacleAvoid {
        constexpr uint8_t DETECTION_DISTANCE = 18;  // Detection Distance (cm)
        constexpr uint8_t MOVE_SPEED = 70;         // Move Speed
        constexpr uint8_t TURN_SPEED = 60;         // Turn Speed
        constexpr uint16_t BACKUP_TIME = 100;       // Backup Time (ms)
        constexpr uint16_t TURN_TIME = 50;          // Turn Time (ms)
    }
    
    // Target Following Mode Configuration
    namespace TargetFollow {
        constexpr uint8_t MIN_DISTANCE = 5;         // Min Distance (cm)
        constexpr uint8_t STOP_DISTANCE = 20;       // Stop Distance (cm)
        constexpr uint8_t FOLLOW_DISTANCE = 30;     // Follow Distance (cm)
        constexpr uint8_t BACKWARD_SPEED = 80;      // Backward Speed
        constexpr uint8_t FORWARD_SPEED = 100;      // Forward Speed
    }
    
}

/**
 * @brief Function Mode Manager Class
 * 
 * Manages various robot function modes:
 * - Line Following Mode
 * - Obstacle Avoidance Mode
 * - Target Following Mode
 * - Manual Control
 * - Standby Mode
 */
class FunctionModeManager {
public:
    /**
     * @brief Initialize Function Mode Manager
     * @param motorSystem Motor Driver System Pointer
     */
    void initialize(MotorDriverSystem* motorSystem);
    
    // ========== Function Mode Execution Functions ==========
    
    /**
     * @brief Execute Line Following Function
     */
    void executeLineFollowing();
    
    /**
     * @brief Execute Obstacle Avoidance Function
     */
    void executeObstacleAvoidance();
    
    /**
     * @brief Execute Target Following Function
     */
    void executeTargetFollowing();
    
    /**
     * @brief Execute Manual Control
     * 
     * @param direction Direction (1=Fwd, 2=Bwd, 3=Left, 4=Right)
     * @param speed Speed (0-255)
     */
    void executeManualControl(uint8_t direction, uint8_t speed);
    
    /**
     * @brief Execute Standby Mode
     */
    void executeStandby();
    
    /**
     * @brief Reset Line Following State
     */
    void resetLineFollowingState();
    
private:
    // Motor Driver System Pointer
    MotorDriverSystem* m_motorSystem;
    
    // ========== Obstacle Avoidance Mode State ==========
    struct {
        bool isFirstEnter;          // Is First Enter
    } m_obstacleAvoid;
    
    // ========== Target Following Mode State ==========
    struct {
        bool isFirstEnter;          // Is First Enter
    } m_targetFollow;
    
    // ========== Helper Functions ==========
    
    /**
     * @brief Check if value is in range
     */
    bool isInRange(long value, long min, long max) {
        return (value >= min) && (value <= max);
    }
    
    /**
     * @brief Safe Delay (Prevent Watchdog Reset)
     */
    void safeDelay(uint16_t ms);
};

#endif // FUNCTION_MODE_MANAGER_H
