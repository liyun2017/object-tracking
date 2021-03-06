#include "opencv2/video/tracking.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <stdio.h>


using namespace cv;
using namespace std;


Mat image;
Point origin;
int trackObject = 0;
Rect selection;
bool selectObject = false;


static void onMouse(int event, int x, int y, int, void*)
{


	if (selectObject)
	{


		selection.x = MIN(x, origin.x);
		selection.y = MIN(y, origin.y);
		selection.width = std::abs(x - origin.x);
		selection.height = std::abs(y - origin.y);


		selection &= Rect(0, 0, image.cols, image.rows);


	}


	switch (event)
	{


	case CV_EVENT_LBUTTONDOWN:
		origin = Point(x, y);
		selection = Rect(x, y, 0, 0);
		selectObject = true;
		break;
	case CV_EVENT_LBUTTONUP:
		selectObject = false;
		if (selection.width > 0 && selection.height > 0)
			trackObject = -1;
		break;


	}


}


int main11(int, char**)
{


	const int stateNum = 4;
	const int meatureNum = 2;


	Rect trackWindow;
	VideoCapture cap;


	cap.open("G:/跟踪/视频/1.avi");


	namedWindow("CamShift+Kalman", WINDOW_AUTOSIZE);
	setMouseCallback("CamShift+Kalman", onMouse, 0);


	KalmanFilter KF(stateNum, meatureNum, 0);
	Mat state(stateNum, 1, CV_32F);
	Mat processNoise(stateNum, 1, CV_32F);
	Mat measurement = Mat::zeros(meatureNum, 1, CV_32F);
	bool paused = false;


	int hsize = 16;
	float hranges[] = {
		1,180
	};
	const float* phranges = hranges;


	Mat frame, hsv, hue, mask, hist, roibackproj;


	for (;;)
	{


		if (!paused)
		{


			cap >> frame;
			if (frame.empty())
				break;


		}
		frame.copyTo(image);


		if (!paused)
		{


			cvtColor(image, hsv, CV_BGR2HSV);


			if (trackObject)
			{


				int vmin = 10, vmax = 256, smin = 30;


				inRange(hsv, Scalar(0, smin, MIN(vmin, vmax)),
					Scalar(180, 256, MAX(vmin, vmax)), mask);
				int ch[] = {
					0, 0
				};
				hue.create(hsv.size(), hsv.depth());
				mixChannels(&hsv, 1, &hue, 1, ch, 1);


				if (trackObject < 0)
				{


					Mat roi(hue, selection), maskroi(mask, selection);
					calcHist(&roi, 1, 0, maskroi, hist, 1, &hsize, &phranges);
					normalize(hist, hist, 0, 255, CV_MINMAX);


					KF.transitionMatrix = (Mat_<float>(4, 4) << 1, 0, 1, 0,
						0, 1, 0, 1,
						0, 0, 1, 0,
						0, 0, 0, 1);


					setIdentity(KF.measurementMatrix);
					setIdentity(KF.processNoiseCov, Scalar::all(1e-5));
					setIdentity(KF.measurementNoiseCov, Scalar::all(1e-1));
					setIdentity(KF.errorCovPost, Scalar::all(1));


					//statePost为校正状态，其本质就是前一时刻的状态
					KF.statePost.at<int>(0) = selection.x + selection.width / 2;
					KF.statePost.at<int>(1) = selection.y + selection.height / 2;


					trackWindow = selection;
					trackObject = 1;


				}
				Mat prediction = KF.predict();
				Point predictPt = Point(prediction.at<int>(0) - selection.width / 2, prediction.at<int>(1) - selection.height / 2);
				Rect predictWindow = Rect(predictPt, selection.size());


				/*
				//只对预测目标周围计算投影?????????????????问题


				int m, n;
				m = predictPt.x - 50;
				n = predictPt.y - 50;


				Rect search_window = Rect(m,n, 2*predictWindow.width, 2*predictWindow.height);
				Rect img(0,0,image.cols,image.rows);
				search_window &=img;
				Mat roihue = hue(Rect(m,n, 2*predictWindow.width, 2*predictWindow.height));


				//   roihue &= hue;
				Mat roimask = mask(Rect(m,n, 2*predictWindow.width, 2*predictWindow.height));
				*/
				calcBackProject(&hue, 1, 0, hist, roibackproj, &phranges);
				roibackproj &= mask;
				CamShift(roibackproj, trackWindow, TermCriteria(CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 10, 1));
				/*
				//计算camshift匹配后的trackwindow的直方图相似度
				Mat hist_t, roi_t(hue, trackWindow), maskroi(mask, trackWindow);
				calcHist(&roi_t, 1, 0, maskroi, hist_t, 1, &hsize, &phranges);
				normalize(hist_t, hist_t, 0,255, CV_MINMAX);


				double base_BH = compareHist(hist, hist_t, 3);
				printf("base_BH = %f \n", base_BH);//相似度为0最高


				if ( base_BH >=0.3)
				{


				//将此刻的直方图放入vector<Mat> img中，然后每次与容器里的每一个img比较，
				//前后帧比较，尤其是遮挡或消失重现时候


				}
				*/


				measurement.at<int>(0) = trackWindow.x + trackWindow.width / 2;
				measurement.at<int>(1) = trackWindow.y + trackWindow.height / 2;


				KF.correct(measurement);
				Point statePt = Point(KF.statePost.at<int>(0) - selection.width / 2, KF.statePost.at<int>(1) - selection.height / 2);
				Rect stateWindow = Rect(statePt, selection.size());


				//预测坐标绿色
				rectangle(image, predictWindow, Scalar(0, 255, 0), 3, 8);
				//测量坐标红色
				rectangle(image, trackWindow, Scalar(0, 0, 255), 3, 8);
				//状态坐标蓝色
				rectangle(image, stateWindow, Scalar(255, 0, 0), 3, 8);


			}


		}
		else if (trackObject < 0)
			paused = false;


		if (selectObject && selection.width > 0 && selection.height > 0)
		{


			Mat roi(image, selection);
			bitwise_not(roi, roi);


		}
		imshow("CamShift+Kalman", image);


		char c = (char)waitKey(33);
		if (c == 'q') break;
		else if (c == 's')
			paused = !paused;


	}


	return 0;


}


