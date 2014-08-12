#include <vtkSmartPointer.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLPolyDataWriter.h>

#include "vtkPoissonReconstruction.h"
#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkBinaryContourImageFilter.h"
#include "itkExtractImageFilter.h"
#include "itkCastImageFilter.h"
#include "itkPasteImageFilter.h"


#include "itkImageDuplicator.h"

#include "itkImageToVTKImageFilter.h"

// vtkITK includes
#include "vtkITKArchetypeImageSeriesScalarReader.h"



#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>

#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPointData.h>
#include <vtkCleanPolyData.h>
#include <vtkMaskPoints.h>
#include <vtkMergePoints.h>
#include <vtkPointSource.h>
#include <vtkPolyDataNormals.h>
#include <vtkVertexGlyphFilter.h>
#include <vtkTriangleStrip.h>
#include <vtkMath.h>
#include <iostream>
#include <vtkMarchingCubes.h>
#include <vtkKdTreePointLocator.h>
#include <vtkWindowedSincPolyDataFilter.h>

#include <vtkImageAccumulate.h>
#include <vtkMetaImageReader.h>
#include <vtkDiscreteMarchingCubes.h>
#include <vtkImageThreshold.h>
#include <vtkImageToStructuredPoints.h>
#include <vtkDecimatePro.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkTransform.h>
#include <vtkImageChangeInformation.h>
#include <vtkNew.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkStripper.h>
#include <vtkPolyDataWriter.h>
#include <vtkReverseSense.h>
//#include <vtkAppendPolyData.h>
#include <vtkCleanPolyData.h>
#include <vtkAppendFilter.h>
//#include <vtkGeometryFilter.h>
#include <vtkUnstructuredGrid.h>
//#include <vtkDataSetSurfaceFilter.h>
#include <vtkProperty.h>








typedef itk::Image< short, 3> InputImageType;
typedef itk::Image< short,3 > OutputImageType;
typedef itk::Image< unsigned char, 3> MidImageType;
typedef itk::Image< unsigned char, 2> SliceImageType;

vtkSmartPointer<vtkPoints>  getVolumeContourPoints (const MidImageType::Pointer midImage) 
{
	typedef itk::ExtractImageFilter< MidImageType,SliceImageType > 
		extractFilterType;
	typedef itk::BinaryContourImageFilter <SliceImageType,SliceImageType >
		binaryContourImageFilterType;
	typedef itk::CastImageFilter< SliceImageType, MidImageType > 
		CastFilterType;
	typedef itk::PasteImageFilter<MidImageType,MidImageType > 
		PasteFilterType;


		
	//Get Resion, size and start of input midImage	
	MidImageType::RegionType inputRegion =
		midImage->GetLargestPossibleRegion();
	MidImageType::SizeType inputSize =
		inputRegion.GetSize();
	MidImageType::IndexType start = inputRegion.GetIndex();
	MidImageType::RegionType desiredRegion;

	//New a output volume used to contain contour volume
	MidImageType::Pointer  outputContour = MidImageType::New();
	outputContour->SetRegions(inputRegion);
	outputContour->Allocate();
	outputContour = midImage;
	

	extractFilterType::Pointer extractFilter = extractFilterType::New();
	extractFilter->SetDirectionCollapseToIdentity();
		

	binaryContourImageFilterType::Pointer binaryContourFilter = 
		binaryContourImageFilterType::New ();
	CastFilterType::Pointer castFilter = CastFilterType::New();
	PasteFilterType::Pointer pasteFilter = PasteFilterType::New();

	unsigned int zSize=inputSize[2];
	inputSize[2]=0;
	for (unsigned int k=0; k<zSize; k++)
	{
		//Extract one slice from midImage
		start[2] = k;
		desiredRegion.SetSize(  inputSize  );
		desiredRegion.SetIndex( start );
		extractFilter->SetExtractionRegion( desiredRegion );
		extractFilter->SetInput(midImage );

		//Extract a contour of one slice
		binaryContourFilter->SetInput(extractFilter->GetOutput() );

		//Case one contour slice of 2D to a slice of 3D which can be a input of pasteFilter
		castFilter->SetInput(binaryContourFilter->GetOutput());
		castFilter->UpdateLargestPossibleRegion();

	
		//Paste a contour slice to outputContour volume
		pasteFilter->SetSourceImage(castFilter->GetOutput() );
		pasteFilter->SetDestinationImage( outputContour );
		pasteFilter->SetDestinationIndex( start );

		//sliceImage3D = castFilter->GetOutput();
		pasteFilter->SetSourceRegion( castFilter->GetOutput()->GetBufferedRegion() );
		pasteFilter->Update();
	
		outputContour=pasteFilter->GetOutput();

	}

	inputSize[2]=zSize;
	
	//debug
	typedef itk::ImageFileWriter<MidImageType> WriterType3D;
	WriterType3D::Pointer writeroutconture = WriterType3D::New();
	writeroutconture->SetFileName("D:\\outputcontour.nrrd");
	writeroutconture->SetInput(outputContour);
	writeroutconture->Update();
	
	
	//Get points at boundary  	
	MidImageType::IndexType index;
	vtkSmartPointer<vtkPoints> pointsInit = vtkSmartPointer<vtkPoints>::New();
	MidImageType::PointType contourPoint;
  

	float pointXYZ[3];
	const float matrixVTK2Slicer [3][3]={{-1, 0, 0}, {0,-1, 0}, {0, 0,1}};
	float pointXYZ_S[3]; //slicer point
	int pointNum=0;

	for(unsigned int k=0; k<inputSize[2]; k++)
	{
		for(unsigned int j=0; j<inputSize[1]; j++)
		{
			for(unsigned int i=0; i<inputSize[0]; i++)
			{
				index[0]=i;
				index[1]=j;
				index[2]=k;
		
				if ( outputContour->GetPixel(index)!=0)
				{

					pointNum++;
					outputContour->TransformIndexToPhysicalPoint(index,contourPoint);
					pointXYZ[2]=contourPoint[2];
					pointXYZ[1]=contourPoint[1];
					pointXYZ[0]=contourPoint[0];
					vtkMath::Multiply3x3(matrixVTK2Slicer, pointXYZ,pointXYZ_S); 
					pointsInit->InsertNextPoint(pointXYZ_S); 


				}
				
			}
	
		}
	
	}
	

	return pointsInit;
}

