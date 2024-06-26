/**
 * @file procsim.c
 * @brief Program entry point.  Runs the process scheduler simulation
 *
 * Course: CSC3210
 * Section: 002
 * Assignment: Process Simulator
 * Name: Christian Basso
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct process {
    int pid;
    int priority;
    int state; // 0: Ready, 1: Running, 2: Blocked
    struct process *next; //Used for the queue. It is the next process in the queue
    int io; // device it is waiting on
    int readywait; //How long the process watis in the ready queue
    int iowait; //How long the process waits in the block queue
    int iostart; //when the process starts waiting for io
} Process;

typedef struct {
    Process *head;
    int size;
} Queue;

Queue readyQueue;  // Queue for ready processes
Queue blockQueue;  // Queue for io blocked processes
Queue finishedQueue; // Queue for finsihed process
Process *currentProcess = NULL; // Currently running process
int currentTime = 0;
int idleTime = 0; // for tracking idle time in the cpu
int preemptive; // flag to see if the system is preemptive or not
int nextPid = 1; // next process pid

// ------------------------------------QUEUE FUNCTIONS----------------------------------------
/**
 * These priority queue functions were generated by github co-pilot. I made some modifications to them as needed to fit my spesific needs.
*/

/**
 * Add something to queue
*/
void enqueue(Queue *q, Process *p) {
    Process **curr = &q->head;
    while (*curr && (*curr)->priority >= p->priority) {
        curr = &(*curr)->next;
    }
    p->next = *curr;
    *curr = p;
    q->size++;
}



/**
 * Remove something from queue and return it
*/
Process* dequeue(Queue *q) {
    if (q->head == NULL) return NULL;
    Process *temp = q->head;
    q->head = q->head->next;
    q->size--;
    return temp;
}


/**
 * Init queue
*/
void initializeQueue(Queue *q) {
    q->head = NULL;
    q->size = 0;
}

//-------------------These are queue function I wrote --------------------------------------


/**
 * loop trhough a queue and free all processes on it
*/
void freeQueue(Queue *q) {
    Process *current = q->head;
    while (current != NULL) {
        Process *temp = current;
        current = current->next;
        free(temp);
    }
    q->head = NULL;
    q->size = 0;
}

/**
 * return true if a process is on a queue
*/
int in_queue(Queue *q, Process *p) {
    Process *current = q->head;
    while (current != NULL) {
        printf("Searching PID %d", current->pid);
        if (current == p) {
            return 1;
        }
        current = current->next;
    }
    return 0; 
}

/**
 * Remove a process from a queue using the process's pid
*/

Process* removeProcessByPid(Queue *q, int pid) {
    Process *current = q->head;
    Process *prev = NULL;

    while (current != NULL) {
        if (current->pid == pid) {
            if (prev != NULL) {
                prev->next = current->next;
            } else {
                q->head = current->next;
            }
            q->size--;
            Process *removedProcess = current;
            
            return removedProcess;
        }
        prev = current;
        current = current->next;
    }

    return NULL; // Process not found in queue
}

//--------------------------------------FINISH QUEUE FUNCTIONS---------------------------------------------------------


/**
 * This function handles when a process is started
*/
void handle_process_start(int priority) {
    // Make space for the new process
    Process *newProcess = malloc(sizeof(Process));

    // assign process its features
    newProcess->pid = nextPid++;
    newProcess->priority = priority;
    newProcess->state = 0; // Ready state
    newProcess->readywait = 0;
    newProcess->iowait = 0;
    newProcess-> iostart = 0;

    // Process gets put on the ready queue once its added to the system
    enqueue(&readyQueue, newProcess);

    // Print out the new process
    printf("%d: Starting process with PID: %d PRIORITY: %d\n", currentTime, newProcess->pid, priority);
}

/**
 * Function to handle when IO starts for the process on the cpu. It takes in the device and the current time to log on the process
*/
void handle_io_request(int device, int current_time) {
    // Assign the process on the cpu its io device
    currentProcess -> io = device;
    // this is used to calculate total io wait time later
    currentProcess -> iostart = current_time;
    // If there is a current process
    if (currentProcess) {
        // Set the process state to blocked
        currentProcess->state = 2; 
        // add the current process to the block queue
        enqueue(&blockQueue, currentProcess);
        printf("%d: Process with PID: %d waiting for I/O device %d\n", currentTime, currentProcess->pid, device);
        //Take the process off the cpu
        currentProcess = NULL;
    }
}


/**
 * Function to handle when IO finsihs for a device. It takes in the device and the current time to log on the process
*/

