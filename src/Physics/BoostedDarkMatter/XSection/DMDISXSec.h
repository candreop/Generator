//____________________________________________________________________________
/*!

\class    genie::DMDISXSec

\brief    Computes the DMDIS Cross Section. \n
          Is a concrete implementation of the XSecIntegratorI interface. \n

\author   Joshua Berger <jberger \at physics.wisc.edu>
          University of Wisconsin-Madison

          Costas Andreopoulos <c.andreopoulos \at cern.ch>
          University of Liverpool

\created  September 4, 2017

\cpright  Copyright (c) 2003-2025, The GENIE Collaboration
          For the full text of the license visit http://copyright.genie-mc.org
          
*/
//____________________________________________________________________________

#ifndef _DMDIS_XSEC_H_
#define _DMDIS_XSEC_H_

#include "Physics/XSectionIntegration/XSecIntegratorI.h"

namespace genie {

class DMDISXSec : public XSecIntegratorI {

public:
  DMDISXSec();
  DMDISXSec(string config);
  virtual ~DMDISXSec();

  //! XSecIntegratorI interface implementation
  double Integrate(const XSecAlgorithmI * model, const Interaction * i) const;

  //! Overload the Algorithm::Configure() methods to load private data
  //! members from configuration options
  void Configure(const Registry & config);
  void Configure(string config);

private:
  void   LoadConfig (void);

  void   CacheFreeNucleonXSec(const XSecAlgorithmI * model, const Interaction * in) const;
  string CacheBranchName     (const XSecAlgorithmI * model, const Interaction * in) const;

  double fVldEmin;
  double fVldEmax;
};

}       // genie namespace
#endif  // _DMDIS_XSEC_H_
