//____________________________________________________________________________
/*
 Copyright (c) 2003-2025, The GENIE Collaboration
 For the full text of the license visit http://copyright.genie-mc.org
 

 Author: Joshua Berger <jberger \at physics.wisc.edu>
         University of Wisconsin-Madison

         Costas Andreopoulos <c.andreopoulos \at cern.ch>
 University of Liverpool
*/
//____________________________________________________________________________

#include <TMath.h>
#include <Math/IFunction.h>
#include <Math/IntegratorMultiDim.h>
#include "Math/AdaptiveIntegratorMultiDim.h"

#include "Framework/Algorithm/AlgConfigPool.h"
#include "Framework/Conventions/GBuild.h"
#include "Framework/Conventions/Controls.h"
#include "Framework/Conventions/Constants.h"
#include "Framework/Conventions/Units.h"
#include "Physics/BoostedDarkMatter/XSection/DMDISXSec.h"
#include "Physics/XSectionIntegration/GSLXSecFunc.h"
#include "Framework/Messenger/Messenger.h"
#include "Framework/ParticleData/PDGCodes.h"
#include "Framework/ParticleData/PDGUtils.h"
#include "Framework/Utils/RunOpt.h"
#include "Framework/Numerical/MathUtils.h"
#include "Framework/Utils/Range1.h"
#include "Framework/Utils/Cache.h"
#include "Framework/Utils/CacheBranchFx.h"
#include "Framework/Utils/XSecSplineList.h"
#include "Framework/Numerical/GSLUtils.h"

using namespace genie;
using namespace genie::controls;
using namespace genie::constants;

