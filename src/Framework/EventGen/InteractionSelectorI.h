//____________________________________________________________________________
/*!

\class   genie::InteractionSelectorI

\brief   Defines the InteractionSelectorI interface to be implemented by
         algorithms selecting interactions to be generated.

\author  Costas Andreopoulos <c.andreopoulos \at cern.ch>
 University of Liverpool

\created December 05, 2004

\cpright Copyright (c) 2003-2025, The GENIE Collaboration
         For the full text of the license visit http://copyright.genie-mc.org 
*/
//____________________________________________________________________________

#ifndef _INTERACTION_SELECTOR_I_H_
#define _INTERACTION_SELECTOR_I_H_

#include "Framework/Algorithm/Algorithm.h"

class TLorentzVector;

namespace genie {

class InteractionGeneratorMap;
class EventRecord;

class InteractionSelectorI : public Algorithm {

public :
  virtual ~InteractionSelectorI();

  //!  Define the InteractionSelectorI interface
  virtual EventRecord * SelectInteraction
    (const InteractionGeneratorMap * igmp, const TLorentzVector & p4) const = 0;

protected:
  InteractionSelectorI();
  InteractionSelectorI(string name);
  InteractionSelectorI(string name, string config);
};

}      // genie namespace

#endif // _INTERACTION_SELECTOR_I_H_
