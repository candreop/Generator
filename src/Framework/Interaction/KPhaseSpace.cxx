//____________________________________________________________________________
/*
 Copyright (c) 2003-2025, The GENIE Collaboration
 For the full text of the license visit http://copyright.genie-mc.org

 Costas Andreopoulos <c.andreopoulos \at cern.ch>
 University of Liverpool

 Changes required to implement the GENIE Boosted Dark Matter module
 were installed by Josh Berger (Univ. of Wisconsin)
*/
//____________________________________________________________________________

#include <cmath>
#include <cstdlib>

#include <TMath.h>

#include "Framework/Interaction/KPhaseSpace.h"

#include "Framework/Algorithm/AlgConfigPool.h"
#include "Framework/Conventions/Constants.h"
#include "Framework/Conventions/Controls.h"
#include "Framework/Interaction/Interaction.h"
#include "Framework/Interaction/InteractionException.h"
#include "Framework/Messenger/Messenger.h"
#include "Framework/ParticleData/PDGLibrary.h"
#include "Framework/ParticleData/PDGUtils.h"
#include "Framework/ParticleData/PDGCodes.h"
#include "Framework/Registry/Registry.h"
#include "Framework/Utils/KineUtils.h"
#include "Framework/Numerical/MathUtils.h"
#include "Framework/Interaction/SppChannel.h"

using namespace genie;
using namespace genie::utils;
using namespace genie::constants;

ClassImp(KPhaseSpace)

