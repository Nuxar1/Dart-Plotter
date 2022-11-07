#pragma once
#include <iostream>
#include <string>
#include <vector>

#include "ImGui/imgui.h"

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/dnn/all_layers.hpp>
#include <opencv2/core/directx.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

class ImageDetection
{
private:
	std::wstring image_path;
	std::string config;
	std::string weight;
	float GetDistance(cv::Rect a, cv::Rect b);
	void LoadHoles();
public:
	bool init();
	void LoadDistances();
	ImageDetection(std::wstring file_path, std::string cfg_file = "yolov4-tiny-3l-obj.cfg", std::string weights = "yolov4-tiny-3l-obj_final.weights");
	std::vector<cv::Rect> holes;
	std::map<float, int>distance_counts;
	std::vector<float> distances;

	std::vector<cv::Rect> center;
	cv::Mat image;
	
	std::vector<ImVec2> scale_points[2];
};