/*
 * RobotController.h
 * Robot Core Controller
 * 
 * Responsibilities:
 * - System initialization and configuration
 * - Main loop scheduling
 * - Mode management and switching
 * - Subsystem coordination
 * 
 */

#ifndef ROBOT_CONTROLLER_H
#define ROBOT_CONTROLLER_H

#include <Arduino.h>

// Forward declaration (avoid circular dependency)
class MotorDriverSystem;
class FunctionModeManager;
class CommunicationInterface;

// Operation Mode Enumeration
enum class OperationMode : uint8_t {
    Standby = 0,        // Standby Mode
    LineFollowing,      // Line Following Mode
    ObstacleAvoidance,  // Obstacle Avoidance Mode
    TargetFollowing,    // Target Following Mode
    ManualControl,      // Manual Control Mode
    Programming         // Programming Mode (Receive APP commands)
};

/**
 * @brief Robot Controller Main Class
 * 
 * This is the core controller of the entire robot system, responsible for:
 * 1. Initializing all subsystems
 * 2. Coordinating subsystems work
 * 3. Managing operation modes
 * 4. Handling system state
 */
class RobotController {
public:
    // Constructor
    RobotController();
    
    // Destructor
    ~RobotController();
    
    /**
     * @brief Initialize robot system
     * 
     * Initialize all subsystems:
     * - Motor Driver System
     * - Function Mode Manager
     * - Communication Interface
     */
    void initialize();
    
    /**
     * @brief Main loop update
     * 
     * Called in Arduino loop(), executes:
     * 1. Update sensor data
     * 2. Process communication inputs
     * 3. Execute current mode
     * 4. Update output display
     */
    void update();
    
    /**
     * @brief Set operation mode
     * @param mode Target mode
     */
    void setMode(OperationMode mode);
    
    /**
     * @brief Get current mode
     * @return Current operation mode
     */
    OperationMode getMode() const { return m_currentMode; }
    
    /**
     * @brief Set command ID (for APP communication response)
     * @param id Command identifier
     */
    void setCommandId(const char* id);
    
    /**
     * @brief Get command ID
     * @return Command identifier string
     */
    const char* getCommandId() const { return m_commandId; }
    
    // Get subsystem pointers (for use by other modules)
    MotorDriverSystem* getMotorSystem() { return m_motorSystem; }
    FunctionModeManager* getModeManager() { return m_modeManager; }
    CommunicationInterface* getCommInterface() { return m_commInterface; }
    
private:
    // Current State
    OperationMode m_currentMode;      // Current Operation Mode
    OperationMode m_previousMode;     // Previous Mode
    char m_commandId[16];             // Current Command ID
    
    // Subsystem Pointers
    MotorDriverSystem* m_motorSystem;           // Motor Driver System
    FunctionModeManager* m_modeManager;         // Function Mode Manager
    CommunicationInterface* m_commInterface;    // Communication Interface
    
    // Internal Methods
    
    /**
     * @brief Execute logic for current mode
     */
    void executeCurrentMode();
};

// Global Robot Controller Instance Declaration
extern RobotController g_robot;

#endif // ROBOT_CONTROLLER_H

