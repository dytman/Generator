//____________________________________________________________________________
/*!

\class    genie::KNOPythiaHadronization

\brief    A 'composite' hadronization model using a KNO-based hadronization
          model at low W and PYTHIA/JETSET at higher W.
          Contains no new hadronization code but merely a configurable KNO to
          PYTHIA transition scheme.

\author   Costas Andreopoulos <C.V.Andreopoulos@rl.ac.uk>
          CCLRC, Rutherford Appleton Laboratory

\created  June 08, 2006

\cpright  Copyright (c) 2003-2006, GENIE Neutrino MC Generator Collaboration
          All rights reserved.
          For the licensing terms see $GENIE/USER_LICENSE.
*/
//____________________________________________________________________________

#ifndef _KNO_PYTHIA_HADRONIZATION_H_
#define _KNO_PYTHIA_HADRONIZATION_H_

#include "Fragmentation/HadronizationModelI.h"

namespace genie {

class KNOPythiaHadronization : public HadronizationModelI {

public:

  KNOPythiaHadronization();
  KNOPythiaHadronization(string config);
  virtual ~KNOPythiaHadronization();

  //! implement the HadronizationModelI interface
  void           Initialize   (void)                 const;
  TClonesArray * Hadronize    (const Interaction * ) const;
  double         Weight       (void)                 const;

  //! overload the Algorithm::Configure() methods to load private data
  //! members from configuration options
  void Configure(const Registry & config);
  void Configure(string config);

private:

  void LoadConfig (void);

  //! methods for specific transition methods
  TClonesArray * LinearTransitionWindowMethod(const Interaction *) const;

  mutable double fWeight;

  //! configuration

  const HadronizationModelI * fKNOHadronizer;    ///< KNO Hadronizer
  const HadronizationModelI * fPythiaHadronizer; ///< PYTHIA Hadronizer

  int    fMethod;       ///< KNO -> PYTHIA transition method
  double fWminTrWindow; ///< min W in transition region (pure KNO    < Wmin)
  double fWmaxTrWindow; ///< max W in transition region (pure PYTHIA > Wmax)
};

}         // genie namespace

#endif    // _KNO_PYTHIA_HADRONIZATION_H_

