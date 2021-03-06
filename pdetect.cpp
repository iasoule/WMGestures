/* Posture detection using blob analysis 
   To the glory of Yeshua Ha'Mashiach
   Idris Soule, Michael Pang
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <math.h>
#include <float.h>

#include <errno.h>
#include <highgui.h>
#include <pthread.h>
#include <signal.h>

#include <windows.h>
#include "fsm.h"

#include "Blob.h"
#include "BlobResult.h"

#include "NPTrackingTools.h"

#if OCV_DEBUG 
#include "ocv.h"
#endif

#define W 200
#define H 200
#define KEY_ESC 27
#define KEY_NOTPRESSED 0
#define MAX_NUM_CAMERAS 3

#pragma warning(disable:4716) //disable missing return from function error 

int key = KEY_NOTPRESSED;
pthread_mutex_t keyMutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER;

typedef struct {
	unsigned int i;
	IplImage *displayImage;
}CameraData_t;


int sortLowHigh(const void * a, const void *b)
{
		return (int)(*(double*)a - *(double*)b);
}

/* Filter to retrieve modifiers in the track state 
** These modifiers are for left and right click
*/
class ModifierFilter {
public:
	static const int MAX_BLOBS = 10;

	ModifierFilter() {
	bres = new CBlobResult();
	};
	~ModifierFilter() { 
		if(bres) 
			delete bres; 
	}
	CBlobResult * modifiers(const CBlobResult *cbr);
	CBlob keyFeature(const CBlobResult *cbr);


private:
	CBlobResult *bres;
	CBlob kf;
	double m1[10], m2[10], m3[10];
	double m4[10];
};
/* ModifierFilter::modifiers
	
   Returns the two modifiers left, right
   @cbr: a CBlobResult of the current hand 
   @return: the two filtered modifiers
*/
CBlobResult * ModifierFilter::modifiers(const CBlobResult *cbr)
{
	const int numBlobs = cbr->GetNumBlobs();
	assert(numBlobs <= MAX_BLOBS);
	
	for(int i = 0; i < numBlobs; i++){
		CBlob blob = cbr->GetBlob(i);
		if(blob.MaxY() >= W) //blobs of image width/height
			m1[i] = W;	
		else
			m1[i] = cbr->GetBlob(i).MinX();
	}

	qsort(m1, numBlobs, sizeof(double), &sortLowHigh);

	for(int j = 0; j < numBlobs; j++)
	{
		CBlob blob = cbr->GetBlob(j);
		if(blob.MaxY() >= W) //blobs of image width/height
			m2[j] = W;	
		else
			m2[j] = cbr->GetBlob(j).MaxX();
	}

	qsort(m2, numBlobs, sizeof(double), &sortLowHigh);

	for(int k = 0; k < numBlobs; k++) //RC sphere
	{
		CBlob blob = cbr->GetBlob(k);
		if(blob.MaxY() >= W) //blobs of image width/height
			m3[k] = W;	
		else
			m3[k] = cbr->GetBlob(k).MinY();
	}

	qsort(m3, numBlobs, sizeof(double), &sortLowHigh);
	//check blobs against m1, m2, m3
	for(int i = 0; i < numBlobs; i++)
	{
		CBlob b = cbr->GetBlob(i);
		if(m1[0] == b.MinX() && m2[0] == b.MaxX())
			bres->AddBlob(&cbr->GetBlob(i));

		if(m3[0] == b.MinY())
			bres->AddBlob(&cbr->GetBlob(i));
	}

	return bres;
}
/*
	Modifier::keyFeature
	Calculate keyfeature
*/
CBlob ModifierFilter::keyFeature(const CBlobResult *cbr)
{
	assert(cbr);

	const int numBlobs = cbr->GetNumBlobs();
	assert(numBlobs <= MAX_BLOBS);

	for(int i = 0; i < numBlobs; i++){
		CBlob blob = cbr->GetBlob(i);
		if(blob.MaxY() >= W) //blobs of image width/height
			m4[i] = -W;	
		else
			m4[i] = cbr->GetBlob(i).MaxY();
	}

	qsort(m4, numBlobs, sizeof(double), &sortLowHigh);

	for(int i = 0; i < numBlobs; i++)
	{
		CBlob b = cbr->GetBlob(i);
		if(b.MaxY() == m4[numBlobs - 1]){
			kf = cbr->GetBlob(i) ;
		}
	}
	return kf;
}