void handle_io_end(int device, int current_time) {
    //Need to itterate through the block queue
    Process *p = blockQueue.head;
    Process *prev = NULL;
    Process *next = NULL;
    while (p != NULL) {
        //If found device 
        if (p->io == device) {
            // set io wait time on that process
            p->iowait = current_time -p->iostart;
            // if process is not the first element in queue...
            if (prev != NULL) {
                prev->next = p->next;
            //if the process is the first element in the queue... 
            } else {
                blockQueue.head = p->next; 
            }
            //set next process in the block queue to the temp 
            next = p->next;
            // Set the process state to ready
            p->state = 0; // Ready stat
            // Remove the process off the block queue
            removeProcessByPid(&blockQueue, p->pid);
            // Add the process to the ready queue
            enqueue(&readyQueue, p);
            p->next = next;
            // Increase idle time since the process isnt doing anything important
            idleTime++;
        }
        // If not found the device, go to next process in queue
        prev = p;
        p = p->next;
    }
    printf("%d: I/O completed for I/O device %d\n", currentTime, device);
}

/**
 * Function to hadle when a process ends
*/
void handle_process_end() {
    // make sure there is a process running on the cpu
    if (currentProcess) {
        //print
        printf("%d: Ending process with PID: %d\n", currentTime, currentProcess->pid);
        // Add process to the finished queue. This is how I print all results at the end.
        enqueue(&finishedQueue, currentProcess);
        // Since the process is not on any other queues when it is running, it can just be taken off the cpu.
        currentProcess = NULL;
    }
}

/**
 * Simulate the process scheduler for 1 unit time
*/
void simulate() {
    // Simulation loop should continue until all queues are empty and no current process
    // If nothing is on the cpu currently
    if (!currentProcess) { 
        // If there is something in the ready queue
            if (readyQueue.size > 0) {
                // Take the top priority process off the ready queue and put it on the cpu
                currentProcess = dequeue(&readyQueue);
                // set that process's state to running
                currentProcess->state = 1; 
                // print
                printf("%d: Process scheduled to run with PID: %d PRIORITY: %d\n", 
                currentTime, currentProcess->pid, currentProcess->priority);

            //If there is nothing on the ready queue and nothing on the cpu, increase idle time
            } else {
                idleTime++; 
            }

        // if there is a process running on the cpu
        } else {
            // Check for preemption
            // See if there is anything in the ready queue and if the top priority process is higher than the current process
            if (preemptive && readyQueue.head && readyQueue.head->priority > currentProcess->priority) {
                //print
                printf("%d: Process scheduled to run with PID: %d PRIORITY: %d\n",
                       currentTime, readyQueue.head->pid, readyQueue.head->priority);

                // this is just a second check to make sure the process is not in the block queue
                if (!in_queue(&blockQueue, currentProcess)) { 
                    //Add removed process back to the ready queue
                    enqueue(&readyQueue, currentProcess); 
                }
                // set the current process to the process with the highest priority
                currentProcess = dequeue(&readyQueue);
            }
        }

        //Everything that is in the ready queue during this time should increase its ready wait time by 1
        Process *t = readyQueue.head;
        while (t != NULL) {
            t->readywait++;
            t = t->next;
        }

        // Time passes by 1 time unit
        currentTime++;
}

/**
 * @brief Program entry procedure for the process scheduler simulation
 */
int main(int argc, char *argv[]) {

    // HAd to add this line to stop a warning in the make file
    (void)argc;

    FILE* file = fopen(argv[1], "r");
    if (!file) {
        perror("Failed to open input file");
        return 1;
    }

    // Read whether scheduling is preemptive
    fscanf(file, "%d", &preemptive); 
    if (preemptive) {
        printf("Simulation started: Preemption: True\n\n");
    } 
    else {
        printf("Simulation started: Preemption: False\n\n");
    }

    //Initialize queues
    initializeQueue(&readyQueue);
    initializeQueue(&blockQueue);

    int event_time, operation_code, priority, io_device;
    while (fscanf(file, "%d %d", &event_time, &operation_code) == 2) {
        // Update simulation time to event time
        currentTime = event_time; 
        switch(operation_code) {
            case 1: // Process start
                fscanf(file, "%d", &priority);
                handle_process_start(priority);
                simulate(); // Run the simulation
                break;
            case 2: // I/O request
                fscanf(file, "%d", &io_device);
                handle_io_request(io_device, event_time);
                simulate(); // Run the simulation
                break;
            case 3: // I/O end
                fscanf(file, "%d", &io_device);
                handle_io_end(io_device, event_time);
                simulate(); // Run the simulation
                break;
            case 4: // Process end
                handle_process_end();
                simulate(); // Run the simulation
                break;
        }
    }

    // Close the file
    fclose(file);
    // end simulation when while loop finished
    printf("\nSimulation ended at time: %d\nSystem idle time: %d\n", currentTime - 1, idleTime);

    printf("\nProcess Information: \n");
    //Loop through all elemtns in the finished queue
    Process *p = finishedQueue.head;
    while (p != NULL) {
        printf("PID: %d, PRIORITY: %d, READY WAIT TIME: %d, I/O WAIT TIME: %d\n", p->pid, p->priority, p->readywait, p->iowait);
        p = p->next;
    }
    //Free all process stucts
    freeQueue(&finishedQueue);

    return 0;
}