// Copyright 2018 Slightech Co., Ltd. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "camera_p.h"

#include <stdexcept>

#include <opencv2/imgproc/imgproc.hpp>

#include "log.hpp"



using namespace mynteye;



CameraPrivate::CameraPrivate(Camera *q)
	: q_ptr(q), etron_di_(nullptr), dev_sel_info_({ -1 }), stream_info_dev_index_(-1) {
	DBG_LOGD(__func__);

	/*! \fn int EtronDI_Init(
	void **ppHandleEtronDI,
	bool bIsLogEnabled)
	\brief entry point of Etron camera SDK. This API allocates resource and find all the eSPI camera devices connected to the system.
	\param ppHandleEtronDI	a pointer of pointer to receive EtronDI SDK instance
	\param bIsLogEnabled	set to true to generate log file, named log.txt in current folder
	\return success: none negative integer to indicate numbers of devices found in the system.

	*/
	int ret = EtronDI_Init(&etron_di_, false);
	DBG_LOGI("EtronDI_Init: %d", ret);
	unused(ret);

	stream_color_info_ptr_ = (PETRONDI_STREAM_INFO)malloc(sizeof(ETRONDI_STREAM_INFO) * 64);
	stream_depth_info_ptr_ = (PETRONDI_STREAM_INFO)malloc(sizeof(ETRONDI_STREAM_INFO) * 64);
	color_res_index_ = 0;
	depth_res_index_ = 0;

	framerate_ = 30;

	depth_img_buf_ = nullptr;
}

CameraPrivate::~CameraPrivate() {
    DBG_LOGD(__func__);
    EtronDI_Release(&etron_di_);

    free(stream_color_info_ptr_);
    free(stream_depth_info_ptr_);

    Close();
}

void CameraPrivate::GetDevices(std::vector<DeviceInfo> &dev_infos) {
	dev_infos.clear();

	int count = EtronDI_GetDeviceNumber(etron_di_);
	DBG_LOGD("EtronDI_GetDeviceNumber: %d", count);

	DEVSELINFO dev_sel_info;
	DEVINFORMATION *p_dev_info = (DEVINFORMATION*)malloc(sizeof(DEVINFORMATION)*count);

	for (int i = 0; i < count; i++) {
		dev_sel_info.index = i;

		EtronDI_GetDeviceInfo(etron_di_, &dev_sel_info, p_dev_info + i);

		char sz_buf[256];
		int actual_length = 0;
		if (ETronDI_OK == EtronDI_GetFwVersion(etron_di_, &dev_sel_info, sz_buf, 256, &actual_length)) {
			DeviceInfo info;
			info.index = i;
			info.name = p_dev_info[i].strDevName;
			info.type = p_dev_info[i].nDevType;
			info.pid = p_dev_info[i].wPID;
			info.vid = p_dev_info[i].wVID;
			info.chip_id = p_dev_info[i].nChipID;
			info.fw_version = sz_buf;
			dev_infos.push_back(std::move(info));
		}
	}

	free(p_dev_info);
}

void CameraPrivate::GetResolutions(const std::int32_t &dev_index,
	std::vector<StreamInfo> &color_infos, std::vector<StreamInfo> &depth_infos) {
	color_infos.clear();
	depth_infos.clear();

	memset(stream_color_info_ptr_, 0, sizeof(ETRONDI_STREAM_INFO) * 64);
	memset(stream_depth_info_ptr_, 0, sizeof(ETRONDI_STREAM_INFO) * 64);

	DEVSELINFO dev_sel_info{ dev_index };
	EtronDI_GetDeviceResolutionList(etron_di_, &dev_sel_info, 64, stream_color_info_ptr_, 64, stream_depth_info_ptr_);

	PETRONDI_STREAM_INFO stream_temp_info_ptr = stream_color_info_ptr_;
	int i = 0;
	while (i < 64) {
		if (stream_temp_info_ptr->nWidth > 0) {
			StreamInfo info;
			info.index = i;
			info.width = stream_temp_info_ptr->nWidth;
			info.height = stream_temp_info_ptr->nHeight;
			info.format = stream_temp_info_ptr->bFormatMJPG ? StreamFormat::STREAM_MJPG : StreamFormat::STREAM_YUYV;
			color_infos.push_back(info);
		}
		stream_temp_info_ptr++;
		i++;
	}

	stream_temp_info_ptr = stream_depth_info_ptr_;
	i = 0;
	while (i < 64) {
		if (stream_temp_info_ptr->nWidth > 0) {
			StreamInfo info;
			info.index = i;
			info.width = stream_temp_info_ptr->nWidth;
			info.height = stream_temp_info_ptr->nHeight;
			info.format = stream_temp_info_ptr->bFormatMJPG ? StreamFormat::STREAM_MJPG : StreamFormat::STREAM_YUYV;
			depth_infos.push_back(info);
		}
		stream_temp_info_ptr++;
		i++;
	}

	stream_info_dev_index_ = dev_index;
}

