/*
 * RobotCar_main.ino
 * Smart Robot Car Main Program
 */

#include <avr/wdt.h>
#include "RobotController.h"


/**
 * @brief Arduino setup() function
 * 
 * Executed once at system startup
 * Responsible for:
 * 1. Enabling watchdog timer
 * 2. Initializing robot controller
 */
void setup() {
    // Enable watchdog timer (2 seconds timeout)
    // Prevent program deadlock
    wdt_enable(WDTO_2S);
    
    // Initialize global robot controller
    // This will initialize all subsystems:
    // - Motor Driver System
    // - Function Mode Manager
    // - Communication Interface
    g_robot.initialize();
    
    // Initialization complete prompt (with version info)
    Serial.println(F("========================================"));
    Serial.println(F("   Smart Robot Car V1.0.1 - Ready!"));
    Serial.println(F("========================================"));
}

/**
 * @brief Arduino loop() function
 * 
 * Main loop, executes continuously
 * Responsible for:
 * 1. Feeding the dog (Prevent watchdog reset)
 * 2. Updating robot state (Includes all logic)
 */
void loop() {
    // Feed the dog - Reset watchdog timer
    wdt_reset();
    
    // Update robot state
    // This function internally will:
    // 1. Update all sensor data
    // 2. Process all inputs (Serial/IR/Buttons)
    // 3. Execute current operation mode
    // 4. Update LED display
    g_robot.update();
}

/*
 * Main Program Description:
 * 
 * This main program file is extremely concise (only about 15 lines of effective code),
 * All complex logic is encapsulated in various subsystems:
 * 
 * RobotController (Core Controller)
 *   ├── MotorDriverSystem (Motor Driver System)
 *   │   ├── Motor Control
 *   │   ├── Sensor Reading (Ultrasonic, Line Tracking, Voltage)
 *   │   └── Servo Control
 *   │
 *   ├── FunctionModeManager (Function Mode Manager)
 *   │   ├── Line Following Mode
 *   │   ├── Obstacle Avoidance Mode
 *   │   ├── Target Following Mode
 *   │   ├── Manual Control
 *   │   └── Standby Mode
 *   │
 *   └── CommunicationInterface (Communication Interface)
 *       ├── Serial Command Parsing
 *       ├── IR Remote Processing
 *       ├── Button Detection
 *       └── LED Display Control
 * 

 */

