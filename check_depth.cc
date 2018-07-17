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
	// You can choose the resolution of each camera.
	params.color_info_index = 4;
	params.depth_info_index = 1;
	// You can choose the intensity of infrared light.
	params.ir_intensity = 3;

	// Open the camera, start auto exposure till close.
	cam.Open(params);

	cout << endl;
	if (!cam.IsOpened()) {
		cerr << "Error: Open camera failed" << endl;
		return 1;
	}
	cout << "Open device success" << endl << endl;

	cout << "\033[1;32mPress ESC/Q on Windows to terminate\033[0m" << endl;

	ushort prevsDepth = 0;
	
	for (;;) {
		// Retrieve and get the depth at the center.
		if (cam.RetrieveDepth() == ErrorCode::SUCCESS) {
			ushort depth = cam.GetMinDepth();
			depth /= 10;
			if (depth >= 20 && depth <= 80) { // Range of depth is [20, 80]
				if(abs(depth - prevsDepth) >= 1)
					cout << "The depth at center is : " << depth << "cm" << endl;
				prevsDepth = depth;
			}
			Sleep(100);
		}
	}

	cam.Close();
	return 0;
}