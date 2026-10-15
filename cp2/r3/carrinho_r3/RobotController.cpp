/*
 * RobotController.cpp
 * Robot Core Controller Implementation
 * 
 */

#include "RobotController.h"
#include "MotorDriverSystem.h"
#include "FunctionModeManager.h"
#include "CommunicationInterface.h"
#include <avr/wdt.h>

// Global Robot Controller Instance Definition
RobotController g_robot;

// Constructor
RobotController::RobotController() 
    : m_currentMode(OperationMode::Standby)
    , m_previousMode(OperationMode::Standby)
    , m_motorSystem(nullptr)
    , m_modeManager(nullptr)
    , m_commInterface(nullptr)
{
    memset(m_commandId, 0, sizeof(m_commandId));
}

// Destructor
RobotController::~RobotController() {
    // Cleanup subsystems
    if (m_motorSystem) delete m_motorSystem;
    if (m_modeManager) delete m_modeManager;
    if (m_commInterface) delete m_commInterface;
}

/**
 * @brief Initialize robot system
 */
void RobotController::initialize() {
    // Initialize Serial
    Serial.begin(9600);
    Serial.println(F("Robot Controller Initializing..."));
    
    // Create subsystem instances
    m_motorSystem = new MotorDriverSystem();
    m_modeManager = new FunctionModeManager();
    m_commInterface = new CommunicationInterface();
    
    // Initialize subsystems
    Serial.println(F("Initializing Motor System..."));
    m_motorSystem->initialize();
    
    Serial.println(F("Initializing Mode Manager..."));
    m_modeManager->initialize(m_motorSystem);
    
    Serial.println(F("Initializing Communication..."));
    m_commInterface->initialize();
    
    // Set initial state
    m_currentMode = OperationMode::Standby;
    m_previousMode = OperationMode::Standby;
    
    Serial.println(F("Robot Controller Initialized!"));
    Serial.println(F("Ready to Start..."));
}

/**
 * @brief Main Loop Update
 */
void RobotController::update() {
    
    if (m_currentMode == OperationMode::LineFollowing) {
        // Line Following Mode: Skip all sensor updates (Line sensors read directly in executeLineFollowing)
        // No ultrasonic needed, no voltage monitoring needed (LED updates at lower frequency)
    } else if (m_currentMode == OperationMode::ObstacleAvoidance || 
               m_currentMode == OperationMode::TargetFollowing) {
        // Obstacle/Target Following Mode: Need ultrasonic, no line sensors
        m_motorSystem->updateSensors();
    } else {
        // Other Modes: Update all sensors (Standby, Manual, etc.)
        m_motorSystem->updateSensors();
    }
    
    // 2. Process Inputs (Serial, IR, Buttons) - Must execute, otherwise cannot switch modes
    m_commInterface->processSerialCommands(this);
    m_commInterface->processIRRemote(this);
    m_commInterface->processKeyPress(this);
    
    // 2.5. Check IR direction key timeout (Auto stop after 300ms)
    m_commInterface->checkDirectionKeyTimeout(this);
    
    // 3. Execute Current Mode
    executeCurrentMode();
    
    // 4. Update LED Display (Low frequency update, reduce overhead)
    static uint32_t lastLEDUpdate = 0;
    if (millis() - lastLEDUpdate > 100) { // Update every 100ms (Further reduce frequency)
        lastLEDUpdate = millis();
        const SensorData& sensorData = m_motorSystem->getSensorData();
        m_commInterface->updateLEDStatus(
            static_cast<uint8_t>(m_currentMode),
            sensorData.power.isLowBattery
        );
    }
}

/**
 * @brief Set Operation Mode
 * 
 * @param mode Target Mode
 */
void RobotController::setMode(OperationMode mode) {
    if (m_currentMode != mode) {
        m_previousMode = m_currentMode;
        m_currentMode = mode;
        
        // Debug Output
        Serial.print(F("Mode changed: "));
        Serial.print(static_cast<int>(m_previousMode));
        Serial.print(F(" -> "));
        Serial.println(static_cast<int>(m_currentMode));
    }
}

/**
 * @brief Set Command ID
 * 
 * @param id Command Identifier
 */
void RobotController::setCommandId(const char* id) {
    if (id) {
        strncpy(m_commandId, id, sizeof(m_commandId) - 1);
        m_commandId[sizeof(m_commandId) - 1] = '\0';
    }
}

/**
 * @brief Execute Current Mode Logic
 */
void RobotController::executeCurrentMode() {
    switch (m_currentMode) {
        case OperationMode::Standby:
            // Standby Mode
            m_modeManager->executeStandby();
            break;
            
        case OperationMode::LineFollowing:
            // Line Following Mode
            m_modeManager->executeLineFollowing();
            break;
            
        case OperationMode::ObstacleAvoidance:
            // Obstacle Avoidance Mode
            m_modeManager->executeObstacleAvoidance();
            break;
            
        case OperationMode::TargetFollowing:
            // Target Following Mode
            m_modeManager->executeTargetFollowing();
            break;
            
        case OperationMode::ManualControl:
            // Manual Control Mode (Driven by external commands, no execution needed here)
            break;
            
        case OperationMode::Programming:
            // Programming Mode (Waiting for commands, no execution needed here)
            break;
            
        default:
            // Unknown mode, switch to Standby
            setMode(OperationMode::Standby);
            break;
    }
}


