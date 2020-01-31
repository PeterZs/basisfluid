
#include "Application.h"

#include "VectorField2D.h"

#include <memory>

using namespace glm;
using namespace std;

bool Application::Init_DataBuffers() {

    _velocityField = make_unique<VectorField2D>(
        _domainLeft, _domainRight, _domainBottom, _domainTop,
        _nbCellsXTotal, _nbCellsYTotal,
        VectorField2D::GridNodeLocation::CORNER,
        VectorField2D::BoundaryCondition::FLAT);
    _velocityField->createVectorCpuStorage();
    _velocityField->createVectorTexture2DStorage(
        GL_RG, GL_RG32F, GL_RG, GL_FLOAT, 1);
    _velocityField->populateWithFunction( [=](float /*x*/, float /*y*/) {
        return vec2(0);
    });


    _prevVelocityField = make_unique<VectorField2D>(_domainLeft, _domainRight, _domainBottom, _domainTop,
        _nbCellsXTotal, _nbCellsYTotal,
        VectorField2D::GridNodeLocation::CORNER,
        VectorField2D::BoundaryCondition::FLAT);
    _prevVelocityField->createVectorCpuStorage();
    _prevVelocityField->createVectorTexture2DStorage(
        GL_RG, GL_RG32F, GL_RG, GL_FLOAT, 1);
    _prevVelocityField->populateWithFunction(
        [=]
    (float /*x*/, float /*y*/) {
        return vec2(0, 0);
    });



    _nbParticlesPerCell = make_unique<DataBuffer2D<unsigned int>>(_nbCellsXTotal, _nbCellsYTotal);
    _nbParticlesPerCell->createCpuStorage();
    _nbParticlesPerCell->createTexture2DStorage(
        GL_RED_INTEGER, GL_R32I, GL_RED_INTEGER, GL_INT, 1);

    for (unsigned int i = 0; i < _nbParticlesPerCell->mNbElementsX; ++i) {
        for (unsigned int j = 0; j < _nbParticlesPerCell->mNbElementsY; ++j) {
            _nbParticlesPerCell->setCpuData(i, j, 0);
        }
    }



    _forceField = make_unique<VectorField2D>(_domainLeft, _domainRight, _domainBottom, _domainTop,
        forcesGridRes, forcesGridRes,
        VectorField2D::GridNodeLocation::CORNER,
        VectorField2D::BoundaryCondition::FLAT);
    _forceField->createVectorCpuStorage();
    _forceField->createVectorTexture2DStorage(GL_RG, GL_RG32F, GL_RG, GL_FLOAT, 1);


    //localVectorField = new VectorField2D(
    //    0, 1, 0, 1, // location will change later
    //    LOCAL_VECTOR_FIELD_SIZE, LOCAL_VECTOR_FIELD_SIZE,
    //    VectorField2D::GridNodeLocation::CORNER,
    //    VectorField2D::BoundaryCondition::LINEAR
    //);
    //localVectorField->createVectorCpuStorage();
    //localVectorField->createVectorTexture2DStorage(GL_RG, GL_RG32F, GL_RG, GL_FLOAT, 1);




    //all translated basis flows parameters
    _basisFlowParams = make_unique<DataBuffer1D<BasisFlow>>(1);
    _basisFlowParams->sourceStorageType = DataBuffer1D<BasisFlow>::StorageType::BUFFER;
    _basisFlowParams->createCpuStorage();
    _basisFlowParams->createBufferStorage(GL_FLOAT, sizeof(BasisFlow) / sizeof(float));
    _basisFlowParams->resize(0);



    //initialize basis flows. all basis templated are centered at (0,0), freq
    //1-1 has support [-0.5,0.5]^2 and other frequencies have smaller supports
    //according to their frequencies. We only need to create one basis template
    //per anisotropy ratio, so here we compute lvlX=0 and lvlY=iRatio.    
    _basisFlowTemplates = new std::unique_ptr<VectorField2D>[_maxAnisoLvl + 1];
    for (unsigned int iRatio = 0; iRatio < _maxAnisoLvl + 1; iRatio++) {
        _basisFlowTemplates[iRatio] = make_unique<VectorField2D>(
            -0.5f, 0.5f,
            -0.5f / float(1 << iRatio), 0.5f / float(1 << iRatio),
            _nbCellsXBasis, _nbCellsYBasis,
            VectorField2D::GridNodeLocation::CORNER,
            VectorField2D::BoundaryCondition::ZERO);
        _basisFlowTemplates[iRatio]->createVectorCpuStorage();
        _basisFlowTemplates[iRatio]->createVectorTexture2DStorage(
            GL_RG, GL_RG32F, GL_RG, GL_FLOAT, 1);
        _basisFlowTemplates[iRatio]->populateWithFunction(
            [=](float x, float y) {
            return vec2(flowBasisHat(dvec2(x, y), iRatio));
        }
        );
    }


    _partPos = make_unique<DataBuffer1D<vec2>>(1);
    _partPos->sourceStorageType = DataBuffer1D<vec2>::StorageType::CPU;
    _partPos->createCpuStorage();
    _partPos->createBufferStorage(GL_FLOAT, 2);
    _partPos->resize(0);

    _partVecs = make_unique<DataBuffer1D<vec2>>(1);
    _partVecs->sourceStorageType = DataBuffer1D<vec2>::StorageType::CPU;
    _partVecs->createCpuStorage();
    _partVecs->createBufferStorage(GL_FLOAT, 2);
    _partVecs->resize(0);


    _partAges = make_unique<DataBuffer1D<float>>(1);
    _partAges->sourceStorageType = DataBuffer1D<float>::StorageType::CPU;
    _partAges->createCpuStorage();
    _partAges->createBufferStorage(GL_FLOAT, 1);
    _partAges->resize(0);

    // acceleration grid for particles
    //_accelParticles = make_unique<GridData2D<vector<unsigned int>*>>(
    _accelParticles = make_unique<GridData2D<vector<unsigned int>>>(
        _domainLeft, _domainRight,
        _domainBottom, _domainTop,
        _accelParticlesRes, _accelParticlesRes
        );
    _accelParticles->createCpuStorage();
    //for (uint i = 0; i < _accelParticlesRes; i++) {
    //    for (uint j = 0; j < _accelParticlesRes; j++) {
    //        _accelParticles->setCpuData(i, j, new vector<unsigned int>);
    //    }
    //}





    _bufferGridPoints = make_unique<DataBuffer1D<vec2>>(
        _velocityField->nbElementsX() * _velocityField->nbElementsY());
    _bufferGridPoints->createCpuStorage();
    _bufferGridPoints->createBufferStorage(GL_FLOAT, 2);
    _bufferGridPoints->sourceStorageType = DataBuffer1D<vec2>::StorageType::CPU;

    _bufferArrows = make_unique<DataBuffer1D<vec2>>(
        _velocityField->nbElementsX() * _velocityField->nbElementsY());
    _bufferArrows->createCpuStorage();
    _bufferArrows->createBufferStorage(GL_FLOAT, 2);
    _bufferArrows->sourceStorageType = DataBuffer1D<vec2>::StorageType::CPU;


    //colors = new DataBuffer1D<vec3>(1);
    //colors->sourceStorageType = DataBuffer1D<vec3>::StorageType::CPU;
    //colors->createCpuStorage();
    //colors->createBufferStorage(GL_FLOAT, 3);
    //colors->resize(0);


    _obstacleLines = make_unique<DataBuffer1D<vec2>>(1);
    _obstacleLines->sourceStorageType = DataBuffer1D<vec2>::StorageType::CPU;
    _obstacleLines->createCpuStorage();
    _obstacleLines->createBufferStorage(GL_FLOAT, 2);
    _obstacleLines->resize(0);



    _vecX = make_unique<DataBuffer1D<scalar_inversion_storage>>(0);
    _vecX->sourceStorageType = DataBuffer1D<scalar_inversion_storage>::StorageType::CPU;
    _vecX->createCpuStorage();

    _vecTemp = make_unique<DataBuffer1D<scalar_inversion_storage>>(0);
    _vecTemp->sourceStorageType = DataBuffer1D<scalar_inversion_storage>::StorageType::CPU;
    _vecTemp->createCpuStorage();

    _vecXForces = make_unique<DataBuffer1D<scalar_inversion_storage>>(0);
    _vecXForces->sourceStorageType = DataBuffer1D<scalar_inversion_storage>::StorageType::CPU;
    _vecXForces->createCpuStorage();

    _vecXBoundaryForces = make_unique<DataBuffer1D<scalar_inversion_storage>>(0);
    _vecXBoundaryForces->sourceStorageType = DataBuffer1D<scalar_inversion_storage>::StorageType::CPU;
    _vecXBoundaryForces->createCpuStorage();

    _vecB = make_unique<DataBuffer1D<scalar_inversion_storage>>(0);
    _vecB->sourceStorageType = DataBuffer1D<scalar_inversion_storage>::StorageType::CPU;
    _vecB->createCpuStorage();


    /*_accelBasisCentersIds = new DataBuffer2D<std::vector<unsigned int>*>(accelBasisRes, accelBasisRes);
    _accelBasisCentersIds->sourceStorageType = DataBuffer2D<std::vector<unsigned int>*>::StorageType::CPU;*/
    _accelBasisCentersIds = make_unique<DataBuffer2D<std::vector<unsigned int>>>(_accelBasisRes, _accelBasisRes);
    _accelBasisCentersIds->sourceStorageType = DataBuffer2D<std::vector<unsigned int>>::StorageType::CPU;
    _accelBasisCentersIds->createCpuStorage();
    /*for (uint i = 0; i < accelBasisRes; i++) {
        for (uint j = 0; j < accelBasisRes; j++) {
            _accelBasisCentersIds->setCpuData(i, j, new vector<unsigned int>);
        }
    }*/


    _integrationGridGpu = make_unique<DataBuffer2D<vec4>>(((_integralGridRes + 1) - 1) / INTEGRAL_GPU_GROUP_DIM + 1, ((_integralGridRes + 1) - 1) / INTEGRAL_GPU_GROUP_DIM + 1);
    _integrationGridGpu->sourceStorageType = DataBuffer2D<vec4>::StorageType::TEXTURE2D;
    _integrationGridGpu->createTexture2DStorage(GL_RGBA, GL_RGBA32F, GL_RGBA, GL_FLOAT, 1);

    // Read from buffer instead of image directly, so we only need to transfer a single float instead of the whole image data
    _integrationTransferBufferGpu = make_unique<DataBuffer1D<vec4>>(1);
    _integrationTransferBufferGpu->sourceStorageType = DataBuffer1D<vec4>::StorageType::CPU;
    _integrationTransferBufferGpu->createCpuStorage();
    _integrationTransferBufferGpu->createBufferStorage(GL_FLOAT, 4);
    _integrationTransferBufferGpu->sourceStorageType = DataBuffer1D<vec4>::StorageType::BUFFER;



    _integrationMultipleTransferBufferGpu = make_unique<DataBuffer1D<float>>(1);
    _integrationMultipleTransferBufferGpu->sourceStorageType = DataBuffer1D<float>::StorageType::CPU;
    _integrationMultipleTransferBufferGpu->createCpuStorage();
    _integrationMultipleTransferBufferGpu->setCpuData(0, 1.2345f);
    _integrationMultipleTransferBufferGpu->createBufferStorage(GL_FLOAT, 1, GL_MAP_READ_BIT | GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT);
    _integrationMultipleTransferBufferGpu->sourceStorageType = DataBuffer1D<float>::StorageType::BUFFER;


    //integrationBasisCentersBufferGpu = new DataBuffer1D<vec2>(1);
    _integrationBasisCentersBufferGpu = make_unique<DataBuffer1D<vec2>>(1);
    _integrationBasisCentersBufferGpu->sourceStorageType = DataBuffer1D<vec2>::StorageType::CPU;
    _integrationBasisCentersBufferGpu->createCpuStorage();
    _integrationBasisCentersBufferGpu->createBufferStorage(GL_FLOAT, 2, GL_MAP_WRITE_BIT); // aligned to float4 for GPU.
    _integrationBasisCentersBufferGpu->sourceStorageType = DataBuffer1D<vec2>::StorageType::BUFFER;


    //_intersectingBasesIds = make_unique<DataBuffer1D<std::vector<unsigned int>*>>(0);
    _intersectingBasesIds = make_unique<DataBuffer1D<vector<unsigned int>>>(0);
    _intersectingBasesIds->createCpuStorage();
    _intersectingBasesIds->sourceStorageType = DataBuffer1D<vector<unsigned int>*>::StorageType::CPU;
    _intersectingBasesIds->sourceStorageType = DataBuffer1D<vector<unsigned int>>::StorageType::CPU;

    //_intersectingBasesSignificantBBIds = make_unique<DataBuffer1D<std::vector<unsigned int>*>>(0);
    _intersectingBasesSignificantBBIds = make_unique<DataBuffer1D<vector<unsigned int>>>(0);
    _intersectingBasesSignificantBBIds->createCpuStorage();
    //_intersectingBasesSignificantBBIds->sourceStorageType = DataBuffer1D<std::vector<unsigned int>*>::StorageType::CPU;
    _intersectingBasesSignificantBBIds->sourceStorageType = DataBuffer1D<vector<unsigned int>>::StorageType::CPU;

    //_intersectingBasesIdsTransport = make_unique<DataBuffer1D<std::vector<unsigned int>*>>(0);
    _intersectingBasesIdsTransport = make_unique<DataBuffer1D<vector<unsigned int>>>(0);
    _intersectingBasesIdsTransport->createCpuStorage();
    //_intersectingBasesIdsTransport->sourceStorageType = DataBuffer1D<std::vector<unsigned int>*>::StorageType::CPU;
    _intersectingBasesIdsTransport->sourceStorageType = DataBuffer1D<vector<unsigned int>>::StorageType::CPU;

    for (int iRelFreq = 0; iRelFreq < _nbExplicitTransferFreqs; iRelFreq++) {
        //_intersectingBasesIdsDeformation[iRelFreq] = make_unique<DataBuffer1D<std::vector<CoeffBBDecompressedIntersectionInfo>*>>(0);
        _intersectingBasesIdsDeformation[iRelFreq] = make_unique<DataBuffer1D<vector<CoeffBBDecompressedIntersectionInfo>>>(0);
        _intersectingBasesIdsDeformation[iRelFreq]->createCpuStorage();
        //_intersectingBasesIdsDeformation[iRelFreq]->sourceStorageType = DataBuffer1D<std::vector<CoeffBBDecompressedIntersectionInfo>*>::StorageType::CPU;
        _intersectingBasesIdsDeformation[iRelFreq]->sourceStorageType = DataBuffer1D<vector<CoeffBBDecompressedIntersectionInfo>>::StorageType::CPU;
    }


    return true;
}