std::vector<int> getLabels(vtkSmartPointer<vtkImageData> midImageVTK)
{
	vtkSmartPointer<vtkImageAccumulate> hist = vtkSmartPointer<vtkImageAccumulate>::New();
	hist->SetInput(midImageVTK);
	int extentMax = 0;
	double dImageScalarMax = midImageVTK->GetScalarTypeMax();
	extentMax = (int)(floor(dImageScalarMax - 1.0));

	int biggestBin = 1000000;     // VTK_INT_MAX - 1;
	if (extentMax < 0 || extentMax > biggestBin)
	{
		std::cout << "\nWARNING: due to lack of color label information and an image with a scalar maximum of "
					<< dImageScalarMax << ", using  " << biggestBin << " as the histogram number of bins" << endl;
		extentMax = biggestBin;
	}
	else
	{
		std::cout
		<<
		"\nWARNING: due to lack of color label information, using the full scalar range of the input image when calculating the histogram over the image: "
		<< extentMax << endl;
	}
	
	hist->SetComponentExtent(0, extentMax, 0, 0, 0, 0);
	hist->SetComponentOrigin(0, 0, 0);
	hist->SetComponentSpacing(1, 1, 1);
	hist->Update();
		
	double *max = hist->GetMax();
	double *min = hist->GetMin();
	if (min[0] == 0 || min[0] <0)
		min[0]=1;

	int StartLabel = (int)floor(min[0]);
	int EndLabel = (int)floor(max[0]);

	int numModelsToGenerate = 0;
	for(int i = StartLabel; i <= EndLabel; i++)
	{
		if ((int)floor((((hist->GetOutput())->GetPointData())->GetScalars())->GetTuple1(i)) > 0)
		{
			numModelsToGenerate++;
		}
	}

	std::cout<<
	"\nNumber of models to be generated "<< numModelsToGenerate << endl;

	// ModelMakerMarch
	double      labelFrequency = 0.0;;
	std::vector<int> loopLabels;
	std::vector<int> skippedModels;
	std::vector<int> madeModels;
	// set up the loop list with all the labels between start and end
	for(int i = StartLabel; i <= EndLabel; i++)
	{
		loopLabels.push_back(i);
	}
    
	for(::size_t l = 0; l < loopLabels.size(); l++)
	{
		// get the label out of the vector
		int i = loopLabels[l];
			
		labelFrequency = (((hist->GetOutput())->GetPointData())->GetScalars())->GetTuple1(i);
		if (labelFrequency == 0.0)
		{
			skippedModels.push_back(i);
			continue;
		}
			else
		{
			madeModels.push_back(i);
		}
	}

	return madeModels;


}

