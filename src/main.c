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

/** Macros ********************************************************************/

#define ACK_PACKET 		(uint32_t)2001
#define NUM_OF_TRIES 	(uint8_t)4
#define PACKET_SENT		(uint8_t)1

/** End of Macros *************************************************************/

/** Type Declarations	*******************************************************/

// Packet structure
typedef struct {
    uint8_t destination;//1 byte
    uint8_t sender;		//1 bytes
    uint32_t sequence;	//4 bytes
    uint16_t length;	//2 bytes
    char data[];        //(lenght-8)  bytes
} Packet_t;				//total: (lenght)  bytes

// Queue handle
QueueHandle_t SwitchQueue1;    // treated as a king  B)
QueueHandle_t SwitchQueue2;    // treated as a king  B)
QueueHandle_t Receiver3Queue;
QueueHandle_t Receiver4Queue;


/** random receiver */
int getRandom3or4() {
    return 3 + (rand() % 2);  // rand() % 2 gives 0 or 1
}


/**      Tasks           **************************************************************/

void Sender1Task(void *pvParameters) {


    uint16_t seq = 0;

    while (1) {

    	Packet_t *packet = (Packet_t *)malloc(sizeof(Packet_t)+1000-8);
    		        if (!packet) {
    		            trace_puts("Sender1: malloc failed");
    		            vTaskDelay(pdMS_TO_TICKS(200));
    		            continue;
    		        }

    	               vTaskDelay(pdMS_TO_TICKS(200));

        // Create packet
        packet->destination = getRandom3or4();      // Example destination
        packet->sender =1;
        packet->sequence = seq++;
        packet->length = 1000;
        memset(packet->data, 0x0, packet->length-8);  // all payload bytes set to 0x0

        snprintf(packet->data, packet->length-8, "Hello from Sender1 #%u", packet->sequence);


        // Send to queue
        if (xQueueSend(SwitchQueue1, &packet, portMAX_DELAY) == pdPASS) {
                   trace_printf("Sender1: sent packet #%u to %u\n", packet->sequence, packet->destination);
               } else {
                   trace_puts("Sender1: failed to send");
                   free(packet);
               }


    }
}

void Sender2Task(void *pvParameters) {



   uint16_t seq = 0;

   while (1) {
	   Packet_t *packet = (Packet_t *)malloc(sizeof(Packet_t)+1000-8);
	   	        if (!packet) {
	   	            trace_puts("Sender2: malloc failed");
	   	            vTaskDelay(pdMS_TO_TICKS(200));
	   	           continue;
	   	        }

	               vTaskDelay(pdMS_TO_TICKS(200));

       // Create packet
       packet->destination = getRandom3or4();      // Example destination
       packet->sender =2;
       packet->sequence = seq++;
       packet->length = 1000;
       memset(packet->data, 0xF, packet->length-8);  // all payload bytes set to 0x0

       snprintf(packet->data, packet->length-8, "Hello from Sender1 #%u", packet->sequence);

       // Send to queue
       if (xQueueSend(SwitchQueue2, &packet, portMAX_DELAY) == pdPASS) {
                  trace_printf("Sender2: sent packet #%u to %u\n", packet->sequence, packet->destination);
              } else {
                  trace_puts("Sender2: failed to send");
                  free(packet);
              }
   }
}


void packet1handler(Packet_t* packet1){

	            // Forward to ReceiverQueue
	            if(packet1->sender!=0){
	            	 if(packet1->destination == 3+((rand() % 100) == 0 ? 1 : 0)){
	            	            	 if (xQueueSend(Receiver3Queue, &packet1, portMAX_DELAY) != pdPASS) {
	            	            	                trace_puts("ReceiverQueue full. Packet lost.");
	            	            	                free(packet1);
	            	            	            } else {
	            	            	                trace_printf("Switch: Packet #%u forwarded from %u to %u \n" ,packet1->sequence,packet1->sender,packet1->destination);
	            	            	            }

	            	            }
	            	            else{
	            	           	 if (xQueueSend(Receiver4Queue, &packet1, portMAX_DELAY) != pdPASS) {
	            	           	                trace_puts("ReceiverQueue full. Packet lost.");
	            	           	                free(packet1);
	            	           	            } else {
	            	           	                trace_printf("Switch: Packet #%u forwarded from %u to %u \n" ,packet1->sequence,packet1->sender,packet1->destination);
	            	           	            }
	            	            }


	            }



}

void packet2handler(Packet_t* packet2){

	 if(packet2->sender!=0){
	            	if(packet2->destination == 3+((rand() % 100) == 0 ? 1 : 0)){
	            	                       	 if (xQueueSend(Receiver3Queue, &packet2, portMAX_DELAY) != pdPASS) {
	            	                       	                trace_puts("ReceiverQueue full. Packet lost.");
	            	                       	                free(packet2);
	            	                       	            } else {
	            	                       	                trace_printf("Switch: Packet #%u forwarded from %u to %u \n" ,packet2->sequence,packet2->sender,packet2->destination);
	            	                       	            }

	            	                       }
	            	                       else{
	            	                      	 if (xQueueSend(Receiver4Queue, &packet2, portMAX_DELAY) != pdPASS) {
	            	                      	                trace_puts("ReceiverQueue full. Packet lost.");
	            	                      	                free(packet2);
	            	                      	            } else {
	            	                      	                trace_printf("Switch: Packet #%u forwarded from %u to %u \n" ,packet2->sequence,packet2->sender,packet2->destination);
	            	                      	            }
	            	                       }

	            }
}

