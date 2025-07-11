//____________________________________________________________________________
/*!

\class    genie::FRHadronicSystemGenerator

\brief    Generates the f/s hadronic system in diffractive interactions.
          Is a concrete implementation of the EventRecordVisitorI interface.

\author   Costas Andreopoulos <c.andreopoulos \at cern.ch>
          University of Liverpool

\created  October 03, 2004

\cpright  Copyright (c) 2003-2025, The GENIE Collaboration
          For the full text of the license visit http://copyright.genie-mc.org          
*/
//____________________________________________________________________________

#ifndef _DIFFRACTIVE_HADRONIC_SYSTEM_GENERATOR_H_
#define _DIFFRACTIVE_HADRONIC_SYSTEM_GENERATOR_H_

#include "Physics/Common/HadronicSystemGenerator.h"

namespace genie {

class DFRHadronicSystemGenerator : public HadronicSystemGenerator {

public :
  DFRHadronicSystemGenerator();
  DFRHadronicSystemGenerator(string config);
 ~DFRHadronicSystemGenerator();

  //-- implement the EventRecordVisitorI interface
  void ProcessEventRecord(GHepRecord * event_rec) const;
};

}      // genie namespace
#endif // _DIFFRACTIVE_HADRONIC_SYSTEM_GENERATOR_H_
