// TT_TEST.cpp : Test file to play with various API calls
// Initially ignoring so deemed errors
/* adding code to interafce with opencv + pthreads-win32
   Idris Soule
   Help of Yeshua Ha'Mashiach
*/

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <errno.h>
#include <highgui.h>
#include <pthread.h>

#include "NPTrackingTools.h"
#include "ocv.h"

#define W 380
#define H 300
#define KEY_ESC 27
#define KEY_NOTPRESSED 0
#define MAX_NUM_CAMERAS 3

#pragma warning(disable:4716) //disable missing return from function error 

int key = KEY_NOTPRESSED;
pthread_mutex_t keyMutex;

OpenCV_Test tsuite;

typedef struct {
	unsigned int i;
	IplImage *displayImage;
}CameraData_t;

typedef struct Convexctx_t {
	IplImage *image;
	CvSeq *contour;
	void (*destroy)(struct Convexctx_t *, void *); //fp to clean up resources
}Convexctx_t;

IplImage *createConvexHull(Convexctx_t *ctx);

/* allocationCleanup: frees allocated memory from ContourTrees allocation
					  of various structures
					  * Allocation of ctx is still valid after call, callee make
					  sure to free(ctx).

   @ctx: the context of ContourTree computation 
   @cl: user defined context to clean up 
	    [cl == NULL indicates no user defined data]
*/
void allocationCleanup(Convexctx_t *ctx, void *cl)
{
	/* as per allocation in compute_ContourTree
	   cleanup of 8-bit 3 channel image, sequence of points,
	   storage for sequence, storage for contours
	   Note: implicitly a CvSeq holds a pointer to storage 
    */
	assert(ctx);
	cvReleaseImage(&ctx->image);
	//further cleanup 
	free(cl);
}

/* compute_ContourTree: Compute the contours of the filtered camera image
						Returns updated image and sequence of points (CvPoint)
						Image returned within context shall be a 3-channel RGB image
	@img_8uc1: An 8-bit single channel image 
*/
IplImage * compute_ContourTree(IplImage *img_8uc1)
{
	IplImage *img_edge = cvCreateImage(cvGetSize(img_8uc1), 8, 1);
	IplImage *img_8uc3 = cvCreateImage(cvGetSize(img_8uc1), 8, 3);

	CvSeq *ptSeq = NULL; //point sequence
	CvMemStorage *storage = cvCreateMemStorage(); //storage for contours creation
	
	cvThreshold(img_8uc1, img_edge, 128, 255, CV_THRESH_BINARY);

	CvSeq *c, *first_contour = NULL;
	CvSeq *biggestContour = NULL;
	int numContours = cvFindContours(img_edge,storage,&first_contour, sizeof(CvContour), CV_RETR_LIST);

	if(numContours == 0)
		return NULL;
	/*
	double result1, result2;
	result1 = result2 = 0;

	// find the largest contour 
	  // this is the whole hand
	
	for(c = first_contour; c != NULL; c=c->h_next){
		result1 = cvContourArea(c,CV_WHOLE_SEQ);
		
		if(result1 > result2){
			result2 = result1;
			biggestContour = c;
		}
	}
	*/
	for(c = first_contour; c != NULL; c=c->h_next){
	cvCvtColor(img_8uc1, img_8uc3, CV_GRAY2BGR);
	cvDrawContours(img_8uc3,c,CVX_RED,CVX_BLUE, 1,1,8); //note define (CVX...) if not including ocv.h

	CvSeq *hull;
	hull = cvConvexHull2(c, 0, CV_CLOCKWISE, 0);
	CvPoint pt0;

	pt0 = *(CvPoint *)cvGetSeqElem(hull, hull->total - 1); //??
	for(int i=0; i < hull->total; ++i){
		CvPoint pt = *(CvPoint *)cvGetSeqElem(hull, i);
		cvLine(img_8uc3, pt0, pt, CV_RGB( 0, 255, 0 ));
		pt0 = pt;
	}
	cvShowImage("CONVEX WINDOW", img_8uc3);
	}
	
	/*Convexctx_t *retCtx = (Convexctx_t *)malloc(sizeof(*retCtx));
	retCtx->image = img_8uc3;
	retCtx->contour = c;
	retCtx->destroy = allocationCleanup; //destructor
	cvReleaseImage(&img_edge);
	return retCtx;
	*/
	return img_8uc3;

}