void SwitchTask(void *parameters) {
    Packet_t* packet1;
    Packet_t* packet2;

    for (;;) {
        // Wait for a packet from SwitchQueue
    	 if (xQueueReceive(SwitchQueue1, &packet1, portMAX_DELAY) == pdPASS ) {
    	            if ((rand() % 100) < 1 && packet1!=NULL) {  // 1% drop
    	                trace_printf("Switch: dropped packet #%u from %u\n", packet1->sequence, packet1->sender);
    	                free(packet1);
    	            }

    	    }


    	            if(xQueueReceive(SwitchQueue2, &packet2, portMAX_DELAY) == pdPASS){

    	            	  if ((rand() % 100) < 1 && packet2!=NULL) {  // 1% drop
    	            	    	                trace_printf("Switch: dropped packet #%u from %u\n", packet2->sequence, packet2->sender);
    	            	    	                free(packet2);
    	            	    	            }
    	            }

    	            vTaskDelay(pdMS_TO_TICKS(200)); // Delay for 200ms
    	               	if(packet1!=NULL){packet1handler(packet1);}
    	               if(packet2!=NULL){packet2handler(packet2);}

        }
    }


void Receiver3Task(void* pvParameters)
{
     Packet_t temp;
     Packet_t *LastreceivedPacket = &temp;
     Packet_t *RecentreceivedPacket;
     uint16_t total_received_from_1 = 0;
     uint16_t total_received_from_2 = 0;
     uint16_t total_lost_from_1 = 0;
     uint16_t total_lost_from_2 = 0;

     LastreceivedPacket->sequence = 0;

    for (;;)
    {
        if (xQueueReceive(Receiver3Queue, &RecentreceivedPacket, portMAX_DELAY) == pdPASS)
        {
            char msg[100];
            if (RecentreceivedPacket->destination == 3) {
                sprintf(msg, "Receiver3: Got packet with dest. %u from Sender %d with Seq #%d",
                        RecentreceivedPacket->destination,
                        RecentreceivedPacket->sender,
                        RecentreceivedPacket->sequence);
                trace_puts(msg);
            } else {
                sprintf(msg, "_Error_ Receiver3: Got packet with dest. %u from Sender %d with Seq #%d",
                        RecentreceivedPacket->destination,
                        RecentreceivedPacket->sender,
                        RecentreceivedPacket->sequence);
                trace_puts(msg);
                continue;
            }

            if (RecentreceivedPacket->sender == 1) {
                total_received_from_1++;
                total_lost_from_1 = RecentreceivedPacket->sequence - LastreceivedPacket->sequence;
            } else {
                total_received_from_2++;
                total_lost_from_2 = RecentreceivedPacket->sequence - LastreceivedPacket->sequence;
            }

            LastreceivedPacket = RecentreceivedPacket;
            free(RecentreceivedPacket);
        }
    }
}


void Receiver4Task(void* pvParameters)
{
     Packet_t temp;
     Packet_t *LastreceivedPacket = &temp;
     Packet_t *RecentreceivedPacket;
     uint16_t total_received_from_1 = 0;
     uint16_t total_received_from_2 = 0;
     uint16_t total_lost_from_1 = 0;
     uint16_t total_lost_from_2 = 0;

     LastreceivedPacket->sequence = 0;

    for (;;)
    {
        if (xQueueReceive(Receiver4Queue, &RecentreceivedPacket, portMAX_DELAY) == pdPASS)
        {
            char msg[100];
            if (RecentreceivedPacket->destination == 4) {
                sprintf(msg, "Receiver4: Got packet with dest. %u from Sender %d with Seq #%d",
                        RecentreceivedPacket->destination,
                        RecentreceivedPacket->sender,
                        RecentreceivedPacket->sequence);
                trace_puts(msg);
            } else {
                sprintf(msg, "_Error_ Receiver4: Got packet with dest. %u from Sender %d with Seq #%d",
                        RecentreceivedPacket->destination,
                        RecentreceivedPacket->sender,
                        RecentreceivedPacket->sequence);
                trace_puts(msg);
                continue;
            }

            if (RecentreceivedPacket->sender == 1) {
                total_received_from_1++;
                total_lost_from_1 = RecentreceivedPacket->sequence - LastreceivedPacket->sequence;
            } else {
                total_received_from_2++;
                total_lost_from_2 = RecentreceivedPacket->sequence - LastreceivedPacket->sequence;
            }

            LastreceivedPacket = RecentreceivedPacket;
            free(RecentreceivedPacket);
        }
    }
}




/**         main()       *******************************************************/

void main(int argc, char* argv[]){

	SwitchQueue1 = xQueueCreate(10, sizeof(Packet_t*));
	SwitchQueue2 = xQueueCreate(10, sizeof(Packet_t*));
	Receiver3Queue = xQueueCreate(10, sizeof(Packet_t*));
	Receiver4Queue = xQueueCreate(10, sizeof(Packet_t*));

	if (SwitchQueue1 == NULL) {
	    trace_puts("Failed to create SwitchQueue1");
	}
	if (SwitchQueue2 == NULL) {
		    trace_puts("Failed to create SwitchQueue2");
		}
	if (Receiver3Queue == NULL) {
		    trace_puts("Failed to create Receiver1Queue");
		}
	if (Receiver4Queue == NULL) {
			    trace_puts("Failed to create Receiver2Queue");
			}


	xTaskCreate(Sender1Task, "Sender1", 1024, NULL, 1, NULL);
	xTaskCreate(Sender2Task, "Sender2", 1024, NULL, 1, NULL);
	xTaskCreate(SwitchTask, "Switch", 1024, NULL, 2, NULL);
	xTaskCreate(Receiver3Task, "Receiver3", 1024, NULL, 1, NULL);
	xTaskCreate(Receiver4Task, "Receiver4", 1024, NULL, 1, NULL);



	vTaskStartScheduler();


	 for (;;);

}





/**       end of  main()       *******************************************************/



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
