/*
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>

using namespace cv;
using namespace std;

int main(int argc, char* argv[])
{
VideoCapture video(0);

Mat srcImage;

while (video.read(srcImage)) {

Mat srcGrayImage;
if (srcImage.channels() == 3)
cvtColor(srcImage, srcGrayImage, CV_RGB2GRAY);
else
srcImage.copyTo(srcGrayImage);

Mat new_srcGrayImage;

GaussianBlur(srcGrayImage, new_srcGrayImage, Size(5, 5), 0, 0);
vector<KeyPoint>detectKeyPoint;
Mat keyPointImage;

Ptr<FastFeatureDetector> fast = FastFeatureDetector::create();
fast->detect(new_srcGrayImage, detectKeyPoint);
//drawKeypoints(srcImage, detectKeyPoint, keyPointImage, Scalar(0, 0, 255), DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
drawKeypoints(srcImage, detectKeyPoint, keyPointImage, Scalar(0, 0, 255), DrawMatchesFlags::DEFAULT);


imshow("src image", srcImage);
imshow("keyPoint image2", keyPointImage);

int k = waitKey(1);
if (k == 27) break;   //ESC
}
return 0;
}
*/



#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/features2d/features2d.hpp"
#include <deque>
#include <iostream>
#include <ctype.h>
#include <windows.h>
#include <string>
#include <stdio.h>

using namespace cv;
using namespace std;

#define PI 3.1415926

deque<vector<Point2f>> windows;
const int MAX_WINDOWS_SIZE = 60;
const double MIN_DISTANCE = 3;
const int circle_num = 4;
bool clockwise[circle_num] = { true, false, true, false };
double phase0[circle_num] = { .0, .0, PI, PI };

static void help()
{
	// print a welcome message, and the OpenCV version
	cout << "\nThis is a demo of Lukas-Kanade optical flow lkdemo(),\n"
		"Using OpenCV version " << CV_VERSION << endl;
	cout << "\nIt uses camera by default, but you can provide a path to video as an argument.\n";
	cout << "\nHot keys: \n"
		"\tESC - quit the program\n"
		"\tr - auto-initialize tracking\n"
		"\tc - delete all the points\n"
		"\tn - switch the \"night\" mode on/off\n"
		"To add/remove a feature point click it\n" << endl;
}

//Point2f point;
//bool addRemovePt = false;

/*
static void onMouse(int event, int x, int y, int flags, void* param)
{
if (event == CV_EVENT_LBUTTONDOWN)
{
point = Point2f((float)x, (float)y);
addRemovePt = true;
}
}
*/

Point2f get_coordinate(int i, double t)
{
	double arc = phase0[i] + t * PI;
	return Point2f((float)cos(arc), (float)sin(arc));
}

bool acceptTrackedPoint(int i)
{
	double cnt = 0;
	for (int j = 1; j < windows.size(); ++j)
		//cout << windows.at(j)[i] << ' ' << windows.at(j - 1)[i] << endl;
		cnt += (abs(windows.at(j)[i].x - windows.at(j - 1)[i].x) + abs(windows.at(j)[i].y - windows.at(j - 1)[i].y));
	if (windows.size() <= 1) return true;

	cnt = cnt / (windows.size() - 1);
	//cout << cnt << endl;
	if (cnt > MIN_DISTANCE)
		return true; // ˵�����ƶ�
	return false;
}

template<class InputIt1, class InputIt2>
double pearson(InputIt1 firstX, InputIt2 firstY, int n) {
	double xy_sum = inner_product(firstX, firstX + n, firstY, 0);
	double x2_sum = inner_product(firstX, firstX + n, firstX, 0);
	double y2_sum = inner_product(firstY, firstY + n, firstY, 0);
	double x_sum = accumulate(firstX, firstX + n, 0);
	double y_sum = accumulate(firstY, firstY + n, 0);

	double deno = sqrt((x2_sum - 1.0 * pow(x_sum, 2) / n)*(y2_sum - 1.0 * pow(y_sum, 2) / n));
	return (xy_sum - 1.0 * x_sum * y_sum / n) / deno;
}