vtkSmartPointer<vtkPolyData>  getMarchingcubePoly (vtkSmartPointer<vtkImageData> midImageVTK, int labelValue, vtkSmartPointer<vtkTransform> transformIJKtoRAS ) 
{
	vtkSmartPointer <vtkImageThreshold> imageThreshold = vtkSmartPointer<vtkImageThreshold>::New();
	imageThreshold->SetInput(midImageVTK);
	imageThreshold->SetReplaceIn(1);
	imageThreshold->SetReplaceOut(1);
	imageThreshold->SetInValue(200);
	imageThreshold->SetOutValue(0);

	imageThreshold->ThresholdBetween(labelValue, labelValue);
	(imageThreshold->GetOutput())->ReleaseDataFlagOn();
	imageThreshold->ReleaseDataFlagOn();
	vtkSmartPointer <vtkImageToStructuredPoints> imageToStructuredPoints = vtkSmartPointer<vtkImageToStructuredPoints>::New();
	imageToStructuredPoints->SetInput(imageThreshold->GetOutput());
	try
	{
		imageToStructuredPoints->Update();
	}
	catch(...)
	{
		std::cerr << "ERROR while updating image to structured points for label " << labelValue << std::endl;
		return NULL;
	}
	imageToStructuredPoints->ReleaseDataFlagOn();
      

	vtkSmartPointer <vtkMarchingCubes> mcubes = vtkSmartPointer<vtkMarchingCubes>::New();

	mcubes->SetInput(imageToStructuredPoints->GetOutput());
	mcubes->SetValue(0, 100.5);
	mcubes->ComputeScalarsOff();
	mcubes->ComputeGradientsOff();
	mcubes->ComputeNormalsOff();
	(mcubes->GetOutput())->ReleaseDataFlagOn();
	try
	{
		mcubes->Update();
	}
	catch(...)
	{
		std::cerr << "ERROR while running marching cubes, for label " << labelValue << std::endl;
		return NULL;
	}


	if ((mcubes->GetOutput())->GetNumberOfPolys()  == 0)
	{
		std::cout << "Cannot create a model from label " << labelValue
		<< "\nNo polygons can be created,\nthere may be no voxels with this label in the volume." << endl;

		if (transformIJKtoRAS)
		{
			transformIJKtoRAS = NULL;
		}
		
		if (imageThreshold)
		{
      
			imageThreshold->SetInput(NULL);
			imageThreshold->RemoveAllInputs();
			imageThreshold = NULL;

		}
		if (imageToStructuredPoints)
		{
			imageToStructuredPoints->SetInput(NULL);
			imageToStructuredPoints = NULL;
		}
		if (mcubes)
		{
			mcubes->SetInput(NULL);
			mcubes = NULL;
		}

		std::cout << "...continuing" << endl;

		//return EXIT_FAILURE;
		return NULL;
	}
	  

	vtkSmartPointer <vtkDecimatePro> decimator = vtkSmartPointer<vtkDecimatePro>::New();
	decimator->SetInput(mcubes->GetOutput());
	decimator->SetFeatureAngle(60);
	decimator->SplittingOff();
	decimator->PreserveTopologyOn();

	decimator->SetMaximumError(1);
	decimator->SetTargetReduction(0.01);
	(decimator->GetOutput())->ReleaseDataFlagOff();

	try
	{
	decimator->Update();
	}
	catch(...)
	{
	std::cerr << "ERROR decimating model " << labelValue << std::endl;
	return NULL;
	}
	 
	 
	vtkSmartPointer<vtkReverseSense> reverser = vtkSmartPointer<vtkReverseSense>::New();
	 
	if ((transformIJKtoRAS->GetMatrix())->Determinant() < 0)
	{
	reverser->SetInput(decimator->GetOutput());
	reverser->ReverseNormalsOn();
	(reverser->GetOutput())->ReleaseDataFlagOn();
	}  
	 
	 
	//Smooth
	vtkSmartPointer <vtkSmoothPolyDataFilter> smootherPoly = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
	smootherPoly->SetRelaxationFactor(0.33);
	smootherPoly->SetFeatureAngle(60);
	smootherPoly->SetConvergence(0);
	 
	if ((transformIJKtoRAS->GetMatrix())->Determinant() < 0)
	{
		smootherPoly->SetInput(reverser->GetOutput());
	}
	else
	{
		smootherPoly->SetInput(decimator->GetOutput());
	}
	 
	 

	smootherPoly->SetNumberOfIterations(98);
	smootherPoly->FeatureEdgeSmoothingOff();
	smootherPoly->BoundarySmoothingOff();
	(smootherPoly->GetOutput())->ReleaseDataFlagOn();

	try
	{
		smootherPoly->Update();
	}
	catch(...)
	{
		std::cerr << "ERROR updating Poly smoother for model " << labelValue << std::endl;
		return NULL;
	}
      
	vtkSmartPointer<vtkTransformPolyDataFilter> transformer = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
	transformer->SetInput(smootherPoly->GetOutput());
	transformer->SetTransform(transformIJKtoRAS);
	(transformer->GetOutput())->ReleaseDataFlagOn();
		
	vtkSmartPointer<vtkPolyDataNormals> normals = vtkSmartPointer<vtkPolyDataNormals>::New();
	normals->ComputePointNormalsOn();
	normals->SetInput(transformer->GetOutput());
	normals->SetFeatureAngle(60);
	normals->SetSplitting(true);
	(normals->GetOutput())->ReleaseDataFlagOn();

	vtkSmartPointer<vtkStripper> stripper = vtkSmartPointer<vtkStripper>::New();
	stripper->SetInput(normals->GetOutput());
	(stripper->GetOutput())->ReleaseDataFlagOff();
	  
	   
	// the poly data output from the stripper can be set as an input to a
	// model's polydata
	try
	{
		(stripper->GetOutput())->Update();
	}
	catch(...)
	{
		std::cerr << "ERROR updating stripper for model " << labelValue << std::endl;
		return NULL;
	}

	return stripper->GetOutput();

}

