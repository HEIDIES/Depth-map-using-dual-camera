#include <iomanip>
#include <iostream>

#include <windows.h>

#include "camera.h"

#define WIN_FLAGS cv::WINDOW_AUTOSIZE

using namespace std;
using namespace mynteye;

int main(int argc, char const *argv[]) {
	string dashes(80, '-');

	Camera cam;

	DeviceInfo dev_info;
	{
		vector<DeviceInfo> dev_infos = cam.GetDevices();
		size_t n = dev_infos.size();
		if (n <= 0) {
			cerr << "Error: Device not found" << endl;
			return 1;
		}

		cout << dashes << endl;
		cout << "Index | Device Information" << endl;
		cout << dashes << endl;
		for (auto &&info : dev_infos) {
			cout << setw(5) << info.index << " | " << info << endl;
		}
		cout << dashes << endl;
		dev_info = dev_infos[0];

	}

	{
		vector<StreamInfo> color_infos;
		vector<StreamInfo> depth_infos;
		cam.GetResolutions(dev_info.index, color_infos, depth_infos);

		cout << dashes << endl;
		cout << "Index | Color Stream Information" << endl;
		cout << dashes << endl;
		for (auto &&info : color_infos) {
			cout << setw(5) << info.index << " | " << info << endl;
		}
		cout << dashes << endl << endl;

		cout << dashes << endl;
		cout << "Index | Depth Stream Information" << endl;
		cout << dashes << endl;
		for (auto &&info : depth_infos) {
			cout << setw(5) << info.index << " | " << info << endl;
		}
		cout << dashes << endl << endl;
	}

	cout << "Open device: " << dev_info.index << ", " << dev_info.name << endl << endl;

	// Warning: Color stream format MJPG doesn't work.
	InitParams params(dev_info.index);
	params.depth_mode = DepthMode::DEPTH_NON_16UC1;
	params.color_info_index = 4;
	params.depth_info_index = 1;
	params.ir_intensity = 3;

	cam.Open(params);

	cout << endl;
	if (!cam.IsOpened()) {
		cerr << "Error: Open camera failed" << endl;
		return 1;
	}
	cout << "Open device success" << endl << endl;

	cout << "\033[1;32mPress ESC/Q on Windows to terminate\033[0m" << endl;

	cv::namedWindow("color", WIN_FLAGS);
	cv::namedWindow("depth", WIN_FLAGS);

	DepthRegion depth_region(3);


	ushort depthAtCenter;
	for (;;) {
		cout << "The depth of center is : " << cam.RetrieveDepth << endl;
		sleep(10);
	}

	cam.Close();
	return 0;
}