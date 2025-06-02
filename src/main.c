/*
 * This file is part of the µOS++ distribution.
 *   (https://github.com/micro-os-plus)
 * Copyright (c) 2014 Liviu Ionescu.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

// ----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "diag/trace.h"
#include <stdint.h>
#include <time.h> // used by rand()
#include <assert.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"	// To use semaphores

#define CCM_RAM __attribute__((section(".ccmram")))

// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
//
// Semihosting STM32F4 empty sample (trace via DEBUG).
//
// Trace support is enabled by adding the TRACE macro definition.
// By default the trace messages are forwarded to the DEBUG output,
// but can be rerouted to any device or completely suppressed, by
// changing the definitions required in system/src/diag/trace-impl.c
// (currently OS_USE_TRACE_ITM, OS_USE_TRACE_SEMIHOSTING_DEBUG/_STDOUT).
//

// ----- main() ---------------------------------------------------------------

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"
#pragma pack(push, 1)

/** Macros ********************************************************************/


#define T1 					( pdMS_TO_TICKS(100) )
#define T2 					( pdMS_TO_TICKS(200) )
#define Tout 				( pdMS_TO_TICKS(150) )      // tp be replaced {150, 175, 200, 225} msec
#define Pdrop 				( (double)0.01 )            // to be replaced {0.01, 0.02, 0.04, 0.08}
#define P_ack 				( (double)0.01 )
#define P_WRONG_PACKET		( (double)0.005 )
#define Tdelay				( pdMS_TO_TICKS(200) )
#define D					( pdMS_TO_TICKS(5) )         // Propagation delay constant  5 msec

static const uint32_t	L1 = 500;
static const uint32_t	L2 = 1500;
static const uint8_t	K = 40;                         // ack backet size
static const uint32_t	C = 100000;                     // link capacity (bits/sec)
static const uint8_t	N = 1;                          // Buffer Queue size {1 , 2 , 4 , 8 , 16}


/** End of Macros *************************************************************/

QueueHandle_t Sender1_GenQueue;
QueueHandle_t Sender1_BufferQueue;
QueueHandle_t Sender2_GenQueue;
QueueHandle_t Sender2_BufferQueue;;

QueueHandle_t SwitchQueue1;
QueueHandle_t SwitchQueue2;
QueueHandle_t SwitchQueue3;
QueueHandle_t SwitchQueue4;

QueueHandle_t Receiver3Queue;
QueueHandle_t Receiver4Queue;

TimerHandle_t sender1Timer;
TimerHandle_t sender2Timer;

uint8_t getRandom3or4() {
    return (rand() % 2) ? 3 : 4;
}

int RandVal(int min, int max) {
    return rand() % (max - min + 1) + min;
}

typedef struct {
    uint8_t destination;
    uint8_t sender;
    uint16_t sequence;
    uint16_t length;
} Header_t;

typedef struct {
    Header_t header;
    char data[];
} Packet_t;


static void Sender1TimerCallback(TimerHandle_t xTimer) {
    static uint16_t seq = 0;

    int length = RandVal(L1, L2);
    Packet_t *packet = (Packet_t *)malloc(sizeof(Packet_t) + length - 8);
    if (!packet) {
        trace_puts("Sender1: malloc failed");
        return;
    }

    packet->header.destination = getRandom3or4();
    packet->header.sender = 1;
    packet->header.sequence = seq++;
    packet->header.length = length;
    memset(packet->data, 0x0, length - 8);
    snprintf(packet->data, length - 8, "Hello from Sender1 #%u", packet->header.sequence);

    if (xQueueSend(Sender1_GenQueue, &packet, portMAX_DELAY) == pdPASS) {
        trace_printf("Sender1: sent packet #%u to %u (in generation queue)\n", packet->header.sequence, packet->header.destination);
    } else {
        trace_puts("Sender1: failed to send");
        free(packet);
    }

    // Restart timer with new random delay between t1 and t2
    int nextInterval = RandVal(T1, T2);
    xTimerChangePeriod(sender1Timer, pdMS_TO_TICKS(nextInterval), 0);
}

void Sender1Task(void *pvParameters) {
   trace_puts("Sender1: Timer-based task started");

    sender1Timer = xTimerCreate("Sender1Timer", pdMS_TO_TICKS(RandVal(T1, T2)), pdFALSE, NULL, Sender1TimerCallback);
    if (sender1Timer != NULL) {
        xTimerStart(sender1Timer, 0);
    }

    // Suspend the task, everything is done in the timer
    vTaskSuspend(NULL);
}