MidImageType::Pointer getMask (const MidImageType::Pointer midImage, int labelValue)
{

	//Set un-zero pixel to 255, this is a limit of binaryContourImageFilterType
	MidImageType::SizeType midImageSize =
	midImage->GetLargestPossibleRegion().GetSize();
	MidImageType::IndexType indexMid;
	unsigned char pixelvalue;
			
	typedef itk::ImageDuplicator< MidImageType > DuplicatorType;
	DuplicatorType::Pointer duplicator = DuplicatorType::New();
	duplicator->SetInputImage(midImage);
	duplicator->Update();
	MidImageType::Pointer clonedImage = duplicator->GetOutput();
 


	for(unsigned int n=0; n<midImageSize[2]; n++)
	{
		for(unsigned int m=0; m<midImageSize[1]; m++)
		{
			for(unsigned int l=0; l<midImageSize[0]; l++)
			{
				indexMid[0]=l;
				indexMid[1]=m;
				indexMid[2]=n;
				pixelvalue=clonedImage->GetPixel(indexMid);

				if (pixelvalue == unsigned char (labelValue))
					clonedImage->SetPixel(indexMid, 255);
				else
					clonedImage->SetPixel(indexMid, 0);
			}
		}
	}
	
    return clonedImage;

}

double max (double a, double b)
{
	if (a>=b)
		return a;
	else
		return b;
}

double min (double a, double b)
{
	if (a<=b)
		return a;
	else
		return b;
}
void combinePoints (vtkSmartPointer<vtkPolyData> pointsetA, vtkSmartPointer<vtkPolyData> pointsetB)

{

	vtkIdType id;
	vtkSmartPointer<vtkMergePoints> mergePoints = 
	vtkSmartPointer<vtkMergePoints>::New();
	mergePoints->SetDataSet(pointsetA);
	mergePoints->SetDivisions(10,10,10);
	
	double boundsA[6];
	double boundsB[6];
	
	pointsetA->GetBounds(boundsA);
	pointsetB->GetBounds(boundsB);
	double bounds[6];
	bounds[0]=min(boundsA[0], boundsB[0]);
	bounds[2]=min(boundsA[2], boundsB[2]);
	bounds[4]=min(boundsA[4], boundsB[4]);
	bounds[1]=max(boundsA[1], boundsB[1]);
	bounds[3]=max(boundsA[3], boundsB[3]);
	bounds[5]=max(boundsA[5], boundsB[5]);
	
	mergePoints->InitPointInsertion(pointsetA->GetPoints(), bounds);
    
	for (vtkIdType i = 0; i < pointsetA->GetNumberOfPoints(); i++)
	{
	mergePoints->InsertUniquePoint(pointsetA->GetPoint(i), id);
	}

	for (vtkIdType i = 0; i < pointsetB->GetNumberOfPoints(); i++)
	{
	mergePoints->InsertUniquePoint(pointsetB->GetPoint(i), id);
		//std::cout << "There are now "
		//<< pointsetA->GetNumberOfPoints() << " points." << std::endl;
	}

	std::cout << "There are now "
		<< pointsetA->GetNumberOfPoints() << " points. inside function" << std::endl;



	//vtkSmartPointer<vtkPolyData> polydataCor = vtkSmartPointer<vtkPolyData>::New();  

	//polydataCor->ShallowCopy(pointsetA); 
	//
	
	//vtkSmartPointer<vtkXMLPolyDataWriter> writerPointCom =
	//vtkSmartPointer<vtkXMLPolyDataWriter>::New();
 // 
	//writerPointCom->SetInput(polydataCor);
	//writerPointCom->SetFileName("pointCom.vtp");
	//writerPointCom->Write();
	
	mergePoints = NULL;

}
vtkSmartPointer<vtkPolyData> combinePolys (vtkSmartPointer<vtkPolyData> polyA, vtkSmartPointer<vtkPolyData> polyB, vtkSmartPointer<vtkPolyData> polyC)
{
	
	vtkSmartPointer<vtkPolyData> polyMarchingCombine = vtkSmartPointer<vtkPolyData>::New();
	vtkSmartPointer<vtkAppendFilter> appendFilterAS =   vtkSmartPointer<vtkAppendFilter>::New();
	vtkSmartPointer<vtkAppendFilter> appendFilterASC =   vtkSmartPointer<vtkAppendFilter>::New();


		appendFilterAS->AddInputConnection(polyA->GetProducerPort());
		appendFilterAS->AddInputConnection(polyB->GetProducerPort());
		appendFilterAS->Update();

		if (polyC)
		{
			appendFilterASC->AddInputConnection(appendFilterAS->GetOutput()->GetProducerPort());
			appendFilterASC->AddInputConnection(polyC->GetProducerPort());
			polyMarchingCombine->ShallowCopy(appendFilterASC->GetOutput()); 
		}
		else
		{

			polyMarchingCombine->ShallowCopy(appendFilterAS->GetOutput()); 
		}
	

	//appendFilterAS->Delete();


	return polyMarchingCombine;
}


