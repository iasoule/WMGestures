/* A test harness based system for OpenCV
   Idris Soule
*/

#include <highgui.h>
#include <cv.h>

#if !defined (CVX_RED) && !defined (CVX_BLUE)
#define CVX_RED		CV_RGB(0xff,0x00,0x00)
#define CVX_BLUE	CV_RGB(0x00,0x00,0xff)
#endif

#define W 380
#define H 300

using namespace std;

class OpenCV_Test {
public:
	void showPicture(const char * name);
	void histogramTest(const char *imageName);
	void drawContour(const char *imageName);
	void pts2convexhull(void);
	IplImage* extractContourConvex(IplImage *img_8uc1);

	void RGB_2_GRAY(const char *dir);
/**
** |------- HAAR Training Stubs -------|
*/ 
	void generatePositiveSampleData(const char *poseName, int n);
	void generateNegativeSampleData(void);

};

void OpenCV_Test::RGB_2_GRAY(const char *poseName)
{
	IplImage *src, *dst;
	char *fn = (char*)malloc(sizeof(char) * 100);
	for(int i = 0; i < 8610; i++){
		sprintf(fn, "Postures\\1pose\\%s-%d.jpg",poseName,i+1);
		src = cvLoadImage(fn);
		dst = cvCreateImage(cvSize(src->width, src->height), IPL_DEPTH_8U, 1);
		cvCvtColor(src, dst, CV_RGB2GRAY);
		cvSaveImage(fn, dst); //overwrite

		if(i % 2000 == 0 && i != 0)
			printf("N = %d\a\n", i / 2000);
		cvReleaseImage(&dst);
		//cvReleaseImage(&src);
	}
	free(fn);
}

//will generate the idx file for 1st part of HAAR training
void OpenCV_Test::generatePositiveSampleData(const char *poseName, int n)
{
	FILE *out;
	int c = 1;

	char *resultFileName = (char *)malloc(sizeof(char) * 50);
	char *fmt = (char *)malloc(sizeof(char) * 50);
	sprintf(resultFileName, "%s.idx",poseName);
	out = fopen(resultFileName, "w");

	for(; c <= n; c++){
		sprintf(fmt, "Postures\\%s-%d.jpg 1 0 150 380 300\n", poseName,c);
		fprintf(out, fmt);
	}
	fclose(out);
	free(fmt); free(resultFileName);
}


void OpenCV_Test::generateNegativeSampleData(void)
{
	FILE *out;
	int j,i = 1;
	int an[] = {3000 /*8160*/, 1209, 1602, 2093}; //images in each positive sample

	out = fopen("background.idx", "w");
	for(j = 0; j < 1; j++){
		i = 1;
		for(; i <= an[j]; i++)
			fprintf(out, "PosturesNegative\\%dpose-%d.jpg\n",j+1,i);
	}
	fclose(out);
}

/* Operations for HAAR Training */


void OpenCV_Test::showPicture(const char * name)
{
	IplImage *img = cvLoadImage(name);
	cvNamedWindow("Example 1", CV_WINDOW_AUTOSIZE);
	cvShowImage("Example 1", img);
	cvWaitKey(0);
	cvReleaseImage(&img);
	cvDestroyWindow("Example 1");
}