/*
void Sender1Task(void *pvParameters) {
    trace_printf("Sender1: started! task \n");
    uint16_t seq = 0;

    while (1) {
        int length = RandVal(L1, L2);
        Packet_t *packet = (Packet_t *)malloc(sizeof(Packet_t) + length - 8);
        if (!packet) {
            trace_puts("Sender1: malloc failed");
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(200));

        packet->header.destination = getRandom3or4();
        packet->header.sender = 1;
        packet->header.sequence = seq++;
        packet->header.length = length;
        memset(packet->data, 0x0, length - 8);
        snprintf(packet->data, length - 8, "Hello from Sender1 #%u", packet->header.sequence);

        if (xQueueSend(Sender1_GenQueue, &packet, portMAX_DELAY) == pdPASS) {
            trace_printf("Sender1: sent packet #%u to %u (in generation)\n", packet->header.sequence, packet->header.destination);
        } else {
            trace_puts("Sender1: failed to send");
            free(packet);
        }
    }
}*/

static void Sender2TimerCallback(TimerHandle_t xTimer) {
    static uint16_t seq = 0;

    int length = RandVal(L1, L2);
    Packet_t *packet = (Packet_t *)malloc(sizeof(Packet_t) + length - 8);
    if (!packet) {
        trace_puts("Sender2: malloc failed");
        return;
    }

    packet->header.destination = getRandom3or4();
    packet->header.sender = 2;
    packet->header.sequence = seq++;
    packet->header.length = length;
    memset(packet->data, 0x0, length - 8);
    snprintf(packet->data, length - 8, "Hello from Sender2 #%u", packet->header.sequence);

    if (xQueueSend(Sender2_GenQueue, &packet, portMAX_DELAY) == pdPASS) {
        trace_printf("Sender2: sent packet #%u to %u (in generation queue)\n", packet->header.sequence, packet->header.destination);
    } else {
        trace_puts("Sender2: failed to send");
        free(packet);
    }

    // Restart timer with new random delay between t1 and t2
    int nextInterval = RandVal(T1, T2);
    xTimerChangePeriod(sender2Timer, pdMS_TO_TICKS(nextInterval), 0);
}


void Sender2Task(void *pvParameters) {
   trace_puts("Sender2: Timer-based task started");

    sender2Timer = xTimerCreate("Sender2Timer", pdMS_TO_TICKS(RandVal(T1, T2)), pdFALSE, NULL, Sender2TimerCallback);
    if (sender2Timer != NULL) {
        xTimerStart(sender2Timer, 0);
    }

    // Suspend the task, everything is done in the timer
    vTaskSuspend(NULL);
}


void packetHandler(Packet_t* packet) {
    if (!packet || packet->header.sender == 0) return;

    QueueHandle_t destQueue = (packet->header.destination == 3) ? Receiver3Queue : Receiver4Queue;
    if (xQueueSend(destQueue, &packet, portMAX_DELAY) != pdPASS) {
        trace_puts("ReceiverQueue full. Packet lost.");
        free(packet);
    } else {
        trace_printf("Switch: Packet #%u forwarded from %u to %u \n",
            packet->header.sequence, packet->header.sender, packet->header.destination);
    }
}



void SwitchTask(void *parameters) {
    Packet_t* packet1 = NULL;
    Packet_t* packet2 = NULL;
    trace_printf("Switch: started! task \n");

    while (1) {
        if (xQueueReceive(SwitchQueue1, &packet1, pdMS_TO_TICKS(10)) == pdPASS && packet1) {
            if ((rand() % 100) < Pdrop*100) {
                trace_printf("Switch: dropped packet #%u from %u\n", packet1->header.sequence, packet1->header.sender);
                free(packet1);
                packet1 = NULL;
            }
        }

        if (xQueueReceive(SwitchQueue2, &packet2, pdMS_TO_TICKS(10)) == pdPASS && packet2) {
            if ((rand() % 100) < Pdrop*100) {
                trace_printf("Switch: dropped packet #%u from %u\n", packet2->header.sequence, packet2->header.sender);
                free(packet2);
                packet2 = NULL;
            }
        }

        if (packet1) {
            vTaskDelay(D + pdMS_TO_TICKS((packet1->header.length * 8) / C));
            packetHandler(packet1);
            packet1 = NULL;
        }

        if (packet2) {
            vTaskDelay(D + pdMS_TO_TICKS((packet2->header.length * 8) / C));
            packetHandler(packet2);
            packet2 = NULL;
        }
    }
}


