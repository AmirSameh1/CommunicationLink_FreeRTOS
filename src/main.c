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
#include <stdbool.h>


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


#define False			( ( BaseType_t ) 0 )
#define True			( ( BaseType_t ) 1 )

#define T1 					( (uint8_t) 100 )
#define T2 					( (uint8_t) 200 )
#define Tout 				( pdMS_TO_TICKS(150) )      // tp be replaced {150, 175, 200, 225} msec
#define Pdrop 				( (double)0.01 )            // to be replaced {0.01, 0.02, 0.04, 0.08}
#define P_ack 				( (double)0.01 )
#define P_WRONG_PACKET		( (double)0.0 )
//#define Tdelay				( pdMS_TO_TICKS(200) )
#define D					( pdMS_TO_TICKS(5) )         // Propagation delay constant  5 msec
#define Resend				( (uint8_t) 4)
#define NUM_INPUT_QUEUES    ( (uint8_t) 4)

static const uint32_t	L1 = 500;
static const uint32_t	L2 = 1500;
static const uint8_t	K = 40;                         // ack backet size
static const uint32_t	C = 100000;                     // link capacity (bits/sec)
static const uint8_t	N = 1;                          // Buffer Queue size {1 , 2 , 4 , 8 , 16}


/** End of Macros *************************************************************/

QueueHandle_t Sender1_GenQueue;
QueueHandle_t Sender1_BufferQueue;
QueueHandle_t Sender1_AckQueue;

QueueHandle_t Sender2_GenQueue;
QueueHandle_t Sender2_BufferQueue;
QueueHandle_t Sender2_AckQueue;

QueueHandle_t SwitchQueue[4];


QueueHandle_t Receiver3Queue;
QueueHandle_t Receiver4Queue;

//QueueSetHandle_t xSwitchQueueSet;  // very very very special  -- note : it didn't work at all it gives 'undefined reference error'

TimerHandle_t sender1Timer;
TimerHandle_t sender2Timer;
TimerHandle_t sender1Tout;
TimerHandle_t sender2Tout;


SemaphoreHandle_t xGenMutex;

SemaphoreHandle_t sender1_BlockSemaphore;
SemaphoreHandle_t sender2_BlockSemaphore;


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


typedef struct{

QueueHandle_t GenQueue;
QueueHandle_t BufferQueue;
QueueHandle_t ACKQueue;
TimerHandle_t ToutTimer;
TimerHandle_t SendTimer;
SemaphoreHandle_t BlockSemaphore;
uint32_t seq;
uint8_t NodeNum;
bool GenStarted;

}Node;


static Node Sender1;
static Node Sender2;
static Node Reciever1;
static Node Reciever2;


Packet_Manager(QueueHandle_t Queue , uint16_t length , uint32_t seq , uint8_t src ,uint8_t dest){

    Packet_t *packet = (Packet_t *)malloc(sizeof(Packet_t) + length - 8);
    if (!packet) {
        trace_printf("Sender%u: malloc failed",src);
        return;
    }

    packet->header.destination = RandVal(3,4);
    packet->header.sender = src;
    packet->header.sequence = seq;
    packet->header.length = length;
    memset(packet->data, 0x0, length - 8);
    snprintf(packet->data, length - 8, "Hello from Sender1 #%u", packet->header.sequence);

    if (xQueueSend(Queue, &packet, portMAX_DELAY) == pdPASS) {
        trace_printf("Sender%u: sent packet #%u to %u (in generation queue)\n", packet->header.sender ,packet->header.sequence, packet->header.destination);
    } else {
        trace_puts("Sender%u: failed to send");
        free(packet);
    }
}


static void SenderTimerCallback(TimerHandle_t xTimer) {

	 Node* node = (Node*) pvTimerGetTimerID(xTimer);

 if (xSemaphoreTake(xGenMutex, 0) == pdTRUE) {
     //trace_printf("node %d took semaphore \n" ,node->NodeNum);
     uint8_t dest = (uint8_t) RandVal(3,4);
     uint16_t length = (uint16_t) RandVal(L1, L2);

     Packet_Manager(node->GenQueue,length,node->seq ,node->NodeNum ,dest);
        node->seq++;

     //trace_printf("node %d gived semaphore \n" ,node->NodeNum);
     node->GenStarted =True;
     xSemaphoreGive(xGenMutex);

    }
    else {
        trace_printf("node %d couldnt take semaphore \n" ,node->NodeNum);
    }



    // Restart timer
    int nextInterval = RandVal(T1, T2);
    xTimerChangePeriod(xTimer, pdMS_TO_TICKS(nextInterval), 0);
}