/* Draws the convex hull of random points */
void OpenCV_Test::pts2convexhull(void)
{
#define ARRAY 0	 
	IplImage* img = cvCreateImage( cvSize( 500, 500 ), 8, 3 );
    cvNamedWindow( "hull", 1 );
	int hullcount = 0;
#if !ARRAY
        CvMemStorage* storage = cvCreateMemStorage(); //64Kb default block
#endif

    for(;;)
    {
        int i, count = rand();
        CvPoint pt0;
#if !ARRAY
        CvSeq* ptseq = cvCreateSeq( CV_SEQ_KIND_GENERIC|CV_32SC2,
                                    sizeof(CvContour),
                                    sizeof(CvPoint),
                                    storage );
        CvSeq* hull;

        for( i = 0; i < count; i++ )
        {
            pt0.x = rand();
            pt0.y = rand();
            cvSeqPush( ptseq, &pt0 );
        }
        hull = cvConvexHull2( ptseq, 0, CV_CLOCKWISE, 0 );
        hullcount = hull->total;
#else
        CvPoint* points = (CvPoint*)malloc( count * sizeof(points[0]));
        int* hull = (int*)malloc( count * sizeof(hull[0]));
        CvMat point_mat = cvMat( 1, count, CV_32SC2, points );
        CvMat hull_mat = cvMat( 1, count, CV_32SC1, hull );

        for( i = 0; i < count; i++ )
        {
            pt0.x = rand()
            pt0.y = rand()
            points[i] = pt0;
        }
        cvConvexHull2( &point_mat, &hull_mat, CV_CLOCKWISE, 0 );
        hullcount = hull_mat.cols;
#endif
        cvZero( img );
        for( i = 0; i < count; i++ )
        {
#if !ARRAY
            pt0 = *CV_GET_SEQ_ELEM( CvPoint, ptseq, i );
#else
            pt0 = points[i];
#endif
            cvCircle( img, pt0, 2, CV_RGB( 255, 0, 0 ), CV_FILLED );
        }

#if !ARRAY
        pt0 = **CV_GET_SEQ_ELEM( CvPoint*, hull, hullcount - 1 );
#else
        pt0 = points[hull[hullcount-1]];
#endif

        for( i = 0; i < hullcount; i++ )
        {
#if !ARRAY
            CvPoint pt = **CV_GET_SEQ_ELEM( CvPoint*, hull, i );
#else
            CvPoint pt = points[hull[i]];
#endif
            cvLine( img, pt0, pt, CV_RGB( 0, 255, 0 ));
            pt0 = pt;
        }

        cvShowImage( "hull", img );

        int key = cvWaitKey(0);
        if( key == 27 ) // 'ESC'
            break;

#if !ARRAY
        cvClearMemStorage( storage );
#else
        free( points );
        free( hull );
#endif
    }
}




/* Finding and drawing contours on an input image */
void OpenCV_Test::drawContour(const char *imageName)
{
	cvNamedWindow("Contour Example");
	IplImage *img_8uc1 = cvLoadImage(imageName, CV_LOAD_IMAGE_GRAYSCALE);
	IplImage *img_edge = cvCreateImage(cvGetSize(img_8uc1), 8, 1);
	IplImage *img_8uc3 = cvCreateImage(cvGetSize(img_8uc1), 8, 3);

	cvThreshold(img_8uc1, img_edge, 128, 255, CV_THRESH_BINARY);

	CvMemStorage *storage = cvCreateMemStorage();
	CvSeq *first_contour = NULL;

	//# of contours 
	int Nc = cvFindContours(img_edge,storage,&first_contour, sizeof(CvContour), CV_RETR_TREE);

	int n=0;
	printf("Total Contours detected; %d\n", Nc);

	for(CvSeq *c=first_contour; c != NULL; c=c->h_next){
		cvCvtColor(img_8uc1, img_8uc3, CV_GRAY2BGR);
		cvDrawContours(img_8uc3,c,CVX_RED,CVX_BLUE, 1,1,8);

		printf("Contour #%d\n", n);
		cvShowImage("Contour Example", img_8uc3);
		printf(" %d elements:\n", c->total);

		for(int i = 0; i < c->total; ++i){
			CvPoint *p = CV_GET_SEQ_ELEM(CvPoint, c, i);
			printf("    (%d,%d)\n", p->x, p->y);
		}
		printf("isconvex = %d\n", cvCheckContourConvexity(c));
		cvWaitKey(0);
		n++;
	}

	printf("Finished all contours\a\n");
	cvCvtColor(img_8uc1, img_8uc3, CV_GRAY2BGR);
	cvShowImage("Contour Example", img_8uc3);
	cvWaitKey(0);

	cvDestroyWindow("Contour Example");
	cvReleaseImage(&img_8uc1);
	cvReleaseImage(&img_8uc3);
	cvReleaseImage(&img_edge);
}