//____________________________________________________________________________
DMDISXSec::DMDISXSec() :
XSecIntegratorI("genie::DMDISXSec")
{

}
//____________________________________________________________________________
DMDISXSec::DMDISXSec(string config) :
XSecIntegratorI("genie::DMDISXSec", config)
{

}
//____________________________________________________________________________
DMDISXSec::~DMDISXSec()
{

}
//____________________________________________________________________________
double DMDISXSec::Integrate(
                 const XSecAlgorithmI * model, const Interaction * in) const
{
  if(! model->ValidProcess(in) ) return 0.;

  const KPhaseSpace & kps = in->PhaseSpace();
  if(!kps.IsAboveThreshold()) {
     LOG("DMDISXSec", pDEBUG)  << "*** Below energy threshold";
     return 0;
  }

  const InitialState & init_state = in->InitState();
  double Ed = init_state.ProbeE(kRfHitNucRest);

  int nucpdgc = init_state.Tgt().HitNucPdg();
  int NNucl   = (pdg::IsProton(nucpdgc)) ? 
                   init_state.Tgt().Z() : init_state.Tgt().N();
  
  // If the input interaction is off a nuclear target, then chek whether 
  // the corresponding free nucleon cross section already exists at the 
  // cross section spline list. 
  // If yes, calculate the nuclear cross section based on that value.
  //
  XSecSplineList * xsl = XSecSplineList::Instance();
  if(init_state.Tgt().IsNucleus() && !xsl->IsEmpty() ) {
    Interaction * interaction = new Interaction(*in);
    Target * target = interaction->InitStatePtr()->TgtPtr();
    if(pdg::IsProton(nucpdgc)) { target->SetId(kPdgTgtFreeP); }
    else                       { target->SetId(kPdgTgtFreeN); }
    if(xsl->SplineExists(model,interaction)) {
      const Spline * spl = xsl->GetSpline(model, interaction);
      double xsec = spl->Evaluate(Ed);
      LOG("DMDISXSec", pINFO)  
        << "From XSecSplineList: XSec[DIS,free nucleon] (E = " << Ed << " GeV) = " << xsec;
      if(! interaction->TestBit(kIAssumeFreeNucleon) ) { 
          xsec *= NNucl; 
          LOG("DMDISXSec", pINFO)  << "XSec[DIS] (E = " << Ed << " GeV) = " << xsec;
      }
      delete interaction;
      return xsec;
    }
    delete interaction;
  }

  // There was no corresponding free nucleon spline saved in XSecSplineList that
  // could be used to speed up this calculation.
  // Check whether local caching of free nucleon cross sections is allowed.
  // If yes, store free nucleon cross sections at a cache branch and use those 
  // at any subsequent call.
  //
  bool precalc_bare_xsec = RunOpt::Instance()->BareXSecPreCalc();
  if(precalc_bare_xsec) {
     Cache * cache = Cache::Instance();
     Interaction * interaction = new Interaction(*in);
     string key = this->CacheBranchName(model,interaction);
     LOG("DMDISXSec", pINFO) << "Finding cache branch with key: " << key;
     CacheBranchFx * cache_branch =
           dynamic_cast<CacheBranchFx *> (cache->FindCacheBranch(key));
     if(!cache_branch) {
         this->CacheFreeNucleonXSec(model,interaction);
         cache_branch =
           dynamic_cast<CacheBranchFx *> (cache->FindCacheBranch(key));
         assert(cache_branch);
     }
     const CacheBranchFx & cb = (*cache_branch);
     double xsec = cb(Ed);
     if(! interaction->TestBit(kIAssumeFreeNucleon) ) { xsec *= NNucl; }
     LOG("DMDISXSec", pINFO)  << "XSec[DIS] (E = " << Ed << " GeV) = " << xsec;
     delete interaction;
     return xsec;
  }
  else {
    // Just go ahead and integrate the input differential cross section for the 
    // specified interaction.
    //
     Interaction * interaction = new Interaction(*in);
     interaction->SetBit(kISkipProcessChk);
//   interaction->SetBit(kISkipKinematicChk);

     // **Important note** 
     // Based on discussions with Hugh at the GENIE mini-workshop / RAL - July '07
     // The DIS nuclear corrections re-distribute the strength in x,y but do not
     // affect the total cross-section They should be disabled at this step.
     // But they should be enabled at the DIS thread's kinematical selection.
     // Since nuclear corrections don't need to be included at this stage, all the
     // nuclear cross sections can be trivially built from the free nucleon ones.
     //
     interaction->SetBit(kINoNuclearCorrection);

     Range1D_t Wl  = kps.WLim();
     Range1D_t Q2l = kps.Q2Lim();
     LOG("DMDISXSec", pINFO)  
            << "W integration range = [" << Wl.min << ", " << Wl.max << "]";
     LOG("DMDISXSec", pINFO)  
         << "Q2 integration range = [" << Q2l.min << ", " << Q2l.max << "]";

     bool phsp_ok = 
          (Q2l.min >= 0. && Q2l.max >= 0. && Q2l.max >= Q2l.min &&
            Wl.min >= 0. &&  Wl.max >= 0. &&  Wl.max >=  Wl.min);

     double xsec = 0.;

     if(phsp_ok) {
       ROOT::Math::IBaseFunctionMultiDim * func = 
          new utils::gsl::d2XSec_dWdQ2_E(model, interaction);
       ROOT::Math::IntegrationMultiDim::Type ig_type = 
           utils::gsl::IntegrationNDimTypeFromString(fGSLIntgType);
           
       double abstol = 1; //We mostly care about relative tolerance.
       ROOT::Math::IntegratorMultiDim ig(*func, ig_type, abstol, fGSLRelTol, fGSLMaxEval);
       double kine_min[2] = { Wl.min, Q2l.min };
       double kine_max[2] = { Wl.max, Q2l.max };
       xsec = ig.Integral(kine_min, kine_max) * (1E-38 * units::cm2);
       delete func;
     }//phase space ok?

     LOG("DMDISXSec", pINFO)  << "XSec[DIS] (E = " << Ed << " GeV) = " << xsec;

     delete interaction;

     return xsec;
  }
  return 0;
}
//____________________________________________________________________________
void DMDISXSec::Configure(const Registry & config)
{
  Algorithm::Configure(config);
  this->LoadConfig();
}
//____________________________________________________________________________
void DMDISXSec::Configure(string config)
{
  Algorithm::Configure(config);
  this->LoadConfig();
}
//____________________________________________________________________________
void DMDISXSec::LoadConfig(void)
{
  // Get GSL integration type & relative tolerance
  GetParamDef("gsl-integration-type", fGSLIntgType, string("adaptive") ) ;
  GetParamDef( "gsl-relative-tolerance", fGSLRelTol, 1E-2 ) ;

  int max_eval, min_eval ;
  GetParamDef( "gsl-max-eval", max_eval, 500000 ) ;
  GetParamDef( "gsl-min-eval", min_eval, 10000 ) ;

  fGSLMaxEval  = (unsigned int) max_eval ;
  fGSLMinEval  = (unsigned int) min_eval ;

  // Energy range for cached splines
  GetParam( "GVLD-Emin", fVldEmin) ;
  GetParam( "GVLD-Emax", fVldEmax) ;

}
//____________________________________________________________________________
void DMDISXSec::CacheFreeNucleonXSec(
          const XSecAlgorithmI * model, const Interaction * interaction) const
{
  LOG("DMDISXSec", pWARN)  
      << "Wait while computing/caching free nucleon DIS xsections first...";

  // Create the cache branch
  Cache * cache = Cache::Instance();
  string key = this->CacheBranchName(model,interaction);
  CacheBranchFx * cache_branch =
           dynamic_cast<CacheBranchFx *> (cache->FindCacheBranch(key));
  assert(!cache_branch);
  cache_branch = new CacheBranchFx("DMDIS XSec");
  cache->AddCacheBranch(key, cache_branch);

  // Tweak interaction to be on a free nucleon target
  Target * target = interaction->InitStatePtr()->TgtPtr();
  int nucpdgc = target->HitNucPdg();
  if(pdg::IsProton(nucpdgc)) { target->SetId(kPdgTgtFreeP); }
  else                       { target->SetId(kPdgTgtFreeN); }

  // Compute threshold
  const KPhaseSpace & kps = interaction->PhaseSpace();
  double Ethr = kps.Threshold();

  // Compute the number of spline knots - use at least 10 knots per decade 
  // && at least 40 knots in the full energy range
  const double Emin       = fVldEmin/3.; 
  const double Emax       = fVldEmax*3.; 
  const int    nknots_min = (int) (10*(TMath::Log(Emax) - TMath::Log(Emin))); 
  const int    nknots     = TMath::Max(40, nknots_min); 

  // Distribute the knots in the energy range as is being done in the
  // XSecSplineList so that the energy threshold is treated correctly
  // in the spline - see comments there in.
  double * E = new double[nknots]; 
  int nkb = (Ethr>Emin) ? 5 : 0; // number of knots <  threshold
  int nka = nknots-nkb;          // number of knots >= threshold
  // knots < energy threshold
  double dEb =  (Ethr>Emin) ? (Ethr - Emin) / nkb : 0;
  for(int i=0; i<nkb; i++) {
      E[i] = Emin + i*dEb;
  }
  // knots >= energy threshold
  double E0  = TMath::Max(Ethr,Emin);
  double dEa = (TMath::Log10(Emax) - TMath::Log10(E0)) /(nka-1);
  for(int i=0; i<nka; i++) {
      E[i+nkb] = TMath::Power(10., TMath::Log10(E0) + i * dEa);
  }

  // Create the integrand
  ROOT::Math::IBaseFunctionMultiDim * func = 
     new utils::gsl::d2XSec_dWdQ2_E(model, interaction);

  // Compute the cross section at the given set of knots
  double Md = interaction->InitStatePtr()->GetProbeP4()->M();
  double Md2 = Md*Md;
  for(int ie=0; ie<nknots; ie++) {
    LOG("DMDISXSec", pDEBUG) << "Dealing with knot " << ie << " out of " << nknots;
    double Ed = E[ie];
    double pd = TMath::Max(Ed*Ed - Md2,0.);
    pd = TMath::Sqrt(pd);
    TLorentzVector p4(0,0,pd,Ed);
    interaction->InitStatePtr()->SetProbeP4(p4);
    double xsec = 0.;
    if(Ed>Ethr+kASmallNum) {
       Range1D_t Wl  = kps.WLim();
       Range1D_t Q2l = kps.Q2Lim();
       LOG("DMDISXSec", pINFO)  
            << "W integration range = [" << Wl.min << ", " << Wl.max << "]";
       LOG("DMDISXSec", pINFO)  
         << "Q2 integration range = [" << Q2l.min << ", " << Q2l.max << "]";

       bool phsp_ok = 
          (Q2l.min >= 0. && Q2l.max >= 0. && Q2l.max >= Q2l.min &&
            Wl.min >= 0. &&  Wl.max >= 0. &&  Wl.max >=  Wl.min);

       if(phsp_ok) {
         ROOT::Math::IntegrationMultiDim::Type ig_type = 
             utils::gsl::IntegrationNDimTypeFromString(fGSLIntgType);
         double abstol = 1; //We mostly care about relative tolerance.
         ROOT::Math::IntegratorMultiDim ig(*func, ig_type, abstol, fGSLRelTol, fGSLMaxEval);

         if (ig_type == ROOT::Math::IntegrationMultiDim::kADAPTIVE) {
            ROOT::Math::AdaptiveIntegratorMultiDim * cast =
              dynamic_cast<ROOT::Math::AdaptiveIntegratorMultiDim*>( ig.GetIntegrator() );
            assert(cast);
            cast->SetMinPts(fGSLMinEval);
         }
         double kine_min[2] = { Wl.min, Q2l.min };
         double kine_max[2] = { Wl.max, Q2l.max };
         xsec = ig.Integral(kine_min, kine_max) * (1E-38 * units::cm2);
       }// phase space limits ok?
    }//Ev>threshold

    LOG("DMDISXSec", pNOTICE)  
       << "Caching: XSec[DMDIS] (E = " << Ed << " GeV) = " 
       << xsec / (1E-38 * units::cm2) << " x 1E-38 cm^2";
    cache_branch->AddValues(Ed,xsec);
  }//ie

  // Create the spline
  cache_branch->CreateSpline();

  delete [] E;
  delete func;
}
//____________________________________________________________________________
string DMDISXSec::CacheBranchName(
          const XSecAlgorithmI * model, const Interaction * interaction) const
{
// Build a unique name for the cache branch

  Cache * cache = Cache::Instance();
      
  string algkey = model->Id().Key();
  string ikey   = interaction->AsString();  
  string key    = cache->CacheBranchKey(algkey, ikey);
  return key;
}
//____________________________________________________________________________