static inline Point calcPoint(Point2f center, double R, double angle)
{
	return center + Point2f((float)cos(angle), (float)-sin(angle))*(float)R;
}
static void help()
{
	printf("\nExample of c calls to OpenCV's Kalman filter.\n"
		"   Tracking of rotating point.\n"
		"   Rotation speed is constant.\n"
		"   Both state and measurements vectors are 1D (a point angle),\n"
		"   Measurement is the real point angle + gaussian noise.\n"
		"   The real and the estimated points are connected with yellow line segment,\n"
		"   the real and the measured points are connected with red line segment.\n"
		"   (if Kalman filter works correctly,\n"
		"    the yellow segment should be shorter than the red one).\n"
		"\n"
		"   Pressing any key (except ESC) will reset the tracking with a different speed.\n"
		"   Pressing ESC will stop the program.\n"
	);
}
int main22(int, char**)
{
	help();
	Mat img(500, 500, CV_8UC3);
	KalmanFilter KF(2, 1, 0);
	Mat state(2, 1, CV_32F); /* (phi, delta_phi) */
	Mat processNoise(2, 1, CV_32F);
	Mat measurement = Mat::zeros(1, 1, CV_32F);
	char code = (char)-1;
	for (;;)
	{
		randn(state, Scalar::all(0), Scalar::all(0.1));
		KF.transitionMatrix = (Mat_<float>(2, 2) << 1, 1, 0, 1);
		setIdentity(KF.measurementMatrix);
		setIdentity(KF.processNoiseCov, Scalar::all(1e-5));
		setIdentity(KF.measurementNoiseCov, Scalar::all(1e-1));
		setIdentity(KF.errorCovPost, Scalar::all(1));
		randn(KF.statePost, Scalar::all(0), Scalar::all(0.1));
		for (;;)
		{
			Point2f center(img.cols*0.5f, img.rows*0.5f);
			float R = img.cols / 3.f;
			double stateAngle = state.at<float>(0);
			Point statePt = calcPoint(center, R, stateAngle);
			Mat prediction = KF.predict();
			double predictAngle = prediction.at<float>(0);
			Point predictPt = calcPoint(center, R, predictAngle);
			randn(measurement, Scalar::all(0), Scalar::all(KF.measurementNoiseCov.at<float>(0)));
			// generate measurement
			measurement += KF.measurementMatrix*state;
			double measAngle = measurement.at<float>(0);
			Point measPt = calcPoint(center, R, measAngle);
			// plot points
#define drawCross( center, color, d )                                        \
                line( img, Point( center.x - d, center.y - d ),                          \
                             Point( center.x + d, center.y + d ), color, 1, LINE_AA, 0); \
                line( img, Point( center.x + d, center.y - d ),                          \
                             Point( center.x - d, center.y + d ), color, 1, LINE_AA, 0 )
			img = Scalar::all(0);
			drawCross(statePt, Scalar(255, 255, 255), 3);
			drawCross(measPt, Scalar(0, 0, 255), 3);
			drawCross(predictPt, Scalar(0, 255, 0), 3);
			line(img, statePt, measPt, Scalar(0, 0, 255), 3, LINE_AA, 0);
			line(img, statePt, predictPt, Scalar(0, 255, 255), 3, LINE_AA, 0);
			if (theRNG().uniform(0, 4) != 0)
				KF.correct(measurement);
			randn(processNoise, Scalar(0), Scalar::all(sqrt(KF.processNoiseCov.at<float>(0, 0))));
			state = KF.transitionMatrix*state + processNoise;
			imshow("Kalman", img);
			code = (char)waitKey(100);
			if (code > 0)
				break;
		}
		if (code == 27 || code == 'q' || code == 'Q')
			break;
	}
	return 0;
}


