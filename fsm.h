/* Finite State Machine Implementation
   Emitting mouse events to the windows event queue

   Idris Soule
*/

#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <windows.h>
#include <pthread.h>

#pragma warning(disable:4716) //disable missing return from function error 

#define MAX_QUEUE_ENTRY   256
#define NUM_STATES         4

extern POINT cursor;

typedef enum {INITIAL = 0x0A, TIMING, ACCEPTING ,FINAL} stateType_t;
typedef enum {LEFT = 0x01, RIGHT, ZOOM, TRACK, QUIT, NOP} stateEvent_t ;
typedef enum {TIMER_EXPIRED, TIMER_ALIVE, TIMER_RESET} timerState_t;

typedef struct FSMState_t {
	stateType_t   stateType;
    stateEvent_t  sEvent, prevEvent;
    void * (*emit)(void *);
	/* payload for mouse events */
	INPUT mouseEvents[5];
	int numEvents;
}FSMState_t;

static FSMState_t currState; 
static timerState_t clickTimer; //timer between successive clicks

static pthread_mutex_t eventLock = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER;
static pthread_mutex_t stateLock = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER;
static pthread_mutex_t timerLock = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER;
static pthread_mutex_t queueLock = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER;

static pthread_t fsmThread;
static pthread_cond_t timerCond = PTHREAD_COND_INITIALIZER;

/* Bounded buffer for detector to emit posture */
static stateEvent_t fsm_event_queue[MAX_QUEUE_ENTRY];
static int *queueHeadr, *queueTail;



/* timer_thread:
	One-shot timer which allows the machine to distinguish between
	a single-click and a double-click, 475 ms is the interval time to re-click
*/
static void * timer_thread(void *arg)
{
	struct timespec tm;
	tm.tv_sec = (long)time(0) + 2L;
	tm.tv_nsec =  0; //475 ms

	/*
		Single Click constitutes: Timer Expired => Timer beats user to the punch 
		Double Click constitutes: Timer Reset   => User beats timer to the punch
	*/
	pthread_mutex_lock(&timerLock);
	clickTimer = TIMER_ALIVE;
	int rt = 0;

    //TODO: prevent spurious wake ups (small amount of jitter)
	rt = pthread_cond_timedwait(&timerCond, &timerLock, &tm);

	/* condition not signalled in correct amount of time */
	if(rt == ETIMEDOUT) //mutex has been re-acquired 
		clickTimer = TIMER_EXPIRED;
	else
		clickTimer = TIMER_RESET ;//forced reset t < 475 ms

	pthread_mutex_unlock(&timerLock);  
	pthread_exit(NULL);
}

/* functions for triggered events */

static void * event_left_click(void *payload)
{
	stateEvent_t eventPrev;
	pthread_t timerThread;

	/* Three Cases 
	   1. Starting a timer (single click or double ?)
	   2. Setting up payload for event_track to fire
	   3. Drag event
   */

	pthread_mutex_lock(&stateLock);
	eventPrev = currState.prevEvent;
	pthread_mutex_unlock(&stateLock);

	switch(eventPrev){ 
		case TRACK:
			/* assume single-click */
			if(clickTimer == TIMER_RESET){ //start the timer thread
				pthread_create(&timerThread, NULL, timer_thread, NULL);
			}
			else if(clickTimer == TIMER_ALIVE){ //set double click payload + kill timer
				pthread_cond_signal(&timerCond);
				pthread_mutex_lock(&stateLock);
				
				static DWORD mouseEvents[4] ={MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP,
											  MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP};
				ZeroMemory(currState.mouseEvents, sizeof(currState.mouseEvents) * 5);
				currState.numEvents = 4;
				for(int i = 0; i < currState.numEvents; i++){
					currState.mouseEvents[i].type = INPUT_MOUSE;
					currState.mouseEvents[i].mi.dwFlags = mouseEvents[i];
				}
				pthread_mutex_unlock(&stateLock);
				clickTimer = TIMER_RESET;
			}
			else { //clickTimer == TIMER_EXPIRED ... single click payload
				
				pthread_mutex_lock(&stateLock);
				ZeroMemory(currState.mouseEvents, sizeof(currState.mouseEvents) * 5);
				currState.mouseEvents[0].type = INPUT_MOUSE;
				currState.mouseEvents[0].mi.dwFlags   = MOUSEEVENTF_LEFTDOWN;
				currState.mouseEvents[1].type = INPUT_MOUSE;
				currState.mouseEvents[1].mi.dwFlags   = MOUSEEVENTF_LEFTUP;
				currState.numEvents = 2;
				pthread_mutex_unlock(&stateLock);
				clickTimer = TIMER_RESET; //mutex not needed
			}
		break;

		case LEFT: 
			//Kalman Filter(..) drag event
		break;

	}
	pthread_mutex_lock(&stateLock);
	currState.prevEvent = LEFT;
	pthread_mutex_unlock(&stateLock);
	pthread_exit(NULL);
}


