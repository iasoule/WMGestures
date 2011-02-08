/*
   Implementation of Random Forests (Tree) for posture detection
   + Two modes of operation, training and running

   Idris Soule
   Help of Yeshua Ha'Mashiach
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <conio.h>
#include <errno.h>
#include <highgui.h>
#include <pthread.h>
#include <signal.h>
#include <ml.h> //Random Forests (Machine Learning)

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

CvRTrees forest;

/*
	setupRandomForest - trains the forest
*/
bool setupRandomForest(CvRTrees *forest, const CvMat *featureVector, 
										 const CvMat *response, const char *name)
{
	CvMat *var_type;
	
	//since we are doing classification 
	var_type = cvCreateMat(featureVector->cols + 1, 1, CV_8U );
    cvSet(var_type, cvScalarAll(CV_VAR_ORDERED));

	return forest->train(featureVector, CV_ROW_SAMPLE, response, 0,0,var_type,0,
					CvRTParams(20,10,0,false,2,0,false,
							   44,50,0.05f,CV_TERMCRIT_ITER | CV_TERMCRIT_EPS));
}

/* Prepare data for up-scale conversion from 8-bit UC 1-channel image
   to a 32-bit FC 1-channel image
   Read the list of images from a given directory and convert 
   Each converted image is stripped to its imageData constituents 
   and placed in a N * 1 Matrix where N is number of images in the directory

   @dirName: "Postures\\1pose\\"
   @N:		Rows in matrix i.e # of images in dir 
   @return: a 32FC1 matrix which is the feature vector
			client must free allocated matrix
			on error a NULL casted CvMat is returned
*/
CvMat * upscaleCvt_FeatureVector(const char *dirName, int N, bool single)
{
	CvMat *fvector = cvCreateMat(N, 1, CV_32FC1);
	IplImage *tmp8 = NULL, *tmp32 = NULL;
	char *imageName = (char *)malloc(sizeof(char) * 100);
	int n;

	//pre-conditions
	assert(dirName), assert(N >= 1);

	for(n = 0; n < N; n++){
		sprintf(imageName, "%s%dpose-%d.jpg",dirName,single ? 2 : 1,n+1);
		
		if((tmp8 = cvLoadImage(imageName,0)) == NULL){
			fprintf(stderr, "Error: Couldn't open %s!", imageName);
			return (CvMat *)NULL;
		}
		
		tmp32 = cvCreateImage(cvSize(tmp8->width, tmp8->height), IPL_DEPTH_32F, 1);
		cvConvertScale(tmp8, tmp32, 1/255.); //upscale the image

	/* fill the rows of the matrix with ImageData from the converted image */
		*( (float *)CV_MAT_ELEM_PTR(*fvector, n, 0)) = *(float*)tmp32->imageData;
		cvReleaseImage(&tmp32);
		tmp32 = NULL;
	}

	free(imageName), imageName = NULL;
	return fvector;
}


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

		/* INSERT CODE */
		pthread_mutex_unlock(&keyMutex);
	}
	cvReleaseImage(&myCam->displayImage);
	pthread_exit(NULL);
}

void printImage(IplImage *img)
{
	const int lr = img->width * img->height * img->nChannels;
	int count = 0;
	for(int i = 0; i < lr; i++){
		if((unsigned char *)img->imageData[i] == 0 ||
			img->imageData[i] & 0x80) //clipping
			continue;
		count++;
	printf(" %d", (unsigned char *)img->imageData[i]);
	}
	printf("\nCount = %d\n", count);

	/*
	printf("Integral Image*");

	IplImage *ig = cvCreateImage(cvSize(img->width+1, img->height+1), IPL_DEPTH_32S, 1);
	cvIntegral(img, ig, 0, 0);
	
	for(int i = 0; i < ig->imageSize; i++){
		if((int *)ig->imageData[i] == 0) continue;
		printf(" %d", (int*)ig->imageData[i]);
	}
	*/

}


int main()
{
#if TRAIN
	int N = 2000;
	CvMat *response = cvCreateMat(N, 1, CV_32FC1);
	for(int i = 0; i < N; i++)
		*( (float *)CV_MAT_ELEM_PTR(*response, i, 0)) = 7.00F; //Response for 1-pose
	
	CvMat *m = upscaleCvt_FeatureVector("Postures\\1pose\\",N, false);
	bool res = setupRandomForest(&forest, m, response, "1-pose");
	if(!res)
		printf("Problem with training!\n");
	else
		printf("OK!!\n");
	
	printf("Trying to predict ...\n");
	float pResult;
	CvMat *pmat = upscaleCvt_FeatureVector("Postures\\2pose\\",1, true);
	pResult = forest.predict(pmat, 0);
	printf("Prediction value = %f\n", pResult);
	cvReleaseMat(&m);cvReleaseMat(&pmat);cvReleaseMat(&response);
	

	IplImage *img = cvLoadImage("Postures\\4pose\\4pose-500.jpg", 0);
	printImage(img);
	system("pause");
	return 0;
#else
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
			Sleep(10UL); //wait for updated frame 1/sleeptime[ms] = frame-rate
	}

	for(int i = 0; i < cameraCount; i++)
		pthread_join(threads[i], NULL);
	
	pthread_mutex_destroy(&keyMutex);
	cvDestroyAllWindows();
	TT_Shutdown();
	TT_FinalCleanup();
	return 0;
#endif
}
