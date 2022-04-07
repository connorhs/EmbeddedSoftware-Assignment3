
#define watchdogPin 26
#define digitalInputPin 33
#define frequencyMeasurePin 27
#define ledPin 25
#define analogueReadPin 32
#define timingPin 14

#define semaphoreWaitTime 1000

// A struct to hold shared data
struct Data
{
  unsigned char buttonState;
  unsigned int measuredFrequency;
  float averageAnalogueReading;
} taskData = { 0, 0, 0 };

// Queue and semaphore declarations
SemaphoreHandle_t dataProtectionSemaphore;
QueueHandle_t analogueReadingQueue, errorCodeQueue;

// A struct to hold the period of a task, as well as a handle(ended up not being used)
struct Task 
{
  unsigned int taskDelay;
  TaskHandle_t taskHandle;
};

/*
 *  Task definitions
 */

// Task 1: Digital watchdog
static void digitalWatchdogTask(void *pvParameters)
{
    // Task 1 period: 20ms
    Task task1 = {20, 0};
    
    for (;;)
    { 
       // Pulse the output high for 50 microseconds
       digitalWrite(watchdogPin, HIGH);
       // For such a short time a blocking delay was used over vTaskDelay
       delayMicroseconds(50);
       // Set the output low until the task is re-executed 20ms later
       digitalWrite(watchdogPin, LOW);
    
       // Check stack size
//       unsigned int temp = uxTaskGetStackHighWaterMark(nullptr);
//       printf("\nHigh stack water mark is %d", temp);
          
       // Free up the CPU while delaying the task, allowing freeRTOS to run other tasks
       vTaskDelay(task1.taskDelay); 
    }
}

// Task 2: Digital input
static void digitalInputTask(void *pvParameters)
{
    Task task2 = {200, 0};

    unsigned char timingState = 0;
  
    unsigned char state = 0;
    unsigned char oldState = 0;
    unsigned char buttonState = 0;

    for (;;)
    { 
      // If the button has been pressed
      if (digitalRead(digitalInputPin)) { state = 1; }
      else { state = 0; }

      if (state != oldState)
      {
        // Toggle the input state variable
        if (buttonState == 0) buttonState = 1;
        else buttonState = 0;  

        oldState = state;
      }

      // Update the data struct. This is a shared resource and must be protected with a semaphore
      xSemaphoreTake(dataProtectionSemaphore, semaphoreWaitTime / portTICK_PERIOD_MS);
      taskData.buttonState = state;
      xSemaphoreGive(dataProtectionSemaphore);

      if (timingState == 0)
      {
        timingState = 1;
        digitalWrite(timingPin, HIGH);
      }
      else
      {
        timingState = 0;
        digitalWrite(timingPin, LOW);  
      }
      
      // Check stack size
//      unsigned int temp = uxTaskGetStackHighWaterMark(nullptr);
//      printf("\nHigh stack water mark is %d", temp);
        
      // Free up the CPU while delaying the task, allowing freeRTOS to run other tasks
      vTaskDelay(task2.taskDelay); 
    }
}

// Task 3: Measure frequency of square wave
static void frequencyMeasureTask(void *pvParameters)
{
  Task task3 = {1000, 0};

  for (;;)
  {
    // Use the pulseIn() Arduino function to get the high period of the square wave in microseconds. This is half the period of the full square wave
    // This function will time out after 2ms
    float period = pulseIn(frequencyMeasurePin, HIGH, 2);
    // Double the high period to get the period of the full square wave
    period *= 2;
    // Get the frequency in Hz: f = 1/T(s) = 1000000/T(us)
    xSemaphoreTake(dataProtectionSemaphore, semaphoreWaitTime / portTICK_PERIOD_MS);
    taskData.measuredFrequency = 1000000 / period;
    xSemaphoreGive(dataProtectionSemaphore);
    //measuredFrequency = 1000000 / period;
    
    // Check stack size
//    unsigned int temp = uxTaskGetStackHighWaterMark(nullptr);
//    printf("\nHigh stack water mark is %d", temp);
      
    // Free up the CPU while delaying the task, allowing freeRTOS to run other tasks
    vTaskDelay(task3.taskDelay); 
  }
}

