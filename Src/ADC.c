#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

// Assuming SPI, GPIO, and timer libraries are available
#include "BSP_SPI.h"
#include "gpio.h"
#include "timer.h"

// Define ADS7953 commands and settings
#define ADS7953_CMD_AUTO_1           0x2000
#define ADS7953_CMD_RANGE_SELECT     0x4000
#define ADS7953_CMD_EXT_REF          0x1000
#define ADS7953_CMD_2X_GAIN          0x0800
#define ADS7953_NUM_CHANNELS         16
#define QUEUE_SIZE                   4  // Adjust based on your memory constraints and desired averaging window

// Define GPIO pins (replace with actual pin numbers)
#define ADS7953_CS_PIN   10
#define ADS7953_BUSY_PIN 11

// ADS7953 configuration structure
typedef struct {
    uint8_t spi_channel;
    uint32_t spi_speed;
} ADS7953_Config;

// Circular queue structure
typedef struct {
    uint16_t data[QUEUE_SIZE];
    int front;
    int rear;
    int size;
} Queue;

// Structure to hold sampled data and averages
typedef struct {
    Queue channel_queues[ADS7953_NUM_CHANNELS];
    uint16_t channel_averages[ADS7953_NUM_CHANNELS];
    uint32_t timestamp;
} ADS7953_SampleSet;

// Function prototypes
void ADS7953_Init(ADS7953_Config *config);
void ADS7953_StartSampling(void);
bool ADS7953_IsSamplingComplete(void);
void ADS7953_GetResults(ADS7953_SampleSet *results);
void ADS7953_ProcessSamples(void);

// Queue operations
void Queue_Init(Queue *q);
void Queue_Enqueue(Queue *q, uint16_t value);
uint16_t Queue_Dequeue(Queue *q);
uint16_t Queue_Average(Queue *q);

// Global variables
static ADS7953_SampleSet g_sample_set;
static volatile bool g_sampling_complete = false;
static uint32_t g_start_time;

// Initialize the ADS7953
void ADS7953_Init(ADS7953_Config *config) {
    // Initialize SPI
    SPI_Init(config->spi_channel, config->spi_speed);
    
    // Configure GPIO
    GPIO_SetOutput(ADS7953_CS_PIN);
    GPIO_SetInput(ADS7953_BUSY_PIN);
    
    // Set CS high (inactive)
    GPIO_SetHigh(ADS7953_CS_PIN);
    
    // Configure ADS7953 for Auto-1 mode, external reference, and 2x gain
    uint16_t config_cmd = ADS7953_CMD_AUTO_1 | ADS7953_CMD_EXT_REF | ADS7953_CMD_2X_GAIN;
    
    GPIO_SetLow(ADS7953_CS_PIN);
    SPI_Transfer16(config->spi_channel, config_cmd);
    GPIO_SetHigh(ADS7953_CS_PIN);
    
    // Initialize the sample set and queues
    memset(&g_sample_set, 0, sizeof(ADS7953_SampleSet));
    for (int i = 0; i < ADS7953_NUM_CHANNELS; i++) {
        Queue_Init(&g_sample_set.channel_queues[i]);
    }
}

// Start sampling all channels
void ADS7953_StartSampling(void) {
    g_sampling_complete = false;
    g_start_time = Timer_GetCurrentTime();
    
    // Start Auto-1 sequence
    GPIO_SetLow(ADS7953_CS_PIN);
    SPI_Transfer16(ADS7953_CS_PIN, ADS7953_CMD_AUTO_1);
    
    // First transfer to start conversion
    SPI_Transfer16(ADS7953_CS_PIN, 0);
}

// Check if sampling is complete
bool ADS7953_IsSamplingComplete(void) {
    if (!g_sampling_complete) {
        uint32_t current_time = Timer_GetCurrentTime();
        if (current_time - g_start_time >= 60000) {  // 60 seconds in milliseconds
            g_sampling_complete = true;
            GPIO_SetHigh(ADS7953_CS_PIN);  // End SPI transaction
        }
    }
    return g_sampling_complete;
}

// Read results after sampling is complete
void ADS7953_GetResults(ADS7953_SampleSet *results) {
    if (g_sampling_complete) {
        memcpy(results, &g_sample_set, sizeof(ADS7953_SampleSet));
    }
}

// This function should be called in the main loop or a timer interrupt
void ADS7953_ProcessSamples(void) {
    static uint8_t current_channel = 0;
    
    if (!g_sampling_complete) {
        // Wait for conversion to complete
        while (GPIO_Read(ADS7953_BUSY_PIN) == 0);
        
        // Read the result and start next conversion
        uint16_t result = SPI_Transfer16(ADS7953_CS_PIN, 0);
        
        // Store the result in the appropriate queue
        Queue_Enqueue(&g_sample_set.channel_queues[current_channel], result & 0x0FFF);
        
        // Calculate and store the average
        g_sample_set.channel_averages[current_channel] = Queue_Average(&g_sample_set.channel_queues[current_channel]);
        
        // Move to next channel
        current_channel = (current_channel + 1) % ADS7953_NUM_CHANNELS;
        
        // Update timestamp if we've completed a full cycle
        if (current_channel == 0) {
            g_sample_set.timestamp = Timer_GetCurrentTime() - g_start_time;
        }
        
        // Check if sampling is complete
        ADS7953_IsSamplingComplete();
    }
}

// Initialize a queue
void Queue_Init(Queue *q) {
    q->front = 0;
    q->rear = -1;
    q->size = 0;
}

// Add an element to the queue
void Queue_Enqueue(Queue *q, uint16_t value) {
    if (q->size == QUEUE_SIZE) {
        // Queue is full, remove the oldest element
        Queue_Dequeue(q);
    }
    q->rear = (q->rear + 1) % QUEUE_SIZE;
    q->data[q->rear] = value;
    q->size++;
}

// Remove an element from the queue
uint16_t Queue_Dequeue(Queue *q) {
    if (q->size == 0) {
        return 0;  // Queue is empty
    }
    uint16_t value = q->data[q->front];
    q->front = (q->front + 1) % QUEUE_SIZE;
    q->size--;
    return value;
}

// Calculate the average of all elements in the queue
uint16_t Queue_Average(Queue *q) {
    if (q->size == 0) {
        return 0;
    }
    uint32_t sum = 0;
    for (int i = 0; i < q->size; i++) {
        sum += q->data[(q->front + i) % QUEUE_SIZE];
    }
    return (uint16_t)(sum / q->size);
}