static void * event_right_click(void *payload)
{
	stateEvent_t eventPrev;

	pthread_mutex_lock(&stateLock);
	eventPrev = currState.prevEvent;
	pthread_mutex_unlock(&stateLock);

	/* only care about Track, Left previous state don't care about RIGHT*/
	switch(eventPrev){
		case TRACK:
			/* setup RIGHT single-click payload */
			pthread_mutex_lock(&stateLock);
			ZeroMemory(currState.mouseEvents, sizeof(currState.mouseEvents) * 5);
			currState.mouseEvents[0].type = INPUT_MOUSE;
			currState.mouseEvents[0].mi.dwFlags   = MOUSEEVENTF_RIGHTDOWN;
			currState.mouseEvents[1].type = INPUT_MOUSE;
			currState.mouseEvents[1].mi.dwFlags   = MOUSEEVENTF_RIGHTUP;
			currState.numEvents = 2;
			pthread_mutex_unlock(&stateLock);
		break;

		case LEFT:
			/* Special case T L R T 
			Needs to set two payloads
			*/
			pthread_mutex_lock(&stateLock);
			static DWORD mouseEvents[4] ={MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP,
										 MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP};
			ZeroMemory(currState.mouseEvents, sizeof(currState.mouseEvents) * 5);
			currState.numEvents = 4;
			for(int i = 0; i < currState.numEvents; i++){
				currState.mouseEvents[i].type = INPUT_MOUSE;
				currState.mouseEvents[i].mi.dwFlags = mouseEvents[i];
			}
			pthread_mutex_unlock(&stateLock);
		break;
	
	}

	pthread_mutex_lock(&stateLock);
	currState.prevEvent = RIGHT;
	pthread_mutex_unlock(&stateLock);
	pthread_exit(NULL);
}

static void * event_zoom(void *payload)
{
	
	
	
	
	pthread_mutex_lock(&stateLock);
	currState.prevEvent = ZOOM;
	pthread_mutex_unlock(&stateLock);
    pthread_exit(NULL);
}

/* event_track:
	The initial position, final and tracking (x,y-coord) state
	Takes care of executing other events based on the states from the queue
	State information is read to determine previous state.
	Based on state respective thread is fired and mouse event in that thread is fired.
*/
static void * event_track(void *arg)
{
	
	stateEvent_t eventPrev;
	/* Two cases to worry about 
	   1. Moving the cursor
	   2. Firing an event from the previous state (L, R, Z)
	*/

	pthread_mutex_lock(&stateLock);
	eventPrev = currState.prevEvent;
	pthread_mutex_unlock(&stateLock);

	switch(eventPrev){
		case TRACK:
		/* get current cursor position (kalman stuff) and setcursorpos */
		SetCursorPos((cursor.x - 40 + 1)*8.95, (cursor.y - 63 + 1)*7.7);	
		break;

		case LEFT:
			while(clickTimer == TIMER_ALIVE)
				; //wait at most 475ms
			/* now either single/double click is ready */
			pthread_mutex_lock(&stateLock);
			SendInput(currState.numEvents, currState.mouseEvents, sizeof(INPUT));
			pthread_mutex_unlock(&stateLock);
		break;

		case RIGHT:
			/* Fire right click or left-right click */
			pthread_mutex_lock(&stateLock);
			UINT sendresult;
			sendresult = SendInput(currState.numEvents, currState.mouseEvents, sizeof(INPUT));
			assert(sendresult == currState.numEvents);
			pthread_mutex_unlock(&stateLock);
		break;

		case ZOOM:
		/* fire zoom payload */
		break;
		default: // internal error
		break;
	}
	
	pthread_mutex_lock(&stateLock);
	currState.prevEvent = TRACK;
	pthread_mutex_unlock(&stateLock);
    pthread_exit(NULL);
}