// Task 4: Read the analogue value of a potentiometer
static void analogueReadTask(void *pvParameters)
{
  Task task4 = {42, 0};

  for (;;)
  {
      // Use the Arduino analogRead() command to get a value between 0 and 4095 based on the input voltage of the potentiometer (0V - 3.3V)
      unsigned int analogueReading = analogRead(analogueReadPin);
      // Map the reading back to a voltage (0V - 3.3V). The map() function cannot take a float argument so the reading will be mapped to 3300 then devided into a float
      float analogueVoltage = map(analogueReading, 0, 4095, 0, 3300);
      // Cast int to float and divide into correct range
      analogueVoltage = (float)analogueVoltage / 1000;
      //newestAnalogueReading = analogueVoltage;
      // Send the newest reading to the queue, where it will be recieved by the averaging task. If the queue is full, then the average has not been taken yet
      // and this task should wait. The maximum wait time is defined as 10 ticks
      xQueueSend(analogueReadingQueue, &analogueVoltage, 10);
      
      // Check stack size
//      unsigned int temp = uxTaskGetStackHighWaterMark(nullptr);
//      printf("\nHigh stack water mark is %d", temp);
      
      // Free up the CPU while delaying the task, allowing freeRTOS to run other tasks
      vTaskDelay(task4.taskDelay);
  }
}



// Task 5: Average the last four readings of task 4
float filterArray[] = {0, 0, 0, 0};
static void analogueAverageTask(void *pvParameters)
{  
    Task task5 = {42, 0};
    
    for (;;)
    {
        
        float filteredVoltage = 0.0;

        // Get the most recent analogue reading from the queue
        float newestAnalogueReading = 0.0;
        xQueueReceive(analogueReadingQueue, &newestAnalogueReading, 10);
        
        // Shift new reading into filter
        for (int i = 3; i >= 0; i--)
        {
          // Elements 1, 2 nd 3 are right-shifted
          if (i > 0)
          {
            filterArray[i] = filterArray[i - 1];
          }
          // New reading is shifted into element 0
          else
          {
            filterArray[i] = newestAnalogueReading;
          }
          // Sum all of the elements in the updated array
          filteredVoltage += filterArray[i];
        }
        
        // Divide to get the average value of the updated array
        filteredVoltage /= 4.0;

        // Update the data struct. This is a shared resource and must be protected with a semaphore
        xSemaphoreTake(dataProtectionSemaphore, semaphoreWaitTime / portTICK_PERIOD_MS);
        taskData.averageAnalogueReading = filteredVoltage;
        xSemaphoreGive(dataProtectionSemaphore);
        
        // Check stack size
//        unsigned int temp = uxTaskGetStackHighWaterMark(nullptr);
//        printf("\nHigh stack water mark is %d", temp);
        
        // Free up the CPU while delaying the task, allowing freeRTOS to run other tasks
        vTaskDelay(task5.taskDelay);
    }
}

// Task 6: Execute the "nop" command 1000 times
static void NOPTask(void *pvParameters)
{
  Task task6 = {100, 0};

  for (;;)
  {
    for(int i = 0; i < 1000; i++)
    {
       __asm__ __volatile__ ("nop");  
    }
    
    // Check stack size
//    unsigned int temp = uxTaskGetStackHighWaterMark(nullptr);
//    printf("\nHigh stack water mark is %d", temp);
      
    // Free up the CPU while delaying the task, allowing freeRTOS to run other tasks
    vTaskDelay(task6.taskDelay); 
  }
}

// Task 7: Get an error code based on the current filtered potentiometer reading
static void errorCodeTask(void *pvParameters)
{
  Task task7 = {333, 0};

  for (;;)
  {
    // Get the average reading from the protected data struct
    xSemaphoreTake(dataProtectionSemaphore, semaphoreWaitTime / portTICK_PERIOD_MS);
    float filteredAnalgoueReading = taskData.averageAnalogueReading;
    xSemaphoreGive(dataProtectionSemaphore);

    // If filtered analogue value is greater than half of the maximum range (3.3V / 2 = 1.65V)
    unsigned char errorCode;
    if (filteredAnalgoueReading > 1.65)
    {
      errorCode = 1;
    }
    else
    {
      errorCode = 0;
    }

    // Send the updated error code to the queue
    xQueueSend(errorCodeQueue, &errorCode, 10);
    
    // Check stack size
//    unsigned int temp = uxTaskGetStackHighWaterMark(nullptr);
//    printf("\nHigh stack water mark is %d", temp);
      
    // Free up the CPU while delaying the task, allowing freeRTOS to run other tasks
    vTaskDelay(task7.taskDelay);
  }
}