/* thread to execute display of camera frames */
void *showCameraWindow(void *arg)
{
	CameraData_t *myCam = (CameraData_t *)arg;
	const char *windowName = TT_CameraName(myCam->i);
	const CBlobResult *hand;

	int x  = 0, y  = 0;
	int lx = 0, ly = 0;
	int rx = 0, ry = 0;

	enum {EMPTY = 2, DRAG = 4, _ZOOM = 8, _TRACK = 9};

	cvNamedWindow(windowName,CV_WINDOW_AUTOSIZE);
	
	if(myCam->i != 0)
		pthread_exit(NULL); //use Camera 21 for now

	for( ;key != KEY_ESC; ){
		TT_CameraFrameBuffer(myCam->i, W, H, 0, 8, (unsigned char *)myCam->displayImage->imageData);
		cvFlip(myCam->displayImage, 0, -1);
		cvShowImage(windowName, myCam->displayImage);
		
		pthread_mutex_lock(&keyMutex);
		hand = new CBlobResult(myCam->displayImage, 0, 20, false);
		
		switch(hand->GetNumBlobs()){
			case EMPTY:
				puts("Hand not in view!");
			break;
		
			case _ZOOM: puts("ZOOM");
			break;

                        case _TRACK:
			do {

			ModifierFilter modFilter;
			CBlobResult *filteredHand = modFilter.modifiers(hand);
			CBlob keyF = modFilter.keyFeature(hand);
#if 1
			POINT cursor;
			
			CBlobGetXCenter centreX;
			CBlobGetYCenter centreY;

			cursor.x = (LONG) centreX(keyF);
			cursor.y = (LONG) centreY(keyF);


#endif

			if( x >= cursor.x - 2 && x <= cursor.x + 2 &&
				y >= cursor.y - 2 && y <= cursor.y + 2) {//some type of click?
				POINT lSphere, rSphere;

				lSphere.x = (LONG) centreX(filteredHand->GetBlob(1)); //TODO Assert idx 1
				lSphere.y = (LONG) centreY(filteredHand->GetBlob(1));

				rSphere.x = (LONG) centreX(filteredHand->GetBlob(0));
				rSphere.y = (LONG) centreY(filteredHand->GetBlob(0));

				const int threshold = 3;
				if( ly >= (lSphere.y - threshold) && ly <= (lSphere.y + threshold)){ //LEFT CLICK
						printf("LEFT CLICK \n");
						lx = lSphere.x;
						ly = lSphere.y;
				}
				
			}
		
			else { //purely tracking as keyfeature has changed
				printf("-- true tracking\n");

				x = cursor.x;
				y = cursor.y;
			}


			}
			while(0);
			break;
			default:
				puts("<<UNKNOWN>>\n");
		}

		delete hand;
		key = cvWaitKey(50); //delay atleast n ms for TT firmware propogation delay
		pthread_mutex_unlock(&keyMutex);
	}
	cvReleaseImage(&myCam->displayImage);
	pthread_exit(NULL);
}

int main()
{

	TT_Initialize(); //setup TT cameras
	printf("Opening Calibration: %s\n", 
		TT_LoadCalibration("CalibrationResult 2010-12-30 4.39pm.cal") == NPRESULT_SUCCESS ?
		"PASS" : "ERROR");

	int cameraCount = TT_CameraCount();
	CameraData_t cameras[MAX_NUM_CAMERAS];
	pthread_t threads[MAX_NUM_CAMERAS]; 
	
	assert(MAX_NUM_CAMERAS == cameraCount);

	TT_SetCameraSettings(0, NPVIDEOTYPE_PRECISION,300, 150, 15);
	TT_SetCameraSettings(1, NPVIDEOTYPE_PRECISION,300, 150, 15);
	TT_SetCameraSettings(2, NPVIDEOTYPE_PRECISION,300, 150, 15);
	/* 1. Change camera settings ^
	   2. Allocate space for the displays 
	*/
	for(int i = 0; i < cameraCount; i++){
		cameras[i].i = i;
		cameras[i].displayImage = cvCreateImage(cvSize(W,H), IPL_DEPTH_8U, 1);
	}

	/* Setup priority for the process, pthreads-win32 threads inherit process priority 
	   threads don't support RT or near-RT threads natively 
    */

	SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS/*(HIGH)REALTIME_PRIORITY_CLASS*/); 
	fsm_initialize(TRACK);

	for(int i = 0; i < cameraCount; i++){
		if(pthread_create(&threads[i], NULL, showCameraWindow, (void*)&cameras[i])){
			printf("\aThread couldn't be created!");
			cvDestroyAllWindows();
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

	for(int i = 0; i < cameraCount; i++)
		pthread_join(threads[i], NULL);
	
	pthread_mutex_destroy(&keyMutex);
	cvDestroyAllWindows();
	TT_Shutdown();
	TT_FinalCleanup();
return 0;

}
