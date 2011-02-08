/*
Multilayer Perceptron - Artificial Neural Network

+ Includes both training and testing of the network 
+ Modified by toggling defined symbols

Idris Soule
Help of Yeshua Ha'Mashiach
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if WIN_32
#include <conio.h>
#endif

#include <errno.h>
#include <highgui.h>
#include <pthread.h>
#include <signal.h>
#include <ml.h> //ANN (Machine Learning)

#if WIN_32
#include "ocv.h"
#include "NPTrackingTools.h"
#endif

#define W 200
#define H 200
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

//debugging purposes
void displayMatrix(CvMat *c)
{
  assert(c);
  for ( int row = 0; row < c->rows; ++ row )
  {
    for ( int col = 0; col < c->cols; ++ col )
    {
        float e = (float)cvmGet( c, row, col );
        //if(e < 1/255.) continue;
        printf("%.5f ",  e);
    }
    printf("\n");
  }
}

/* transform the image from CV_8UC1 => CV_32FC1 => Matrix of CV_32FC1

   @img: the real-time image of the hand
   @tmp32: a pointer to temporary storage to which this function will allocate
           (this allows the user to then cvReleaseImage(...))
   @return: A 1D feature vector

*/
CvMat *TMatrix(IplImage *img, IplImage **tmp32)
{
    assert(img);
    *tmp32 = cvCreateImage(cvSize(img->width, img->height), IPL_DEPTH_32F, 1);
    cvConvertScale(img, *tmp32, 1/255.); //intensities on [0,1]

    // put imageData into 32FC1 matrix

    CvRect rect = cvRect(0, 0, img->width, img->height);
    CvMat  *mat = cvCreateMat(img->width, img->height, CV_32F);
    CvMat  *sr  = cvGetSubRect(*tmp32, mat, rect); //2D Vector

    //1D column order vector of 114,000 features
    CvMat *cord = cvCreateMat(1, img->width * img->height, CV_32F);

    //Do a transformation from R^2 to R^1
    int feature = 0;
    for(int row = 0; row < img->height; row++){
        for(int col = 0; col < img->width; col++){ //column is fastest running index
             float e = (float)cvmGet(sr, row, col);
             cvmSet(cord, 0, feature++, e);
         }
    }
    return cord;
}

/* Loads the images from a directory to be processed by the TMatrix
   Once processed by the TMatrix the (380 * 300) pixels i.e "variables" are
   copied into the data (input) matrix and the response matrix is filled with
   the correct class for each modulo 500 images
   The directory is commonly "Postures"
*/
void preprocessANN_Input(CvMat **data, CvMat **responses, const char *dir)
{
    static const char *poseNames[] = {"1pose", "2pose", "3pose", "4pose"};
    IplImage *loadImage, *tmpImage;
    const int varCount = W * H;
    CvMat *mat = NULL;
    char **filenames = (char **)malloc(sizeof(char *) * 1200);
    int fcount = 0;

    //get the file names in the directory
    for(int i = 0; i < 4; i++){
        for(int j = 0; j < 300; j++){
            filenames[fcount] = (char *)malloc(sizeof(char) * 35);
            sprintf(filenames[fcount],"%s\\%s\\%s-%d.jpg",dir,poseNames[i],poseNames[i],j+1);
            fcount++;
        }
    }

    *data = cvCreateMat(4 * 300, varCount, CV_32FC1); //(200, 40000)
    *responses = cvCreateMat(4 * 300, 4, CV_32FC1); //(200, 4)

    float classKind = 0.0f;
    for(int i = 0; i < 1200; i++){ //process rows
        loadImage = cvLoadImage(filenames[i], 0); //load greyscale
        mat = TMatrix(loadImage, &tmpImage);

        //fill each row in data with 1D mat
        for(int feature = 0; feature < varCount; feature++){
                float e = (float)cvmGet(mat, 0, feature);
                cvmSet(*data, i, feature, e); //populate *data with features from mat
        }
        cvReleaseImage(&tmpImage); //free the 1D mat

        //now for the responses
        classKind += (i % 300 == 0) ? 1.0f : 0.0f;
        for(int j = 0; j < 4; j++){
            if(classKind == j + 1)
                cvmSet(*responses, i, j, classKind);
            else{
                float punishment = 10 * classKind;
                cvmSet(*responses, i, j, -punishment); //punishment
            }
        }
    }

    //free storage for filenames
    for(int i = 0; i < 1200; i++){
        free(filenames[i]);
        filenames[i] = NULL;
    }
    free(filenames);
}

#if WIN_32
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
#endif
int main()
{
    static const char *path = "//home//idris//src//OpenCV//Postures//4pose//4pose-14.jpg";
    CvANN_MLP mlp; // the ann its self (default) constructor called


    const int classCount = 4;
    CvMat *data, *responses, *mlpResponse;
    data = responses = mlpResponse = NULL;
#if TRAIN
    printf("Loading samples from the database ...\n");
    preprocessANN_Input(&data, &responses, "Postures");


    int layer_sz[] = {W*H, 500, 500, classCount};

    CvMat layer_sizes = cvMat(1, (int)(sizeof(layer_sz) / sizeof(layer_sz[0])), CV_32S, layer_sz);
    mlp.create(&layer_sizes);

	system("echo %date%-%time%");
    printf("Training the classifier, might take a few minutes (~10min) ...\n");
    mlp.train(data, responses, (const CvMat *)0, (const CvMat *)0, CvANN_MLP_TrainParams(cvTermCriteria(CV_TERMCRIT_ITER, 300, 0.01F),
                                                             CvANN_MLP_TrainParams::BACKPROP, 0.001));

    printf("Time Taken:\n");
    system("echo %date%-%time%");
    mlpResponse = cvCreateMat(1, classCount, CV_32F);

    //load a temporary image to test mlpResponse values

	/*
    IplImage *t = NULL;
    IplImage *img = cvLoadImage(path, 0);

    CvMat *mat = TMatrix(img, &t);
    mlp.predict(mat, mlpResponse);

    printf("Response information\n");
    displayMatrix(mlpResponse);
	*/
    mlp.save("t1.xml");

    //cvReleaseImage(&img);
   // cvReleaseImage(&t);
    cvReleaseMat(&data);
    cvReleaseMat(&responses);
    cvReleaseMat(&mlpResponse);

#elif !RUN
    mlp.load("t1.xml"/*file.xml*/);
    mlpResponse = cvCreateMat(1, classCount, CV_32F);

    IplImage *t = NULL;
    IplImage *img = cvLoadImage(path, 0);

    CvMat *mat = TMatrix(img, &t);
    mlp.predict(mat, mlpResponse);

    displayMatrix(mlpResponse);

    cvReleaseImage(&img);
    cvReleaseImage(&t);
    cvReleaseMat(&mlpResponse);

#elif WIN32
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
	system("pause");
return 0;

}
