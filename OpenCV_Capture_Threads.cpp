// TT_TEST.cpp : Test file to play with various API calls
// Initially ignoring so deemed errors
/* adding code to interafce with opencv + pthreads-win32
   Idris Soule, Michael Pang
*/

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <errno.h>
#include <highgui.h>
#include <pthread.h>
#include <signal.h>


#include "ocv.h"
#include "NPTrackingTools.h"

#define W 380
#define H 300
#define KEY_ESC 27
#define KEY_NOTPRESSED 0
#define MAX_NUM_CAMERAS 3

#pragma warning(disable:4716) //disable missing return from function error 

int key = KEY_NOTPRESSED;
pthread_mutex_t keyMutex;

typedef struct {
	unsigned int i;
	IplImage *displayImage;
}CameraData_t;


/* thread to execute display of camera frames */
void *showCameraWindow(void *arg)
{
	CameraData_t *myCam = (CameraData_t *)arg; 
	const char *windowName = TT_CameraName(myCam->i);

	cvNamedWindow(windowName,CV_WINDOW_AUTOSIZE);

	for( ;key != KEY_ESC; ){
		TT_CameraFrameBuffer(myCam->i, W, H, 0, 8, (unsigned char *)myCam->displayImage->imageData);
		cvShowImage(windowName, myCam->displayImage);
		pthread_mutex_lock(&keyMutex);
		key = cvWaitKey(15); //delay atleast n ms for TT firmware propogation delay
		pthread_mutex_unlock(&keyMutex);
	}
	cvReleaseImage(&myCam->displayImage);
	pthread_exit(NULL);
}

		
int main()
{
	TT_Initialize(); //setup TT cameras
	printf("Opening Calibration: %s\n", 
		TT_LoadCalibration("CalibrationResult 2010-10-20 9.36pm.cal") == NPRESULT_SUCCESS ?
		"PASS" : "ERROR");

	CameraData_t cameras[MAX_NUM_CAMERAS];
	pthread_t threads[MAX_NUM_CAMERAS - 1]; 
	
	TT_SetCameraSettings(1, NPVIDEOTYPE_GRAYSCALE,300, 150, 15);
	TT_SetCameraSettings(2, NPVIDEOTYPE_PRECISION,300, 150, 15);
	TT_SetCameraSettings(3, NPVIDEOTYPE_GRAYSCALE,300, 150, 15);
	/* 1. Change camera settings ^
	   2. Allocate space for the displays 
	*/
	for(int i = 1; i < TT_CameraCount(); i++){
		cameras[i].i = i;
		cameras[i].displayImage = cvCreateImage(cvSize(W,H), IPL_DEPTH_8U, 1);
	}

	/* call the threads for display of camera data */
	
	pthread_mutex_init(&keyMutex, NULL);

	for(int i = 0; i < TT_CameraCount()-1; i++){
		if(pthread_create(&threads[i], NULL, showCameraWindow, (void*)&cameras[i+1])){
			printf("\aThread couldn't be created!");
			TT_Shutdown();
			TT_FinalCleanup();
			exit(-1);
		}
	}
	
	printf("Press any Key to Exit!\n"); 
	while(!_kbhit()){
		int result = TT_Update();
		if(result != NPRESULT_SUCCESS)
			Sleep(10UL); //wait for updated frame 1/sleeptime[ms] = frame-rate
	}

	for(int i = 0; i < MAX_NUM_CAMERAS-1; i++)
		pthread_join(threads[i], NULL);
	
	pthread_mutex_destroy(&keyMutex);
	cvDestroyAllWindows();
	TT_Shutdown();
	TT_FinalCleanup();
	return 0;
}