void Receiver3Task(void* pvParameters) {
      Packet_t temp;
    Packet_t *LastPacket = &temp;
    Packet_t *CurrentPacket;
    uint16_t total_received_1 = 0, total_lost_1 = 0;
    uint16_t total_received_2 = 0, total_lost_2 = 0;
    LastPacket->header.sequence = 0;

    for (;;) {
        if (xQueueReceive(Receiver3Queue, &CurrentPacket, portMAX_DELAY) == pdPASS) {
            if (CurrentPacket->header.destination != 3) {
                trace_printf("_Error_ Receiver%u: Wrong dest %u from %u Seq #%u\n",
                             3, CurrentPacket->header.destination,
                             CurrentPacket->header.sender,
                             CurrentPacket->header.sequence);
                free(CurrentPacket);
                continue;
            }

            trace_printf("Receiver%u: Got packet from Sender %u with Seq #%u\n",
                         3, CurrentPacket->header.sender, CurrentPacket->header.sequence);

            if (CurrentPacket->header.sender == 1) {
                total_received_1++;
                total_lost_1 = CurrentPacket->header.sequence - LastPacket->header.sequence;
            } else {
                total_received_2++;
                total_lost_2 = CurrentPacket->header.sequence - LastPacket->header.sequence;
            }

            LastPacket->header.sequence = CurrentPacket->header.sequence;
            free(CurrentPacket);
        }
    }
}

void Receiver4Task(void* pvParameters) {
    Packet_t temp;
    Packet_t *LastPacket = &temp;
    Packet_t *CurrentPacket;
    uint16_t total_received_1 = 0, total_lost_1 = 0;
    uint16_t total_received_2 = 0, total_lost_2 = 0;
    LastPacket->header.sequence = 0;

    for (;;) {
        if (xQueueReceive(Receiver4Queue, &CurrentPacket, portMAX_DELAY) == pdPASS) {
            if (CurrentPacket->header.destination != 4) {
                trace_printf("_Error_ Receiver%u: Wrong dest %u from %u Seq #%u\n",
                             4, CurrentPacket->header.destination,
                             CurrentPacket->header.sender,
                             CurrentPacket->header.sequence);
                free(CurrentPacket);
                continue;
            }

            trace_printf("Receiver%u: Got packet from Sender %u with Seq #%u\n",
                         4, CurrentPacket->header.sender, CurrentPacket->header.sequence);

            if (CurrentPacket->header.sender == 1) {
                total_received_1++;
                total_lost_1 = CurrentPacket->header.sequence - LastPacket->header.sequence;
            } else {
                total_received_2++;
                total_lost_2 = CurrentPacket->header.sequence - LastPacket->header.sequence;
            }

            LastPacket->header.sequence = CurrentPacket->header.sequence;
            free(CurrentPacket);
        }
    }
}

void main(int argc, char* argv[]) {

    Sender1_GenQueue = xQueueCreate(10, sizeof(Packet_t*));
    Sender1_BufferQueue = xQueueCreate(N, sizeof(Packet_t*));
    Sender2_GenQueue = xQueueCreate(10, sizeof(Packet_t*));
    Sender2_BufferQueue = xQueueCreate(N, sizeof(Packet_t*));

    SwitchQueue1 = xQueueCreate(10, sizeof(Packet_t*));
    SwitchQueue2 = xQueueCreate(10, sizeof(Packet_t*));
    SwitchQueue3 = xQueueCreate(10, sizeof(Packet_t*));
    SwitchQueue4 = xQueueCreate(10, sizeof(Packet_t*));

    Receiver3Queue = xQueueCreate(10, sizeof(Packet_t*));
    Receiver4Queue = xQueueCreate(10, sizeof(Packet_t*));



    xTaskCreate(Sender1Task, "Sender1", 256, NULL, 1, NULL);
    xTaskCreate(Sender2Task, "Sender2", 256, NULL, 1, NULL);
    xTaskCreate(SwitchTask, "Switch", 256, NULL, 2, NULL);
    xTaskCreate(Receiver3Task, "Receiver3", 256, NULL, 1, NULL);
    xTaskCreate(Receiver4Task, "Receiver4", 256, NULL, 1, NULL);

    vTaskStartScheduler();
    for (;;);
}


void vApplicationMallocFailedHook( void )
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
volatile size_t xFreeStackSpace;

	/* This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amout of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 100 )
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */

	}
}

void vApplicationTickHook(void) {
}

StaticTask_t xIdleTaskTCB CCM_RAM;
StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE] CCM_RAM;

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
  /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
  state will be stored. */
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

  /* Pass out the array that will be used as the Idle task's stack. */
  *ppxIdleTaskStackBuffer = uxIdleTaskStack;

  /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
  Note that, as the array is necessarily of type StackType_t,
  configMINIMAL_STACK_SIZE is specified in words, not bytes. */
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB CCM_RAM;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH] CCM_RAM;

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize) {
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
  *ppxTimerTaskStackBuffer = uxTimerTaskStack;
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