int main(int argc, char *argv[])
{
	int flagAxialImage=0;
	int flagSagittalImage=0;
	int flagCoronalImage=0;
	vtkstd::string inputFileNameAxial;
	vtkstd::string inputFileNameSag;
	vtkstd::string inputFileNameCor;
	
	if( argc == 2 )
    {
    cout << "Axial input " << endl;
	flagAxialImage=1;
	inputFileNameAxial = argv[1];
    }

	if(argc==3)
	{
    cout << "Axial input and Sagittal input" << endl;
	flagAxialImage=1;
	flagSagittalImage=1;
	inputFileNameAxial = argv[1];
	inputFileNameSag = argv[2];
    }

   if(argc==4)
	{
    cout << "Axial input, Sagittal input and cornonal input" << endl;
	flagAxialImage=1;
	flagSagittalImage=1;
	flagCoronalImage=1;
	inputFileNameAxial = argv[1];
	inputFileNameSag = argv[2];
	inputFileNameCor = argv[3];
    }

	typedef itk::ImageFileReader<InputImageType>  ReaderType;
	typedef itk::ImageFileWriter<OutputImageType> WriterType;
	typedef itk::CastImageFilter< InputImageType, MidImageType > CastFilterType;

	//ReaderType::Pointer reader = ReaderType::New();
	//WriterType::Pointer writer = WriterType::New();

	MidImageType::Pointer midImageAixal= MidImageType ::New();
	MidImageType::Pointer midImageSag= MidImageType ::New();
	MidImageType::Pointer midImageCor= MidImageType ::New();
	
	vtkSmartPointer<vtkImageData> midImageVTKAxial = vtkSmartPointer<vtkImageData>::New();
	vtkSmartPointer<vtkImageData> midImageVTKSag = vtkSmartPointer<vtkImageData>::New();
	vtkSmartPointer<vtkImageData> midImageVTKCor = vtkSmartPointer<vtkImageData>::New();

	vtkSmartPointer<vtkPoints> pointsInitAxial = vtkSmartPointer<vtkPoints>::New();
	vtkSmartPointer<vtkPoints> pointsInitSag = vtkSmartPointer<vtkPoints>::New();
	vtkSmartPointer<vtkPoints> pointsInitCor = vtkSmartPointer<vtkPoints>::New();
	  
	
	vtkSmartPointer<vtkPolyData> polydataAxial = vtkSmartPointer<vtkPolyData>::New(); 
	vtkSmartPointer<vtkPolyData> polydataSag = vtkSmartPointer<vtkPolyData>::New();
	vtkSmartPointer<vtkPolyData> polydataCor = vtkSmartPointer<vtkPolyData>::New();

	

	vtkSmartPointer<vtkPolyData> marchingPolyAxial = vtkSmartPointer<vtkPolyData>::New();
	vtkSmartPointer<vtkPolyData> marchingPolySag = vtkSmartPointer<vtkPolyData>::New();
	vtkSmartPointer<vtkPolyData> marchingPolyCor = vtkSmartPointer<vtkPolyData>::New();

	vtkSmartPointer<vtkTransform> transformIJKtoRASAxial = vtkSmartPointer<vtkTransform>::New();
	vtkSmartPointer<vtkTransform> transformIJKtoRASSag = vtkSmartPointer<vtkTransform>::New();
	vtkSmartPointer<vtkTransform> transformIJKtoRASCor = vtkSmartPointer<vtkTransform>::New();

	vtkSmartPointer<vtkPolyData> polyMarchingCombine;
	vtkSmartPointer<vtkKdTreePointLocator> kDTree;

	
	std::string labelName="brain0811_";
	std::vector<int> madeModels; //The labels for surface reconstruction
	
	if (flagAxialImage ) //read Axial Image
	{
		ReaderType::Pointer reader = ReaderType::New();
		reader->SetFileName(inputFileNameAxial.c_str());
		reader->Update();
	
		//Cast input volume to a volume with unsigned char pixel
		CastFilterType::Pointer castInputFilterA = CastFilterType::New();
		castInputFilterA->SetInput(reader->GetOutput());
		castInputFilterA->Update();
		midImageAixal= castInputFilterA->GetOutput();
		
		//itk to vtk
		typedef itk::ImageToVTKImageFilter<MidImageType> ConnectorType;
		ConnectorType::Pointer connector =ConnectorType::New();
		connector->SetInput(midImageAixal);
	
		vtkNew<vtkImageChangeInformation> ici;
		ici->SetInput(connector->GetOutput());
		ici->SetOutputSpacing(1, 1, 1);
		ici->SetOutputOrigin(0, 0, 0);
		ici->Update();

		midImageVTKAxial = ici->GetOutput();
		midImageVTKAxial->Update();

	
		//for acquiring transform
		vtkITKArchetypeImageSeriesScalarReader *scalarReader = vtkITKArchetypeImageSeriesScalarReader::New();
		scalarReader->SetArchetype(inputFileNameAxial.c_str());
		scalarReader->SetOutputScalarTypeToNative();
		scalarReader->SetDesiredCoordinateOrientationToNative();
		scalarReader->SetUseNativeOriginOn();
		scalarReader->Update();
		transformIJKtoRASAxial->SetMatrix(scalarReader->GetRasToIjkMatrix());
		transformIJKtoRASAxial->Inverse();

		//Get labels to be surface reconstructed
		madeModels = getLabels(midImageVTKAxial);
		


}

	
	if (flagSagittalImage) //read sagittle image
	{
		ReaderType::Pointer reader = ReaderType::New();
		reader->SetFileName(inputFileNameSag.c_str());
		reader->Update();


		//Cast input volume to a volume with unsigned char pixel
		CastFilterType::Pointer castInputFilterS = CastFilterType::New();
		castInputFilterS->SetInput(reader->GetOutput());
		castInputFilterS->Update();
		//MidImageType::Pointer midImageSag= MidImageType ::New();
		midImageSag= castInputFilterS->GetOutput();
	
		//itk to vtk
		typedef itk::ImageToVTKImageFilter<MidImageType> ConnectorType;
		ConnectorType::Pointer connector =ConnectorType::New();
		connector->SetInput(midImageSag);
	
		vtkNew<vtkImageChangeInformation> ici;
		ici->SetInput(connector->GetOutput());
		ici->SetOutputSpacing(1, 1, 1);
		ici->SetOutputOrigin(0, 0, 0);
		ici->Update();

		midImageVTKSag = ici->GetOutput();
		midImageVTKSag->Update();

	
		//for acquiring transform
		vtkITKArchetypeImageSeriesScalarReader *scalarReader = vtkITKArchetypeImageSeriesScalarReader::New();

		
		scalarReader->SetArchetype(inputFileNameSag.c_str());
		scalarReader->SetOutputScalarTypeToNative();
		scalarReader->SetDesiredCoordinateOrientationToNative();
		scalarReader->SetUseNativeOriginOn();
		scalarReader->Update();
		transformIJKtoRASSag->SetMatrix(scalarReader->GetRasToIjkMatrix());
		transformIJKtoRASSag->Inverse();

		//Generate a histogram of the labels
		std::vector<int> madeModelsSag = getLabels(midImageVTKSag);
		

	}


	if (flagCoronalImage) //read sagittle image
	{
		ReaderType::Pointer reader = ReaderType::New();
		reader->SetFileName(inputFileNameCor.c_str());
		reader->Update();


		//Cast input volume to a volume with unsigned char pixel
		CastFilterType::Pointer castInputFilterC = CastFilterType::New();
		castInputFilterC->SetInput(reader->GetOutput());
		castInputFilterC->Update();
		midImageCor= castInputFilterC->GetOutput();
	
		//itk to vtk
		typedef itk::ImageToVTKImageFilter<MidImageType> ConnectorType;
		ConnectorType::Pointer connector =ConnectorType::New();
		connector->SetInput(midImageCor);
	
		vtkNew<vtkImageChangeInformation> ici;
		ici->SetInput(connector->GetOutput());
		ici->SetOutputSpacing(1, 1, 1);
		ici->SetOutputOrigin(0, 0, 0);
		ici->Update();

		midImageVTKCor = ici->GetOutput();
		midImageVTKCor->Update();

	
		//for acquiring transform
		vtkITKArchetypeImageSeriesScalarReader *scalarReader = vtkITKArchetypeImageSeriesScalarReader::New();

		scalarReader->SetArchetype(inputFileNameCor.c_str());
		scalarReader->SetOutputScalarTypeToNative();
		scalarReader->SetDesiredCoordinateOrientationToNative();
		scalarReader->SetUseNativeOriginOn();
		scalarReader->Update();
		transformIJKtoRASCor->SetMatrix(scalarReader->GetRasToIjkMatrix());
		transformIJKtoRASCor->Inverse();

		//Generate a histogram of the labels
		std::vector<int> madeModelsCor = getLabels(midImageVTKCor);
	}
	
	
	
	
	for (::size_t l =0; l < madeModels.size(); l++)
	{
		int labelValue= madeModels[l];
		std::stringstream lable;
		lable<<labelValue;

		
		if(flagAxialImage)
		{
			//Get points at boundary  	
			pointsInitAxial=getVolumeContourPoints(getMask(midImageAixal,labelValue));

			/*	Add the points to a polydata     */    
			polydataAxial->SetPoints(pointsInitAxial); 
 		   
			//Get marching cube result
			marchingPolyAxial=getMarchingcubePoly(midImageVTKAxial,labelValue, transformIJKtoRASAxial);
		
			if(polyMarchingCombine)
			{
				polyMarchingCombine=NULL;
			}
			polyMarchingCombine = vtkSmartPointer<vtkPolyData>::New();
			polyMarchingCombine->ShallowCopy(marchingPolyAxial); 

			if(flagSagittalImage)
			{
				//Get points at boundary  	
				pointsInitSag=getVolumeContourPoints(getMask(midImageSag,labelValue));

				/*	Add the points to a polydata     */    
				polydataSag->SetPoints(pointsInitSag); 
 		   
				//Get marching cube result
				marchingPolySag=getMarchingcubePoly(midImageVTKSag,labelValue, transformIJKtoRASSag);
			
				//Combine axial and saggital points
				std::cout << "There are  "
				<< polydataAxial->GetNumberOfPoints() << "input points in polydataAxial before" << std::endl;
				combinePoints (polydataAxial, polydataSag);
				//combinePoints (polydataAxial, polydataCor);
				std::cout << "There are  "
				<< polydataAxial->GetNumberOfPoints() << "input points in polydataAxial after" << std::endl;


				//combine marching cube results
				std::cout << "There are  "
				<< marchingPolyAxial->GetNumberOfPoints() << "input points." << std::endl;
				
				if(polyMarchingCombine)
				{
					polyMarchingCombine=NULL;
				}
				polyMarchingCombine = vtkSmartPointer<vtkPolyData>::New();
				polyMarchingCombine->ShallowCopy(combinePolys(marchingPolyAxial, marchingPolySag,NULL)); 
				
				std::cout << "There are  "
				<< polyMarchingCombine->GetNumberOfPoints() << "input points." << std::endl;
	
				vtkSmartPointer<vtkXMLPolyDataWriter> writerMarchingCom =
				vtkSmartPointer<vtkXMLPolyDataWriter>::New();
  
				writerMarchingCom->SetInput(polyMarchingCombine);
				writerMarchingCom->SetFileName("marchingCom.vtp");
				writerMarchingCom->Write();

				if(flagCoronalImage)
				{
					//Get points at boundary  	
					pointsInitCor=getVolumeContourPoints(getMask(midImageCor,labelValue));

					/*	Add the points to a polydata     */    
					polydataCor->SetPoints(pointsInitCor); 
 		   
					//Get marching cube result
					marchingPolyCor=getMarchingcubePoly(midImageVTKCor,labelValue, transformIJKtoRASCor);
		


					//Combine axial, saggital and coronal points
					std::cout << "There are  "
					<< polydataAxial->GetNumberOfPoints() << "input points in polydataAxial before" << std::endl;
					combinePoints (polydataAxial, polydataCor);
					std::cout << "There are  "
					<< polydataAxial->GetNumberOfPoints() << "input points in polydataAxial after" << std::endl;


					//combine marching cube results
					std::cout << "There are  "
					<< marchingPolyAxial->GetNumberOfPoints() << "input points." << std::endl;
				
					if(polyMarchingCombine)
					{
						polyMarchingCombine=NULL;
					}
					polyMarchingCombine = vtkSmartPointer<vtkPolyData>::New();
					polyMarchingCombine->ShallowCopy(combinePolys(marchingPolyAxial, marchingPolySag, marchingPolyCor));
				
					std::cout << "There are  "
					<< polyMarchingCombine->GetNumberOfPoints() << "input points." << std::endl;
	
					vtkSmartPointer<vtkXMLPolyDataWriter> writerMarchingCom3 =
					vtkSmartPointer<vtkXMLPolyDataWriter>::New();
  
					writerMarchingCom3->SetInput(polyMarchingCombine);
					writerMarchingCom3->SetFileName("marchingCom.vtp");
					writerMarchingCom3->Write();
				
				
				
				}
			
			
			}



		}

	
		// Create the tree
		
		
		if(kDTree)
		{
			kDTree->SetDataSet(NULL);		
			kDTree=NULL;
		}
		kDTree=	vtkSmartPointer<vtkKdTreePointLocator>::New();
		kDTree->SetDataSet(polyMarchingCombine);
		kDTree->BuildLocator();

		vtkSmartPointer<vtkFloatArray> pointNormalsRetrieved = 
		vtkFloatArray::SafeDownCast(kDTree->GetDataSet()->GetPointData()->GetNormals());
 
		double pTTrans[3];
		vtkIdType iD;
		double closestPoint[3];
		double pNdouble[3];
		float pN[3];
		double dotRslt;
		double normpNPre;
		double normpN;
		ofstream normalOut("normalsExtract.txt");

		vtkSmartPointer<vtkFloatArray> pointNormalsArray = vtkSmartPointer<vtkFloatArray>::New();
		pointNormalsArray->SetNumberOfComponents(3); //3d normals (ie x,y,z)
		pointNormalsArray->SetNumberOfTuples(polyMarchingCombine->GetNumberOfPoints());

  
		for (unsigned int i=0; i< polydataAxial->GetNumberOfPoints(); i++)
		{
		polydataAxial->GetPoint(i, pTTrans);
  
		// Find the closest points to TestPoint
		iD = kDTree->FindClosestPoint(pTTrans);
		normalOut << "The closest point is point " << iD << std::endl;
 
		//Get the coordinates of the closest point
 
		kDTree->GetDataSet()->GetPoint(iD, closestPoint);
		normalOut << "Coordinates:    " << closestPoint[0] << " " << closestPoint[1] << " " << closestPoint[2] << std::endl;
		normalOut <<"points on contour" <<pTTrans[0] << " " <<pTTrans[1]<<" " << pTTrans[2] << std::endl;


		pointNormalsRetrieved->GetTuple(iD, pNdouble);
		normalOut << "Point normal " << iD << ": "  << pNdouble[0] << " " << pNdouble[1] << " " << pNdouble[2] << std::endl;

		pN[0]=(float)pNdouble[0];
		pN[1]=(float)pNdouble[1];
		pN[2]=(float)pNdouble[2];

		pointNormalsArray->SetTuple(i, pN);  

		}

		// Add the normals to the points in the polydata
		polydataAxial->GetPointData()->SetNormals(pointNormalsArray); 


		//Make a vtkPolyData with a vertex on each point.
		vtkSmartPointer<vtkVertexGlyphFilter> vertexFilter = vtkSmartPointer<vtkVertexGlyphFilter>::New();
		vertexFilter->SetInputConnection(polydataAxial->GetProducerPort());
		vertexFilter->Update();


		vtkSmartPointer<vtkPolyData> polydataNew = vtkSmartPointer<vtkPolyData>::New();
		polydataNew->ShallowCopy(vertexFilter->GetOutput()); 
  


		vtkSmartPointer<vtkXMLPolyDataWriter> writerPoly = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
		writerPoly->SetInput(polydataNew);
		writerPoly->SetFileName("PolywithNorm_comb.vtp");
		writerPoly->Update();


		vtkSmartPointer<vtkXMLPolyDataReader> readerPoly = vtkSmartPointer<vtkXMLPolyDataReader>::New();
		readerPoly->SetFileName("PolywithNorm_comb.vtp");
		readerPoly->Update();

		//PoissonReconstruction
		vtkSmartPointer<vtkPoissonReconstruction> poissonFilter = 
		vtkSmartPointer<vtkPoissonReconstruction>::New();
		poissonFilter->SetDepth(12);
		poissonFilter->SetInputConnection(readerPoly->GetOutputPort());
		poissonFilter->Update();
  
  
  
		//Write the file
		std::string fileName;
		fileName = "d:" + std::string("/") + labelName + lable.str()+ std::string(".vtp");
		vtkSmartPointer<vtkXMLPolyDataWriter> writerSurface =
		vtkSmartPointer<vtkXMLPolyDataWriter>::New();
		writerSurface->SetInputConnection(poissonFilter->GetOutputPort());
		writerSurface->SetFileName(fileName.c_str());
		writerSurface->Update();

	}
	


  return EXIT_SUCCESS;
}
