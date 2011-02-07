// TT_TEST.cpp : Test file to play with various API calls
// Initially ignoring so deemed errors
/* adding code to interafce with opencv + pthreads-win32
   Idris Soule
   Help of Yeshua Ha'Mashiach
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <errno.h>
#include <highgui.h>
#include <pthread.h>
#include <signal.h>
#include <cv.h>		//haartraining

#include "ocv.h"
#include "NPTrackingTools.h"

#define W 200//380
#define H 200//300
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

CvHaarClassifierCascade *cascade;

/* Draw a bounding box (rectangle) around the location of the hand */
void detectPosture(IplImage *img)
{

 CvMemStorage* storage = cvCreateMemStorage(0);
 CvSeq* hand;
 int i, scale = 1;
 hand = cvHaarDetectObjects( img, cascade, storage, 1.1, 2, 0/*CV_HAAR_DO_CANNY_PRUNING*/, cvSize(90,90));
 
 IplImage *image = cvCreateImage(cvSize(img->width,img->height),8,3);
 
    /* draw all the rectangles */
    for( i = 0; i < hand->total; i++ )
    {
        /* extract the rectanlges only */
        CvRect face_rect = *(CvRect*)cvGetSeqElem(hand, i );
        cvRectangle( image, cvPoint(face_rect.x*scale,face_rect.y*scale),
                     cvPoint((face_rect.x+face_rect.width)*scale,
                             (face_rect.y+face_rect.height)*scale),
                     CV_RGB(25,255,112), 3 );
		cvNamedWindow("Bounding BOX DISPLAY",CV_WINDOW_AUTOSIZE);
		cvShowImage("Bounding BOX DISPLAY", image);
    }
	cvClearMemStorage(storage);
	cvReleaseImage(&image);
}

void snapPicture(const char *threadName, IplImage *img, int *count)
{
	static int pcount = 1;
	//imagename: <threadName>999.jpg
	char *resultName = (char *)malloc(sizeof(char) * 110);//(strlen(threadName) + 8) + 1);

	sprintf(resultName, "Postures\\%s\\%s-%d.jpg",threadName,threadName,pcount);
	pcount++;
	cvSaveImage(resultName, img);
	free(resultName);
	*count = pcount;
}

/* thread to execute display of camera frames */
void *showCameraWindow(void *arg)
{
	CameraData_t *myCam = (CameraData_t *)arg; 
	const char *windowName = TT_CameraName(myCam->i);

	cvNamedWindow(windowName,CV_WINDOW_AUTOSIZE);

#if 1
	if(myCam->i != 0)
		pthread_exit(NULL);
#endif
	for( ;key != KEY_ESC; ){
		TT_CameraFrameBuffer(myCam->i, W, H, 0, 8, (unsigned char *)myCam->displayImage->imageData);
		cvShowImage(windowName, myCam->displayImage);
		pthread_mutex_lock(&keyMutex);
		key = cvWaitKey(250); //delay atleast n ms for TT firmware propogation delay

		//detectPosture(myCam->displayImage);
		int count = 0;
		snapPicture("4pose",myCam->displayImage, &count);
		if(count > 500) {printf("\a\a\a"); key = KEY_ESC;}
		pthread_mutex_unlock(&keyMutex);
	}
	cvReleaseImage(&myCam->displayImage);
	pthread_exit(NULL);
}


int main()
{
#if 0
	OpenCV_Test tsuite;
	/*tsuite.generatePositiveSampleData("1pose", 8610);
	//tsuite.generatePositiveSampleData("2pose", 1209);
	//tsuite.generatePositiveSampleData("3pose", 1602);
	tsuite.generatePositiveSampleData("4pose", 2093);*/
	
	tsuite.generateNegativeSampleData();
#else 
	TT_Initialize(); //setup TT cameras
	printf("Opening Calibration: %s\n", 
		TT_LoadCalibration("CalibrationResult 2010-12-30 4.39pm.cal") == NPRESULT_SUCCESS ?
		"PASS" : "ERROR");
	
		//open Cascade
	cascade = (CvHaarClassifierCascade *)cvLoad("HandClassifier_1Pose.xml",0,0,0);

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

	/* call the threads for display of camera data */
	
	pthread_mutex_init(&keyMutex, NULL);

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
			Sleep(10UL/*10UL*/); //wait for updated frame 1/sleeptime[ms] = frame-rate
	}

	for(int i = 0; i < cameraCount; i++)
		pthread_join(threads[i], NULL);
	
	pthread_mutex_destroy(&keyMutex);
	cvDestroyAllWindows();
	TT_Shutdown();
	TT_FinalCleanup();
#endif
	return 0;
}