ErrorCode CameraPrivate::SetAutoExposureEnabled(bool enabled) {
	bool ok;
	if (enabled) {
		ok = ETronDI_OK == EtronDI_EnableAE(etron_di_, &dev_sel_info_);
	}
	else {
		ok = ETronDI_OK == EtronDI_DisableAE(etron_di_, &dev_sel_info_);
	}
	if (ok) {
		LOGI("-- Auto-exposure state: %s", enabled ? "enabled" : "disabled");
	}
	else {
		LOGW("-- %s auto-exposure failed", enabled ? "Enable" : "Disable");
	}
	return ok ? ErrorCode::SUCCESS : ErrorCode::ERROR_FAILURE;
}

ErrorCode CameraPrivate::SetAutoWhiteBalanceEnabled(bool enabled) {
	bool ok;
	if (enabled) {
		ok = ETronDI_OK == EtronDI_EnableAWB(etron_di_, &dev_sel_info_);
	}
	else {
		ok = ETronDI_OK == EtronDI_DisableAWB(etron_di_, &dev_sel_info_);
	}
	if (ok) {
		LOGI("-- Auto-white balance state: %s", enabled ? "enabled" : "disabled");
	}
	else {
		LOGW("-- %s auto-white balance failed", enabled ? "Enable" : "Disable");
	}
	return ok ? ErrorCode::SUCCESS : ErrorCode::ERROR_FAILURE;
}

ErrorCode CameraPrivate::Open(const InitParams &params) {
	dev_sel_info_.index = params.dev_index;
	depth_data_type_ = 2;
	EtronDI_SetDepthDataType(etron_di_, &dev_sel_info_, depth_data_type_);
	DBG_LOGI("EtronDI_SetDepthDataType: %d", depth_data_type_);

	SetAutoExposureEnabled(params.state_ae);
	SetAutoWhiteBalanceEnabled(params.state_awb);

	if (params.framerate > 0) framerate_ = params.framerate;
	LOGI("-- Framerate: %d", framerate_);

	depth_mode_ = params.depth_mode;

	if (params.dev_index != stream_info_dev_index_) {
		std::vector<StreamInfo> color_infos;
		std::vector<StreamInfo> depth_infos;
		GetResolutions(params.dev_index, color_infos, depth_infos);
	}
	if (params.color_info_index > -1) {
		color_res_index_ = params.color_info_index;
	}
	if (params.depth_info_index > -1) {
		depth_res_index_ = params.depth_info_index;
	}
	LOGI("-- Color Stream: %dx%d %s",
		stream_color_info_ptr_[color_res_index_].nWidth,
		stream_color_info_ptr_[color_res_index_].nHeight,
		stream_color_info_ptr_[color_res_index_].bFormatMJPG ? "MJPG" : "YUYV");
	LOGI("-- Depth Stream: %dx%d %s",
		stream_depth_info_ptr_[depth_res_index_].nWidth,
		stream_depth_info_ptr_[depth_res_index_].nHeight,
		stream_depth_info_ptr_[depth_res_index_].bFormatMJPG ? "MJPG" : "YUYV");

	if (depth_data_type_ != 1 && depth_data_type_ != 2) {
		throw std::runtime_error(format_string("Error: Depth data type (%d) not supported.", depth_data_type_));
	}

	if (params.ir_intensity >= 0) {
		if (SetFWRegister(0xE0, params.ir_intensity)) {
			LOGI("-- IR intensity: %d", params.ir_intensity);
		}
		else {
			LOGI("-- IR intensity: %d (failed)", params.ir_intensity);
		}
	}

	ReleaseBuf();

	// int EtronDI_OpenDeviceEx(
	//     void* pHandleEtronDI,
	//     PDEVSELINFO pDevSelInfo,
	//     int colorStreamIndex,
	//     bool toRgb,
	//     int depthStreamIndex,
	//     int depthStreamSwitch,
	//     EtronDI_ImgCallbackFn callbackFn,
	//     void* pCallbackParam,
	//     int* pFps,
	//     BYTE ctrlMode)

	bool toRgb = true;
	// Depth0: none
	// Depth1: unshort
	// Depth2: ?
	int depthStreamSwitch = EtronDIDepthSwitch::Depth1;
	// 0x01: color and depth frame output synchrously, for depth map module only
	// 0x02: enable post-process, for Depth Map module only
	// 0x04: stitch images if this bit is set, for fisheye spherical module only
	// 0x08: use OpenCL in stitching. This bit effective only when bit-2 is set.
	BYTE ctrlMode = 0x01;

	int ret = EtronDI_OpenDeviceEx(etron_di_, &dev_sel_info_,
		color_res_index_, toRgb,
		depth_res_index_, depthStreamSwitch,
		CameraPrivate::ImgCallback, this, &framerate_, ctrlMode);

	if (ret == ETronDI_OK) {
		return ErrorCode::SUCCESS;
	}
	else {
		dev_sel_info_.index = -1;  // reset flag
		return ErrorCode::ERROR_CAMERA_OPEN_FAILED;
	}
}

bool CameraPrivate::IsOpened() {
	return dev_sel_info_.index != -1;
}

