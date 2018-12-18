// Sample.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "ThicknessChecker.h"
#include <vector>
#include <iostream>
#include <fstream>

#include "igl/jet.h"

// see https://github.com/ephtracy/voxel-model/blob/master/MagicaVoxel-file-format-vox.txt
////
// implementation
////
#if defined(_WIN32) || defined(_WIN64)
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __attribute__((visibility("default")))
#endif

extern "C" DLLEXPORT float detectThickness(char* someText, double optValue, char* pOptBuffer1, int optBuffer1Size, char* pOptBuffer2, int optBuffer2Size, char** zData)
{
	//// input
	// someText: file name to be opened
	// input filename, output filename (thickness as color), outputfilename (thickness (segment) as group)
	// optValue: encoded value for minimumThickness, betterThickness, height
	////
	// decode parameters
	//const double minimumThickness = optValue - std::floor(optValue / 1024) * 1024;
	//const double preferredThickness = optValue / 1024.0 - std::floor(optValue / (1024.0 * 1024.0)) * 1024.0;
	//const double height = optValue / (1024.0 * 1024.0) - std::floor(optValue / (1024.0 * 1024.0 * 1024.0)) * 1024.0;
	std::string ZBtext(someText);
	std::string separator(",");
	size_t separator_length = separator.length();

	std::vector<std::string> ZBtextList({});

	if (separator_length == 0) {
		ZBtextList.push_back(ZBtext);
	}
	else {
		size_t offset = std::string::size_type(0);
		while (true) {
			size_t pos = ZBtext.find(separator, offset);
			if (pos == std::string::npos) {
				ZBtextList.push_back(ZBtext.substr(offset));
				break;
			}
			ZBtextList.push_back(ZBtext.substr(offset, pos - offset));
			offset = pos + separator_length;
		}
	}

	std::string tmp;
	for (int i = 1; i < 5; ++i)
	{
		tmp = ZBtextList.at(i);
		ZBtextList.at(i) = ZBtextList.at(0) + tmp;
	}

	std::ifstream ifs(ZBtextList.at(2), std::ios::in | std::ios::binary);
	if (!ifs)
	{
		std::cerr << "some error in opening file..." << std::endl;
	}
	float height;
	float preferredThickness;
	float minimumThickness;

	ifs.read((char*)&height, sizeof(float));
	ifs.read((char*)&preferredThickness, sizeof(float));
	ifs.read((char*)&minimumThickness, sizeof(float));
	ifs.close();

#if !(defined(_WIN32) || defined(_WIN64))
	// if Mac, ZBrush gives me invalid prefix with FileNameResolvePath ...
	for (auto& s : ZBtextList)
	{
		s.erase(s.begin(), s.begin() + 2);
	}
#endif

	Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> V;
	Eigen::Matrix<   int, Eigen::Dynamic, Eigen::Dynamic> VC;
	Eigen::Matrix<   int, Eigen::Dynamic, Eigen::Dynamic> F;
	Eigen::Matrix<   int, Eigen::Dynamic, Eigen::Dynamic> FG;
	Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> F_RAWSDF;
	Eigen::Matrix<   int, Eigen::Dynamic, Eigen::Dynamic> F_Segment;

	// read triangle from file
	read_OBJ(ZBtextList.at(1), V, F, VC, FG);

	// scale for make height (Y) is user-given height.
	const double scale = (height / (V.col(1).maxCoeff() - V.col(1).minCoeff()));

	CGAL_SDF(V*scale, F, F_RAWSDF, F_Segment);

	// convert F_SDF to V_SDF (simple average)
	Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> V_SDF;
	Eigen::Matrix<   int, Eigen::Dynamic, Eigen::Dynamic> V_Count;
	V_SDF.resize(V.rows(), 1);
	V_SDF.setZero();
	V_Count.resize(V.rows(), 1);
	V_Count.setZero();
	for (int f = 0; f < F.rows(); ++f)
	{
		for (int v = 0; v < F.cols(); ++v)
		{
			V_SDF(F(f, v), 0) += F_RAWSDF(f, 0);
			V_Count(F(f, v), 0) += 1;
		}
	}
	for (int v = 0; v < V.rows(); ++v)
	{
		V_SDF(v, 0) /= double(V_Count(v, 0));
	}

	// export with
	// jet color (update polypaint, keep polygroup)
	// segment   (update polygroup, keep polypaint)

	Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> VC_Thicknessd;
	Eigen::Matrix<   int, Eigen::Dynamic, Eigen::Dynamic> VC_Thicknessi;
	igl::jet(V_SDF, preferredThickness, minimumThickness, VC_Thicknessd);
	VC_Thicknessi.resize(VC_Thicknessd.rows(), 4);
	for (int v = 0; v < V.rows(); ++v)
	{
		VC_Thicknessi(v, 0) = 255; // A (M?)
		VC_Thicknessi(v, 1) = int(VC_Thicknessd(v, 0) * 255); // R
		VC_Thicknessi(v, 2) = int(VC_Thicknessd(v, 1) * 255); // G
		VC_Thicknessi(v, 3) = int(VC_Thicknessd(v, 2) * 255); // B
	}

	write_OBJ(ZBtextList.at(3), V, F, VC_Thicknessi, FG);
	write_OBJ(ZBtextList.at(4), V, F, VC, F_Segment);

	return 0.0f;
}