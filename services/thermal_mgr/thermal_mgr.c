#include "thermal_mgr.h"
#include "errors.h"
#include "lm75bd.h"
#include "console.h"
#include "logging.h"

#include <FreeRTOS.h>
#include <os_task.h>
#include <os_queue.h>


#include <string.h>


#define THERMAL_MGR_STACK_SIZE 256U


static TaskHandle_t thermalMgrTaskHandle;
static StaticTask_t thermalMgrTaskBuffer;
static StackType_t thermalMgrTaskStack[THERMAL_MGR_STACK_SIZE];


#define THERMAL_MGR_QUEUE_LENGTH 10U
#define THERMAL_MGR_QUEUE_ITEM_SIZE sizeof(thermal_mgr_event_t)


static QueueHandle_t thermalMgrQueueHandle;
static StaticQueue_t thermalMgrQueueBuffer;
static uint8_t thermalMgrQueueStorageArea[THERMAL_MGR_QUEUE_LENGTH * THERMAL_MGR_QUEUE_ITEM_SIZE];


static void thermalMgr(void *pvParameters);


void initThermalSystemManager(lm75bd_config_t *config) {
 memset(&thermalMgrTaskBuffer, 0, sizeof(thermalMgrTaskBuffer));
 memset(thermalMgrTaskStack, 0, sizeof(thermalMgrTaskStack));
  thermalMgrTaskHandle = xTaskCreateStatic(
   thermalMgr, "thermalMgr", THERMAL_MGR_STACK_SIZE,
   config, 1, thermalMgrTaskStack, &thermalMgrTaskBuffer);


 memset(&thermalMgrQueueBuffer, 0, sizeof(thermalMgrQueueBuffer));
 memset(thermalMgrQueueStorageArea, 0, sizeof(thermalMgrQueueStorageArea));


 thermalMgrQueueHandle = xQueueCreateStatic(
   THERMAL_MGR_QUEUE_LENGTH, THERMAL_MGR_QUEUE_ITEM_SIZE,
   thermalMgrQueueStorageArea, &thermalMgrQueueBuffer);


}


error_code_t thermalMgrSendEvent(thermal_mgr_event_t *event) {
 /* Send an event to the thermal manager queue */
 if(event==NULL)return ERR_CODE_INVALID_ARG;        //if invalid argument
 if(thermalMgrQueueHandle==NULL)return ERR_CODE_INVALID_STATE;      //fail if thermalmgrqueuehandle doesnt work
 if(xQueueSend(thermalMgrQueueHandle,event,0)!=pdTRUE) return ERR_CODE_QUEUE_FULL;   //if queue is full
 return ERR_CODE_SUCCESS;
}


void osHandlerLM75BD(void) {
 /* Implement this function */
 thermal_mgr_event_t event = {.type=THERMAL_MGR_EVENT_OVER_TEMP_RECEIVED};
 thermalMgrSendEvent(&event);
}


static void thermalMgr(void *pvParameters) {
  /* Implement this task */
  while (1) {
    thermal_mgr_event_t event;
    if (xQueueReceive(thermalMgrQueueHandle, &event, portMAX_DELAY) == pdTRUE) {
      // Only read the temperature when the event type is THERMAL_MGR_EVENT_MEASURE_TEMP_CMD
      if (event.type == THERMAL_MGR_EVENT_MEASURE_TEMP_CMD) {
        float tempval;
        error_code_t errCode=readTempLM75BD(LM75BD_OBC_I2C_ADDR, &tempval);
        LOG_IF_ERROR_CODE(errCode);
        if (errCode != ERR_CODE_SUCCESS) {
          continue;
        }
        addTemperatureTelemetry(tempval);
      } 
      else if (event.type == THERMAL_MGR_EVENT_OVER_TEMP_RECEIVED) {
        float tempval;
        error_code_t errCode=readTempLM75BD(LM75BD_OBC_I2C_ADDR, &tempval);
        LOG_IF_ERROR_CODE(errCode);
        if (errCode != ERR_CODE_SUCCESS) {
          continue;
        }
        if (tempval >= LM75BD_DEFAULT_OT_THRESH) {
          overTemperatureDetected();
        }
        if (tempval <= LM75BD_DEFAULT_HYST_THRESH) {
          safeOperatingConditions();
        }
      }//
      else{
        error_code_t errCode=ERR_CODE_INVALID_EVENT;
        LOG_IF_ERROR_CODE(errCode);
      }
    }
  }
}




void addTemperatureTelemetry(float tempC) {
 printConsole("Temperature telemetry: %f deg C\n", tempC);
}


void overTemperatureDetected(void) {
 printConsole("Over temperature detected!\n");
}


void safeOperatingConditions(void) {
 printConsole("Returned to safe operating conditions!\n");
}
