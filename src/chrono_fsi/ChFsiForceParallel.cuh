// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All right reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Author: Arman Pazouki
// =============================================================================
//
// Base class for processing sph force in fsi system.//
// =============================================================================

#ifndef CH_FSI_FORCEPARALLEL_H_
#define CH_FSI_FORCEPARALLEL_H_

#include "chrono_fsi/ChApiFsi.h"
#include "chrono_fsi/ChFsiGeneral.cuh"
#include "chrono_fsi/ChFsiDataManager.cuh"
#include "chrono_fsi/ChBce.cuh"
#include "chrono_fsi/ChCollisionSystemFsi.cuh"

namespace chrono {
namespace fsi {

class CH_FSI_API ChFsiForceParallel : public ChFsiGeneral {
 public:
  ChFsiForceParallel(ChBce* otherBceWorker,
                     SphMarkerDataD* otherSortedSphMarkersD,
                     ProximityDataD* otherMarkersProximityD,
                     FsiGeneralData* otherFsiGeneralData,
                     SimParams* otherParamsH,
                     NumberOfObjects* otherNumObjects);
  ~ChFsiForceParallel();

  /**
* @brief Calculates the force on each particles. See collideSphereSphere.cuh for more info.
* @details See collideSphereSphere.cuh for more info
*/
  void ForceSPH(SphMarkerDataD* otherSphMarkersD, FsiBodiesDataD* otherFsiBodiesD);

  virtual void Finalize();

  ///////////////////////////////////////////////////////////////////
  static void CopySortedToOriginal_Invasive_R3(thrust::device_vector<Real3>& original,
                                               thrust::device_vector<Real3>& sorted,
                                               const thrust::device_vector<uint>& gridMarkerIndex);
  static void CopySortedToOriginal_NonInvasive_R3(thrust::device_vector<Real3>& original,
                                                  thrust::device_vector<Real3>& sorted,
                                                  const thrust::device_vector<uint>& gridMarkerIndex);
  static void CopySortedToOriginal_Invasive_R4(thrust::device_vector<Real4>& original,
                                               thrust::device_vector<Real4>& sorted,
                                               const thrust::device_vector<uint>& gridMarkerIndex);
  static void CopySortedToOriginal_NonInvasive_R4(thrust::device_vector<Real4>& original,
                                                  thrust::device_vector<Real4>& sorted,
                                                  const thrust::device_vector<uint>& gridMarkerIndex);
  ///////////////////////////////////////////////////////////////////

 private:
  void CalculateXSPH_velocity();
  void CollideWrapper();
  ChCollisionSystemFsi* fsiCollisionSystem;
  ChBce* bceWorker;

  SphMarkerDataD* sphMarkersD;
  SphMarkerDataD* sortedSphMarkersD;
  ProximityDataD* markersProximityD;
  FsiGeneralData* fsiGeneralData;

  SimParams* paramsH;
  NumberOfObjects* numObjectsH;

  thrust::device_vector<Real3> vel_XSPH_Sorted_D;

  void collide(thrust::device_vector<Real4>& sortedDerivVelRho_fsi_D,
               thrust::device_vector<Real3>& sortedPosRad,
               thrust::device_vector<Real3>& sortedVelMas,
               thrust::device_vector<Real3>& vel_XSPH_Sorted_D,
               thrust::device_vector<Real4>& sortedRhoPreMu,
               thrust::device_vector<Real3>& velMas_ModifiedBCE,
               thrust::device_vector<Real4>& rhoPreMu_ModifiedBCE,

               thrust::device_vector<uint>& gridMarkerIndex,
               thrust::device_vector<uint>& cellStart,
               thrust::device_vector<uint>& cellEnd);

  void RecalcVelocity_XSPH(thrust::device_vector<Real3>& vel_XSPH_Sorted_D,
                           thrust::device_vector<Real3>& sortedPosRad,
                           thrust::device_vector<Real3>& sortedVelMas,
                           thrust::device_vector<Real4>& sortedRhoPreMu,
                           thrust::device_vector<uint>& gridMarkerIndex,
                           thrust::device_vector<uint>& cellStart,
                           thrust::device_vector<uint>& cellEnd);

  void AddGravityToFluid();
};
}  // end namespace fsi
}  // end namespace chrono
#endif /* CH_COLLISIONSYSTEM_FSI_H_ */
