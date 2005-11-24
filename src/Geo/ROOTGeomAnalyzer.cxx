//____________________________________________________________________________
/*!

\class   genie::geometry::ROOTGeomAnalyzer

\brief   A ROOT/GEANT Geometry Analyzer

\author  Anselmo Meregaglia <anselmo.meregaglia@cern.ch>
         ETH Zurich

\created May 24, 2005

*/
//____________________________________________________________________________

#include <cassert>

#include <TGeoVolume.h>
#include <TGeoManager.h>
#include <TGeoShape.h>
#include <TGeoMedium.h>
#include <TGeoMaterial.h>
#include <TObjArray.h>
#include <TLorentzVector.h>
#include <TVector3.h>
#include <TSystem.h>

#include "Conventions/Units.h"
#include "EVGDrivers/PathLengthList.h"
#include "Geo/ROOTGeomAnalyzer.h"
#include "Messenger/Messenger.h"
#include "PDG/PDGCodeList.h"
#include "PDG/PDGLibrary.h"
#include "Utils/PrintUtils.h"

#include <TPolyMarker3D.h>
#include <TGeoBBox.h>
#include "Numerical/RandomGen.h"

using namespace genie;
using namespace genie::geometry;

//___________________________________________________________________________
ROOTGeomAnalyzer::ROOTGeomAnalyzer(string geometry_filename) :
GeomAnalyzerI()
{
  this->Initialize();
  this->Load(geometry_filename);
}
//___________________________________________________________________________
ROOTGeomAnalyzer::ROOTGeomAnalyzer(TGeoManager * gm) :
GeomAnalyzerI()
{
  this->Initialize();
  this->Load(gm);
}
//___________________________________________________________________________
ROOTGeomAnalyzer::~ROOTGeomAnalyzer()
{
  this->CleanUp();
}
//___________________________________________________________________________
void ROOTGeomAnalyzer::SetUnits(double u)
{
// Use the units of your input geometry, eg
//               geom.SetUnits(genie::units::centimeter)
// GENIE uses the physical system of units (hbar=c=1) almost throughtout so
// everything is expressed in GeV but when analyzing detector geometries we
// use meters. Setting your input geometry units will allow us to figure the
// conversion factor.
// As input, use one of the constants in $GENIE/src/Conventions/Units.h

  fScale = u/units::meter;

  LOG("GROOTGeom",pNOTICE) << "Geometry units scale factor: " << fScale;
}
//___________________________________________________________________________
void ROOTGeomAnalyzer::SetTopVolName(string name)
{
// Set the name of the top volume.
// This driver would ask the TGeoManager::GetTopVolume() for the top volume.
// Use this method for changing this if for example you want to set a smaller
// volume as the top one so as to generate events only in a specific part of
// your detector.

  fTopVolumeName = name;

  LOG("GROOTGeom",pNOTICE) << "Geometry Top Volume name: " << fTopVolumeName;

  TGeoVolume * gvol = fGeometry->GetVolume(fTopVolumeName.c_str());

  if(!gvol) {
     LOG("GROOTGeom",pWARN) << "Could not find volume: " << name.c_str();
     LOG("GROOTGeom",pWARN) << "Will not change the current top volume";
     fTopVolumeName = "";
     return;
  }
  fTopVolume = gvol;
}
//___________________________________________________________________________
const PathLengthList & ROOTGeomAnalyzer::ComputeMaxPathLengths(void)
{
  LOG("GROOTGeom", pINFO)
                  << "Computing the maximum path lengths for all materials";

  if(!fGeometry) {
      LOG("GROOTGeom",pERROR) << "No ROOT geometry is loaded!";
      return *fCurrMaxPathLengthList;
  }

  //-- initialize max path lengths
  fCurrMaxPathLengthList->SetAllToZero();

  //-- get a bounding box

  LOG("GROOTGeom", pINFO) << "Getting a TGeoBBox enclosing the detector";
  TGeoShape * TS  = fTopVolume->GetShape();
  TGeoBBox *  box = (TGeoBBox *)TS;

  //get box origin and dimensions
  double dx = box->GetDX(); // half-length
  double dy = box->GetDY(); // half-length
  double dz = box->GetDZ(); // half-length
  double ox = (box->GetOrigin())[0];
  double oy = (box->GetOrigin())[1];
  double oz = (box->GetOrigin())[2];
  LOG("GROOTGeom",pINFO)
     << "Box dimensions : x = "<< 2*dx << ", y = "<< 2*dy << ", z = "<< 2*dz;
  LOG("GROOTGeom",pINFO)
     << "Box origin     : x = "<< ox   << ", y = "<< oy   << ", z = "<<   oz;

  //generate 200 random points on each surface, use 200 rays to
  //calculate maximum path for each material

  RandomGen* rand=RandomGen::Instance();
  TRandom & r3=rand->Random3();

  LOG("GROOTGeom",pINFO)
        << "Will generate [" << fNPoints << "] random points on each box surface";
  LOG("GROOTGeom",pINFO)
        << "Will generate [" << fNRays   << "] rays for each point";

  //loop on materials

  int pdgc(0);
  vector<int>::iterator itrPDG;
  for(itrPDG=fCurrPDGCodeList->begin();itrPDG!=fCurrPDGCodeList->end();itrPDG++)
    {
      pdgc=*itrPDG;
      LOG("GROOTGeom", pINFO)
             <<" Calculating max path length for material: " << pdgc;

      int igen(0);
      int Rgen(0);
      int maxPoints(fNPoints);
      int maxRays(fNRays);
      double xyz[3];
      double direction[3];
      double MaxPath(0);
      double Length(0);
      double dirTot(0);

      //top
      LOG("GROOTGeom",pINFO) << "Box surface scanned: [TOP]";
      igen=0;
      while(igen<maxPoints)
        {
          igen++;
          xyz[0] = ox-dx+2*dx*r3.Rndm();
          xyz[1] = oy+dy;
          xyz[2] = oz-dz+2*dz*r3.Rndm();

          Rgen=0;
          while(Rgen<maxRays)
            {
              Rgen++;
              direction[0]=-0.5+r3.Rndm();
              direction[1]=-r3.Rndm();
              direction[2]=-0.5+r3.Rndm();

              dirTot=sqrt( direction[0]*direction[0] + direction[1]*direction[1] + direction[2]*direction[2]);

              direction[0]/=dirTot;
              direction[1]/=dirTot;
              direction[2]/=dirTot;

              Length=ComputeMaxPathLengthPDG(xyz,direction,pdgc);
              if(Length>MaxPath)
                MaxPath=Length;

            }
        }


      //bottom
      LOG("GROOTGeom",pINFO) << "Box surface scanned: [BOTTOM]";
      igen=0;
      while(igen<maxPoints)
        {
          igen++;
          xyz[0] = ox-dx+2*dx*r3.Rndm();
          xyz[1] = oy-dy;
          xyz[2] = oz-dz+2*dz*r3.Rndm();

          Rgen=0;
          while(Rgen<maxRays)
            {
              Rgen++;
              direction[0]=-0.5+r3.Rndm();
              direction[1]=r3.Rndm();
              direction[2]=-0.5+r3.Rndm();

              dirTot=sqrt( direction[0]*direction[0] + direction[1]*direction[1] + direction[2]*direction[2]);

              direction[0]/=dirTot;
              direction[1]/=dirTot;
              direction[2]/=dirTot;

              Length=ComputeMaxPathLengthPDG(xyz,direction,pdgc);
              if(Length>MaxPath)
                MaxPath=Length;
            }
        }

      //left
      LOG("GROOTGeom",pINFO) << "Box surface scanned: [LEFT]";
      igen=0;
      while(igen<maxPoints)
        {
          igen++;
          xyz[0] = ox-dx;
          xyz[1] = oy-dy+2*dy*r3.Rndm();
          xyz[2] = oz-dz+2*dz*r3.Rndm();

          Rgen=0;
          while(Rgen<maxRays)
            {
              Rgen++;
              direction[0]=r3.Rndm();
              direction[1]=-0.5+r3.Rndm();
              direction[2]=-0.5+r3.Rndm();

              dirTot=sqrt( direction[0]*direction[0] + direction[1]*direction[1] + direction[2]*direction[2]);

              direction[0]/=dirTot;
              direction[1]/=dirTot;
              direction[2]/=dirTot;

              Length=ComputeMaxPathLengthPDG(xyz,direction,pdgc);
              if(Length>MaxPath)
                MaxPath=Length;
            }
        }

      //right
      LOG("GROOTGeom",pINFO) << "Box surface scanned: [RIGHT]";
      igen=0;
      while(igen<maxPoints)
        {
          igen++;
          xyz[0] = ox+dx;
          xyz[1] = oy-dy+2*dy*r3.Rndm();
          xyz[2] = oz-dz+2*dz*r3.Rndm();

          Rgen=0;
          while(Rgen<maxRays)
            {
              Rgen++;
              direction[0]=-r3.Rndm();
              direction[1]=-0.5+r3.Rndm();
              direction[2]=-0.5+r3.Rndm();

              dirTot=sqrt( direction[0]*direction[0] + direction[1]*direction[1] + direction[2]*direction[2]);

              direction[0]/=dirTot;
              direction[1]/=dirTot;
              direction[2]/=dirTot;

              Length=ComputeMaxPathLengthPDG(xyz,direction,pdgc);
              if(Length>MaxPath)
                MaxPath=Length;
            }
        }

      //back
      LOG("GROOTGeom",pINFO) << "Box surface scanned: [BACK]";
      igen=0;
      while(igen<maxPoints)
        {
          igen++;
          xyz[0] = ox-dx+2*dx*r3.Rndm();
          xyz[1] = oy-dy+2*dy*r3.Rndm();
          xyz[2] = oz-dz;

          Rgen=0;
          while(Rgen<maxRays)
            {
              Rgen++;
              direction[0]=-0.5+r3.Rndm();
              direction[1]=-0.5+r3.Rndm();
              direction[2]=r3.Rndm();

              dirTot=sqrt( direction[0]*direction[0] + direction[1]*direction[1] + direction[2]*direction[2]);

              direction[0]/=dirTot;
              direction[1]/=dirTot;
              direction[2]/=dirTot;

              Length=ComputeMaxPathLengthPDG(xyz,direction,pdgc);
              if(Length>MaxPath)
                MaxPath=Length;
            }
        }

      //front
      LOG("GROOTGeom",pINFO) << "Box surface scanned: [FRONT]";
      igen=0;
      while(igen<maxPoints)
        {
          igen++;
          xyz[0] = ox-dx+2*dx*r3.Rndm();
          xyz[1] = oy-dy+2*dy*r3.Rndm();
          xyz[2] = oz+dz;

          Rgen=0;
          while(Rgen<maxRays)
            {
              Rgen++;
              direction[0]=-0.5+r3.Rndm();
              direction[1]=-0.5+r3.Rndm();
              direction[2]=-r3.Rndm();

              dirTot=sqrt( direction[0]*direction[0] + direction[1]*direction[1] + direction[2]*direction[2]);

              direction[0]/=dirTot;
              direction[1]/=dirTot;
              direction[2]/=dirTot;

              Length=ComputeMaxPathLengthPDG(xyz,direction,pdgc);
              if(Length>MaxPath)
                MaxPath=Length;
            }
        }

      LOG("GROOTGeom", pINFO) << "Max path length found = " << MaxPath;

      fCurrMaxPathLengthList->AddPathLength(pdgc,MaxPath);
    }

  this->ScalePathLengths(*fCurrMaxPathLengthList);

  return *fCurrMaxPathLengthList;
}
//________________________________________________________________________
void ROOTGeomAnalyzer::Initialize()
{
  LOG("GROOTGeom", pINFO) << "Initializing ROOT geometry driver";

  fCurrMaxPathLengthList = 0;
  fCurrPathLengthList    = 0;
  fCurrPDGCodeList       = 0;
  fTopVolume             = 0;
  fTopVolumeName         = "";

  // some defaults:
  this -> SetScannerNPoints    (200);
  this -> SetScannerNRays      (200);
  this -> SetUnits             (genie::units::meter);
  this -> SetWeightWithDensity (true);
}
//___________________________________________________________________________
void ROOTGeomAnalyzer::CleanUp(void)
{
  if( fCurrPathLengthList    ) delete fCurrPathLengthList;
  if( fCurrMaxPathLengthList ) delete fCurrMaxPathLengthList;
  if( fCurrPDGCodeList       ) delete fCurrPDGCodeList;
}
//___________________________________________________________________________
void ROOTGeomAnalyzer::Load(string filename)
{
  LOG("GROOTGeom", pINFO) << "Loading geometry from: " << filename;

  bool is_accessible = ! (gSystem->AccessPathName( filename.c_str() ));
  if (!is_accessible) {
     LOG("GROOTGeom", pERROR)
       << "The ROOT geometry doesn't exist! Initialization failed!";
     return;
  }
  fGeometry = TGeoManager::Import(filename.c_str());

  if(!fGeometry) {
    LOG("GROOTGeom", pFATAL) << "Null TGeoManager! Aborting";
    assert(fGeometry);
  }

  this->BuildListOfTargetNuclei();

  const PDGCodeList & pdglist = this->ListOfTargetNuclei();

  fCurrPathLengthList    = new PathLengthList(pdglist);
  fCurrMaxPathLengthList = new PathLengthList(pdglist);
  fCurrVertex            = new TVector3(0.,0.,0.);

  // ask geometry manager for its top volume
  fTopVolume = fGeometry->GetTopVolume();
  LOG("GROOTGeom", pFATAL) << "Could not get top volume!!!";
  assert(fTopVolume);
}
//___________________________________________________________________________
void ROOTGeomAnalyzer::Load(TGeoManager * gm)
{
  LOG("GROOTGeom", pINFO)
                   << "A TGeoManager is being passed to the geometry driver";
  fGeometry = gm;

  if(!fGeometry) {
    LOG("GROOTGeom", pFATAL) << "Null TGeoManager! Aborting";
    assert(fGeometry);
  }

  this->BuildListOfTargetNuclei();

  const PDGCodeList & pdglist = this->ListOfTargetNuclei();

  fCurrPathLengthList    = new PathLengthList(pdglist);
  fCurrMaxPathLengthList = new PathLengthList(pdglist);
  fCurrVertex            = new TVector3(0.,0.,0.);

  // ask geometry manager for its top volume
  fTopVolume = fGeometry->GetTopVolume();
  LOG("GROOTGeom", pFATAL) << "Could not get top volume!!!";
  assert(fTopVolume);
}
//___________________________________________________________________________
const PDGCodeList & ROOTGeomAnalyzer::ListOfTargetNuclei(void)
{
  return *fCurrPDGCodeList;
}
//___________________________________________________________________________
const PathLengthList & ROOTGeomAnalyzer::ComputePathLengths(
                          const TLorentzVector & x, const TLorentzVector & p)
{
// Computes the path-length within each detector material for a neutrino
// starting from point x and travelling along the direction of p.

  LOG("GROOTGeom", pINFO) << "Computing path-lengths for the input neutrino";

  LOG("GROOTGeom", pDEBUG)
       << "\nInput nu: 4p = " << utils::print::P4AsShortString(&p)
       << ", 4x = " << utils::print::X4AsString(&x);

  // reset current list of path-lengths
  fCurrPathLengthList->SetAllToZero();

  TGeoVolume *   vol = 0;
  TGeoMedium *   med = 0;
  TGeoMaterial * mat = 0;

  int    FlagNotInYet(0);
  bool   condition(kTRUE);
  double step(0);

  TVector3 r(x.X(), x.Y(), x.Z()); // current position

  fGeometry->SetCurrentDirection(p.Px()/p.P(),p.Py()/p.P(),p.Pz()/p.P());

  while((!FlagNotInYet) || condition) {

      condition = kTRUE;

      LOG("GROOTGeom",pDEBUG)
           << "Position = " << utils::print::Vec3AsString(&r)
                             << ", flag(not in yet) = " << FlagNotInYet;

      fGeometry -> SetCurrentPoint (r[0],r[1],r[2]);
      fGeometry -> FindNode        (r[0],r[1],r[2]);

      vol =  fGeometry->GetCurrentVolume();
      med = 0;
      mat = 0;

      LOG("GROOTGeom", pDEBUG) << "Current volume: " << vol->GetName();

      if(fGeometry->IsOutside() || !vol) {

        if(FlagNotInYet) break;
        condition = kFALSE;

        fGeometry->FindNextBoundary();
        step=fGeometry->GetStep();

        while(!fGeometry->IsEntering()) {
          fGeometry->Step();
          step=fGeometry->GetStep();
          LOG("GROOTGeom",pDEBUG) <<"Stepping...dr = " << step;

          if(this->WillNeverEnter(step)) return *fCurrPathLengthList;
        }
        r[0] += (step * p.Px()/p.P());
        r[1] += (step * p.Py()/p.P());
        r[2] += (step * p.Pz()/p.P());
      }

      if(condition) {
        if(!FlagNotInYet) FlagNotInYet=1;
        med = vol->GetMedium();
        if (!med) condition=kFALSE;
      }

      if(condition) {
          mat = med->GetMaterial();
          if (!mat) condition=kFALSE;
      }

      if(condition) {
          bool ismixt = mat->IsMixture();

          LOG("GROOTGeom",pDEBUG)
              << "Current medium:   " << med->GetName();
          LOG("GROOTGeom",pDEBUG)
              << "Current material: " << mat->GetName()
                 << " (A = " << mat->GetA() << ", Z = " << mat->GetZ() << ")";
          LOG("GROOTGeom",pDEBUG)
              << "Material is mix:  " << utils::print::BoolAsYNString(ismixt);

          if(ismixt) {
            TGeoMixture * mixt = dynamic_cast <TGeoMixture*> (mat);

            int Nelements = mixt->GetNelements();
            LOG("GROOTGeom",pDEBUG) << "Number of elements = " << Nelements;

            TGeoElement * ele = 0;
            for(int i=0; i<Nelements; i++) {

               ele = mixt->GetElement(i);

               int   ion_pdgc = this->GetTargetPdgCode(ele);
               double weight  = this->GetWeight(mat);

               fGeometry->FindNextBoundary();
               step=fGeometry->GetStep();
               while(!fGeometry->IsEntering()) {
                  fGeometry->Step();
                  step=fGeometry->GetStep();
               }
               LOG("GROOTGeom",pDEBUG) <<" IsEntering   = "
                        << utils::print::BoolAsYNString(fGeometry->IsEntering());
               LOG("GROOTGeom",pDEBUG) <<" IsOnBoundary = "
                        << utils::print::BoolAsYNString(fGeometry->IsOnBoundary());
               LOG("GROOTGeom",pDEBUG)
                      <<" PDG-Code = " << ion_pdgc << ", Step = "<<step;

               fCurrPathLengthList->AddPathLength(ion_pdgc,step*weight);
               r[0] += (step * p.Px()/p.P());
               r[1] += (step * p.Py()/p.P());
               r[2] += (step * p.Pz()/p.P());
           }//elements
         } // is mixture

         else {
           int    ion_pdgc = this->GetTargetPdgCode(mat);
           double weight   = this->GetWeight(mat);

           fGeometry->FindNextBoundary();
           step=fGeometry->GetStep();
           while(!fGeometry->IsEntering()) {
              fGeometry->Step();
              step=fGeometry->GetStep();
           }
           LOG("GROOTGeom",pDEBUG) <<" IsEntering   = "
                   << utils::print::BoolAsYNString(fGeometry->IsEntering());
           LOG("GROOTGeom",pDEBUG) <<" IsOnBoundary = "
                   << utils::print::BoolAsYNString(fGeometry->IsOnBoundary());
           LOG("GROOTGeom",pDEBUG)
                   <<" PDG-Code = " << ion_pdgc << ", Step = "<<step;

           fCurrPathLengthList->AddPathLength(ion_pdgc,step*weight);

           r[0] += (step * p.Px()/p.P());
           r[1] += (step * p.Py()/p.P());
           r[2] += (step * p.Pz()/p.P());
         }
     }//condition
  }//while

  this->ScalePathLengths(*fCurrPathLengthList);

  return *fCurrPathLengthList;
}
//___________________________________________________________________________
const TVector3 & ROOTGeomAnalyzer::GenerateVertex(
               const TLorentzVector & x, const TLorentzVector & p, int tgtpdg)
{
// Generates a random vertex, within the detector material with the input
// PDG code, for a neutrino starting from point x and travelling along the
// direction of p

  LOG("GROOTGeom", pINFO)
           << "Generating vtx in material: " << tgtpdg
                                     << " along the input neutrino direction";
  // reset current interaction vertex
  fCurrVertex->SetXYZ(0.,0.,0.);

  LOG("GROOTGeom", pDEBUG)
       << "\nInput nu: 4p = " << utils::print::P4AsShortString(&p)
       << ", 4x = " << utils::print::X4AsString(&x);

  if(!fGeometry) {
      LOG("GROOTGeom",pERROR) << "No ROOT geometry is loaded!";
      return *fCurrVertex;
  }

  //calculate length weighted with density
  double dist(0);

  TGeoVolume *   vol = 0;
  TGeoMedium *   med = 0;
  TGeoMaterial * mat = 0;

  int FlagNotInYet(0);
  bool condition(kTRUE);
  double step(0);

  TVector3 r(x.X(), x.Y(), x.Z()); // current position

  fGeometry->SetCurrentDirection(p.Px()/p.P(),p.Py()/p.P(),p.Pz()/p.P());

  while((!FlagNotInYet) || condition) {

      condition=kTRUE;

      LOG("GROOTGeom",pDEBUG)
           << "Position = " << utils::print::Vec3AsString(&r)
                             << ", flag(not in yet) = " << FlagNotInYet;

      fGeometry -> SetCurrentPoint (r[0],r[1],r[2]);
      fGeometry -> FindNode        (r[0],r[1],r[2]);

      vol = fGeometry->GetCurrentVolume();
      med = 0;
      mat = 0;

      LOG("GROOTGeom", pDEBUG) << "Current volume: " << vol->GetName();

      if (fGeometry->IsOutside() || !vol) {

         condition=kFALSE;
         if(FlagNotInYet) break;

         fGeometry->FindNextBoundary();
         step=fGeometry->GetStep();
         while(!fGeometry->IsEntering()) {
           fGeometry->Step();
           step=fGeometry->GetStep();
         }
         r[1] += (step * p.Px()/p.P());
         r[2] += (step * p.Py()/p.P());
         r[3] += (step * p.Pz()/p.P());
      }

      if(condition) {
        if(!FlagNotInYet) FlagNotInYet=1;
        med = vol->GetMedium();
        if (!med) condition=kFALSE;
      }

      if(condition) {
        mat = med->GetMaterial();
        if (!mat) condition=kFALSE;
        }

      if(condition) {

          bool ismixt = mat->IsMixture();

          LOG("GROOTGeom",pDEBUG)
              << "Current medium:   " << med->GetName();
          LOG("GROOTGeom",pDEBUG)
              << "Current material: " << mat->GetName()
                 << " (A = " << mat->GetA() << ", Z = " << mat->GetZ() << ")";
          LOG("GROOTGeom",pDEBUG)
              << "Material is mix:  " << utils::print::BoolAsYNString(ismixt);

          int ion_pdgc  = this->GetTargetPdgCode(mat);
          double weight = this->GetWeight(mat);

          fGeometry->FindNextBoundary();
          step=fGeometry->GetStep();
          while(!fGeometry->IsEntering()) {
              fGeometry->Step();
              step=fGeometry->GetStep();
          }

          if(ion_pdgc == tgtpdg) dist+=(step*weight);

          r[1] += (step * p.Px()/p.P());
          r[2] += (step * p.Py()/p.P());
          r[3] += (step * p.Pz()/p.P());
      }//condtion
  }//while

  if(dist==0) {
    LOG("GROOTGeom",pERROR)
        <<"No material selected along this direction from set point!!! ";
    return *fCurrVertex;
  }

  LOG("GROOTGeom",pDEBUG) << "(Distance)x(Density) = " << dist;

  //generate random number between 0 and dist
  RandomGen* rand=RandomGen::Instance();
  TRandom & r3 = rand->Random3();
  double distVertex(r3.Rndm()*dist);
  LOG("GROOTGeom",pDEBUG)
        <<" Random distance in selected material "<<distVertex;

  //-- generate the vertex

  r.SetXYZ(x.X(), x.Y(), x.Z());
  double StepIncrease(0.001);
  double distToVtx(0);
  FlagNotInYet=0;
  condition=kTRUE;

  while(((!FlagNotInYet) || condition) && distToVtx<distVertex) {

      condition=kTRUE;

      LOG("GROOTGeom",pDEBUG)
           << "Position = " << utils::print::Vec3AsString(&r)
                             << ", flag(not in yet) = " << FlagNotInYet;

      r[0] += (StepIncrease * p.Px()/p.P());
      r[1] += (StepIncrease * p.Py()/p.P());
      r[2] += (StepIncrease * p.Pz()/p.P());

      fGeometry -> SetCurrentPoint (r[0],r[1],r[2]);
      fGeometry -> FindNode        (r[0],r[1],r[2]);

      vol = fGeometry->GetCurrentVolume();
      med = 0;
      mat = 0;

      LOG("GROOTGeom",pDEBUG) << "Current volume "<<vol->GetName();

      if(fGeometry->IsOutside() || !vol) {
         condition=kFALSE;
         if(FlagNotInYet) break;
      }

      if(condition) {
         if(!FlagNotInYet) FlagNotInYet=1;
         med = vol->GetMedium();
         if (!med) condition=kFALSE;
      }

      if(condition) {
         mat = med->GetMaterial();
         if (!mat) condition=kFALSE;
      }

      if(condition) {
         LOG("GROOTGeom",pDEBUG)
              << "Current medium:   " << med->GetName();
         LOG("GROOTGeom",pDEBUG)
              << "Current material: " << mat->GetName()
              << " (A = " << mat->GetA() << ", Z = " << mat->GetZ() << ")";

         int    ion_pdgc = this->GetTargetPdgCode(mat);
         double weight   = this->GetWeight(mat);

         if(ion_pdgc == tgtpdg) distToVtx+=(StepIncrease*weight);
     }
  }

  r[0] -= (StepIncrease * p.Px()/p.P());
  r[1] -= (StepIncrease * p.Py()/p.P());
  r[2] -= (StepIncrease * p.Pz()/p.P());

  fCurrVertex->SetXYZ(r[0],r[1],r[2]);

  LOG("GROOTGeom",pDEBUG) << "Vertex = " << utils::print::Vec3AsString(&r);

  return *fCurrVertex;
}
//___________________________________________________________________________
void ROOTGeomAnalyzer::BuildListOfTargetNuclei(void)
{
  fCurrPDGCodeList = new PDGCodeList;

  if(!fGeometry) {
    LOG("GROOTGeom", pERROR) << "No ROOT geometry is loaded!";
    return;
  }

  TObjArray * volume_list = fGeometry->GetListOfVolumes();
  if(!volume_list) {
     LOG("GROOTGeom", pERROR)
        << "Null list of geometry volumes. Can not find build target list!";
     return;
  }

  int numVol = volume_list->GetEntries();
  LOG("GROOTGeom", pDEBUG) << "Number of volumes found: " << numVol;

  for(int ivol = 0; ivol < numVol; ivol++) {

      TGeoVolume * volume = dynamic_cast <TGeoVolume *>(volume_list->At(ivol));

      if(!volume) {
         LOG("GROOTGeom", pWARN)
           << "Got a null geometry volume!! Skiping current list element";
         continue;
      }

      TGeoMaterial * mat = volume->GetMedium()->GetMaterial();

      if(mat->IsMixture()) {
         TGeoMixture * mixt = dynamic_cast <TGeoMixture*> (mat);
         int Nelements = mixt->GetNelements();
         for(int i=0; i<Nelements; i++) {
            TGeoElement * ele = mixt->GetElement(i);
            int ion_pdgc = this->GetTargetPdgCode(ele);
            fCurrPDGCodeList->push_back(ion_pdgc);
         }
      } else {
          int ion_pdgc = this->GetTargetPdgCode(mat);
          fCurrPDGCodeList->push_back(ion_pdgc);
      }
  }
}
//___________________________________________________________________________
double ROOTGeomAnalyzer::ComputeMaxPathLengthPDG(double* XYZ,double* direction,int pdgc)
{
  double weight(0);
  TGeoVolume *vol =0;
  int counterloop(0);
  double Length(0);

  int FlagNotInYet(0);
  bool condition(kTRUE);

  float xx,yy,zz;
  double xyz[3];
  double step(0);
  xx=XYZ[0];
  yy=XYZ[1];
  zz=XYZ[2];

  fGeometry->SetCurrentDirection(direction);

  while(((!FlagNotInYet) || condition) && counterloop <100)
    {
      counterloop++;
      condition=kTRUE;

      xyz[0]=xx;
      xyz[1]=yy;
      xyz[2]=zz;

      fGeometry->SetCurrentPoint(xyz);
      fGeometry->FindNode(xyz[0],xyz[1],xyz[2]);
      vol =  fGeometry->GetCurrentVolume();
      TGeoMedium *med;
      TGeoMaterial *mat;

      if (fGeometry->IsOutside() || !vol)
        {
          condition=kFALSE;

          if(FlagNotInYet)
            break;

          fGeometry->FindNextBoundary();
          step=fGeometry->GetStep();
          while(!fGeometry->IsEntering())
            {
              fGeometry->Step();
              step=fGeometry->GetStep();
            }

          xx+=step * direction[0];
          yy+=step * direction[1];
          zz+=step * direction[2];
        }

      if(condition)
        {
          if(!FlagNotInYet)
            FlagNotInYet=1;

          med = vol->GetMedium();
          if (!med)
            condition=kFALSE;
        }

      if(condition)
        {
          mat = med->GetMaterial();
          if (!mat)
            condition=kFALSE;
        }

      if(condition)
        {
          int ion_pdgc = this->GetTargetPdgCode(mat);
          fGeometry->FindNextBoundary();
          step=fGeometry->GetStep();
          while(!fGeometry->IsEntering())
            {
              fGeometry->Step();
              step=fGeometry->GetStep();
            }

          if(ion_pdgc == pdgc)
            {
              Length+=step;
              weight = this->GetWeight(mat);
            }
          xx+=step * direction[0];
          yy+=step * direction[1];
          zz+=step * direction[2];
        }
    }

  return (Length*weight);
}
//___________________________________________________________________________
double ROOTGeomAnalyzer::GetWeight(TGeoMaterial * mat)
{
  double weight = 1.0;
  if (this->WeightWithDensity()) weight = mat->GetDensity();
  return weight;
}
//___________________________________________________________________________
bool ROOTGeomAnalyzer::WillNeverEnter(double step)
{
// If the neutrino trajectory would never enter the detector, then the
// TGeoManager::GetStep returns the maximum step (1E30).
// Compare surrent step with max step and figure out whether the particle
// would never enter the detector

  if(step > 9.99E29) {
     LOG("GROOTGeom", pINFO) << "Wow! Current step is dr = " << step;
     LOG("GROOTGeom", pINFO) << "This trajectory isn't entering the detector";
     return true;
  } else return false;
}
//___________________________________________________________________________
void ROOTGeomAnalyzer::ScalePathLengths(PathLengthList & pl)
{
// convert path lenghts to default GENIE length scale
//
  LOG("GROOTGeom", pDEBUG)
              << "Scaling path-lengths -> meters (scale = " << fScale << ")";

  PathLengthList::iterator pliter;
  for(pliter = pl.begin(); pliter != pl.end(); ++pliter)
  {
    int pdgc = pliter->first;
    pl.ScalePathLength(pdgc,fScale);
  }
}
//___________________________________________________________________________
int ROOTGeomAnalyzer::GetTargetPdgCode(const TGeoMaterial * const m) const
{
  int A = int(m->GetA());
  int Z = int(m->GetZ());

  int pdgc = pdg::IonPdgCode(A,Z);

  return pdgc;
}
//___________________________________________________________________________
int ROOTGeomAnalyzer::GetTargetPdgCode(const TGeoElement * const e) const
{
  int A = int(e->A());
  int Z = int(e->Z());

  int pdgc = pdg::IonPdgCode(A,Z);

  return pdgc;
}
//___________________________________________________________________________