void OpenCV_Test::histogramTest(const char *imageName)
{

	IplImage* src;
	if((src=cvLoadImage(imageName, 1))!= 0) {
		IplImage* h_plane = cvCreateImage( cvGetSize(src), 8, 1 );
        IplImage* s_plane = cvCreateImage( cvGetSize(src), 8, 1 );
        IplImage* v_plane = cvCreateImage( cvGetSize(src), 8, 1 );
        IplImage* planes[] = { h_plane, s_plane };
        IplImage* hsv = cvCreateImage( cvGetSize(src), 8, 3 );
        int h_bins = 30, s_bins = 32;
        int hist_size[] = {h_bins, s_bins};
        /* hue varies from 0 (~0 deg red) to 180 (~360 deg red again) */
        float h_ranges[] = { 0, 360 };
        /* saturation varies from 0 (black-gray-white) to
           255 (pure spectrum color) */
        float s_ranges[] = { 0, 255 };
        float* ranges[] = { h_ranges, s_ranges };
        int scale = 10;
        IplImage* hist_img =
            cvCreateImage( cvSize(h_bins*scale,s_bins*scale), 8, 3 );
        CvHistogram* hist;
        float max_value = 0;
        int h, s;

        cvCvtColor( src, hsv, CV_BGR2HSV );
        cvCvtPixToPlane( hsv, h_plane, s_plane, v_plane, 0 );
        hist = cvCreateHist( 2, hist_size, CV_HIST_ARRAY, ranges, 1 );
        cvCalcHist( planes, hist, 0, 0 );
        cvGetMinMaxHistValue( hist, 0, &max_value, 0, 0 );
        cvZero( hist_img );

        for( h = 0; h < h_bins; h++ )
        {
            for( s = 0; s < s_bins; s++ )
            {
                float bin_val = cvQueryHistValue_2D( hist, h, s );
                int intensity = cvRound(bin_val*255/max_value);
                cvRectangle( hist_img, cvPoint( h*scale, s*scale ),
                             cvPoint( (h+1)*scale - 1, (s+1)*scale - 1),
                             CV_RGB(intensity,intensity,intensity),
                             CV_FILLED );
            }
        }

        cvNamedWindow( "Source", 1 );
        cvShowImage( "Source", src );

        cvNamedWindow( "H-S Histogram", 1 );
        cvShowImage( "H-S Histogram", hist_img );

        cvWaitKey(0);
    }
}



/* AN adaptation from EmguCV (C#) with convex hull done */
IplImage * OpenCV_Test::extractContourConvex(IplImage *img_8uc1)
{
	CvSeq *contours = NULL, *biggestContour = NULL;
	CvSeq *first_contour = NULL;
	CvMemStorage *storage = cvCreateMemStorage(); //storage for contours creation

	IplImage *img_edge = cvCreateImage(cvGetSize(img_8uc1), 8,1);
	IplImage *img_8uc3 = cvCreateImage(cvGetSize(img_8uc1), 8,3);

	//apply thresholding to the image
	cvThreshold(img_8uc1, img_edge, 128, 255, CV_THRESH_BINARY);

	int nc;
	nc = cvFindContours(img_8uc1, storage, &first_contour, sizeof(CvContour), CV_RETR_LIST); 
	if(nc == 0)
		return NULL;

	double result1, result2;
	result1 = result2 = 0;

	//search for the largest contour which should be around the hand
	CvSeq *cp;
	assert(first_contour);
	for(cp = first_contour; cp != NULL; cp=cp->h_next){
		result1 = cvContourArea(cp,CV_WHOLE_SEQ);

		if(result1 > result2){
			result2 = result1;
			biggestContour = cp;
		}
	}

	//cvReleaseMemStorage(&storage);

	if(biggestContour != NULL){ //VALID largest contour
		double arcLen = cvArcLength(biggestContour,CV_WHOLE_SEQ,1); 
		CvMemStorage *storage1 = cvCreateMemStorage();

		CvSeq *currentContour = cvApproxPoly(biggestContour, sizeof(CvContour), storage1,
											 CV_POLY_APPROX_DP, arcLen * 0.0025F);

		cvCvtColor(img_8uc1, img_8uc3, CV_GRAY2BGR);
		cvDrawContours(img_8uc3, biggestContour, CVX_RED, CVX_BLUE,1,1,8);
		biggestContour = currentContour;

		/* Convex Hull computation */
		CvSeq *hull;
		hull = cvConvexHull2(biggestContour,0,CV_CLOCKWISE,0);

		CvBox2D box;
		box = cvMinAreaRect2(biggestContour,0);

		CvPoint2D32f points[4];
		cvBoxPoints(box, points); //retrieve vertices of the min-bounding rect

		/* draw the points of the hull onto an image */
		CvPoint pt0;

		pt0 = *(CvPoint *)cvGetSeqElem(hull, hull->total - 1); 
		for(int i=0; i < hull->total; ++i){
			CvPoint pt = *(CvPoint *)cvGetSeqElem(hull, i);
			cvLine(img_8uc3, pt0, pt, CV_RGB( 200, 125, 75 ));
			pt0 = pt;
		}
		CvPoint circleCenter = {(int)box.center.x, (int)box.center.y};
		cvCircle(img_8uc3,circleCenter, 3, CV_RGB(200,125,75)); //draw circle on image

		/* compute convexity defects of the hull 
		  this allows for characterization of the hull
	      used for later post-processing of gesture detection
		*/

		//CvSeq *defects = cvConvexityDefects(biggestContour, hull, storage);
		cvReleaseMemStorage(&storage1);
	}

	cvReleaseMemStorage(&storage);
	cvReleaseImage(&img_edge);
	return img_8uc3;
}