/* createConvexHull: Create a Convex Hull for a seq. of a contours
					 Convex Hull allows for classification of hand state
					 Returns an image of "Hand contour" with convexity defects
					 Image returned shall be a 3-channel RGB image
   @ctx: context from compute_ContourTree

*/
IplImage *createConvexHull(Convexctx_t *ctx)
{
	assert(ctx);
	
	CvSeq *hull;
	hull = cvConvexHull2(ctx->contour, 0, CV_CLOCKWISE, 0);
	IplImage *img = cvCreateImage(cvSize(W,H), 8, 3);
	
	cvZero(img);
	CvPoint pt0;/*
	for(int i=0; i < ctx->contour->total; ++i){
		pt0 = *(CvPoint *)cvGetSeqElem(ctx->contour, i);
		cvCircle(img, pt0, 2, CV_RGB( 255, 0, 0 ), CV_FILLED);
	}*/
	
	pt0 = *(CvPoint *)cvGetSeqElem(hull, hull->total - 1); //??
	for(int i=0; i < hull->total; ++i){
		CvPoint pt = *(CvPoint *)cvGetSeqElem(hull, i);
		cvLine( img, pt0, pt, CV_RGB( 0, 255, 0 ));
        pt0 = pt;
	}
	return img;
}



/* thread to execute display of camera frames */
void *showCameraWindow(void *arg)
{
	CameraData_t *myCam = (CameraData_t *)arg; 
	Convexctx_t *cameraCtx;
	IplImage *img = NULL;
	const char *windowName = TT_CameraName(myCam->i);

	cvNamedWindow(windowName,CV_WINDOW_AUTOSIZE);

	for( ;key != KEY_ESC; ){
		TT_CameraFrameBuffer(myCam->i, W, H, 0, 8, (unsigned char *)myCam->displayImage->imageData);
		
		pthread_mutex_lock(&keyMutex);
		if(myCam->i == 2){
		img = compute_ContourTree(myCam->displayImage);
		if(img)
			cvShowImage(windowName, img);
		else
			cvShowImage(windowName, myCam->displayImage);
		key = cvWaitKey(15); //delay atleast n ms for TT firmware propogation delay

		if(img)
			cvReleaseImage(&img);
		}
		else
			cvShowImage(windowName, myCam->displayImage);
		pthread_mutex_unlock(&keyMutex);
	}
	cvReleaseImage(&myCam->displayImage);
	pthread_exit(NULL);
}



int main()
{
	
	//tsuite.pts2convexhull();
	//tsuite.pts2convexhull();
	/*IplImage *img, *simg;
	img = cvLoadImage("Infrared_HAND_87e2.png", CV_LOAD_IMAGE_UNCHANGED);

	simg = tsuite.extractContourConvex(img);
	cvNamedWindow("Extract Contour",1);
	cvShowImage("Extract Contour", simg);
	key = cvWaitKey(0);
	*/
#if 1
	TT_Initialize(); //setup TT cameras
	printf("Opening Calibration: %s\n", 
		TT_LoadCalibration("CalibrationResult 2010-11-02 5.03pm.cal") == NPRESULT_SUCCESS ?
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
			TT_Shutdown();
			TT_FinalCleanup();
			exit(0);
		}
	}
	
	printf("Press any Key to Exit!\n"); 
	while(!_kbhit()){
		int result = TT_Update();
		if(result != NPRESULT_SUCCESS)
			Sleep(70UL); //wait for updated frame 1/sleeptime[ms] = frame-rate
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