/*
fsm_queue_emit:
    The given camera thread will call this function
    to emit the detected posture. Function shall write to 
    the non-full queue.

    @id: one of the enumerated states {RIGHT, ZOOM ...}
    @return: notification if the state could be written to the queue
    
 */
bool fsm_queue_emit(stateEvent_t id)
{
    if(queueTail >= queueHeadr + MAX_QUEUE_ENTRY)
        return false; //full queue

	pthread_mutex_lock(&queueLock);
    *queueTail = id;
    queueTail++; //one beyond the latest entry
	pthread_mutex_unlock(&queueLock);
    return true;
}

/*
fsm_queue_consume:
    Consumes the states from the non-empty buffer

    @return: event ( of posture)
*/
static stateEvent_t fsm_queue_consume(void)
{
    static int i;
    stateEvent_t state;
    if(queueHeadr == queueTail){ //empty
        i = 0;
        return NOP; //triggers default in fsm_execute
    }

	assert(queueHeadr == (int *)&fsm_event_queue);
    state = (stateEvent_t)queueHeadr[i++];
	if(queueHeadr + i == queueTail){
		pthread_mutex_lock(&queueLock);
        queueTail = queueHeadr;
		i = 0;
		pthread_mutex_unlock(&queueLock);
	}
    return state;
}

/* 
fsm_execute: 
    The State Machine
    Once an event is consumed from the queue it is classified.
    State information is attributed to the event and a thread is created to execute
    its functionality

    Fine grain granularity is emplored with the locks to keep the event-loop as live as possible.
*/


static void fsm_execute(void)
{
    stateEvent_t sid;
    pthread_t *trackThread;
    pthread_t eventThread[NUM_STATES];

    while(sid = fsm_queue_consume())
    {
        switch(sid){
            case LEFT:
            pthread_mutex_lock(&stateLock);
            currState.emit = event_left_click;
            currState.stateType = TIMING;
            currState.sEvent = LEFT;
            pthread_mutex_unlock(&stateLock);
            trackThread = &eventThread[0];
            break;

            case RIGHT:
            pthread_mutex_lock(&stateLock);
            currState.emit = event_right_click;
            currState.stateType = ACCEPTING;
            currState.sEvent = RIGHT; 
            pthread_mutex_unlock(&stateLock);
            trackThread = &eventThread[1];
            break;

            case ZOOM:
            pthread_mutex_lock(&stateLock);
            currState.emit = event_zoom;
            currState.stateType = ACCEPTING;
            currState.sEvent = ZOOM;
            pthread_mutex_unlock(&stateLock);
            trackThread = &eventThread[2];
            break;

            case TRACK:
            pthread_mutex_lock(&stateLock);
            currState.emit = event_track;
            currState.stateType = INITIAL;
            currState.sEvent = TRACK;
            pthread_mutex_unlock(&stateLock);
            trackThread = &eventThread[3];
            break;

            case QUIT:
				pthread_cancel(*trackThread);
				pthread_cond_destroy(&timerCond);
				pthread_mutex_destroy(&eventLock);
				pthread_mutex_destroy(&stateLock);
				pthread_mutex_destroy(&timerLock);
				pthread_mutex_destroy(&queueLock);
				pthread_exit(NULL);
            break;

            /* Illegal event/No event (empty buffer)*/
            default:
            continue; //ignore it
            break;
        }
        /* Launch Track thread */
		/* NOTE:: Possibility to pass blob analysis as parameter to thread */
        if(pthread_create(trackThread, NULL, currState.emit, (void *)sid)){
			perror("fsm_execute:: couldn't create thread!");
			pthread_exit(NULL);
		}
		//pthread_join(*trackThread, NULL);
    }
}

/* 
fsm_initialize:
    Initializes the state machine.
    Caller will put the machine in a given state
    Once called, threads are created for fsm_execute

    @initial: initial state of FSM
    @return: status of initialization
*/

bool fsm_initialize(stateEvent_t initial)
{
    currState.prevEvent = TRACK;
    currState.sEvent = TRACK;
    currState.stateType = INITIAL;
    currState.emit = event_track;
	queueHeadr = queueTail = (int *)&fsm_event_queue;

	clickTimer = TIMER_RESET;
	SetDoubleClickTime(2000);

    const char *fsmErr = "FSM::%s: Couldn't create %s-thread!";
    if(pthread_create(&fsmThread, NULL, 
					 (void * (*)(void *))fsm_execute, 
					 NULL)){
        printf(fsmErr, __FUNCTION__, "fsm");
        return false;
    }

return true;
}
