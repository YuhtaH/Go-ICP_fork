/********************************************************************
Main Function for point cloud registration with Go-ICP Algorithm
Last modified: Feb 13, 2014

"Go-ICP: Solving 3D Registration Efficiently and Globally Optimally"
Jiaolong Yang, Hongdong Li, Yunde Jia
International Conference on Computer Vision (ICCV), 2013

Copyright (C) 2013 Jiaolong Yang (BIT and ANU)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include <time.h>
#include <iostream>
#include <fstream>
using namespace std;

#include "jly_goicp.h"
#include "ConfigMap.hpp"

#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/point_representation.h>

#include <pcl/io/pcd_io.h>
#include <pcl/conversions.h>
#include <pcl/common/transforms.h>

#define DEFAULT_OUTPUT_FNAME "output.txt"
#define DEFAULT_CONFIG_FNAME "config.txt"
#define DEFAULT_MODEL_FNAME "model.txt"
#define DEFAULT_DATA_FNAME "data.txt"

pcl::PointCloud<pcl::PointXYZ>::Ptr modelPCD, dataPCD;

void parseInput(int argc, char **argv, string & modelFName, string & dataFName, int & NdDownsampled, string & configFName, string & outputFName);
void readConfig(string FName, GoICP & goicp);
int loadPointCloud(string FName, int & N, POINT3D **  p);
int loadPCDtoPoint3D(string FName, int &N, POINT3D **p, pcl::PointCloud<pcl::PointXYZ>::Ptr &cloud);

int main(int argc, char** argv)
{
	int Nm, Nd, NdDownsampled;
	clock_t  clockBegin, clockEnd;
	string modelFName, dataFName, configFName, outputFname;
	POINT3D * pModel, * pData;
	GoICP goicp;
	int i,j;

	parseInput(argc, argv, modelFName, dataFName, NdDownsampled, configFName, outputFname);
	readConfig(configFName, goicp);

	// Load model and data point clouds
	// loadPointCloud(modelFName, Nm, &pModel);
	// loadPointCloud(dataFName, Nd, &pData);
	modelPCD.reset(new pcl::PointCloud<pcl::PointXYZ>);
	dataPCD.reset(new pcl::PointCloud<pcl::PointXYZ>);
	loadPCDtoPoint3D(modelFName, Nm, &pModel, modelPCD);
	loadPCDtoPoint3D(dataFName, Nd, &pData, dataPCD);
	
	goicp.pModel = pModel;
	goicp.Nm = Nm;
	goicp.pData = pData;
	goicp.Nd = Nd;

	// Build Distance Transform
	cout << "Building Distance Transform..." << flush;
	clockBegin = clock();
	goicp.BuildDT();
	clockEnd = clock();
	cout << (double)(clockEnd - clockBegin)/CLOCKS_PER_SEC << "s (CPU)" << endl;

	// Run GO-ICP
	if(NdDownsampled > 0)
	{
		goicp.Nd = NdDownsampled; // Only use first NdDownsampled data points (assumes data points are randomly ordered)
	}
	cout << "Model ID: " << modelFName << " (" << goicp.Nm << "), Data ID: " << dataFName << " (" << goicp.Nd << ")" << endl;
	cout << "Registering..." << endl;
	clockBegin = clock();
	goicp.Register();
	clockEnd = clock();
	double time = (double)(clockEnd - clockBegin)/CLOCKS_PER_SEC;
	cout << "Optimal Rotation Matrix:" << endl;
	cout << goicp.optR << endl;
	cout << "Optimal Translation Vector:" << endl;
	cout << goicp.optT << endl;
	cout << "Finished in " << time << endl;

	Eigen::Matrix4d trans_mat;
	trans_mat.setIdentity();
	char buffer[1024];
	for(i=0;i<3;i++)
	{
		for(j=0;j<3;j++)
		{
			sprintf(buffer,"%.7f ",goicp.optR.val[i][j]);
			printf("goicp.optR.val[%d][%d] = %s", i, j, buffer);
			trans_mat(i,j) = std::atof(buffer);
		}
		sprintf(buffer,"%.7f ",goicp.optT.val[i][0]);
		printf("goicp.optT.val[%d][0] = %s", i, buffer);
		trans_mat(i,3) = std::atof(buffer);
		trans_mat(3,i) = 0.0;
	}
	trans_mat(3,3) = 1.0;

	pcl::PointCloud<pcl::PointXYZ> outputPCD;
	pcl::transformPointCloud(*dataPCD, outputPCD, trans_mat);
	pcl::io::savePCDFileASCII("output.pcd", outputPCD);

	ofstream ofile;
	ofile.open(outputFname.c_str(), ofstream::out);
	ofile << time << endl;
	ofile << goicp.optR << endl;
	ofile << goicp.optT << endl;
	ofile.close();

	delete(pModel);
	delete(pData);

	delete(&modelPCD);
	delete(&dataPCD);

	return 0;
}

void parseInput(int argc, char **argv, string & modelFName, string & dataFName, int & NdDownsampled, string & configFName, string & outputFName)
{
	// Set default values
	modelFName = DEFAULT_MODEL_FNAME;
	dataFName = DEFAULT_DATA_FNAME;
	configFName = DEFAULT_CONFIG_FNAME;
	outputFName = DEFAULT_OUTPUT_FNAME;
	NdDownsampled = 0; // No downsampling

	//cout << endl;
	//cout << "USAGE:" << "./GOICP <MODEL FILENAME> <DATA FILENAME> <NUM DOWNSAMPLED DATA POINTS> <CONFIG FILENAME> <OUTPUT FILENAME>" << endl;
	//cout << endl;

	if(argc > 5)
	{
		outputFName = argv[5];
	}
	if(argc > 4)
	{
		configFName = argv[4];
	}
	if(argc > 3)
	{
		NdDownsampled = atoi(argv[3]);
	}
	if(argc > 2)
	{
		dataFName = argv[2];
	}
	if(argc > 1)
	{
		modelFName = argv[1];
	}

	cout << "INPUT:" << endl;
	cout << "(modelFName)->(" << modelFName << ")" << endl;
	cout << "(dataFName)->(" << dataFName << ")" << endl;
	cout << "(NdDownsampled)->(" << NdDownsampled << ")" << endl;
	cout << "(configFName)->(" << configFName << ")" << endl;
	cout << "(outputFName)->(" << outputFName << ")" << endl;
	cout << endl;
}

void readConfig(string FName, GoICP & goicp)
{
	// Open and parse the associated config file
	ConfigMap config(FName.c_str());

	goicp.MSEThresh = config.getF("MSEThresh");
	goicp.initNodeRot.a = config.getF("rotMinX");
	goicp.initNodeRot.b = config.getF("rotMinY");
	goicp.initNodeRot.c = config.getF("rotMinZ");
	goicp.initNodeRot.w = config.getF("rotWidth");
	goicp.initNodeTrans.x = config.getF("transMinX");
	goicp.initNodeTrans.y = config.getF("transMinY");
	goicp.initNodeTrans.z = config.getF("transMinZ");
	goicp.initNodeTrans.w = config.getF("transWidth");
	goicp.trimFraction = config.getF("trimFraction");
	// If < 0.1% trimming specified, do no trimming
	if(goicp.trimFraction < 0.001)
	{
		goicp.doTrim = false;
	}
	goicp.dt.SIZE = config.getI("distTransSize");
	goicp.dt.expandFactor = config.getF("distTransExpandFactor");

	cout << "CONFIG:" << endl;
	config.print();
	//cout << "(doTrim)->(" << goicp.doTrim << ")" << endl;
	cout << endl;
}

int loadPointCloud(string FName, int & N, POINT3D ** p)
{
	int i;
	ifstream ifile;

	ifile.open(FName.c_str(), ifstream::in);
	if(!ifile.is_open())
	{
		cout << "Unable to open point file '" << FName << "'" << endl;
		exit(-1);
	}
	ifile >> N; // First line has number of points to follow
	*p = (POINT3D *)malloc(sizeof(POINT3D) * N);
	for(i = 0; i < N; i++)
	{
		ifile >> (*p)[i].x >> (*p)[i].y >> (*p)[i].z;
	}

	ifile.close();

	return 0;
}

int loadPCDtoPoint3D(string FName, int & N, POINT3D ** p, pcl::PointCloud<pcl::PointXYZ>::Ptr &cloud)
{
	int i;

	pcl::io::loadPCDFile(FName, *cloud);
	N = cloud->points.size();

	*p = (POINT3D *)malloc(sizeof(POINT3D) * N);
	for(i = 0; i < N; i++)
	{
		(*p)[i].x = cloud->points[i].x;
		(*p)[i].y = cloud->points[i].z;
		(*p)[i].z = cloud->points[i].z;
	}

	return 0;
}
