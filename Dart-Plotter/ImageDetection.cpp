#include "ImageDetection.h"
#include <fstream>
#include <iostream>
#include <windows.h>
#include <iomanip>

constexpr float CONFIDENCE_THRESHOLD = 0.0;
constexpr float NMS_THRESHOLD = 0.4;
constexpr int NUM_CLASSES = 2;

float ImageDetection::GetDistance(cv::Rect a, cv::Rect b)
{
	cv::Vec2f delta = cv::Vec2f{ (float)a.x - (float)b.x, (float)a.y - (float)b.y };
	return sqrt(delta.dot(delta));
}

void ImageDetection::LoadHoles()
{
	std::vector<std::string> class_names = { "center", "hole" };
	auto net = cv::dnn::readNetFromDarknet(config, weight);

	net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
	net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

	auto output_names = net.getUnconnectedOutLayersNames();

	cv::Mat blob;
	std::vector<cv::Mat> detections;

	auto total_start = std::chrono::steady_clock::now();
	cv::dnn::blobFromImage(image, blob, 0.00392, cv::Size(960, 960), cv::Scalar(), true, false, CV_32F);
	net.setInput(blob);

	auto dnn_start = std::chrono::steady_clock::now();
	net.forward(detections, output_names);
	auto dnn_end = std::chrono::steady_clock::now();

	std::vector<int> indices[NUM_CLASSES];
	std::vector<cv::Rect> boxes[NUM_CLASSES];
	std::vector<float> scores[NUM_CLASSES];

	for (auto& output : detections)
	{
		const auto num_boxes = output.rows;
		for (int i = 0; i < num_boxes; i++)
		{
			auto x = output.at<float>(i, 0) * image.cols;
			auto y = output.at<float>(i, 1) * image.rows;
			auto width = output.at<float>(i, 2) * image.cols;
			auto height = output.at<float>(i, 3) * image.rows;
			cv::Rect rect(x - width / 2, y - height / 2, width, height);

			for (int c = 0; c < NUM_CLASSES; c++)
			{
				auto confidence = *output.ptr<float>(i, 5 + c);
				if (confidence >= CONFIDENCE_THRESHOLD)
				{
					boxes[c].push_back(rect);
					scores[c].push_back(confidence);
				}
			}
		}
	}

	for (int c = 0; c < NUM_CLASSES; c++)
		cv::dnn::NMSBoxes(boxes[c], scores[c], 0.0, NMS_THRESHOLD, indices[c]);


	for (size_t i = 0; i < indices[1].size(); ++i)
	{
		auto idx = indices[1][i];
		holes.push_back(boxes[1][idx]);
	}
	for (size_t i = 0; i < indices[0].size(); ++i)
	{
		auto idx = indices[0][i];
		center.push_back(boxes[0][idx]);
	}
}

bool ImageDetection::init()
{
	std::ifstream file(image_path, std::ios::binary | std::ios::ate);
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> image_data;
	image_data.resize(size);
	if (!file.read(image_data.data(), size))
	{
		std::wcout << L"Could not read the image: \"" << image_path << L"\" error code: " << GetLastError() << std::endl;
		return false;
	}
	image = cv::imdecode(image_data, 1);
	if (image.empty())
	{
		std::wcout << L"Could not read the image: \"" << image_path << L"\" error code: " << GetLastError() << std::endl;
		return false;
	}
	try
	{
		LoadHoles();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what();
		return false;
	}

	LoadDistances();
	return true;
}

void ImageDetection::LoadDistances()
{
	distances.clear();
	distance_counts.clear();
	if (center.size() > 0) {
		for (size_t i = 0; i < holes.size(); i++)
		{
			distances.push_back(GetDistance(holes[i], center[0]));
		}
		for (size_t i = 0; i < distances.size(); i++)
		{
			distance_counts[((int)(distances[i] / 100) * 100)]++;
		}
	}
}

ImageDetection::ImageDetection(std::wstring file_path, std::string cfg_file, std::string weight_file) : image_path(file_path), config(cfg_file), weight(weight_file)
{}