int main(int argc, char** argv)
{
	help();
	
	double start = (double)getTickCount();
	VideoCapture cap;
	TermCriteria termcrit(CV_TERMCRIT_ITER | CV_TERMCRIT_EPS, 20, 0.03);
	Size  winSize(51, 51);
	//Size subPixWinSize(10, 10);

	const int MAX_COUNT = 500;
	bool needToInit = true;
	bool nightMode = false;
	const int threshold = 20;

	char fps_string[10];

	if (argc == 1 || (argc == 2 && strlen(argv[1]) == 1 && isdigit(argv[1][0])))
		cap.open(argc == 2 ? argv[1][0] - '0' : 0);
	else if (argc == 2)
		cap.open(argv[1]);

	if (!cap.isOpened())
	{
		cout << "Could not initialize capturing...\n";
		return 0;
	}

	namedWindow("LK Demo", 1);
	//setMouseCallback("LK Demo", onMouse, 0);

	Mat gray, prevGray, image;
	vector<Point2f> points[2]; // 0 for prev, 1 for next

	Ptr<FastFeatureDetector> m_fastDetector = FastFeatureDetector::create(threshold);
	vector<KeyPoint> keyPoints;

	double t = 0;
	double fps = 0;
	for (int cnt = 0;; cnt++)
	{
		t = (double)getTickCount();

		Mat frame;
		cap >> frame;
		if (frame.empty())
			break;

		frame.copyTo(image);
		cvtColor(image, gray, COLOR_BGR2GRAY);

		Mat tmp_image;
		GaussianBlur(image, tmp_image, Size(5, 5), 0, 0); //  Gauss

		image = tmp_image;

		if (nightMode)
			image = Scalar::all(0);

		if (needToInit)
		{
			// automatic initialization
			//goodFeaturesToTrack(gray, points[1], MAX_COUNT, 0.01, 10, Mat(), 3, false, 0.04);
			//cornerSubPix(gray, points[1], subPixWinSize, Size(-1, -1), termcrit);

			m_fastDetector->detect(gray, keyPoints);
			KeyPoint::convert(keyPoints, points[1]);

			windows.clear();
			//addRemovePt = false;
		}
		else if (!points[0].empty())
		{
			vector<uchar> status;
			vector<float> err;
			if (prevGray.empty())
				gray.copyTo(prevGray);
			calcOpticalFlowPyrLK(prevGray, gray, points[0], points[1], status, err, winSize,
				3, termcrit, 0, 0.001);
			windows.push_back(points[1]);
			if (windows.size() > MAX_WINDOWS_SIZE)
				windows.pop_front();

			size_t i, k;
			for (i = k = 0; i < points[1].size(); i++)
			{
				/*
				if (addRemovePt)
				{
				if (norm(point - points[1][i]) <= 5)
				{
				addRemovePt = false;
				continue;
				}
				}
				*/
				if (!status[i]) //missing
					continue;
				if (!acceptTrackedPoint(int(i)))  //relative stable --> non-candidate
					circle(image, points[1][i], 3, Scalar(255, 0, 0), -1, 8);
				else
					circle(image, points[1][i], 3, Scalar(0, 255, 0), -1, 8);
				//points[1][k++] = points[1][i];

			}
			//points[1].resize(k);
		}
		/*
		if (addRemovePt && points[1].size() < (size_t)MAX_COUNT)
		{
		vector<Point2f> tmp;
		tmp.push_back(point);
		cornerSubPix(gray, tmp, winSize, cvSize(-1, -1), termcrit);
		points[1].push_back(tmp[0]);
		addRemovePt = false;
		}
		*/
		needToInit = false;
		if (cnt == 120)
		{
			needToInit = true;
			cnt = 0;
		}

		t = ((double)getTickCount() - t) / getTickFrequency();
		fps = 1.0 / t;
		sprintf(fps_string, "%.2f", fps);

		string fps_string_show("fps:");
		fps_string_show += fps_string;
		putText(image, fps_string_show,
			cv::Point(5, 20),           // �������꣬�����½�Ϊԭ��
			cv::FONT_HERSHEY_SIMPLEX,   // ��������
			0.5, // �����С
			cv::Scalar(0, 0, 0));       // ������ɫ

		imshow("LK Demo", image);

		char c = (char)waitKey(10);
		if (c == 27)
			break;
		switch (c)
		{
		case 'r':
			needToInit = true;
			break;
		case 'c':
			points[0].clear();
			points[1].clear();
			break;
		case 'n':
			nightMode = !nightMode;
			break;
		}

		std::swap(points[1], points[0]);
		cv::swap(prevGray, gray);
	}

	return 0;
}