#include "opencv2/video/tracking.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <stdio.h>
using namespace cv;
using namespace std;

const int winHeight = 600;
const int winWidth = 800;


Point mousePosition = Point(winWidth >> 1, winHeight >> 1);

//mouse event callback
void mouseEvent(int event, int x, int y, int flags, void *param)
{
	if (event == CV_EVENT_MOUSEMOVE) {
		mousePosition = Point(x, y);
	}
}

void main33(void)
{
	RNG rng;
	//1.kalman filter setup
	const int stateNum = 4;                                      //状态值4×1向量(x,y,△x,△y)
	const int measureNum = 2;                                    //测量值2×1向量(x,y)	
	KalmanFilter KF(stateNum, measureNum, 0);

	KF.transitionMatrix = (Mat_<float>(4, 4) << 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1);  //转移矩阵A
	setIdentity(KF.measurementMatrix);                                             //测量矩阵H
	setIdentity(KF.processNoiseCov, Scalar::all(1e-5));                            //系统噪声方差矩阵Q
	setIdentity(KF.measurementNoiseCov, Scalar::all(1e-1));                        //测量噪声方差矩阵R
	setIdentity(KF.errorCovPost, Scalar::all(1));                                  //后验错误估计协方差矩阵P
	rng.fill(KF.statePost, RNG::UNIFORM, 0, winHeight>winWidth ? winWidth : winHeight);   //初始状态值x(0)
	Mat measurement = Mat::zeros(measureNum, 1, CV_32F);                           //初始测量值x'(0)，因为后面要更新这个值，所以必须先定义

	namedWindow("kalman");
	setMouseCallback("kalman", mouseEvent);

	Mat image(winHeight, winWidth, CV_8UC3, Scalar(0));

	while (1)
	{
		//2.kalman prediction
		Mat prediction = KF.predict();
		Point predict_pt = Point(prediction.at<float>(0), prediction.at<float>(1));   //预测值(x',y')

		//3.update measurement
		measurement.at<float>(0) = (float)mousePosition.x;
		measurement.at<float>(1) = (float)mousePosition.y;

		//4.update
		KF.correct(measurement);

		//draw 
		image.setTo(Scalar(255, 255, 255, 0));
		circle(image, predict_pt, 5, Scalar(0, 255, 0), 3);    //predicted point with green
		circle(image, mousePosition, 5, Scalar(255, 0, 0), 3); //current position with red		

		char buf[256];
		sprintf_s(buf, 256, "predicted position:(%3d,%3d)", predict_pt.x, predict_pt.y);
		putText(image, buf, Point(10, 30), CV_FONT_HERSHEY_SCRIPT_COMPLEX, 1, Scalar(0, 0, 0), 1, 8);
		sprintf_s(buf, 256, "current position :(%3d,%3d)", mousePosition.x, mousePosition.y);
		putText(image, buf, cvPoint(10, 60), CV_FONT_HERSHEY_SCRIPT_COMPLEX, 1, Scalar(0, 0, 0), 1, 8);

		imshow("kalman", image);
		int key = waitKey(3);
		if (key == 27) {//esc   
			break;
		}
	}
}