static void Sender1ToutCallback(TimerHandle_t xTimer){
    // Node* node = (Node*) pvTimerGetTimerID(xTimer);

    trace_printf("sender1:Tout is up! task unblocked\n");
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // Give semaphore from ISR to unblock the task
    xSemaphoreGiveFromISR(sender1_BlockSemaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

}

static void Sender2ToutCallback(TimerHandle_t xTimer){
    //Node* node = (Node*) pvTimerGetTimerID(xTimer);
 trace_printf("sender2:Tout is up! task unblocked\n");
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // Give semaphore from ISR to unblock the task
    xSemaphoreGiveFromISR(sender2_BlockSemaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

}


void SenderTask(void *pvParameters) {

    Node* node = (Node*) pvParameters;
     UBaseType_t count ;
     UBaseType_t bufferFree;
    UBaseType_t ACKsRecived;
    uint8_t UnAckedPackets;

    trace_printf("Node %d: task started\n" , node->NodeNum);



    if(node->NodeNum == 1){
        node->SendTimer = xTimerCreate("Sender1Timer", pdMS_TO_TICKS(RandVal(T1, T2)), pdFALSE, (void*) node, SenderTimerCallback);
        node->ToutTimer = xTimerCreate("Sender1ToutTimer", Tout, pdFALSE, (void*) node, Sender1ToutCallback);
    }
    else{
        node->SendTimer = xTimerCreate("Sender2Timer", pdMS_TO_TICKS(RandVal(T1, T2)), pdFALSE, (void*) node, SenderTimerCallback);
         node->ToutTimer = xTimerCreate("Sender2ToutTimer", Tout, pdFALSE, (void*) node, Sender2ToutCallback);
    }


     if (node->SendTimer != NULL) {
            xTimerStart(node->SendTimer, 0);
        }


    while(1){

        // now we must check if the Gen_Queue has at least N elements so we send them to the  buffer queue
         count = uxQueueMessagesWaiting(node->GenQueue);
         bufferFree = uxQueueSpacesAvailable(node->BufferQueue);



        // we must start the Tout timer for the backets in buffer queue
        // it must be one shot each time the buffer queue is full

        if (count >= N && bufferFree >=N) { // we here move N packets from gen to buffer queue


            for (uint8_t i = 0; i < N; i++)
            {
                Packet_t* pkt;

                // Receive from generation queue (remove from gen queue)
                if (xQueueReceive(node->GenQueue, &pkt, 0) == pdTRUE) {
                    // Send to buffer queue (add to buffer queue)
                    if (xQueueSend(node->BufferQueue, &pkt, 0) != pdTRUE) {
                        // Failed to send to buffer queue, handle error (e.g., re-enqueue in gen queue or discard)
                        trace_printf("Error: Buffer queue full\n");
                    }
                }
                else
                {
                    // Failed to receive packet from gen queue (shouldn't happen here)
                    trace_printf("Error: Gen queue empty\n");
                }

            }

        }

         bufferFree = uxQueueSpacesAvailable(node->BufferQueue);

        for (uint8_t i = 0; (bufferFree < N && i < Resend); i++) {

            UnAckedPackets = N - bufferFree;

            //------------------  Next section is very very very critical --------------------//

            // This method temporarily removes packets, copies them, then restores them


                Packet_t* temp_packets[UnAckedPackets];  // Array to hold temporarily removed packets
                Packet_t* packet_copies[UnAckedPackets]; // Array for copies to send
                uint8_t packets_removed = 0;
                uint8_t copies_created = 0;

                // Step 1: Remove first N packets from BufferQueue and store them
                for (uint8_t j = 0; j < UnAckedPackets; j++) {
                    if (xQueueReceive(node->BufferQueue, &temp_packets[j], 0) == pdTRUE) {
                        packets_removed++;
                    } else {
                        // No more packets in buffer queue
                        trace_printf("Only %d packets available in buffer (wanted %d)\n", packets_removed, UnAckedPackets);
                        break;
                    }
                }

                // Step 2: Create copies and send to switch
                for (uint8_t j = 0; j < packets_removed; j++) {
                    // Create copy
                    packet_copies[j] = (Packet_t*)malloc(sizeof(Packet_t));
                    if (packet_copies[j] != NULL) {
                        memcpy(packet_copies[j], temp_packets[j], sizeof(Packet_t));

                        // Send copy to switch
                        if (xQueueSend(SwitchQueue[node->NodeNum-1], &packet_copies[j], 0) == pdTRUE) {
                            copies_created++;
                            trace_printf("Sender%u: Sent copy of packet %d to switch  try: %u\n", node->NodeNum, packet_copies[j]->header.sequence,i);
                        } else {
                            // Switch queue full - free the copy and stop
                            free(packet_copies[j]);
                            trace_printf("Switch queue full after sending %d copies\n", copies_created);
                            break;
                        }
                    } else {
                        trace_printf("Failed to allocate memory for packet copy %d\n", j);
                        break;
                    }
                }

                // Step 3: CRITICAL - Restore original packets back to BufferQueue (in correct order)
                for (uint8_t j = 0; j < packets_removed; j++) {
                    if (xQueueSend(node->BufferQueue, &temp_packets[j], 0) != pdTRUE) {
                        trace_printf("CRITICAL ERROR: Failed to restore packet %d to buffer queue!\n", j);
                        // This is a serious error - you've lost a packet
                    }
                }

                // if every thing goes as intended copies_created must equal packets_removed
               // trace_printf("Sender%u: Copied %d packets to switch, restored %d packets to buffer\n", node->NodeNum,
                          //  copies_created, packets_removed);



                if (node->ToutTimer != NULL) {
                    xTimerStart(node->ToutTimer, 0);
                }

                trace_printf("sender%u: started Tout counting! task blocked\n",node->NodeNum);
            if (xSemaphoreTake(node->BlockSemaphore, portMAX_DELAY) == pdTRUE){ // we wait for Tout to unblock the task

                ACKsRecived = N-uxQueueSpacesAvailable(node->ACKQueue);

                if(ACKsRecived ==0){
                     trace_printf("sender%u: no acks for me at all \n", node->NodeNum);
                }

             for (int j = 0; j < ACKsRecived; j++){
                Packet_t* pkt_ack;
                Packet_t* pkt_buff;

                if(xQueueReceive(node->ACKQueue, &pkt_ack, 0) == pdTRUE){

                    if(xQueuePeek(node->BufferQueue, &pkt_buff, 0) == pdTRUE){

                        if(pkt_ack->header.sequence == pkt_buff->header.sequence){

                             if(xQueueReceive(node->BufferQueue, &pkt_buff, 0)==pdTRUE){
                                trace_printf("sender%u: recieved ack and freed from buffer queue\n", node->NodeNum);
                                 //   UnAckedPackets--;// not needed
                                free(pkt_buff);
                                free(pkt_ack);
                             }

                        }
                        else{
                            trace_printf("sender%u:some acks are missing\n", node->NodeNum);
                            free(pkt_ack);
                            break;
                        }

                    }
                    else{
                        trace_printf("sender%u:peek fails\n", node->NodeNum);
                            free(pkt_ack);
                            break;
                    }
                }

            }

                bufferFree = uxQueueSpacesAvailable(node->BufferQueue);

            }



        }


        if(node->GenStarted){

                for (int j = 0; j < N; j++){
                    Packet_t* pkt_buff;
                    if(xQueueReceive(node->BufferQueue, &pkt_buff, 0)==pdTRUE){
                if(bufferFree >= N){
                    trace_printf("sender%u:all packets sent successfully, now freeing them\n", node->NodeNum);
                }
                else{
                    trace_printf("sender%u:not all packets were successfully sent, now freeing them\n", node->NodeNum);
                }

                                        free(pkt_buff);
                                        trace_printf("sender%u: POP!\n", node->NodeNum);
                    }
                 }

        }




    }

}







void packetHandler(TimerHandle_t xTimer) {
    Packet_t* pkt = (Packet_t*) pvTimerGetTimerID(xTimer);

    if (pkt == NULL) return;

    switch (pkt->header.destination) {  // fun fact :  we used a switch in a switch  ≧◡≦
        case 1:
            if(xQueueSend(Sender1_AckQueue, &pkt, 0)!=pdTRUE){
                trace_printf("switch: Failed to send packet to Sender1_AckQueue\n");
            }else{
                trace_printf("switch: packet sent to 1\n");
            }
            break;
        case 2:
            if(xQueueSend(Sender2_AckQueue, &pkt, 0)!=pdTRUE){
                trace_printf("switch: Failed to send packet to Sender2_AckQueue\n");
            }else{
                trace_printf("switch: packet sent to 2\n");
            }
            break;
        case 3:
            if(xQueueSend(Receiver3Queue, &pkt, 0)!=pdTRUE){
                trace_printf("switch: Failed to send packet to Receiver3Queue\n");
            }else{
                trace_printf("switch: packet sent to 3\n");
            }
            break;
        case 4:
            if(xQueueSend(Receiver4Queue, &pkt, 0)!=pdTRUE){
                trace_printf("switch: Failed to send packet to Receiver4Queue\n");
            }else{
                trace_printf("switch: packet sent to 4\n");
            }
            break;
        default:
            trace_printf("Unknown destination: %u\n", pkt->header.destination);
            break;
    }

    free(pkt);
    xTimerDelete(xTimer, 0);  // Clean up the timer
}

void SwitchTask(void *parameters) {

    trace_printf("Switch: started task \n");

    //QueueSetMemberHandle_t activeQueue;
    Packet_t *pkt;

    while (1) {

        //activeQueue = xQueueSelectFromSet(xSwitchQueueSet, portMAX_DELAY);

        for (int i = 0; i < NUM_INPUT_QUEUES; i++){  // for sorry we use polling as queue sets are not defined  "undefined reference error"

            if (uxQueueMessagesWaiting(SwitchQueue[i]) > 0){

                if (xQueueReceive(SwitchQueue[i], &pkt, 0) == pdTRUE){
                    trace_printf("Switch: received packet from %u to %u\n", pkt->header.sender, pkt->header.destination);

                     TimerHandle_t delayTimer = xTimerCreate("packetHandler",pdMS_TO_TICKS(100),pdFALSE,(void*) pkt,packetHandler);

                      if (delayTimer != NULL) {
                     xTimerStart(delayTimer, 0);
                     } else {
                       trace_printf("Failed to create timer for packet\n");
                    }
                }

            }

          //  vTaskDelay(pdMS_TO_TICKS(50));  //  to allow lower priority tasks to breath

        }

    }


    }



void ReceiverTask(void* pvParameters) {

    Node* node = (Node*) pvParameters;
    trace_printf("Node %d: task started \n" , node->NodeNum);

    Packet_t * pkt;
    Packet_t * ack_pkt;
    // Packet_t temp;
    // Packet_t *LastPacket = &temp;
    // Packet_t *CurrentPacket;
    // uint16_t total_received_1 = 0, total_lost_1 = 0;
    // uint16_t total_received_2 = 0, total_lost_2 = 0;
    // LastPacket->header.sequence = 0;

    while(1) {

            if (node->BufferQueue == NULL){
            	trace_printf("Receiver%u: error\n", node->NodeNum);
            }

        if (uxQueueMessagesWaiting(node->BufferQueue) > 0){

            if (xQueueReceive(node->BufferQueue, &pkt, 0) == pdTRUE) {
            ack_pkt = (Packet_t *)malloc(sizeof(Packet_t) + K - 8);

            if (ack_pkt == NULL) {
             trace_printf("Receiver%u: malloc failed!\n", node->NodeNum);
              continue;
                }

            trace_printf("Receiver%u: Got packet from Sender %u with Seq #%u\n", node->NodeNum, pkt->header.sender, pkt->header.sequence);

            ack_pkt->header.sender = pkt->header.destination;
            ack_pkt->header.destination = pkt->header.sender;
            ack_pkt->header.length = K;
            ack_pkt->header.sequence = pkt->header.sequence;
            memset(ack_pkt->data, 0x0, ack_pkt->header.length - 8);
            snprintf(ack_pkt->data, ack_pkt->header.length - 8, "ACK #%u", ack_pkt->header.sequence);

            free(pkt);
            if (xQueueSend(SwitchQueue[node->NodeNum-1], &ack_pkt, 0) == pdTRUE){

                 trace_printf("Receiver%u: sent ack to  %u replay Seq #%u\n", node->NodeNum,  ack_pkt->header.destination , ack_pkt->header.sequence);
            }
            else{
                free(ack_pkt);
            }
            }

        }


            // if (CurrentPacket->header.sender == 1) {
            //     total_received_1++;
            //     total_lost_1 = CurrentPacket->header.sequence - LastPacket->header.sequence;
            // } else {
            //     total_received_2++;
            //     total_lost_2 = CurrentPacket->header.sequence - LastPacket->header.sequence;
            // }

            // LastPacket->header.sequence = CurrentPacket->header.sequence;
            // free(CurrentPacket);

                    vTaskDelay(pdMS_TO_TICKS(10));
        }

}



void main(int argc, char* argv[]) {
    // Queues
    Sender1_GenQueue = xQueueCreate(10, sizeof(Packet_t*));
    Sender1_BufferQueue = xQueueCreate(N, sizeof(Packet_t*));
    Sender1_AckQueue = xQueueCreate(N, sizeof(Packet_t*));

    Sender2_GenQueue = xQueueCreate(10, sizeof(Packet_t*));
    Sender2_BufferQueue = xQueueCreate(N, sizeof(Packet_t*));
    Sender2_AckQueue = xQueueCreate(N, sizeof(Packet_t*));

    SwitchQueue[0] = xQueueCreate(10, sizeof(Packet_t*));
    SwitchQueue[1] = xQueueCreate(10, sizeof(Packet_t*));
    SwitchQueue[2] = xQueueCreate(10, sizeof(Packet_t*));
    SwitchQueue[3] = xQueueCreate(10, sizeof(Packet_t*));
    Receiver3Queue = xQueueCreate(10, sizeof(Packet_t*));
    Receiver4Queue = xQueueCreate(10, sizeof(Packet_t*));

   // xSwitchQueueSet = xQueueCreateSet(10 * NUM_INPUT_QUEUES); // again  very very very special

     // we will use a queue set handeler to check at all queues at once in the switch
   // for (int i = 0; i < NUM_INPUT_QUEUES; i++) {
   //      xQueueAddToSet(SwitchQueue[i], xSwitchQueueSet);
   // }


    // Semaphore
    xGenMutex = xSemaphoreCreateMutex();
    sender1_BlockSemaphore =xSemaphoreCreateBinary();
    sender2_BlockSemaphore =xSemaphoreCreateBinary();


    if (xGenMutex != NULL &&
        sender1_BlockSemaphore != NULL &&
        sender2_BlockSemaphore != NULL &&
        Sender1_GenQueue != NULL &&
        Sender1_BufferQueue != NULL &&
        Sender1_AckQueue != NULL &&
        Sender2_GenQueue != NULL &&
        Sender2_BufferQueue != NULL &&
        Sender2_AckQueue != NULL &&
        SwitchQueue[0] != NULL &&
        SwitchQueue[1] != NULL &&
        SwitchQueue[2] != NULL &&
        SwitchQueue[3] != NULL &&
        Receiver3Queue != NULL &&
        Receiver4Queue != NULL)
    {
        trace_printf("Ready to go!\n\n\n");
    } else {
        trace_printf("Error creating queues or semaphore\n");
        return;
    }




    Sender1.GenQueue = Sender1_GenQueue;
    Sender1.BufferQueue = Sender1_BufferQueue;
    Sender1.ACKQueue = Sender1_AckQueue;
    Sender1.ToutTimer = NULL;
    Sender1.SendTimer = NULL;
    Sender1.NodeNum = 1;
    Sender1.BlockSemaphore =sender1_BlockSemaphore;
    Sender1.seq = 0;
    Sender1.GenStarted =False;

    Sender2.GenQueue = Sender2_GenQueue;
    Sender2.BufferQueue = Sender2_BufferQueue;
    Sender2.ACKQueue = Sender2_AckQueue;
    Sender2.ToutTimer = NULL;
    Sender2.SendTimer = NULL;
    Sender2.NodeNum = 2;
    Sender2.BlockSemaphore =sender2_BlockSemaphore;
    Sender2.seq = 0;
    Sender2.GenStarted =False;


    Reciever1.BufferQueue = Receiver3Queue;
    Reciever1.GenQueue = NULL;
    Reciever1.ACKQueue = NULL;
    Reciever1.ToutTimer = NULL;
    Reciever1.SendTimer = NULL;
    Reciever1.NodeNum = 3;
    Reciever1.BlockSemaphore =NULL;
    Reciever1.seq = NULL;
    Reciever1.GenStarted =NULL;

    Reciever2.BufferQueue = Receiver4Queue;
    Reciever2.GenQueue = NULL;
    Reciever2.ACKQueue = NULL;
    Reciever2.ToutTimer = NULL;
    Reciever2.SendTimer = NULL;
    Reciever2.NodeNum = 4;
    Reciever2.BlockSemaphore =NULL;
    Reciever2.seq = NULL;
    Reciever2.GenStarted =NULL;

    xTaskCreate(SenderTask, "Sender1", 256, (void*)&Sender1, 1, NULL);
    xTaskCreate(SenderTask, "Sender2", 256, (void*)&Sender2, 1, NULL);
    xTaskCreate(SwitchTask, "Switch", 256, NULL, 1, NULL);
    xTaskCreate(ReceiverTask, "Receiver3", 256, (void*)&Reciever1, 1, NULL);
    xTaskCreate(ReceiverTask, "Receiver4", 256, (void*)&Reciever2, 1, NULL);

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