void CameraPrivate::ImgCallback(EtronDIImageType::Value imgType, int imgId,
	unsigned char *imgBuf, int imgSize, int width, int height,
	int serialNumber, void *pParam) {
	CameraPrivate *p = static_cast<CameraPrivate *>(pParam);
	std::lock_guard<std::mutex> _(p->mtx_imgs_);
	if (EtronDIImageType::IsImageColor(imgType)) {

	}
	else if (EtronDIImageType::IsImageDepth(imgType)) {
		// LOGI("Image callback depth");
		if (!p->depth_img_buf_) {
			unsigned int depth_img_width = (unsigned int)(p->stream_depth_info_ptr_[p->depth_res_index_].nWidth);
			unsigned int depth_img_height = (unsigned int)(p->stream_depth_info_ptr_[p->depth_res_index_].nHeight);
			p->depth_img_buf_ = (unsigned char*)calloc(depth_img_width*depth_img_height * 2, sizeof(unsigned char));
			p->depth_data_size_ = depth_img_width * depth_img_height * 2;
		}
		memcpy(p->depth_img_buf_, imgBuf, p->depth_data_size_);
	}
	else {
		LOGE("Image callback failed. Unknown image type.");
	}
}

ErrorCode CameraPrivate::RetrieveDepth() {
	if (!IsOpened())return ErrorCode::ERROR_CAMERA_NOT_OPENED;
	if (!depth_img_buf_) {
		return ErrorCode::ERROR_CAMERA_RETRIEVE_FAILED;
	}

	std::lock_guard<std::mutex> _(mtx_imgs_);

	bool depth_ok = false;
	if (depth_img_buf_) {
		unsigned int depth_img_width = (unsigned int)(stream_depth_info_ptr_[depth_res_index_].nWidth);
		unsigned int depth_img_height = (unsigned int)(stream_depth_info_ptr_[depth_res_index_].nHeight);

		unsigned int point_x = depth_img_width >> 1;
		unsigned int point_y = depth_img_height >> 1;

		int index = point_y * depth_img_width * 2 + point_x * 2;
		depth_min = ushort(depth_img_buf_[index + 1]) << 8;
		depth_min += ushort(depth_img_buf_[index]);
		depth_ok = true;
	}

	if (depth_ok) {
		return ErrorCode::SUCCESS;
	}
	else {
		return ErrorCode::ERROR_CAMERA_RETRIEVE_FAILED;
	}
}

ushort CameraPrivate::GetMinDepth() {
	return depth_min;
}

void CameraPrivate::Close() {
	if (dev_sel_info_.index != -1) {
		EtronDI_CloseDevice(etron_di_, &dev_sel_info_);
		dev_sel_info_.index = -1;
	}
	ReleaseBuf();
}

void CameraPrivate::ReleaseBuf() {
	if (depth_img_buf_) {
		delete depth_img_buf_;
		depth_img_buf_ = nullptr;
	}
}

bool CameraPrivate::GetSensorRegister(int id, unsigned short address, unsigned short *value, int flag) {
	if (!IsOpened()) throw std::runtime_error("Error: Camera not opened.");
#ifdef OS_WIN
	return ETronDI_OK == EtronDI_GetSensorRegister(etron_di_, &dev_sel_info_, id, address, value, flag, 2);
#else
	return ETronDI_OK == EtronDI_GetSensorRegister(etron_di_, &dev_sel_info_, id, address, value, flag, SENSOR_BOTH);
#endif
}

bool CameraPrivate::GetHWRegister(unsigned short address, unsigned short *value, int flag) {
	if (!IsOpened()) throw std::runtime_error("Error: Camera not opened.");
	return ETronDI_OK == EtronDI_GetHWRegister(etron_di_, &dev_sel_info_, address, value, flag);
}

bool CameraPrivate::GetFWRegister(unsigned short address, unsigned short *value, int flag) {
	if (!IsOpened()) throw std::runtime_error("Error: Camera not opened.");
	return ETronDI_OK == EtronDI_GetFWRegister(etron_di_, &dev_sel_info_, address, value, flag);
}

bool CameraPrivate::SetSensorRegister(int id, unsigned short address, unsigned short value, int flag) {
	if (!IsOpened()) throw std::runtime_error("Error: Camera not opened.");
#ifdef OS_WIN
	return ETronDI_OK == EtronDI_SetSensorRegister(etron_di_, &dev_sel_info_, id, address, value, flag, 2);
#else
	return ETronDI_OK == EtronDI_SetSensorRegister(etron_di_, &dev_sel_info_, id, address, value, flag, SENSOR_BOTH);
#endif
}

bool CameraPrivate::SetHWRegister(unsigned short address, unsigned short value, int flag) {
	if (!IsOpened()) throw std::runtime_error("Error: Camera not opened.");
	return ETronDI_OK == EtronDI_SetHWRegister(etron_di_, &dev_sel_info_, address, value, flag);
}

bool CameraPrivate::SetFWRegister(unsigned short address, unsigned short value, int flag) {
	if (!IsOpened()) throw std::runtime_error("Error: Camera not opened.");
	return ETronDI_OK == EtronDI_SetFWRegister(etron_di_, &dev_sel_info_, address, value, flag);
}