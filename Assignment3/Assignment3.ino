
#define ledPin 25
#define analogueReadPin 32

#define semaphoreWaitTime 1000

float newestAnalogueReading = 0.0;
float filterArray[] = {0, 0, 0, 0};

struct Data
{
  float analogueReading;
  float averageAnalogueReading;
} taskData = { 0, 0 };

SemaphoreHandle_t dataProtectionSemaphore;

struct Task 
{
  unsigned int taskDelay;
  TaskHandle_t taskHandle;
};

static void analogueReadTask(void *pvParameters)
{
  Task analogueRead = {1000, 0};

  for (;;)
  {
      // Use the Arduino analogRead() command to get a value between 0 and 4095 based on the input voltage of the potentiometer (0V - 3.3V)
      unsigned int analogueReading = analogRead(analogueReadPin);
      // Map the reading back to a voltage (0V - 3.3V). The map() function cannot take a float argument so the reading will be mapped to 3300 then devided into a float
      float analogueVoltage = map(analogueReading, 0, 4095, 0, 3300);
      // Cast int to float and divide into correct range
      analogueVoltage = (float)analogueVoltage / 1000;
      newestAnalogueReading = analogueVoltage;
      
      // Update the data struct. This is a shared resource and must be protected with a semaphore
      xSemaphoreTake(dataProtectionSemaphore, semaphoreWaitTime / portTICK_PERIOD_MS);
      taskData.analogueReading = analogueVoltage;
      xSemaphoreGive(dataProtectionSemaphore);
      
      printf("\nAnalogue Reading: %.2f", taskData.analogueReading);
      
      // Check stack size
//      unsigned int temp = uxTaskGetStackHighWaterMark(nullptr);
//      printf("\nHigh stack water mark is %d", temp);
      
      // Free up the CPU while delaying the task, allowing freeRTOS to run other tasks
      vTaskDelay(analogueRead.taskDelay);
  }
}

static void analogueAverageTask(void *pvParameters)
{  
    Task average = {1000, 0};
  
    for (;;)
    {
        float filteredVoltage = 0.0;
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
        
        printf("\nAverage analogue reading: %.2f", taskData.averageAnalogueReading);
            
        // Check stack size
//        unsigned int temp = uxTaskGetStackHighWaterMark(nullptr);
//        printf("\nHigh stack water mark is %d", temp);
        
        // Free up the CPU while delaying the task, allowing freeRTOS to run other tasks
        vTaskDelay(average.taskDelay);
    }
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  delay(500);
  pinMode(ledPin, OUTPUT);

  dataProtectionSemaphore = xSemaphoreCreateBinary();

  xTaskCreate(
    analogueReadTask,  
    "ReadPotentiometer",
    2048,
    NULL,
    1,
    NULL );

  xTaskCreate(
    analogueAverageTask,  
    "AveragePotentiometer",
    2048,
    NULL,
    1,
    NULL );
}

void loop()
{
}