//____________________________________________________________________________
KPhaseSpace::KPhaseSpace(void) :
TObject(), fInteraction(NULL)
{
  this->UseInteraction(0);
}
//___________________________________________________________________________
KPhaseSpace::KPhaseSpace(const Interaction * in) :
TObject(), fInteraction(NULL)
{
  this->UseInteraction(in);
}
//___________________________________________________________________________
KPhaseSpace::~KPhaseSpace(void)
{

}
//___________________________________________________________________________
double KPhaseSpace::GetTMaxDFR()
{
  static bool tMaxLoaded = false;
  static double DFR_tMax = -1;

  if (!tMaxLoaded)
  {
    AlgConfigPool * confp = AlgConfigPool::Instance();
    const Registry * r = confp->CommonList( "Param", "Diffractive" ) ;
    double tmax = r->GetDouble("DFR-t-max");
    DFR_tMax = tmax;
    tMaxLoaded = true;
  }

  return DFR_tMax;

}
//___________________________________________________________________________
void KPhaseSpace::UseInteraction(const Interaction * in)
{
  fInteraction = in;
}
//___________________________________________________________________________
double KPhaseSpace::Threshold(void) const
{
  const ProcessInfo &  pi         = fInteraction->ProcInfo();
  const InitialState & init_state = fInteraction->InitState();
  const XclsTag &      xcls       = fInteraction->ExclTag();
  const Target &       tgt        = init_state.Tgt();

  double ml = fInteraction->FSPrimLepton()->Mass();

  if( ! pi.IsKnown() ) return 0;

  if (pi.IsSinglePion()) {
    double Mi   = tgt.HitNucP4Ptr()->M(); // initial nucleon mass
    double Mf   = (xcls.NProtons()==1) ? kProtonMass : kNeutronMass;
    int pion_pdgc = kPdgPi0;
    if ( xcls.NPiPlus() == 1 )
       pion_pdgc = kPdgPiP;
    else if ( xcls.NPiMinus() == 1 )
       pion_pdgc = kPdgPiM;
    else if ( xcls.NPi0() != 1 )
       throw genie::exceptions::InteractionException("Can't compute threshold");
    double mpi   = PDGLibrary::Instance()->Find(pion_pdgc)->Mass();
    double mi    = PDGLibrary::Instance()->Find( init_state.ProbePdg() )->Mass();
    double mf = ml;
    double mtot = Mf + mf + mpi; // total mass of FS particles
    double Ethresh = (mtot*mtot - Mi*Mi - mi*mi)/2/Mi;
    return Ethresh;
  }
  
  if (pi.IsNorm() ) return 0;
 
  if (pi.IsSingleKaon()) {
    int kaon_pdgc = xcls.StrangeHadronPdg();
    double Mi   = tgt.HitNucP4Ptr()->M(); // initial nucleon mass
    // Final nucleon can be different for K0 interaction
    double Mf = (xcls.NProtons()==1) ? kProtonMass : kNeutronMass;
    double mk   = PDGLibrary::Instance()->Find(kaon_pdgc)->Mass();
    double mtot = Mf + ml + mk; // total mass of FS particles
    double Ethresh = (mtot*mtot - Mi*Mi)/2/Mi;
    return Ethresh;
  }

  if(pi.IsCoherentElastic()) {
    return ml + 0.5*ml*ml/tgt.Mass();
  }

  if (pi.IsCoherentProduction()) {

    int tgtpdgc = tgt.Pdg(); // nuclear target PDG code (10LZZZAAAI)
    double MA   = PDGLibrary::Instance()->Find(tgtpdgc)->Mass();

    double m_other  = controls::kASmallNum ;
    // as a default the mass of hadronic system is the mass of the photon.
    // which is assumed to be a small number to avoid divergences

    if ( xcls.NPions() > 0 ) {
      m_other = pi.IsWeakCC() ? kPionMass : kPi0Mass;
    }

    double m    = ml + m_other ;
    double m2   = TMath::Power(m,2);
    double Ethr = m + 0.5*m2/MA;

    return TMath::Max(0.,Ethr);
  }

  if(pi.IsQuasiElastic()            ||
     pi.IsDarkMatterElastic()       ||
     pi.IsInverseBetaDecay()        ||
     pi.IsResonant()                ||
     pi.IsDeepInelastic()           ||
     pi.IsDarkMatterDeepInelastic() ||
     pi.IsDiffractive())
  {
    assert(tgt.HitNucIsSet());
    double Mn   = tgt.HitNucP4Ptr()->M();
    double Mn2  = TMath::Power(Mn,2);
    double Wmin = kNucleonMass + kPionMass;
    if ( pi.IsQuasiElastic() || pi.IsDarkMatterElastic() || pi.IsInverseBetaDecay() ) {
      int finalNucPDG = tgt.HitNucPdg();
      if ( pi.IsWeakCC() ) finalNucPDG = pdg::SwitchProtonNeutron( finalNucPDG );
      Wmin = PDGLibrary::Instance()->Find( finalNucPDG )->Mass();
    }
    if (pi.IsResonant()) {
        Wmin = kNucleonMass + kPhotontest;
    }

    if(xcls.IsCharmEvent()) {
       if(xcls.IsInclusiveCharm()) {
          Wmin = kNucleonMass+kLightestChmHad;
       } else {
          int cpdg = xcls.CharmHadronPdg();
          double mchm = PDGLibrary::Instance()->Find(cpdg)->Mass();
          if(pi.IsQuasiElastic() || pi.IsInverseBetaDecay()) {
            Wmin = mchm + controls::kASmallNum;
          }
          else {
            Wmin = kNeutronMass + mchm + controls::kASmallNum;
          }
       }//incl.?
    }//charm?

    double smin = TMath::Power(Wmin+ml,2.);
    double Ethr = 0.5*(smin-Mn2)/Mn;
    // threshold is different for dark matter case
    if(pi.IsDarkMatterElastic() || pi.IsDarkMatterDeepInelastic()) {
      // Correction to minimum kinematic variables
      Wmin = Mn;
      smin = TMath::Power(Wmin+ml,2);
      Ethr = TMath::Max(0.5*(smin-Mn2-ml*ml)/Mn,ml);
    }

    return TMath::Max(0.,Ethr);
  }

  if(pi.IsInverseMuDecay() || pi.IsIMDAnnihilation()) {
    double Ethr = 0.5 * (kMuonMass2-kElectronMass2)/kElectronMass;
    return TMath::Max(0.,Ethr);
  }

  if(pi.IsNuElectronElastic() || pi.IsDarkMatterElectronElastic()) {
    return 0;
  }
  if(pi.IsAMNuGamma()) {
    return 0;
  }
  if (pi.IsMEC()) {
    if (tgt.HitNucIsSet()) {
        double Mn   = tgt.HitNucP4Ptr()->M();
        double Mn2  = TMath::Power(Mn,2);
        double Wmin = fInteraction->RecoilNucleon()->Mass(); // mass of the recoil nucleon cluster
        double smin = TMath::Power(Wmin+ml,2.);
        double Ethr = 0.5*(smin-Mn2)/Mn;
        return TMath::Max(0.,Ethr);
    }
    else {
        // this was ... if (pi.IsMECTensor())
        return ml;
    }
  }
  if(pi.IsGlashowResonance()) {
    double Ethr = 0.5 * (ml*ml-kElectronMass2)/kElectronMass;
    return TMath::Max(0.,Ethr);
  }
  if(pi.IsPhotonResonance()) {
    double Mn = tgt.HitNucP4Ptr()->M();
    double Ethr = 0.5 * (ml*ml-TMath::Power(Mn,2))/Mn;
    return TMath::Max(0.,Ethr);
  }
  if(pi.IsPhotonCoherent()) {
    double ml = 0;
    if      (pdg::IsNuE  (TMath::Abs(init_state.ProbePdg()))) ml = kElectronMass;
    else if (pdg::IsNuMu (TMath::Abs(init_state.ProbePdg()))) ml = kMuonMass;
    else if (pdg::IsNuTau(TMath::Abs(init_state.ProbePdg()))) ml = kTauMass;
    double MA = init_state.Tgt().Z()*kProtonMass + init_state.Tgt().N()*kNeutronMass;
    double Ethr = 0.5 * (TMath::Power(kMw+ml,2)-TMath::Power(MA,2))/MA;
    return TMath::Max(0.,Ethr);
  }


  SLOG("KPhaseSpace", pERROR)
         << "Can't compute threshold for \n" << *fInteraction;
  throw genie::exceptions::InteractionException("Can't compute threshold");
  //exit(1);

  return 99999999;
}
//___________________________________________________________________________
Range1D_t KPhaseSpace::Limits(KineVar_t kvar) const
{
  // Compute limits for the input kinematic variable irrespective of any other
  // relevant kinematical variable
  //
  assert(fInteraction);

  switch(kvar) {
  case(kKVW)  : return this->WLim();  break;
  case(kKVQ2) : return this->Q2Lim(); break;
  case(kKVq2) : return this->q2Lim(); break;
  case(kKVx)  : return this->XLim();  break;
  case(kKVy)  : return this->YLim();  break;
  case(kKVt)  : return this->TLim();  break;
  default:
    LOG("KPhaseSpace", pERROR)
      << "Couldn't compute limits for " << KineVar::AsString(kvar);
    Range1D_t R(-1.,-1);
    return R;
  }
}
//____________________________________________________________________________
double KPhaseSpace::Minimum(KineVar_t kvar) const
{
  Range1D_t lim = this->Limits(kvar);
  return lim.min;
}
//___________________________________________________________________________
double KPhaseSpace::Maximum(KineVar_t kvar) const
{
  Range1D_t lim = this->Limits(kvar);
  return lim.max;
}
//___________________________________________________________________________
bool KPhaseSpace::IsAboveThreshold(void) const
{
  double E    = 0.;
  double Ethr = this->Threshold();

  const ProcessInfo &  pi         = fInteraction->ProcInfo();
  const InitialState & init_state = fInteraction->InitState();

  if (pi.IsCoherentElastic()    ||
      pi.IsCoherentProduction() ||
      pi.IsInverseMuDecay()     ||
      pi.IsIMDAnnihilation()    ||
      pi.IsNuElectronElastic()  ||
      pi.IsDarkMatterElectronElastic() ||
      pi.IsMEC()                ||
      pi.IsPhotonCoherent()          ||
      pi.IsPhotonResonance()          ||
      pi.IsGlashowResonance())
  {
      E = init_state.ProbeE(kRfLab);
  }

  if(pi.IsQuasiElastic()            ||
     pi.IsDarkMatterElastic()       ||
     pi.IsInverseBetaDecay()        ||
     pi.IsResonant()                ||
     pi.IsDeepInelastic()           ||
     pi.IsDarkMatterDeepInelastic() ||
     pi.IsDiffractive()             ||
     pi.IsSingleKaon()              ||
     pi.IsAMNuGamma())
  {
      E = init_state.ProbeE(kRfHitNucRest);
  }

  LOG("KPhaseSpace", pDEBUG) << "E = " << E << ", Ethr = " << Ethr;
  return (E>Ethr);
}
//___________________________________________________________________________
bool KPhaseSpace::IsAllowed(void) const
{
  const ProcessInfo & pi   = fInteraction->ProcInfo();
  const Kinematics &  kine = fInteraction->Kine();

  // ASK single kaon:
  // XSec code returns zero when kinematics are not allowed
  // Here just let kinematics always be allowed
  if(pi.IsSingleKaon()) {
    return true;
  }

  // QEL:
  //  Check the running Q2 vs the Q2 limits
  if(pi.IsQuasiElastic() || pi.IsInverseBetaDecay() || pi.IsDarkMatterElastic()) {
    Range1D_t Q2l = this->Q2Lim();
    double    Q2  = kine.Q2();
    bool in_phys = math::IsWithinLimits(Q2, Q2l);
    bool allowed = in_phys;
    return allowed;
  }

  // RES
  //   Check the running W vs the W limits
  //   & the running Q2 vs Q2 limits for the given W
  if(pi.IsResonant()) {
    Range1D_t Wl  = this->WLim();
    Range1D_t Q2l = this->Q2Lim_W();
    double    W   = kine.W();
    double    Q2  = kine.Q2();
    bool in_phys = (math::IsWithinLimits(Q2, Q2l) && math::IsWithinLimits(W, Wl));
    bool allowed = in_phys;
    return allowed;
  }

  // DIS
  if(pi.IsDeepInelastic() || pi.IsDarkMatterDeepInelastic()) {
    Range1D_t Wl  = this->WLim();
    Range1D_t Q2l = this->Q2Lim_W();
    double    W   = kine.W();
    double    Q2  = kine.Q2();
    bool in_phys = (math::IsWithinLimits(Q2, Q2l) && math::IsWithinLimits(W, Wl));
    bool allowed = in_phys;
    return allowed;
  }

  //IMD
  if(pi.IsInverseMuDecay() || pi.IsIMDAnnihilation() || pi.IsNuElectronElastic() || pi.IsDarkMatterElectronElastic()) {
    Range1D_t yl = this->YLim();
    double    y  = kine.y();
    bool in_phys = math::IsWithinLimits(y, yl);
    bool allowed = in_phys;
    return allowed;
  }

  //COH
  if (pi.IsCoherentProduction()) {
    Range1D_t xl = this->XLim();
    Range1D_t yl = this->YLim();
    double    x  = kine.x();
    double    y  = kine.y();
    bool in_phys = (math::IsWithinLimits(x, xl) && math::IsWithinLimits(y, yl));
    bool allowed = in_phys;
    return allowed;
  }

  // CEvNS
  if (pi.IsCoherentElastic()) {
    double Q2 = kine.Q2();
    bool allowed (Q2 > 0);
    return allowed;
  }

  // DFR
  if (pi.IsDiffractive()) {
    // first two checks are the same as RES & DIS
    Range1D_t Wl  = this->WLim();
    Range1D_t Q2l = this->Q2Lim_W();

    kinematics::UpdateWQ2FromXY(fInteraction);
    double    W   = kine.W();
    double    Q2  = kine.Q2();

    LOG("KPhaseSpace", pDEBUG) << " W = " << W << ", limits = [" << Wl.min << "," << Wl.max << "];";
    LOG("KPhaseSpace", pDEBUG) << " Q2 = " << Q2 << ", limits = [" << Q2l.min << "," << Q2l.max << "];";
    bool in_phys = math::IsWithinLimits(W, Wl);
    in_phys = in_phys && math::IsWithinLimits(Q2, Q2l);

    // extra check: there's a t minimum.
    // but only check if W, Q2 is reasonable
    // (otherwise get NaNs in tmin)
    if (in_phys)
    {
      double    t   = kine.t();
      Range1D_t tl  = this->TLim();
      LOG("KPhaseSpace", pDEBUG) << " t = " << t << ", limits = [" << tl.min << "," << tl.max << "];";
      in_phys = in_phys && math::IsWithinLimits(t, tl);
    }
    LOG("KPhaseSpace", pDEBUG) << " phase space point is " << ( in_phys ? "ALLOWED" : "NOT ALLOWED");


    bool allowed = in_phys;
    return allowed;
  }

  // was MECTensor
  if (pi.IsMEC()){
    Range1D_t Q2l = this->Q2Lim();
    double    Q2  = kine.Q2();
    bool in_phys = math::IsWithinLimits(Q2, Q2l);
    bool allowed = in_phys;
    return allowed;
  }

  return false;
}
//___________________________________________________________________________
Range1D_t KPhaseSpace::WLim(void) const
{
// Computes hadronic invariant mass limits.
// For QEL the range reduces to the recoil nucleon mass.
// For DIS & RES the calculation proceeds as in kinematics::InelWLim().
// It is not computed for other interactions
//
  Range1D_t Wl;
  Wl.min = -1;
  Wl.max = -1;

  const ProcessInfo & pi = fInteraction->ProcInfo();

  bool is_em = pi.IsEM();
  bool is_qel  = pi.IsQuasiElastic()  || pi.IsInverseBetaDecay() || pi.IsDarkMatterElastic();
  bool is_inel = pi.IsDeepInelastic() || pi.IsResonant() || pi.IsDiffractive();
  bool is_dmdis = pi.IsDarkMatterDeepInelastic();

  if(is_qel) {
    double MR = fInteraction->RecoilNucleon()->Mass();
    Wl.min = MR;
    Wl.max = MR;
    return Wl;
  }
  if(is_inel) {
    const InitialState & init_state = fInteraction->InitState();
    double Ev = init_state.ProbeE(kRfHitNucRest);
    double M  = init_state.Tgt().HitNucP4Ptr()->M(); //can be off m/shell
    double ml = fInteraction->FSPrimLepton()->Mass();

    Wl = is_em ? kinematics::electromagnetic::InelWLim(Ev,ml,M) : kinematics::InelWLim(Ev,M,ml);

    if(fInteraction->ExclTag().IsCharmEvent()) {
      //Wl.min = TMath::Max(Wl.min, kNeutronMass+kPionMass+kLightestChmHad);
      Wl.min = TMath::Max(Wl.min, kNeutronMass+kLightestChmHad);
    }
    else if (fInteraction->ProcInfo().IsDiffractive())
      Wl.min = TMath::Max(Wl.min, kNeutronMass+kPionMass);
    else if ( fInteraction->ProcInfo().IsDeepInelastic() ) {
      Wl.min = TMath::Max(Wl.min, kNeutronMass + kPionMass );
    }

    // sanity check
    if(Wl.min>Wl.max) {Wl.min=-1; Wl.max=-1;}

    return Wl;
  }
  if(is_dmdis) {
    const InitialState & init_state = fInteraction->InitState();
    double Ev = init_state.ProbeE(kRfHitNucRest);
    double M  = init_state.Tgt().HitNucP4Ptr()->M(); //can be off m/shell
    double ml = fInteraction->FSPrimLepton()->Mass();
    Wl = kinematics::DarkWLim(Ev,M,ml);
    if(fInteraction->ExclTag().IsCharmEvent()) {
      //Wl.min = TMath::Max(Wl.min, kNeutronMass+kPionMass+kLightestChmHad);
      Wl.min = TMath::Max(Wl.min, kNeutronMass+kLightestChmHad);
    }
    else if (fInteraction->ProcInfo().IsDiffractive())
      Wl.min = TMath::Max(Wl.min, kNeutronMass+kPionMass);

    LOG("KPhaseSpace", pDEBUG) << "Found nominal limits: " << Wl.min << ", " << Wl.max;

    // sanity check
    if(Wl.min>Wl.max) {Wl.min=-1; Wl.max=-1;}

    return Wl;
  }
  return Wl;
}
//____________________________________________________________________________
Range1D_t KPhaseSpace::Q2Lim_W(void) const
{
  // Computes momentum transfer (Q2>0) limits @ the input invariant mass
  // The calculation proceeds as in kinematics::InelQ2Lim_W().
  // For QEL, W is set to the recoil nucleon mass
  //
  // TODO: For now, choosing to handle Q2 at fixed W for coherent in the
  // same way as for the general Q2 limits... but shouldn't we just use
  // W = m_pi? - which we do in Q2Lim() anyway... seems like there are
  // cleanup opportunities here.

  Range1D_t Q2l;
  Q2l.min = -1;
  Q2l.max = -1;

  const ProcessInfo & pi = fInteraction->ProcInfo();

  bool is_em = pi.IsEM();
  bool is_qel   = pi.IsQuasiElastic()  || pi.IsInverseBetaDecay();
  bool is_inel  = pi.IsDeepInelastic() || pi.IsResonant() || pi.IsDiffractive();
  bool is_coh   = pi.IsCoherentProduction();
  bool is_dme   = pi.IsDarkMatterElastic();
  bool is_dmdis = pi.IsDarkMatterDeepInelastic();

  if(!is_qel && !is_inel && !is_coh && !is_dme && !is_dmdis) return Q2l;

  if(is_coh) {
    return Q2Lim();
  }

  const InitialState & init_state = fInteraction->InitState();
  double Ev  = init_state.ProbeE(kRfHitNucRest);
  double M   = init_state.Tgt().HitNucP4Ptr()->M(); // can be off m/shell
  double ml  = fInteraction->FSPrimLepton()->Mass();

  double W = 0;
  if(is_qel || is_dme) W = fInteraction->RecoilNucleon()->Mass();
  else       W = kinematics::W(fInteraction);

  if (pi.IsInverseBetaDecay()) {
     Q2l = kinematics::InelQ2Lim_W(Ev,M,ml,W, controls::kMinQ2Limit_VLE);
  } else if (is_dme || is_dmdis) {
    Q2l = kinematics::DarkQ2Lim_W(Ev,M,ml,W);
  } else {
     Q2l = is_em ? kinematics::electromagnetic::InelQ2Lim_W(Ev,ml,M,W) : kinematics::InelQ2Lim_W(Ev,M,ml,W);
  }

  return Q2l;
}
//____________________________________________________________________________
Range1D_t KPhaseSpace::q2Lim_W(void) const
{
// As Q2Lim_W(void) but with reversed sign (Q2 -> q2)
//
  Range1D_t Q2 = this->Q2Lim_W();
  Range1D_t q2;
  q2.min = - Q2.max;
  q2.max = - Q2.min;
  return q2;
}
//____________________________________________________________________________
Range1D_t KPhaseSpace::Q2Lim(void) const
{
  // Computes momentum transfer (Q2>0) limits irrespective of the invariant mass
  // For QEL this is identical to Q2Lim_W (since W is fixed)
  // For RES & DIS, the calculation proceeds as in kinematics::InelQ2Lim().
  //
  Range1D_t Q2l;
  Q2l.min = -1;
  Q2l.max = -1;

  const ProcessInfo & pi = fInteraction->ProcInfo();

  bool is_em    = pi.IsEM();
  bool is_qel   = pi.IsQuasiElastic()  || pi.IsInverseBetaDecay();
  bool is_inel  = pi.IsDeepInelastic() || pi.IsResonant();
  bool is_coh   = pi.IsCoherentProduction();
  bool is_cevns = pi.IsCoherentElastic();
  bool is_dme   = pi.IsDarkMatterElastic();
  bool is_dmdis = pi.IsDarkMatterDeepInelastic();

  if(!is_qel && !is_inel && !is_coh && !is_cevns && !is_dme && !is_dmdis) return Q2l;

  const InitialState & init_state = fInteraction->InitState();
  double Ev  = init_state.ProbeE(kRfHitNucRest);
  double M   = init_state.Tgt().HitNucP4Ptr()->M(); // can be off m/shell
  double ml  = fInteraction->FSPrimLepton()->Mass();

  if(is_cevns) {
     double Ev_lab  = init_state.ProbeE(kRfLab);
     Q2l = kinematics::CEvNSQ2Lim(Ev_lab);
     return Q2l;
  }

  const XclsTag & xcls = fInteraction->ExclTag();

  if(is_coh) {

    double m_other  = controls::kASmallNum ;
    // as a default the mass of hadronic system is the mass of the photon.
    // which is assumed to be a small number to avoid divergences

    if ( xcls.NPions() > 0 ) {
      bool pionIsCharged = pi.IsWeakCC();
      m_other = pionIsCharged ? kPionMass : kPi0Mass;
    }

    Q2l = kinematics::CohQ2Lim(M, m_other, ml, Ev);
    return Q2l;
  }

  // quasi-elastic
  if(is_qel) {
    double W = fInteraction->RecoilNucleon()->Mass();
    if(xcls.IsCharmEvent()) {
      int charm_pdgc = xcls.CharmHadronPdg();
      W = PDGLibrary::Instance()->Find(charm_pdgc)->Mass();
    }  else if(xcls.IsStrangeEvent()) {
      int strange_pdgc = xcls.StrangeHadronPdg();
      W = PDGLibrary::Instance()->Find(strange_pdgc)->Mass();
    }
    if (pi.IsInverseBetaDecay()) {
      Q2l = kinematics::InelQ2Lim_W(Ev,M,ml,W,controls::kMinQ2Limit_VLE);
    } else {
     Q2l = is_em ? kinematics::electromagnetic::InelQ2Lim_W(Ev,ml,M,W) : kinematics::InelQ2Lim_W(Ev,M,ml,W);
    }

    return Q2l;
  }

    // dark mattter elastic
  if(is_dme) {
    double W = fInteraction->RecoilNucleon()->Mass();
    if(xcls.IsCharmEvent()) {
      int charm_pdgc = xcls.CharmHadronPdg();
      W = PDGLibrary::Instance()->Find(charm_pdgc)->Mass();
    }  else if(xcls.IsStrangeEvent()) {
      int strange_pdgc = xcls.StrangeHadronPdg();
      W = PDGLibrary::Instance()->Find(strange_pdgc)->Mass();
    }
    if (pi.IsInverseBetaDecay()) {
      Q2l = kinematics::DarkQ2Lim_W(Ev,M,ml,W,controls::kMinQ2Limit_VLE);
    } else {
      Q2l = kinematics::DarkQ2Lim_W(Ev,M,ml,W);
    }


    return Q2l;
  }

  // was MECTensor
  // TODO: Q2maxConfig
  if (pi.IsMEC()){
    double W = fInteraction->RecoilNucleon()->Mass();
    Q2l = is_em ? kinematics::electromagnetic::InelQ2Lim_W(Ev,ml,M,W) : kinematics::InelQ2Lim_W(Ev,M,ml,W);
    double Q2maxConfig = 1.44; // need to pull from config file somehow?
    if (Q2l.max > Q2maxConfig) Q2l.max = Q2maxConfig;
    return Q2l;
  }

  if (is_dmdis) {
    Q2l = kinematics::DarkQ2Lim(Ev,M,ml);
    return Q2l;
  }

  // inelastic
  Q2l = is_em ? kinematics::electromagnetic::InelQ2Lim(Ev,ml,M) : kinematics::InelQ2Lim(Ev,M,ml);
  return Q2l;
}
//____________________________________________________________________________
Range1D_t KPhaseSpace::q2Lim(void) const
{
// As Q2Lim(void) but with reversed sign (Q2 -> q2)
//
  Range1D_t Q2 = this->Q2Lim();
  Range1D_t q2;
  q2.min = - Q2.max;
  q2.max = - Q2.min;
  return q2;
}
//____________________________________________________________________________
Range1D_t KPhaseSpace::XLim(void) const
{
  // Computes x-limits;

  Range1D_t xl;
  xl.min = -1;
  xl.max = -1;

  const ProcessInfo & pi = fInteraction->ProcInfo();
  bool is_em = pi.IsEM();

  //RES+DIS
  bool is_inel = pi.IsDeepInelastic() || pi.IsResonant();
  if(is_inel) {
    const InitialState & init_state  = fInteraction->InitState();
    double Ev  = init_state.ProbeE(kRfHitNucRest);
    double M   = init_state.Tgt().HitNucP4Ptr()->M(); // can be off m/shell
    double ml  = fInteraction->FSPrimLepton()->Mass();
    xl = is_em ? kinematics::electromagnetic::InelXLim(Ev,ml,M) : kinematics::InelXLim(Ev,M,ml);
    return xl;
  }
  //DMDIS
  bool is_dmdis = pi.IsDarkMatterDeepInelastic();
  if(is_dmdis) {
    const InitialState & init_state  = fInteraction->InitState();
    double Ev  = init_state.ProbeE(kRfHitNucRest);
    double M   = init_state.Tgt().HitNucP4Ptr()->M(); // can be off m/shell
    double ml  = fInteraction->FSPrimLepton()->Mass();
    xl = kinematics::DarkXLim(Ev,M,ml);
    return xl;
  }
  //COH
  bool is_coh = pi.IsCoherentProduction();
  if(is_coh) {
    xl = kinematics::CohXLim();
    return xl;
  }
  //QEL
  bool is_qel = pi.IsQuasiElastic() || pi.IsInverseBetaDecay() || pi.IsDarkMatterElastic();
  if(is_qel) {
    xl.min = 1;
    xl.max = 1;
    return xl;
  }
  bool is_dfr = pi.IsDiffractive();
  if(is_dfr) {
    xl.min =      controls::kASmallNum;
    xl.max = 1. - controls::kASmallNum;
    return xl;
  }

  return xl;
}
//____________________________________________________________________________
Range1D_t KPhaseSpace::YLim(void) const
{
  Range1D_t yl;
  yl.min = -1;
  yl.max = -1;

  const ProcessInfo & pi = fInteraction->ProcInfo();
  bool is_em = pi.IsEM();

  //RES+DIS
  bool is_inel = pi.IsDeepInelastic() || pi.IsResonant();
  if(is_inel) {
    const InitialState & init_state = fInteraction->InitState();
    double Ev  = init_state.ProbeE(kRfHitNucRest);
    double M   = init_state.Tgt().HitNucP4Ptr()->M(); // can be off m/shell
    double ml  = fInteraction->FSPrimLepton()->Mass();
    yl = is_em ? kinematics::electromagnetic::InelYLim(Ev,ml,M) : kinematics::InelYLim(Ev,M,ml);
    return yl;
  }
  //DMDIS
  bool is_dmdis = pi.IsDarkMatterDeepInelastic();
  if(is_dmdis) {
    const InitialState & init_state = fInteraction->InitState();
    double Ev  = init_state.ProbeE(kRfHitNucRest);
    double M   = init_state.Tgt().HitNucP4Ptr()->M(); // can be off m/shell
    double ml  = fInteraction->FSPrimLepton()->Mass();
    yl = kinematics::DarkYLim(Ev,M,ml);
    return yl;
  }
  //COH
  bool is_coh = pi.IsCoherentProduction();
  if(is_coh) {
    const InitialState & init_state = fInteraction->InitState();
    double EvL = init_state.ProbeE(kRfLab);
    double ml  = fInteraction->FSPrimLepton()->Mass();
    yl = kinematics::CohYLim(EvL,ml);
    return yl;
  }
  // IMD
  if(pi.IsInverseMuDecay() || pi.IsIMDAnnihilation() || pi.IsNuElectronElastic()) {
    const InitialState & init_state = fInteraction->InitState();
    double Ev = init_state.ProbeE(kRfLab);
    double ml = fInteraction->FSPrimLepton()->Mass();
    double me = kElectronMass;
    yl.min = controls::kASmallNum;
    yl.max = 1 - (ml*ml + me*me)/(2*me*Ev) - controls::kASmallNum;
    return yl;
  }
  // EDIT: y limits are different for massive probe
  if(pi.IsDarkMatterElectronElastic()) {
    const InitialState & init_state = fInteraction->InitState();
    double Ev = init_state.ProbeE(kRfLab);
    double ml = fInteraction->FSPrimLepton()->Mass();
    double me = kElectronMass;
    yl.min = (Ev*me*me + ml*ml*(Ev + 2.0*me)) / (Ev * (2.0*Ev*me + me*me + ml*ml)) + controls::kASmallNum;
    yl.max = 1.0 - controls::kASmallNum;
    return yl;
  }
  bool is_dfr = pi.IsDiffractive();
  if(is_dfr) {
    const InitialState & init_state = fInteraction -> InitState();
    double Ev = init_state.ProbeE(kRfHitNucRest);
    double ml = fInteraction->FSPrimLepton()->Mass();
    yl.min = kPionMass/Ev + controls::kASmallNum;
    yl.max = 1. -ml/Ev - controls::kASmallNum;
    return yl;
  }
  return yl;
}
//____________________________________________________________________________
Range1D_t KPhaseSpace::YLim_X(void) const
{
// Computes kinematical limits for y @ the input x

  Range1D_t yl;
  yl.min = -1;
  yl.max = -1;

  const ProcessInfo & pi = fInteraction->ProcInfo();
  bool is_em = pi.IsEM();

  //RES+DIS
  bool is_inel = pi.IsDeepInelastic() || pi.IsResonant();
  if(is_inel) {
    const InitialState & init_state = fInteraction->InitState();
    double Ev  = init_state.ProbeE(kRfHitNucRest);
    double M   = init_state.Tgt().HitNucP4Ptr()->M(); // can be off m/shell
    double ml  = fInteraction->FSPrimLepton()->Mass();
    double x   = fInteraction->Kine().x();
    yl = is_em ? kinematics::electromagnetic::InelYLim_X(Ev,ml,M,x) : kinematics::InelYLim_X(Ev,M,ml,x);
    return yl;
  }
  //DMDIS
  bool is_dmdis = pi.IsDarkMatterDeepInelastic();
  if(is_dmdis) {
    const InitialState & init_state = fInteraction->InitState();
    double Ev  = init_state.ProbeE(kRfHitNucRest);
    double M   = init_state.Tgt().HitNucP4Ptr()->M(); // can be off m/shell
    double ml  = fInteraction->FSPrimLepton()->Mass();
    double x   = fInteraction->Kine().x();
    yl = kinematics::DarkYLim_X(Ev,M,ml,x);
    return yl;
  }
  //COH
  bool is_coh = pi.IsCoherentProduction();
  if(is_coh) {
    const InitialState & init_state = fInteraction->InitState();
    double EvL = init_state.ProbeE(kRfLab);
    double ml  = fInteraction->FSPrimLepton()->Mass();
    yl = kinematics::CohYLim(EvL,ml);
    return yl;
  }
  return yl;
}
//____________________________________________________________________________
Range1D_t KPhaseSpace::YLim(double xsi) const
{
  // Paschos-Schalla xsi parameter for y-limits in COH
  // From PRD 80, 033005 (2009)

  Range1D_t yl;
  yl.min = -1;
  yl.max = -1;

  const ProcessInfo & pi = fInteraction->ProcInfo();

  //COH
  bool is_coh = pi.IsCoherentProduction();
  if(is_coh) {
    const InitialState & init_state = fInteraction->InitState();
    const Kinematics & kine = fInteraction->Kine();
    double Ev = init_state.ProbeE(kRfHitNucRest);
    double Q2 = kine.Q2();
    double Mn = init_state.Tgt().Mass();
    double mlep = fInteraction->FSPrimLepton()->Mass();

    double m_other  = controls::kASmallNum ;
    // as a default the mass of hadronic system is the mass of the photon.
    // which is assumed to be a small number to avoid divergences

    const XclsTag & xcls = fInteraction -> ExclTag() ;

    if ( xcls.NPions() > 0 ) {
      bool pionIsCharged = pi.IsWeakCC();
      m_other = pionIsCharged ? kPionMass : kPi0Mass;
    }

    yl = kinematics::CohYLim(Mn, m_other, mlep, Ev, Q2, xsi);
    return yl;
  } else {
    return this->YLim();
  }
}
//____________________________________________________________________________
Range1D_t KPhaseSpace::YLim_X(double xsi) const
{
  // Paschos-Schalla xsi parameter for y-limits in COH
  // From PRD 80, 033005 (2009)

  const ProcessInfo & pi = fInteraction->ProcInfo();

  //COH
  bool is_coh = pi.IsCoherentProduction();
  if(is_coh) {
    return this->YLim(xsi);
  } else {
    return this->YLim_X();
  }
}
//____________________________________________________________________________
Range1D_t KPhaseSpace::TLim(void) const
{
  // t limits for Coherent pion production from
  //   Kartavtsev, Paschos, and Gounaris, PRD 74 054007, and
  //   Paschos and Schalla, PRD 80, 03305
  // TODO: Attempt to assign t bounds for other reactions?
  Range1D_t tl;
  tl.min = -1;
  tl.max = -1;

  const InitialState & init_state = fInteraction->InitState();
  const ProcessInfo & pi = fInteraction->ProcInfo();
  const Kinematics & kine = fInteraction->Kine();
  kinematics::UpdateWQ2FromXY(fInteraction);
  double Ev = init_state.ProbeE(kRfHitNucRest);
  double Q2 = kine.Q2();
  double nu = Ev * kine.y();

  //COH
  if(pi.IsCoherentProduction()) {

    double m_other  = controls::kASmallNum ;
    // as a default the mass of hadronic system is the mass of the photon.
    // which is assumed to be a small number to avoid divergences

    const XclsTag & xcls = fInteraction -> ExclTag() ;

    if ( xcls.NPions() > 0 ) {
      bool pionIsCharged = pi.IsWeakCC();
      m_other = pionIsCharged ? kPionMass : kPi0Mass;
    }

    double m_other2 = m_other * m_other ;

    tl.min = 1.0 * (Q2 + m_other2)/(2.0 * nu) * (Q2 + m_other2)/(2.0 * nu);
    tl.max = 0.05;
    return tl;
  }
  // DFR
  else if (pi.IsDiffractive()) {

    // diffractive tmin from Nucl.Phys.B278,61 (1986), eq. 12

    bool pionIsCharged = pi.IsWeakCC();
    double mpi = pionIsCharged ? kPionMass : kPi0Mass;
    double mpi2 = mpi*mpi;

    double M = init_state.Tgt().HitNucMass();
    double M2 = M*M;
    double nuSqPlusQ2 = nu*nu + Q2;
    double nuOverM = nu / M;
    double mpiQ2term = mpi2 - Q2 - 2*nu*nu;
    double A1 = 1 + 2*nuOverM + nuOverM*nuOverM - nuSqPlusQ2/M2;
    double A2 = (1+nuOverM) * mpiQ2term + 2*nuOverM*nuSqPlusQ2;
    double A3 = mpiQ2term*mpiQ2term - 4*nuSqPlusQ2*(nu*nu - mpi2);

    tl.min = std::abs( (A2 + sqrt(A2*A2 - A1*A3)) / A1 );  // GENIE's convention is that t is positive
    bool tminIsNaN;
    // use std::isnan when C++11 is around
#if __cplusplus >= 201103L
      tminIsNaN = std::isnan(tl.min);
#else
      // this the old-fashioned way to check for NaN:
      // NaN's aren't equal to anything, including themselves
      tminIsNaN = tl.min != tl.min;
#endif
    if (tminIsNaN)
    {
      LOG("KPhaseSpace", pERROR)
        << "tmin for diffractive scattering is NaN "
        << "( Enu = " << Ev << ", Q2 = " << Q2 << ", nu = " << nu << ")";
      throw genie::exceptions::InteractionException("NaN tmin for diffractive scattering");
    }
    tl.max = this->GetTMaxDFR();

    return tl;
  }

  // RES+DIS
  // IMD
  LOG("KPhaseSpace", pWARN) << "It is not sensible to ask for t limits for events that are not coherent or diffractive.";
  return tl;
}
//____________________________________________________________________________
double KPhaseSpace::Threshold_SPP_iso(void) const
{
  const InitialState & init_state = fInteraction->InitState();
  PDGLibrary * pdglib = PDGLibrary::Instance();
  
  // imply isospin symmetry
  double mpi  = (pdglib->Find(kPdgPiP)->Mass() + pdglib->Find(kPdgPi0)->Mass() + pdglib->Find(kPdgPiM)->Mass())/3;
  double M    = (pdglib->Find(kPdgProton)->Mass() + pdglib->Find(kPdgNeutron)->Mass())/2;
  double mi   = PDGLibrary::Instance()->Find( init_state.ProbePdg() )->Mass();
  double mf   = fInteraction->FSPrimLepton()->Mass();
  double mtot = M + mf + mpi; // total mass of FS particles
  double Ethresh = (mtot*mtot - M*M - mi*mi)/2/M;
  return Ethresh;
}
//____________________________________________________________________________
Range1D_t KPhaseSpace::WLim_SPP(void) const
{
  Range1D_t Wl;
  const InitialState & init_state = fInteraction->InitState();
  SppChannel_t spp_channel  = SppChannel::FromInteraction(fInteraction);
  PDGLibrary * pdglib = PDGLibrary::Instance();
  double Mf   = pdglib->Find( SppChannel::FinStateNucleon(spp_channel) )->Mass();
  double mpi  = pdglib->Find( SppChannel::FinStatePion(spp_channel) )->Mass();
  double mf   = fInteraction->FSPrimLepton()->Mass();
  double ECM  = init_state.CMEnergy();
  // kinematic W-limits
  Wl.min = Mf + mpi;
  Wl.max = ECM - mf;
  
  if ( (Wl.max - Wl.min) < (Wl.max + Wl.min)*std::numeric_limits<double>::epsilon() )
  {
      Wl.min = 2*Wl.max*Wl.min/(Wl.max + Wl.min);
      Wl.max = Wl.min;
  }
  else
  {
      Wl.min *= 1. + std::numeric_limits<double>::epsilon();
      Wl.max *= 1. - std::numeric_limits<double>::epsilon();
  }
  
  return Wl;
}
//____________________________________________________________________________
Range1D_t KPhaseSpace::WLim_SPP_iso(void) const
{
  Range1D_t Wl;
  const InitialState & init_state = fInteraction->InitState();
  PDGLibrary * pdglib = PDGLibrary::Instance();
  // imply isospin symmetry
  double M    = (pdglib->Find(kPdgProton)->Mass() + pdglib->Find(kPdgNeutron)->Mass())/2;
  double mpi  = (pdglib->Find(kPdgPiP)->Mass() + pdglib->Find(kPdgPi0)->Mass() + pdglib->Find(kPdgPiM)->Mass())/3;
  double mi   = PDGLibrary::Instance()->Find( init_state.ProbePdg() )->Mass();
  double mf   = fInteraction->FSPrimLepton()->Mass();
  double Ei   = init_state.ProbeE(kRfHitNucRest);
  double ECM  = TMath::Sqrt(M*(M + 2*Ei) + mi*mi);
  // kinematic W-limits
  Wl.min = M + mpi;
  Wl.max = ECM - mf;
  
  if ( (Wl.max - Wl.min) < (Wl.max + Wl.min)*std::numeric_limits<double>::epsilon() )
  {
      Wl.min = 2*Wl.max*Wl.min/(Wl.max + Wl.min);
      Wl.max = Wl.min;
  }
  else
  {
      Wl.min *= 1. + std::numeric_limits<double>::epsilon();
      Wl.max *= 1. - std::numeric_limits<double>::epsilon();
  }
  
  return Wl;
}
//____________________________________________________________________________
Range1D_t KPhaseSpace::Q2Lim_W_SPP (void) const
{
  Range1D_t Q2l;
  const InitialState & init_state = fInteraction->InitState();
  SppChannel_t spp_channel  = SppChannel::FromInteraction(fInteraction);
  PDGLibrary * pdglib = PDGLibrary::Instance();
  double Mi   = pdglib->Find( SppChannel::InitStateNucleon(spp_channel) )->Mass();
  double mi   = pdglib->Find( init_state.ProbePdg() )->Mass();
  double mf   = fInteraction->FSPrimLepton()->Mass();
  double mi2  = mi*mi;
  double mf2  = mf*mf;
  double W    = kinematics::W(fInteraction);

  double ECM = init_state.CMEnergy();
  double s = ECM*ECM;

  double Ei_CM  = (s + mi2 - Mi*Mi)/2/ECM;
  double Ef_CM  = (s + mf2 - W*W)/2/ECM;
  double Pi_CM  = (Ei_CM - mi)<0?0:TMath::Sqrt(Ei_CM*Ei_CM - mi2);
  double Pf_CM  = (Ef_CM - mf)<0?0:TMath::Sqrt(Ef_CM*Ef_CM - mf2);
  // kinematic Q2-limits
  Q2l.min = 2*(Ei_CM*Ef_CM - Pi_CM*Pf_CM) - mi2 - mf2;
  Q2l.max = 2*(Ei_CM*Ef_CM + Pi_CM*Pf_CM) - mi2 - mf2;
  
  if ( (Q2l.max - Q2l.min) < (Q2l.max + Q2l.min)*std::numeric_limits<double>::epsilon() )
  {
      Q2l.min = 2*Q2l.max*Q2l.min/(Q2l.max + Q2l.min);
      Q2l.max = Q2l.min;
  }
  else
  {
      Q2l.min *= 1. + std::numeric_limits<double>::epsilon();
      Q2l.max *= 1. - std::numeric_limits<double>::epsilon();
  }

  return Q2l;
}
//____________________________________________________________________________
Range1D_t KPhaseSpace::Q2Lim_W_SPP_iso(void) const
{
  Range1D_t Q2l;
  const InitialState & init_state = fInteraction->InitState();
  PDGLibrary * pdglib = PDGLibrary::Instance();
  // imply isospin symmetry
  double M   = (pdglib->Find(kPdgProton)->Mass() + pdglib->Find(kPdgNeutron)->Mass())/2;
  double mi  = pdglib->Find( init_state.ProbePdg() )->Mass();
  double mf  = fInteraction->FSPrimLepton()->Mass();
  double mi2 = mi*mi;
  double mf2 = mf*mf;
  double W = kinematics::W(fInteraction);
  
  double Ei = init_state.ProbeE(kRfHitNucRest);
  double s = M*(M + 2*Ei) + mi2;
  double ECM = TMath::Sqrt(s);
  
  double Ei_CM  = (s + mi2 - M*M)/2/ECM;
  double Ef_CM  = (s + mf2 - W*W)/2/ECM;
  double Pi_CM  = (Ei_CM - mi)<0?0:TMath::Sqrt(Ei_CM*Ei_CM - mi2);
  double Pf_CM  = (Ef_CM - mf)<0?0:TMath::Sqrt(Ef_CM*Ef_CM - mf2);
  // kinematic Q2-limits
  Q2l.min = 2*(Ei_CM*Ef_CM - Pi_CM*Pf_CM) - mi2 - mf2;
  Q2l.max = 2*(Ei_CM*Ef_CM + Pi_CM*Pf_CM) - mi2 - mf2;
  
  if ( (Q2l.max - Q2l.min) < (Q2l.max + Q2l.min)*std::numeric_limits<double>::epsilon() )
  {
      Q2l.min = 2*Q2l.max*Q2l.min/(Q2l.max + Q2l.min);
      Q2l.max = Q2l.min;
  }
  else
  {
      Q2l.min *= 1. + std::numeric_limits<double>::epsilon();
      Q2l.max *= 1. - std::numeric_limits<double>::epsilon();
  }
  
  return Q2l;
}
//____________________________________________________________________________