// Task 8: Visualise the error code from task 7
static void errorCodeLEDTask(void *pvParameters)
{
  Task task8 = {333, 0};

  for (;;)
  {
    // Get the most recent value from the error code queue
    unsigned char newestErrorCode = 0;
    xQueueReceive(errorCodeQueue, &newestErrorCode, 10);
    // Turn LED on if the error code is 1
    if (newestErrorCode == 1)
      digitalWrite(ledPin, HIGH);
    // Turn LED off if the error code is 0
    else
      digitalWrite(ledPin, LOW);
    
    // Check stack size
//    unsigned int temp = uxTaskGetStackHighWaterMark(nullptr);
//    printf("\nHigh stack water mark is %d", temp);
      
    // Free up the CPU while delaying the task, allowing freeRTOS to run other tasks
    vTaskDelay(task8.taskDelay); 
  } 
}

// Task 9: Print data
static void dataPrintTask(void *pvParameters)
{
    Task task9 = {1000, 0};

    Data printData = { 0, 0, 0 };
    
    for (;;)
    { 
      // Get data from the semaphore protected data struct
      xSemaphoreTake(dataProtectionSemaphore, semaphoreWaitTime / portTICK_PERIOD_MS);
      printData.buttonState = taskData.buttonState;
      printData.measuredFrequency = taskData.measuredFrequency;
      printData.averageAnalogueReading = taskData.averageAnalogueReading;
      xSemaphoreGive(dataProtectionSemaphore);

      if (printData.buttonState == 1)
      {
        // Once the local data struct has been updated, print the data in the non-critical region
        printf("\n%d, %d, %.2f", printData.buttonState, printData.measuredFrequency, printData.averageAnalogueReading);
      }
      
      // Check stack size
//      unsigned int temp = uxTaskGetStackHighWaterMark(nullptr);
//      printf("\nHigh stack water mark is %d", temp);
        
      // Free up the CPU while delaying the task, allowing freeRTOS to run other tasks
      vTaskDelay(task9.taskDelay); 
    }
}

/*
 *  Set up the inputs and RTOS tasks
 */
void setup() {
  Serial.begin(115200);
  while (!Serial);
  delay(500);
  pinMode(ledPin, OUTPUT);
  pinMode(watchdogPin, OUTPUT);
  pinMode(digitalInputPin, INPUT);
  pinMode(timingPin, OUTPUT);

  // Create a semaphore to protect the data struct resource
  dataProtectionSemaphore = xSemaphoreCreateBinary();
  analogueReadingQueue = xQueueCreate(1, sizeof(float));
  errorCodeQueue = xQueueCreate(1, sizeof(unsigned char));

  // Create tasks 1-9
  // Each task is created with a stack size: determined by checking stack size then altering it until there is no wasted stack (with a bit extra as a buffer)
  // and a priority: set based on period to mimic a rate monotonic style
  xTaskCreate(
    digitalWatchdogTask,  
    "Task1",
    2048,
    NULL,
    1,
    NULL );

  xTaskCreate(
    digitalInputTask,  
    "Task2",
    2048,
    NULL,
    5,
    NULL );

  xTaskCreate(
    frequencyMeasureTask,  
    "Task3",
    2048,
    NULL,
    8,
    NULL );
  
  xTaskCreate(
    analogueReadTask,  
    "Task4",
    2048,
    NULL,
    2,
    NULL );

  xTaskCreate(
    analogueAverageTask,  
    "Task5",
    2048,
    NULL,
    3,
    NULL );

  xTaskCreate(
    NOPTask,  
    "Task6",
    2048,
    NULL,
    4,
    NULL );

  xTaskCreate(
    errorCodeTask,  
    "Task7",
    2048,
    NULL,
    6,
    NULL );

  xTaskCreate(
    errorCodeLEDTask,  
    "Task8",
    2048,
    NULL,
    7,
    NULL );

  xTaskCreate(
    dataPrintTask,  
    "Task9",
    2048,
    NULL,
    9,
    NULL );
}

// Loop funciton is empty
void loop()
{
}
