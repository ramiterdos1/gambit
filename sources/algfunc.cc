//
// FILE: algfunc.cc -- Solution algorithm functions for GCL
//
// $Id$
//

#include "gsm.h"
#include "portion.h"
#include "gsmfunc.h"

#include "rational.h"

#include "gwatch.h"
#include "mixedsol.h"
#include "behavsol.h"
#include "nfg.h"
#include "nfplayer.h"
#include "efg.h"

#include "vertenum.h"


extern Portion *ArrayToList(const gArray<int> &A);
extern Portion *ArrayToList(const gArray<double> &A);
extern Portion *ArrayToList(const gArray<gRational> &A);
extern gVector<double>* ListToVector_Float(ListPortion* list);
extern gVector<gRational>* ListToVector_Rational(ListPortion* list);
extern gMatrix<double>* ListToMatrix_Float(ListPortion* list);
extern gMatrix<gRational>* ListToMatrix_Rational(ListPortion* list);


//
// Useful utilities for creation of lists of profiles
//
class Mixed_ListPortion : public ListPortion   {
  public:
    Mixed_ListPortion(const gList<MixedSolution> &);
    virtual ~Mixed_ListPortion()   { }
};

Mixed_ListPortion::Mixed_ListPortion(const gList<MixedSolution> &list)
{
  rep->_DataType = porMIXED;
  for (int i = 1; i <= list.Length(); i++)
    Append(new MixedPortion(new MixedSolution(list[i])));
}

class Behav_ListPortion : public ListPortion   {
  public:
    Behav_ListPortion(const gList<BehavSolution> &);
    virtual ~Behav_ListPortion()   { }
};

Behav_ListPortion::Behav_ListPortion(const gList<BehavSolution> &list)
{
  rep->_DataType = porBEHAV;
  for (int i = 1; i <= list.Length(); i++)
    Append(new BehavPortion(new BehavSolution(list[i])));
}


//-------------
// AgentForm
//-------------


static Portion *GSM_AgentForm(Portion **param)
{
  Efg &E = *((EfgPortion*) param[0])->Value();
  gWatch watch;

  Nfg *N = MakeAfg(E);
  
  ((NumberPortion *) param[1])->SetValue(watch.Elapsed());

  if (N)
    return new NfgPortion(N);
  else
    throw gclRuntimeError("Conversion to agent form failed");
}

//------------
// Behav
//------------

static Portion *GSM_Behav(Portion **param)
{
  MixedSolution &mp = *((MixedPortion*) param[0])->Value();

  Nfg &N = mp.Game();
  const Efg &E = *(const Efg *) N.AssociatedEfg();

  BehavProfile<gNumber> *bp = new BehavProfile<gNumber>(EFSupport(E));
  MixedToBehav(N, MixedProfile<gNumber>(mp), E, *bp);
  
  // hack to get output profile to be all one precision
  gPrecision prec = precRATIONAL;
  for (int i = 1; i <= bp->Length(); i++) { 
    if ((*bp)[i].Precision() == precDOUBLE)
      prec = precDOUBLE;
  }

  if (prec == precDOUBLE) {
    for (int i = 1; i <= bp->Length(); i++)
      (*bp)[i] = (double) (*bp)[i];
  }
  else {
    for (int i = 1; i <= bp->Length(); i++)
      (*bp)[i] = (*bp)[i].operator gRational();
  }
    
  return new BehavPortion(new BehavSolution(*bp));
}


//------------------
// EnumMixedSolve
//------------------

#include "enum.h"

static Portion *GSM_EnumMixed_Nfg(Portion **param)
{
  NFSupport* S = ((NfSupportPortion*) param[0])->Value();

  EnumParams params;
  params.stopAfter = ((NumberPortion *) param[1])->Value();
  params.precision = ((PrecisionPortion *) param[2])->Value();
  params.tracefile = &((OutputPortion *) param[5])->Value();
  params.trace = ((NumberPortion *) param[6])->Value();
  params.cliques = ((BoolPortion *) param[7])->Value();
  
  gList<MixedSolution> solutions;

  try {
    double time;
    long npivots;
    Enum(*S, params, solutions, npivots, time);
    ((NumberPortion *) param[3])->SetValue(npivots);
    ((NumberPortion *) param[4])->SetValue(time);
  }
  catch (gSignalBreak &) {
    params.status.Reset();
  }

  return new Mixed_ListPortion(solutions);
}

#include "enumsub.h"


static Portion *GSM_EnumMixed_Efg(Portion **param)
{
  EFSupport &support = *((EfSupportPortion *) param[0])->Value();

  if (!((BoolPortion *) param[1])->Value())
    throw gclRuntimeError("algorithm not implemented for extensive forms");

  EnumParams params;
  params.stopAfter = ((NumberPortion *) param[2])->Value();
  params.precision = ((PrecisionPortion *) param[3])->Value();
  params.tracefile = &((OutputPortion *) param[6])->Value();
  params.trace = ((NumberPortion *) param[7])->Value();
  params.cliques = ((BoolPortion *) param[8])->Value();

  gList<BehavSolution> solutions;
  try {
    double time;
    long npivots;
    Enum(support, params, solutions, npivots, time);
    ((NumberPortion *) param[4])->SetValue(npivots);
    ((NumberPortion *) param[5])->SetValue(time);
  }
  catch (gSignalBreak &) {
    params.status.Reset();
  }
  return new Behav_ListPortion(solutions);
}


//-----------------
// EnumPureSolve
//-----------------

#include "nfgpure.h"

static Portion *GSM_EnumPure_Nfg(Portion **param)
{
  NFSupport* S = ((NfSupportPortion*) param[0])->Value();

  gWatch watch;

  gList<MixedSolution> solutions;

  try {
    FindPureNash(*S, ((NumberPortion *) param[1])->Value(),
		 gstatus, solutions);
    ((NumberPortion *) param[3])->SetValue(watch.Elapsed());
  }
  catch (gSignalBreak &) {
    gstatus.Reset();
  }

  return new Mixed_ListPortion(solutions);
}

#include "efgpure.h"
#include "psnesub.h"

static Portion *GSM_EnumPure_Efg(Portion **param)
{
  EFSupport &support = *((EfSupportPortion *) param[0])->Value();

  if (((BoolPortion *) param[1])->Value())   {
    gList<BehavSolution> solutions;

    try {
      double time;
      EnumPureNfg(support, 0, gstatus, solutions, time);
      ((NumberPortion *) param[4])->SetValue(time);
    }
    catch (gSignalBreak &) {
      gstatus.Reset();
    }
    return new Behav_ListPortion(solutions);
  }
  else  {
    gList<BehavSolution> solutions;

    try {
      double time;
      EnumPure(support, solutions, time);
      ((NumberPortion *) param[4])->SetValue(time);
    }
    catch (gSignalBreak &) {
    }
    return new Behav_ListPortion(solutions);
  }
}

//------------------
// GobitGridSolve
//------------------

#include "grid.h"

static Portion *GSM_GobitGrid_Support(Portion **param)
{
  NFSupport& S = * ((NfSupportPortion*) param[0])->Value();

  GridParams GP;
  
  if(((TextPortion*) param[1])->Value() != "")
    GP.pxifile = new gFileOutput(((TextPortion*) param[1])->Value());
  else
    GP.pxifile = &gnull;
  GP.minLam = ((NumberPortion *) param[2])->Value();
  GP.maxLam = ((NumberPortion *) param[3])->Value();
  GP.delLam = ((NumberPortion *) param[4])->Value();
  GP.powLam = ((NumberPortion *) param[5])->Value();
  GP.delp1 = ((NumberPortion *) param[6])->Value();
  GP.tol1 = ((NumberPortion *) param[7])->Value();

  GP.delp2 = ((NumberPortion *) param[8])->Value();
  GP.tol2 = ((NumberPortion *) param[9])->Value();

  GP.multi_grid = 0;
  if (GP.delp2 > 0.0 && GP.tol2 > 0.0)
    GP.multi_grid = 1;
  
  gList<MixedSolution> solutions;

  try {
    GridSolve(S, GP, solutions);
  }
  catch (gSignalBreak &) {
    GP.status.Reset();
  }
  catch (...) {
    if (GP.pxifile != &gnull)  delete GP.pxifile;
    throw;
  }

  if (GP.pxifile != &gnull)  delete GP.pxifile;

  return new Mixed_ListPortion(solutions);
}

//---------------
// GobitSolve
//---------------

#include "ngobit.h"
#include "egobit.h"

static Portion *GSM_Gobit_Start(Portion **param)
{
  if (param[0]->Spec().Type == porMIXED)  {
    MixedSolution &start = *((MixedPortion *) param[0])->Value();
    Nfg &N = start.Game();

    NFGobitParams NP;
    if (((TextPortion *) param[1])->Value() != "")
      NP.pxifile = new gFileOutput(((TextPortion *) param[1])->Value());
    else
      NP.pxifile = &gnull;
    NP.minLam = ((NumberPortion *) param[2])->Value();
    NP.maxLam = ((NumberPortion *) param[3])->Value();
    NP.delLam = ((NumberPortion *) param[4])->Value();
    NP.powLam = ((NumberPortion *) param[5])->Value();
    NP.fullGraph = ((BoolPortion *) param[6])->Value();

    NP.maxitsN = ((NumberPortion *) param[7])->Value();
    NP.tolN = ((NumberPortion *) param[8])->Value();
    NP.maxits1 = ((NumberPortion *) param[9])->Value();
    NP.tol1 = ((NumberPortion *) param[10])->Value();
    
    NP.tracefile = &((OutputPortion *) param[14])->Value();
    NP.trace = ((NumberPortion *) param[15])->Value();

    gList<MixedSolution> solutions;
    try {
      long nevals, niters;
      gWatch watch;
      Gobit(N, NP, MixedProfile<gNumber>(start), solutions, nevals, niters);

      ((NumberPortion *) param[11])->SetValue(watch.Elapsed());
      ((NumberPortion *) param[12])->SetValue(nevals);
      ((NumberPortion *) param[13])->SetValue(niters);
    }
    catch (gSignalBreak &) {
      NP.status.Reset();
    }
    catch (...) {
      if (NP.pxifile != &gnull)  delete NP.pxifile;
      throw;
    }

    if (NP.pxifile != &gnull)  delete NP.pxifile;
    return new Mixed_ListPortion(solutions);
  }
  else  {     // BEHAV
    BehavSolution &start = *((BehavPortion *) param[0])->Value();
    Efg &E = start.Game();
  
    EFGobitParams EP;
    if(((TextPortion*) param[1])->Value() != "")
      EP.pxifile = new gFileOutput(((TextPortion*) param[1])->Value());
    else
      EP.pxifile = &gnull;
    EP.minLam = ((NumberPortion *) param[2])->Value();
    EP.maxLam = ((NumberPortion *) param[3])->Value();
    EP.delLam = ((NumberPortion *) param[4])->Value();
    EP.powLam = ((NumberPortion *) param[5])->Value();
    EP.fullGraph = ((BoolPortion *) param[6])->Value();
    
    EP.maxitsN = ((NumberPortion *) param[7])->Value();
    EP.tolN = ((NumberPortion *) param[8])->Value();
    EP.maxits1 = ((NumberPortion *) param[9])->Value();
    EP.tol1 = ((NumberPortion *) param[10])->Value();
  
    EP.tracefile = &((OutputPortion *) param[14])->Value();
    EP.trace = ((NumberPortion *) param[15])->Value();
    
    gList<BehavSolution> solutions;
    try {
      long nevals, niters;
      gWatch watch;
    
      Gobit(E, EP, BehavProfile<gNumber>(start), solutions, nevals, niters);

      ((NumberPortion *) param[11])->SetValue(watch.Elapsed());
      ((NumberPortion *) param[12])->SetValue(nevals);
      ((NumberPortion *) param[13])->SetValue(niters);
    }
    catch (gSignalBreak &) {
      EP.status.Reset();
    }
    catch (...) {
      if (EP.pxifile != &gnull)  delete EP.pxifile;
      throw;
    }

    if (EP.pxifile != &gnull)   delete EP.pxifile;

    return new Behav_ListPortion(solutions);
  }
}


//---------------
// KGobitSolve
//---------------

static Portion *GSM_KGobit_Start(Portion **param)
{
  if (param[0]->Spec().Type == porMIXED)  {
    MixedSolution &start = *((MixedPortion *) param[0])->Value();
    Nfg &N = start.Game();

    NFGobitParams NP;
    if (((TextPortion *) param[1])->Value() != "")
      NP.pxifile = new gFileOutput(((TextPortion *) param[1])->Value());
    else
      NP.pxifile = &gnull;
    NP.minLam = ((NumberPortion *) param[2])->Value();
    NP.maxLam = ((NumberPortion *) param[3])->Value();
    NP.delLam = ((NumberPortion *) param[4])->Value();
    NP.powLam = ((NumberPortion *) param[5])->Value();
    NP.fullGraph = ((BoolPortion *) param[6])->Value();

    NP.maxitsN = ((NumberPortion *) param[7])->Value();
    NP.tolN = ((NumberPortion *) param[8])->Value();
    NP.maxits1 = ((NumberPortion *) param[9])->Value();
    NP.tol1 = ((NumberPortion *) param[10])->Value();
    
    NP.tracefile = &((OutputPortion *) param[14])->Value();
    NP.trace = ((NumberPortion *) param[15])->Value();

    gList<MixedSolution> solutions;
    try {
      long nevals, niters;
      gWatch watch;
      KGobit(N, NP, MixedProfile<gNumber>(start), solutions, nevals, niters);

      ((NumberPortion *) param[11])->SetValue(watch.Elapsed());
      ((NumberPortion *) param[12])->SetValue(nevals);
      ((NumberPortion *) param[13])->SetValue(niters);
    }
    catch (gSignalBreak &) {
      NP.status.Reset();
    }
    catch (...) {
      if (NP.pxifile != &gnull)  delete NP.pxifile;
      throw;
    }

    if (NP.pxifile != &gnull)  delete NP.pxifile;
    return new Mixed_ListPortion(solutions);
  }
  else  {     // BEHAV
    BehavSolution &start = *((BehavPortion *) param[0])->Value();
    Efg &E = start.Game();
  
    EFGobitParams EP;
    if(((TextPortion*) param[1])->Value() != "")
      EP.pxifile = new gFileOutput(((TextPortion*) param[1])->Value());
    else
      EP.pxifile = &gnull;
    EP.minLam = ((NumberPortion *) param[2])->Value();
    EP.maxLam = ((NumberPortion *) param[3])->Value();
    EP.delLam = ((NumberPortion *) param[4])->Value();
    EP.powLam = ((NumberPortion *) param[5])->Value();
    EP.fullGraph = ((BoolPortion *) param[6])->Value();
    
    EP.maxitsN = ((NumberPortion *) param[7])->Value();
    EP.tolN = ((NumberPortion *) param[8])->Value();
    EP.maxits1 = ((NumberPortion *) param[9])->Value();
    EP.tol1 = ((NumberPortion *) param[10])->Value();
  
    EP.tracefile = &((OutputPortion *) param[14])->Value();
    EP.trace = ((NumberPortion *) param[15])->Value();
    
    gList<BehavSolution> solutions;
    try {
      long nevals, niters;
      gWatch watch;
    
      KGobit(E, EP, BehavProfile<gNumber>(start), solutions, nevals, niters);

      ((NumberPortion *) param[11])->SetValue(watch.Elapsed());
      ((NumberPortion *) param[12])->SetValue(nevals);
      ((NumberPortion *) param[13])->SetValue(niters);
    }
    catch (gSignalBreak &) {
      EP.status.Reset();
    }
    catch (...) {
      if (EP.pxifile != &gnull)  delete EP.pxifile;
      throw;
    }

    if (EP.pxifile != &gnull)   delete EP.pxifile;
    return new Behav_ListPortion(solutions);
  }
}


//------------
// LcpSolve
//------------

#include "lemke.h"

static Portion *GSM_Lcp_Nfg(Portion **param)
{
  NFSupport& S = * ((NfSupportPortion*) param[0])->Value();

  LemkeParams params;
  params.stopAfter = ((NumberPortion *) param[1])->Value();
  params.precision = ((PrecisionPortion *) param[2])->Value();
  params.tracefile = &((OutputPortion *) param[5])->Value();
  params.trace = ((NumberPortion *) param[6])->Value();

  gList<MixedSolution> solutions;
  try {
    double time;
    int npivots;
    Lemke(S, params, solutions, npivots, time);
    ((NumberPortion *) param[3])->SetValue(npivots);
    ((NumberPortion *) param[4])->SetValue(time);
  }
  catch (gSignalBreak &) {
    params.status.Reset();
  }

  return new Mixed_ListPortion(solutions);
}

#include "seqform.h"
#include "lemkesub.h"

static Portion *GSM_Lcp_Efg(Portion **param)
{
  EFSupport& S = *((EfSupportPortion*) param[0])->Value();

  if (((BoolPortion *) param[1])->Value())   {
    LemkeParams params;
    
    params.stopAfter = ((NumberPortion *) param[2])->Value();
    params.precision = ((PrecisionPortion *) param[3])->Value();
    params.tracefile = &((OutputPortion *) param[6])->Value();
    params.trace = ((NumberPortion *) param[7])->Value();

    gList<BehavSolution> solutions;
    try {
      double time;
      int npivots;
      
      Lemke(S, params, solutions, npivots, time);
      ((NumberPortion *) param[4])->SetValue(npivots);
      ((NumberPortion *) param[5])->SetValue(time);
    }
    catch (gSignalBreak &) {
      params.status.Reset();
    }

    return new Behav_ListPortion(solutions);
  }
  else  {
    SeqFormParams params;

    params.stopAfter = ((NumberPortion *) param[2])->Value();
    params.precision = ((PrecisionPortion *) param[3])->Value();
    params.tracefile = &((OutputPortion *) param[6])->Value();
    params.trace = ((NumberPortion *) param[7])->Value();

    gList<BehavSolution> solutions;
    try {
      double time;
      int npivots;

      SeqForm(S, params, solutions, npivots, time);
      ((NumberPortion *) param[4])->SetValue(npivots);
      ((NumberPortion *) param[5])->SetValue(time);
    }
    catch (gSignalBreak &) {
      params.status.Reset();
    }

    return new Behav_ListPortion(solutions);
  }
}

#include "lemketab.h"

Portion* GSM_Lcp_ListNumber(Portion** param)
{
  if (((PrecisionPortion *) param[2])->Value() == precDOUBLE) {
    gMatrix<double> *a = ListToMatrix_Float((ListPortion*) param[0]);
    gVector<double> *b = ListToVector_Float((ListPortion*) param[1]);

    LTableau<double> *tab = new LTableau<double>(*a, *b);
    tab->LemkePath(0);
    gVector<double> vector;
    tab->BasisVector(vector);
    Portion *result = ArrayToList(vector);
    delete tab;
    delete a;
    delete b;
      
    return result;
  }
  else {  // precision == precRATIONAL
    gMatrix<gRational> *a = ListToMatrix_Rational((ListPortion*) param[0]);
    gVector<gRational> *b = ListToVector_Rational((ListPortion*) param[1]);
      
    LTableau<gRational> *tab = new LTableau<gRational>(*a, *b);
    tab->LemkePath(0);
    gVector<gRational> vector;
    tab->BasisVector(vector);
    Portion *result = ArrayToList(vector);
    delete tab;
    delete a;
    delete b;
      
    return result;
  }
}

//-------------
// LiapSolve
//-------------

#include "liapsub.h"
#include "eliap.h"

static Portion *GSM_Liap_Behav(Portion **param)
{
  BehavProfile<gNumber> start(*((BehavPortion *) param[0])->Value());
  Efg &E = start.Game();
  
  if (((BoolPortion *) param[1])->Value())   {
    NFLiapParams LP;

    LP.stopAfter = ((NumberPortion *) param[2])->Value();
    LP.nTries = ((NumberPortion *) param[3])->Value();

    LP.maxitsN = ((NumberPortion *) param[4])->Value();
    LP.tolN = ((NumberPortion *) param[5])->Value();
    LP.maxits1 = ((NumberPortion *) param[6])->Value();
    LP.tol1 = ((NumberPortion *) param[7])->Value();
    
    LP.tracefile = &((OutputPortion *) param[10])->Value();
    LP.trace = ((NumberPortion *) param[11])->Value();

    gWatch watch;

    gList<BehavSolution> solutions;

    try {
      NFLiapBySubgame M(E, LP, start);
      M.Solve();
      solutions = M.GetSolutions();

      ((NumberPortion *) param[8])->SetValue(watch.Elapsed());
      ((NumberPortion *) param[9])->SetValue(M.NumEvals());
    }
    catch (gSignalBreak &) {
      LP.status.Reset();
    }

    return new Behav_ListPortion(solutions);
  }
  else  {
    EFLiapParams LP;

    LP.stopAfter = ((NumberPortion *) param[2])->Value();
    LP.nTries = ((NumberPortion *) param[3])->Value();

    LP.maxitsN = ((NumberPortion *) param[4])->Value();
    LP.tolN = ((NumberPortion *) param[5])->Value();
    LP.maxits1 = ((NumberPortion *) param[6])->Value();
    LP.tol1 = ((NumberPortion *) param[7])->Value();
    
    LP.tracefile = &((OutputPortion *) param[10])->Value();
    LP.trace = ((NumberPortion *) param[11])->Value();

    gWatch watch;

    gList<BehavSolution> solutions;

    try {
      EFLiapBySubgame M(E, LP, start);
      M.Solve();
      solutions = M.GetSolutions();

      ((NumberPortion *) param[8])->SetValue(watch.Elapsed());
      ((NumberPortion *) param[9])->SetValue(M.NumEvals());
    }
    catch (gSignalBreak &) {
      LP.status.Reset();
    }

    return new Behav_ListPortion(solutions);
  }
}

#include "nliap.h"

static Portion *GSM_Liap_Mixed(Portion **param)
{
  MixedProfile<gNumber> start(*((MixedPortion *) param[0])->Value());
  Nfg &N = start.Game();

  NFLiapParams params;
  
  params.stopAfter = ((NumberPortion *) param[1])->Value();
  params.nTries = ((NumberPortion *) param[2])->Value();

  params.maxitsN = ((NumberPortion *) param[3])->Value();
  params.tolN = ((NumberPortion *) param[4])->Value();
  params.maxits1 = ((NumberPortion *) param[5])->Value();
  params.tol1 = ((NumberPortion *) param[6])->Value();
 
  params.tracefile = &((OutputPortion *) param[9])->Value();
  params.trace = ((NumberPortion *) param[10])->Value();

  gList<MixedSolution> solutions;

  try {
    long nevals, niters;
    gWatch watch;
  
    Liap(N, params, start, solutions, nevals, niters);

    ((NumberPortion *) param[7])->SetValue(watch.Elapsed());
    ((NumberPortion *) param[8])->SetValue(nevals);
  }
  catch (gSignalBreak &) {
    params.status.Reset();
  }
  
  return new Mixed_ListPortion(solutions);
}

//------------
// LpSolve
//------------

#include "nfgcsum.h"

static Portion *GSM_Lp_Nfg(Portion **param)
{
  NFSupport& S = * ((NfSupportPortion*) param[0])->Value();
  const Nfg *N = &S.Game();

  ZSumParams params;
  params.stopAfter = ((NumberPortion *) param[1])->Value();
  params.precision = ((PrecisionPortion *) param[2])->Value();
  params.tracefile = &((OutputPortion *) param[5])->Value();
  params.trace = ((NumberPortion *) param[6])->Value();

  if (N->NumPlayers() > 2 || !IsConstSum(*N))
    throw gclRuntimeError("Only valid for two-person zero-sum games");

  gList<MixedSolution> solutions;
 
  try {
    double time;
    int npivots;
    ZSum(S, params, solutions, npivots, time);
    ((NumberPortion *) param[3])->SetValue(npivots);
    ((NumberPortion *) param[4])->SetValue(time);
  }
  catch (gSignalBreak &) {
    params.status.Reset();
  }

  return new Mixed_ListPortion(solutions);
}


#include "lpsolve.h"

Portion* GSM_Lp_List(Portion** param)
{
  if (((PrecisionPortion *) param[4])->Value() == precDOUBLE)  {
    gMatrix<double>* a = ListToMatrix_Float((ListPortion*) param[0]);
    gVector<double>* b = ListToVector_Float((ListPortion*) param[1]);
    gVector<double>* c = ListToVector_Float((ListPortion*) param[2]);

    int nequals = ((NumberPortion*) param[3])->Value();
    bool isFeasible;
    bool isBounded;
  
    LPSolve<double>* s = new LPSolve<double>(*a, *b, *c, nequals);
    Portion* result = ArrayToList(s->OptimumVector());
    isFeasible = s->IsFeasible();
    isBounded = s->IsBounded();
    delete s;
    delete a;
    delete b;
    delete c;
  
    ((BoolPortion*) param[5])->SetValue((isFeasible) ? triTRUE : triFALSE);
    ((BoolPortion*) param[6])->SetValue((isBounded) ? triTRUE : triFALSE);
    return result;
  }
  else  {
    gMatrix<gRational>* a = ListToMatrix_Rational((ListPortion*) param[0]);
    gVector<gRational>* b = ListToVector_Rational((ListPortion*) param[1]);
    gVector<gRational>* c = ListToVector_Rational((ListPortion*) param[2]);

    int nequals = ((NumberPortion*) param[3])->Value();
    bool isFeasible;
    bool isBounded;
  
    LPSolve<gRational>* s = new LPSolve<gRational>(*a, *b, *c, nequals);
    Portion* result = ArrayToList(s->OptimumVector());
    isFeasible = s->IsFeasible();
    isBounded = s->IsBounded();
    delete s;
    delete a;
    delete b;
    delete c;
  
    ((BoolPortion *) param[5])->SetValue((isFeasible) ? triTRUE : triFALSE);
    ((BoolPortion *) param[6])->SetValue((isBounded) ? triTRUE : triFALSE);
    return result;
  }
}


#include "csumsub.h"
#include "efgcsum.h"

static Portion *GSM_Lp_Efg(Portion **param)
{
  EFSupport &support = *((EfSupportPortion *) param[0])->Value();
  const Efg &E = support.Game();
  
  if (E.NumPlayers() > 2 || !E.IsConstSum())
    throw gclRuntimeError("Only valid for two-person zero-sum games");

  if (((BoolPortion *) param[1])->Value())   {
    ZSumParams params;
    params.stopAfter = ((NumberPortion *) param[2])->Value();
    params.precision = ((PrecisionPortion *) param[3])->Value();
    params.tracefile = &((OutputPortion *) param[6])->Value();
    params.trace = ((NumberPortion *) param[7])->Value();

    gList<BehavSolution> solutions;
    try {
      double time;
      int npivots;
      ZSum(support, params, solutions, npivots, time);
      ((NumberPortion *) param[4])->SetValue(npivots);
      ((NumberPortion *) param[5])->SetValue(time);
    }
    catch (gSignalBreak &) {
      params.status.Reset();
    }

    return new Behav_ListPortion(solutions);
  }
  else  {
    CSSeqFormParams params;
    params.stopAfter = ((NumberPortion *) param[2])->Value();
    params.precision = ((PrecisionPortion *) param[3])->Value();
    params.tracefile = &((OutputPortion *) param[6])->Value();
    params.trace = ((NumberPortion *) param[7])->Value();

    gList<BehavSolution> solutions;

    try {
      double time;
      int npivots;
      CSSeqForm(support, params, solutions, npivots, time);
      ((NumberPortion *) param[4])->SetValue(npivots);
      ((NumberPortion *) param[5])->SetValue(time);
    }
    catch (gSignalBreak &) {
      params.status.Reset();
    }

    return new Behav_ListPortion(solutions);
  }
}

#ifndef MINI_POLY

//------------------
// PolEnumSolve
//------------------

#include "polenum.h"

static Portion *GSM_PolEnum_Nfg(Portion **param)
{
  NFSupport* S = ((NfSupportPortion*) param[0])->Value();

  PolEnumParams params;
  params.stopAfter = ((NumberPortion *) param[1])->Value();
  params.precision = ((PrecisionPortion *) param[2])->Value();
  params.tracefile = &((OutputPortion *) param[5])->Value();
  params.trace = ((NumberPortion *) param[6])->Value();
  
  gList<MixedSolution> solutions;

  try {
    long nevals;
    double time;
    PolEnum(*S, params, solutions, nevals, time);

    ((NumberPortion *) param[3])->SetValue(nevals);
    ((NumberPortion *) param[4])->SetValue(time);
  }
  catch (gSignalBreak &) {
    params.status.Reset();
  }

  return new Mixed_ListPortion(solutions);
}

#include "epolenum.h"
#include "polensub.h"

static Portion *GSM_PolEnum_Efg(Portion **param)
{
  EFSupport &support = *((EfSupportPortion *) param[0])->Value();
  
  double time;
  gList<BehavSolution> solutions;
  
  if ( ((BoolPortion *) param[1])->Value() ) {
    PolEnumParams params;
    params.stopAfter = ((NumberPortion *) param[2])->Value();
    params.precision = ((PrecisionPortion *) param[3])->Value();
    params.tracefile = &((OutputPortion *) param[6])->Value();
    params.trace = ((NumberPortion *) param[7])->Value();

    try {
      long npivots;
      PolEnum(support, params, solutions, npivots, time);
      ((NumberPortion *) param[4])->SetValue(npivots);
    }
    catch (gSignalBreak &) {
      params.status.Reset();
    }
  }
  else {
    EfgPolEnumParams params;
    params.stopAfter = ((NumberPortion *) param[2])->Value();
    params.precision = ((PrecisionPortion *) param[3])->Value();
    params.tracefile = &((OutputPortion *) param[6])->Value();
    params.trace = ((NumberPortion *) param[7])->Value();

    try {
      long npivots;
      EfgPolEnum(support, params, solutions, npivots, time);
      ((NumberPortion *) param[4])->SetValue(npivots);
    }
    catch (gSignalBreak &) {
      params.status.Reset();
    }
  }

  ((NumberPortion *) param[5])->SetValue(time);

  return new Behav_ListPortion(solutions);
}

#include "seqeq.h"

static Portion *GSM_SequentialEquilib(Portion **param)
{
  EFBasis &basis = *((EfBasisPortion *) param[0])->Value();
  EFSupport &support = *((EfSupportPortion *) param[1])->Value();
  
  double time;
  gList<BehavSolution> solutions;
  
  SeqEquilibParams params;
  params.stopAfter = ((NumberPortion *) param[2])->Value();
  params.precision = ((PrecisionPortion *) param[3])->Value();
  params.tracefile = &((OutputPortion *) param[6])->Value();
  params.trace = ((NumberPortion *) param[7])->Value();

  try {
    long nevals;
    SequentialEquilib(basis, support, params, solutions, nevals, time);

    ((NumberPortion *) param[4])->SetValue(nevals);
    ((NumberPortion *) param[5])->SetValue(time);
  }
  catch (gSignalBreak &) {
    params.status.Reset();
  }

  return new Behav_ListPortion(solutions);
}

#endif // ! MINI_POLY

//---------
// Nfg
//---------

static Portion *GSM_Nfg(Portion **param)
{
  Efg &E = * ((EfgPortion*) param[0])->Value();
  gWatch watch;

  Nfg *N = MakeReducedNfg(EFSupport(E));

  ((NumberPortion *) param[1])->SetValue(watch.Elapsed());

  if (N)
    return new NfgPortion(N);
  else
    throw gclRuntimeError("Conversion to reduced nfg failed");
}



//----------
// Payoff
//----------

Portion* GSM_Payoff_Behav(Portion** param)
{
  BehavSolution *bp = ((BehavPortion *) param[0])->Value();
  EFPlayer *player = ((EfPlayerPortion *) param[1])->Value();

  return new NumberPortion(bp->Payoff(player->GetNumber()));
}

Portion* GSM_Payoff_Mixed(Portion** param)
{
  MixedProfile<gNumber> mp(*((MixedPortion*) param[0])->Value());
  NFPlayer *player = ((NfPlayerPortion *) param[1])->Value();

  return new NumberPortion(mp.Payoff(player->GetNumber()));
}

//----------------
// SimpDivSolve
//----------------

#include "simpdiv.h"

static Portion *GSM_Simpdiv_Nfg(Portion **param)
{
  NFSupport& S = * ((NfSupportPortion*) param[0])->Value();
  SimpdivParams SP;
  SP.stopAfter = ((NumberPortion *) param[1])->Value();
  SP.nRestarts = ((NumberPortion *) param[2])->Value();
  SP.leashLength = ((NumberPortion *) param[3])->Value();

  SP.tracefile = &((OutputPortion *) param[7])->Value();
  SP.trace = ((NumberPortion *) param[8])->Value();

  if (((PrecisionPortion *) param[4])->Value() == precDOUBLE)  {
    gList<MixedSolution> solutions;

    try {
      SimpdivModule<double> SM(S, SP);
      SM.Simpdiv();
      solutions = SM.GetSolutions();
      ((NumberPortion *) param[5])->SetValue(SM.NumEvals());
      ((NumberPortion *) param[6])->SetValue(SM.Time());
    }
    catch (gSignalBreak &) {
      SP.status.Reset();
    }
    
    return new Mixed_ListPortion(solutions);
  }
  else  {
    gList<MixedSolution> solutions;

    try {
      SimpdivModule<gRational> SM(S, SP);
      SM.Simpdiv();
      solutions = SM.GetSolutions();

      ((NumberPortion *) param[5])->SetValue(SM.NumEvals());
      ((NumberPortion *) param[6])->SetValue(SM.Time());
    }
    catch (gSignalBreak &) {
      SP.status.Reset();
    }
     
    return new Mixed_ListPortion(solutions);
  }
}

#include "simpsub.h"

static Portion *GSM_Simpdiv_Efg(Portion **param)
{
  EFSupport &support = *((EfSupportPortion *) param[0])->Value();

  if (!((BoolPortion *) param[1])->Value())
    throw gclRuntimeError("algorithm not implemented for extensive forms");

  SimpdivParams params;
  params.stopAfter = ((NumberPortion *) param[2])->Value();
  params.nRestarts = ((NumberPortion *) param[3])->Value();
  params.leashLength = ((NumberPortion *) param[4])->Value();
  params.precision = ((PrecisionPortion *) param[5])->Value();
  params.tracefile = &((OutputPortion *) param[8])->Value();
  params.trace = ((NumberPortion *) param[9])->Value();

  gList<BehavSolution> solutions;
  try {
    int nevals, niters;
    double time;
    Simpdiv(support, params, solutions, nevals, niters, time);

    ((NumberPortion *) param[6])->SetValue(nevals);
    ((NumberPortion *) param[7])->SetValue(time);
  }
  catch (gSignalBreak &) { 
    params.status.Reset();
  }

  return new Behav_ListPortion(solutions);
}

static Portion *GSM_VertEnum(Portion** param)
{
  if (((PrecisionPortion *) param[2])->Value() == precDOUBLE)  {
    gMatrix<double>* A = ListToMatrix_Float((ListPortion*) param[0]);
    gVector<double>* b = ListToVector_Float((ListPortion*) param[1]);
    
    gList< gVector< double > > verts;
    
    VertEnum< double >* vertenum = new VertEnum< double >( *A, *b );
    vertenum->Vertices( verts );
    delete vertenum;

    delete A;
    delete b;
    
    ListPortion* list = new ListPortion();
    for (int i = 1; i <= verts.Length(); i++)  
      list->Append( ArrayToList( verts[i] ) );
    
    return list;
  }
  else {
    gMatrix<gRational>* A = ListToMatrix_Rational((ListPortion*) param[0]);
    gVector<gRational>* b = ListToVector_Rational((ListPortion*) param[1]);
    
    gList< gVector< gRational > > verts;
    
    VertEnum< gRational >* vertenum = new VertEnum< gRational >( *A, *b );
    vertenum->Vertices( verts );
    delete vertenum;

    delete A;
    delete b;
    
    ListPortion* list = new ListPortion();
    for (int i = 1; i <= verts.Length(); i++)  
      list->Append( ArrayToList( verts[i] ) );
    
    return list;
  }
}

void Init_algfunc(GSM *gsm)
{
  FuncDescObj *FuncObj;

  FuncObj = new FuncDescObj("AgentForm", 1);
  FuncObj->SetFuncInfo(0, gclSignature(GSM_AgentForm, porNFG, 2));
  FuncObj->SetParamInfo(0, 0, gclParameter("efg", porEFG));
  FuncObj->SetParamInfo(0, 1, gclParameter("time", porNUMBER,
					    new NumberPortion(0), BYREF));
  gsm->AddFunction(FuncObj);



  FuncObj = new FuncDescObj("Behav", 1);
  FuncObj->SetFuncInfo(0, gclSignature(GSM_Behav, porBEHAV, 1));
  FuncObj->SetParamInfo(0, 0, gclParameter("mixed", porMIXED));
  gsm->AddFunction(FuncObj);



  FuncObj = new FuncDescObj("EnumMixedSolve", 2);
  FuncObj->SetFuncInfo(0, gclSignature(GSM_EnumMixed_Nfg, 
				       PortionSpec(porMIXED, 1), 8));
  FuncObj->SetParamInfo(0, 0, gclParameter("support", porNFSUPPORT));
  FuncObj->SetParamInfo(0, 1, gclParameter("stopAfter", porINTEGER,
					    new NumberPortion(0)));
  FuncObj->SetParamInfo(0, 2, gclParameter("precision", porPRECISION,
              new PrecisionPortion(precDOUBLE)));
  FuncObj->SetParamInfo(0, 3, gclParameter("nPivots", porINTEGER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(0, 4, gclParameter("time", porNUMBER,
					    new NumberPortion(0.0), BYREF));
  FuncObj->SetParamInfo(0, 5, gclParameter("traceFile", porOUTPUT,
					    new OutputPortion(gnull), 
					    BYREF));
  FuncObj->SetParamInfo(0, 6, gclParameter("traceLevel", porNUMBER,
					    new NumberPortion(0)));
  FuncObj->SetParamInfo(0, 7, gclParameter("cliques", porBOOLEAN,
					    new BoolPortion(false)));

  FuncObj->SetFuncInfo(1, gclSignature(GSM_EnumMixed_Efg, 
				       PortionSpec(porBEHAV, 1), 9));
  FuncObj->SetParamInfo(1, 0, gclParameter("support", porEFSUPPORT));
  FuncObj->SetParamInfo(1, 1, gclParameter("asNfg", porBOOLEAN,
					    new BoolPortion(false)));
  FuncObj->SetParamInfo(1, 2, gclParameter("stopAfter", porINTEGER,
					    new NumberPortion(0)));
  FuncObj->SetParamInfo(1, 3, gclParameter("precision", porPRECISION,
              new PrecisionPortion(precDOUBLE)));
  FuncObj->SetParamInfo(1, 4, gclParameter("nPivots", porINTEGER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(1, 5, gclParameter("time", porNUMBER,
					    new NumberPortion(0.0), BYREF));
  FuncObj->SetParamInfo(1, 6, gclParameter("traceFile", porOUTPUT,
					    new OutputPortion(gnull), 
					    BYREF));
  FuncObj->SetParamInfo(1, 7, gclParameter("traceLevel", porNUMBER,
					    new NumberPortion(0)));
  FuncObj->SetParamInfo(1, 8, gclParameter("cliques", porBOOLEAN,
					    new BoolPortion(false)));

  gsm->AddFunction(FuncObj);


  FuncObj = new FuncDescObj("EnumPureSolve", 2);
  FuncObj->SetFuncInfo(0, gclSignature(GSM_EnumPure_Nfg, 
				       PortionSpec(porMIXED, 1), 5));
  FuncObj->SetParamInfo(0, 0, gclParameter("support", porNFSUPPORT));
  FuncObj->SetParamInfo(0, 1, gclParameter("stopAfter", porINTEGER,
					    new NumberPortion(0)));
  FuncObj->SetParamInfo(0, 2, gclParameter("time", porNUMBER,
					    new NumberPortion(0.0), BYREF));
  FuncObj->SetParamInfo(0, 3, gclParameter("traceFile", porOUTPUT,
					    new OutputPortion(gnull), 
					    BYREF));
  FuncObj->SetParamInfo(0, 4, gclParameter("traceLevel", porNUMBER,
					    new NumberPortion(0)));

  FuncObj->SetFuncInfo(1, gclSignature(GSM_EnumPure_Efg, 
				       PortionSpec(porBEHAV, 1), 7));
  FuncObj->SetParamInfo(1, 0, gclParameter("support", porEFSUPPORT));
  FuncObj->SetParamInfo(1, 1, gclParameter("asNfg", porBOOLEAN,
					    new BoolPortion(false)));
  FuncObj->SetParamInfo(1, 2, gclParameter("stopAfter", porINTEGER,
					    new NumberPortion(0)));
  FuncObj->SetParamInfo(1, 3, gclParameter("precision", porPRECISION,
              new PrecisionPortion(precDOUBLE)));
  FuncObj->SetParamInfo(1, 4, gclParameter("time", porNUMBER,
					    new NumberPortion(0.0), BYREF));
  FuncObj->SetParamInfo(1, 5, gclParameter("traceFile", porOUTPUT,
					    new OutputPortion(gnull), 
					    BYREF));
  FuncObj->SetParamInfo(1, 6, gclParameter("traceLevel", porNUMBER,
					    new NumberPortion(0)));
  gsm->AddFunction(FuncObj);


  FuncObj = new FuncDescObj("GobitGridSolve", 1);
  FuncObj->SetFuncInfo(0, gclSignature(GSM_GobitGrid_Support, 
				       PortionSpec(porMIXED, 1), 14));
  FuncObj->SetParamInfo(0, 0, gclParameter("support", porNFSUPPORT));
  FuncObj->SetParamInfo(0, 1, gclParameter("pxifile", porTEXT,
					    new TextPortion("")));
  FuncObj->SetParamInfo(0, 2, gclParameter("minLam", porNUMBER,
					    new NumberPortion(0.001)));
  FuncObj->SetParamInfo(0, 3, gclParameter("maxLam", porNUMBER,
					    new NumberPortion(500.0)));
  FuncObj->SetParamInfo(0, 4, gclParameter("delLam", porNUMBER,
					    new NumberPortion(0.02)));
  FuncObj->SetParamInfo(0, 5, gclParameter("powLam", porINTEGER,
					    new NumberPortion(1)));
  FuncObj->SetParamInfo(0, 6, gclParameter("delp1", porNUMBER,
					    new NumberPortion(.1)));
  FuncObj->SetParamInfo(0, 7, gclParameter("tol1", porNUMBER,
					    new NumberPortion(.1)));
  FuncObj->SetParamInfo(0, 8, gclParameter("delp2", porNUMBER,
					    new NumberPortion(.01)));
  FuncObj->SetParamInfo(0, 9, gclParameter("tol2", porNUMBER,
					    new NumberPortion(.01)));
  FuncObj->SetParamInfo(0, 10, gclParameter("nEvals", porNUMBER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(0, 11, gclParameter("time", porNUMBER,
					    new NumberPortion(0.0), BYREF));
  FuncObj->SetParamInfo(0, 12, gclParameter("traceFile", porOUTPUT,
					     new OutputPortion(gnull),
					     BYREF));
  FuncObj->SetParamInfo(0, 13, gclParameter("traceLevel", porNUMBER,
					     new NumberPortion(0)));

  gsm->AddFunction(FuncObj);


  FuncObj = new FuncDescObj("GobitSolve", 1);
  FuncObj->SetFuncInfo(0, gclSignature(GSM_Gobit_Start, 
				       PortionSpec(porMIXED | porBEHAV, 1), 16));
  FuncObj->SetParamInfo(0, 0, gclParameter("start",
					    porMIXED | porBEHAV));
  FuncObj->SetParamInfo(0, 1, gclParameter("pxifile", porTEXT,
					    new TextPortion("")));
  FuncObj->SetParamInfo(0, 2, gclParameter("minLam", porNUMBER,
					    new NumberPortion(0.001)));
  FuncObj->SetParamInfo(0, 3, gclParameter("maxLam", porNUMBER,
					    new NumberPortion(500.0)));
  FuncObj->SetParamInfo(0, 4, gclParameter("delLam", porNUMBER,
					    new NumberPortion(0.02)));
  FuncObj->SetParamInfo(0, 5, gclParameter("powLam", porINTEGER,
					    new NumberPortion(1)));
  FuncObj->SetParamInfo(0, 6, gclParameter("fullGraph", porBOOLEAN,
					    new BoolPortion(false)));
  FuncObj->SetParamInfo(0, 7, gclParameter("maxitsN", porINTEGER,
					    new NumberPortion(20)));
  FuncObj->SetParamInfo(0, 8, gclParameter("tolN", porNUMBER,
					    new NumberPortion(1.0e-10)));
  FuncObj->SetParamInfo(0, 9, gclParameter("maxits1", porINTEGER,
					    new NumberPortion(100)));
  FuncObj->SetParamInfo(0, 10, gclParameter("tol1", porNUMBER,
					     new NumberPortion(2.0e-10)));
  FuncObj->SetParamInfo(0, 11, gclParameter("time", porNUMBER,
					     new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(0, 12, gclParameter("nEvals", porINTEGER,
					     new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(0, 13, gclParameter("nIters", porINTEGER,
					     new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(0, 14, gclParameter("traceFile", porOUTPUT,
					     new OutputPortion(gnull), 
					     BYREF));
  FuncObj->SetParamInfo(0, 15, gclParameter("traceLevel", porNUMBER,
					     new NumberPortion(0)));

  gsm->AddFunction(FuncObj);


  FuncObj = new FuncDescObj("KGobitSolve", 1);
  FuncObj->SetFuncInfo(0, gclSignature(GSM_KGobit_Start, 
				       PortionSpec(porMIXED | porBEHAV , 1), 16));
  FuncObj->SetParamInfo(0, 0, gclParameter("start",
					    porMIXED | porBEHAV));
  FuncObj->SetParamInfo(0, 1, gclParameter("pxifile", porTEXT,
					    new TextPortion("")));
  FuncObj->SetParamInfo(0, 2, gclParameter("minK", porNUMBER,
					    new NumberPortion(0.001)));
  FuncObj->SetParamInfo(0, 3, gclParameter("maxK", porNUMBER,
					    new NumberPortion(500.0)));
  FuncObj->SetParamInfo(0, 4, gclParameter("delK", porNUMBER,
					    new NumberPortion(-0.1)));
  FuncObj->SetParamInfo(0, 5, gclParameter("powK", porINTEGER,
					    new NumberPortion(1)));
  FuncObj->SetParamInfo(0, 6, gclParameter("fullGraph", porBOOLEAN,
					    new BoolPortion(false)));
  FuncObj->SetParamInfo(0, 7, gclParameter("maxitsN", porINTEGER,
					    new NumberPortion(20)));
  FuncObj->SetParamInfo(0, 8, gclParameter("tolN", porNUMBER,
					    new NumberPortion(1.0e-10)));
  FuncObj->SetParamInfo(0, 9, gclParameter("maxits1", porINTEGER,
					    new NumberPortion(100)));
  FuncObj->SetParamInfo(0, 10, gclParameter("tol1", porNUMBER,
					     new NumberPortion(2.0e-10)));
  FuncObj->SetParamInfo(0, 11, gclParameter("time", porNUMBER,
					     new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(0, 12, gclParameter("nEvals", porINTEGER,
					     new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(0, 13, gclParameter("nIters", porINTEGER,
					     new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(0, 14, gclParameter("traceFile", porOUTPUT,
					     new OutputPortion(gnull), 
					     BYREF));
  FuncObj->SetParamInfo(0, 15, gclParameter("traceLevel", porNUMBER,
					     new NumberPortion(0)));

  gsm->AddFunction(FuncObj);


  FuncObj = new FuncDescObj("LcpSolve", 3);
  FuncObj->SetFuncInfo(0, gclSignature(GSM_Lcp_Nfg, 
				       PortionSpec(porMIXED, 1), 7));
  FuncObj->SetParamInfo(0, 0, gclParameter("support", porNFSUPPORT));
  FuncObj->SetParamInfo(0, 1, gclParameter("stopAfter", porINTEGER,
					    new NumberPortion(0)));
  FuncObj->SetParamInfo(0, 2, gclParameter("precision", porPRECISION,
              new PrecisionPortion(precDOUBLE)));
  FuncObj->SetParamInfo(0, 3, gclParameter("nPivots", porINTEGER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(0, 4, gclParameter("time", porNUMBER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(0, 5, gclParameter("traceFile", porOUTPUT,
					    new OutputPortion(gnull),
					    BYREF));
  FuncObj->SetParamInfo(0, 6, gclParameter("traceLevel", porNUMBER,
					    new NumberPortion(0)));

  FuncObj->SetFuncInfo(1, gclSignature(GSM_Lcp_Efg, 
				       PortionSpec(porBEHAV, 1), 8));
  FuncObj->SetParamInfo(1, 0, gclParameter("support", porEFSUPPORT));
  FuncObj->SetParamInfo(1, 1, gclParameter("asNfg", porBOOLEAN,
					    new BoolPortion(false)));
  FuncObj->SetParamInfo(1, 2, gclParameter("stopAfter", porINTEGER,
					    new NumberPortion(0)));
  FuncObj->SetParamInfo(1, 3, gclParameter("precision", porPRECISION,
              new PrecisionPortion(precDOUBLE)));
  FuncObj->SetParamInfo(1, 4, gclParameter("nPivots", porINTEGER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(1, 5, gclParameter("time", porNUMBER,
					    new NumberPortion(0.0), BYREF));
  FuncObj->SetParamInfo(1, 6, gclParameter("traceFile", porOUTPUT,
					    new OutputPortion(gnull), 
					    BYREF));
  FuncObj->SetParamInfo(1, 7, gclParameter("traceLevel", porNUMBER,
					    new NumberPortion(0)));
  
  FuncObj->SetFuncInfo(2, gclSignature(GSM_Lcp_ListNumber, 
				       PortionSpec(porNUMBER, 1), 3));
  FuncObj->SetParamInfo(2, 0, gclParameter("a", PortionSpec(porNUMBER, 2),
					    REQUIRED, BYVAL));
  FuncObj->SetParamInfo(2, 1, gclParameter("b", PortionSpec(porNUMBER, 1),
					    REQUIRED, BYVAL));
  FuncObj->SetParamInfo(2, 2, gclParameter("precision", porPRECISION,
              new PrecisionPortion(precDOUBLE)));

  gsm->AddFunction(FuncObj);


  FuncObj = new FuncDescObj("LiapSolve", 2);
  FuncObj->SetFuncInfo(0, gclSignature(GSM_Liap_Behav,
				       PortionSpec(porBEHAV, 1), 12));
  FuncObj->SetParamInfo(0, 0, gclParameter("start", porBEHAV));
  FuncObj->SetParamInfo(0, 1, gclParameter("asNfg", porBOOLEAN,
					    new BoolPortion(false)));
  FuncObj->SetParamInfo(0, 2, gclParameter("stopAfter", porINTEGER,
					    new NumberPortion(1)));
  FuncObj->SetParamInfo(0, 3, gclParameter("nTries", porINTEGER,
					    new NumberPortion(10)));
  FuncObj->SetParamInfo(0, 4, gclParameter("maxitsN", porINTEGER,
					    new NumberPortion(20)));
  FuncObj->SetParamInfo(0, 5, gclParameter("tolN", porNUMBER,
					    new NumberPortion(1.0e-10)));
  FuncObj->SetParamInfo(0, 6, gclParameter("maxits1", porINTEGER,
					    new NumberPortion(100)));
  FuncObj->SetParamInfo(0, 7, gclParameter("tol1", porNUMBER,
					    new NumberPortion(2.0e-10)));
  FuncObj->SetParamInfo(0, 8, gclParameter("time", porNUMBER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(0, 9, gclParameter("nEvals", porINTEGER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(0, 10, gclParameter("traceFile", porOUTPUT,
					     new OutputPortion(gnull), 
					     BYREF));
  FuncObj->SetParamInfo(0, 11, gclParameter("traceLevel", porNUMBER,
					     new NumberPortion(0)));

  FuncObj->SetFuncInfo(1, gclSignature(GSM_Liap_Mixed,
				       PortionSpec(porMIXED, 1), 11));
  FuncObj->SetParamInfo(1, 0, gclParameter("start", porMIXED));
  FuncObj->SetParamInfo(1, 1, gclParameter("stopAfter", porINTEGER,
					    new NumberPortion(1)));
  FuncObj->SetParamInfo(1, 2, gclParameter("nTries", porINTEGER,
					    new NumberPortion(10)));
  FuncObj->SetParamInfo(1, 3, gclParameter("maxitsN", porINTEGER,
					    new NumberPortion(20)));
  FuncObj->SetParamInfo(1, 4, gclParameter("tolN", porNUMBER,
					    new NumberPortion(1.0e-10)));
  FuncObj->SetParamInfo(1, 5, gclParameter("maxits1", porINTEGER,
					    new NumberPortion(100)));
  FuncObj->SetParamInfo(1, 6, gclParameter("tol1", porNUMBER,
					    new NumberPortion(2.0e-10)));
  FuncObj->SetParamInfo(1, 7, gclParameter("time", porNUMBER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(1, 8, gclParameter("nEvals", porINTEGER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(1, 9, gclParameter("traceFile", porOUTPUT,
					    new OutputPortion(gnull), 
					    BYREF));
  FuncObj->SetParamInfo(1, 10, gclParameter("traceLevel", porNUMBER,
					     new NumberPortion(0)));

  gsm->AddFunction(FuncObj);


  FuncObj = new FuncDescObj("LpSolve", 3);
  FuncObj->SetFuncInfo(0, gclSignature(GSM_Lp_Nfg, 
				       PortionSpec(porMIXED, 1), 7));
  FuncObj->SetParamInfo(0, 0, gclParameter("support", porNFSUPPORT));
  FuncObj->SetParamInfo(0, 1, gclParameter("stopAfter", porINTEGER,
					    new NumberPortion(1)));
  FuncObj->SetParamInfo(0, 2, gclParameter("precision", porPRECISION,
              new PrecisionPortion(precDOUBLE)));
  FuncObj->SetParamInfo(0, 3, gclParameter("nPivots", porINTEGER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(0, 4, gclParameter("time", porNUMBER,
					    new NumberPortion(0.0), BYREF));
  FuncObj->SetParamInfo(0, 5, gclParameter("traceFile", porOUTPUT,
					    new OutputPortion(gnull),
					    BYREF));
  FuncObj->SetParamInfo(0, 6, gclParameter("traceLevel", porNUMBER,
					    new NumberPortion(0)));

  FuncObj->SetFuncInfo(1, gclSignature(GSM_Lp_Efg, 
				       PortionSpec(porBEHAV, 1), 8));
  FuncObj->SetParamInfo(1, 0, gclParameter("support", porEFSUPPORT));
  FuncObj->SetParamInfo(1, 1, gclParameter("asNfg", porBOOLEAN,
					    new BoolPortion(false)));
  FuncObj->SetParamInfo(1, 2, gclParameter("stopAfter", porINTEGER,
					    new NumberPortion(1)));
  FuncObj->SetParamInfo(1, 3, gclParameter("precision", porPRECISION,
              new PrecisionPortion(precDOUBLE)));
  FuncObj->SetParamInfo(1, 4, gclParameter("nPivots", porINTEGER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(1, 5, gclParameter("time", porNUMBER,
					    new NumberPortion(0.0), BYREF));
  FuncObj->SetParamInfo(1, 6, gclParameter("traceFile", porOUTPUT,
					    new OutputPortion(gnull), 
					    BYREF));
  FuncObj->SetParamInfo(1, 7, gclParameter("traceLevel", porNUMBER,
					    new NumberPortion(0)));

  FuncObj->SetFuncInfo(2, gclSignature(GSM_Lp_List, 
				       PortionSpec(porNUMBER, 1), 7));
  FuncObj->SetParamInfo(2, 0, gclParameter("a", PortionSpec(porNUMBER, 2)));
  FuncObj->SetParamInfo(2, 1, gclParameter("b", PortionSpec(porNUMBER, 1)));
  FuncObj->SetParamInfo(2, 2, gclParameter("c", PortionSpec(porNUMBER, 1)));
  FuncObj->SetParamInfo(2, 3, gclParameter("nEqualities", porNUMBER));
  FuncObj->SetParamInfo(2, 4, gclParameter("precision", porPRECISION,
					    new PrecisionPortion(precDOUBLE)));
  FuncObj->SetParamInfo(2, 5, gclParameter("isFeasible", porBOOLEAN,
					    new BoolPortion(false), BYREF));
  FuncObj->SetParamInfo(2, 6, gclParameter("isBounded", porBOOLEAN,
					    new BoolPortion(false), BYREF));

  gsm->AddFunction(FuncObj);


  FuncObj = new FuncDescObj("Nfg", 1);
  FuncObj->SetFuncInfo(0, gclSignature(GSM_Nfg, porNFG, 2));
  FuncObj->SetParamInfo(0, 0, gclParameter("efg", porEFG));
  FuncObj->SetParamInfo(0, 1, gclParameter("time", porNUMBER,
					    new NumberPortion(0), BYREF));
  gsm->AddFunction(FuncObj);

#ifndef MINI_POLY

  FuncObj = new FuncDescObj("PolEnumSolve", 2);
  FuncObj->SetFuncInfo(0, gclSignature(GSM_PolEnum_Nfg, 
				       PortionSpec(porMIXED, 1), 7));
  FuncObj->SetParamInfo(0, 0, gclParameter("support", porNFSUPPORT));
  FuncObj->SetParamInfo(0, 1, gclParameter("stopAfter", porINTEGER,
					    new NumberPortion(0)));
  FuncObj->SetParamInfo(0, 2, gclParameter("precision", porPRECISION,
              new PrecisionPortion(precDOUBLE)));
  FuncObj->SetParamInfo(0, 3, gclParameter("nEvals", porINTEGER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(0, 4, gclParameter("time", porNUMBER,
					    new NumberPortion(0.0), BYREF));
  FuncObj->SetParamInfo(0, 5, gclParameter("traceFile", porOUTPUT,
					    new OutputPortion(gnull), 
					    BYREF));
  FuncObj->SetParamInfo(0, 6, gclParameter("traceLevel", porNUMBER,
					    new NumberPortion(0)));

  FuncObj->SetFuncInfo(1, gclSignature(GSM_PolEnum_Efg, 
				       PortionSpec(porBEHAV, 1), 8));
  FuncObj->SetParamInfo(1, 0, gclParameter("support", porEFSUPPORT));
  FuncObj->SetParamInfo(1, 1, gclParameter("asNfg", porBOOLEAN,
					    new BoolPortion(false)));
  FuncObj->SetParamInfo(1, 2, gclParameter("stopAfter", porINTEGER,
					    new NumberPortion(0)));
  FuncObj->SetParamInfo(1, 3, gclParameter("precision", porPRECISION,
              new PrecisionPortion(precDOUBLE)));
  FuncObj->SetParamInfo(1, 4, gclParameter("nEvals", porINTEGER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(1, 5, gclParameter("time", porNUMBER,
					    new NumberPortion(0.0), BYREF));
  FuncObj->SetParamInfo(1, 6, gclParameter("traceFile", porOUTPUT,
					    new OutputPortion(gnull), 
					    BYREF));
  FuncObj->SetParamInfo(1, 7, gclParameter("traceLevel", porNUMBER,
					    new NumberPortion(0)));
  gsm->AddFunction(FuncObj);

  FuncObj = new FuncDescObj("SeqEquilibSolve", 1);
  FuncObj->SetFuncInfo(0, gclSignature(GSM_SequentialEquilib, 
				       PortionSpec(porBEHAV, 1), 8));
  FuncObj->SetParamInfo(0, 0, gclParameter("basis", porEFBASIS));
  FuncObj->SetParamInfo(0, 1, gclParameter("support", porEFSUPPORT));
  FuncObj->SetParamInfo(0, 2, gclParameter("stopAfter", porINTEGER,
					    new NumberPortion(0)));
  FuncObj->SetParamInfo(0, 3, gclParameter("precision", porPRECISION,
					   new PrecisionPortion(precDOUBLE)));
  FuncObj->SetParamInfo(0, 4, gclParameter("nEvals", porINTEGER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(0, 5, gclParameter("time", porNUMBER,
					    new NumberPortion(0.0), BYREF));
  FuncObj->SetParamInfo(0, 6, gclParameter("traceFile", porOUTPUT,
					    new OutputPortion(gnull), 
					    BYREF));
  FuncObj->SetParamInfo(0, 7, gclParameter("traceLevel", porNUMBER,
					    new NumberPortion(0)));
  gsm->AddFunction(FuncObj);

#endif // ! MINI_POLY


  FuncObj = new FuncDescObj("Payoff", 2);
  FuncObj->SetFuncInfo(0, gclSignature(GSM_Payoff_Behav, porNUMBER, 2,
				       0, funcLISTABLE | funcGAMEMATCH));
  FuncObj->SetParamInfo(0, 0, gclParameter("profile", porBEHAV));
  FuncObj->SetParamInfo(0, 1, gclParameter("player", porEFPLAYER));

  FuncObj->SetFuncInfo(1, gclSignature(GSM_Payoff_Mixed, porNUMBER, 2,
				       0, funcLISTABLE | funcGAMEMATCH));
  FuncObj->SetParamInfo(1, 0, gclParameter("profile", porMIXED));
  FuncObj->SetParamInfo(1, 1, gclParameter("player", porNFPLAYER));
  gsm->AddFunction(FuncObj);



  FuncObj = new FuncDescObj("SimpDivSolve", 2);
  FuncObj->SetFuncInfo(0, gclSignature(GSM_Simpdiv_Nfg,
				       PortionSpec(porMIXED, 1), 9));
  FuncObj->SetParamInfo(0, 0, gclParameter("support", porNFSUPPORT));
  FuncObj->SetParamInfo(0, 1, gclParameter("stopAfter", porINTEGER,
					    new NumberPortion(1)));
  FuncObj->SetParamInfo(0, 2, gclParameter("nRestarts", porINTEGER,
					    new NumberPortion(1)));
  FuncObj->SetParamInfo(0, 3, gclParameter("leashLength", porINTEGER,
					    new NumberPortion(0)));
  FuncObj->SetParamInfo(0, 4, gclParameter("precision", porPRECISION,
              new PrecisionPortion(precDOUBLE)));
  FuncObj->SetParamInfo(0, 5, gclParameter("nEvals", porINTEGER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(0, 6, gclParameter("time", porNUMBER,
					    new NumberPortion(0.0), BYREF));
  FuncObj->SetParamInfo(0, 7, gclParameter("traceFile", porOUTPUT,
					    new OutputPortion(gnull),
					    BYREF));
  FuncObj->SetParamInfo(0, 8, gclParameter("traceLevel", porNUMBER,
					    new NumberPortion(0)));

  FuncObj->SetFuncInfo(1, gclSignature(GSM_Simpdiv_Efg,
				       PortionSpec(porBEHAV, 1), 10));
  FuncObj->SetParamInfo(1, 0, gclParameter("support", porEFSUPPORT));
  FuncObj->SetParamInfo(1, 1, gclParameter("asNfg", porBOOLEAN,
					    new BoolPortion(false)));
  FuncObj->SetParamInfo(1, 2, gclParameter("stopAfter", porINTEGER,
					    new NumberPortion(1)));
  FuncObj->SetParamInfo(1, 3, gclParameter("nRestarts", porINTEGER,
					    new NumberPortion(1)));
  FuncObj->SetParamInfo(1, 4, gclParameter("leashLength", porINTEGER,
					    new NumberPortion(0)));
  FuncObj->SetParamInfo(1, 5, gclParameter("precision", porPRECISION,
					   new PrecisionPortion(precDOUBLE)));
  FuncObj->SetParamInfo(1, 6, gclParameter("nEvals", porINTEGER,
					    new NumberPortion(0), BYREF));
  FuncObj->SetParamInfo(1, 7, gclParameter("time", porNUMBER,
					    new NumberPortion(0.0), BYREF));
  FuncObj->SetParamInfo(1, 8, gclParameter("traceFile", porOUTPUT,
					    new OutputPortion(gnull),
					    BYREF));
  FuncObj->SetParamInfo(1, 9, gclParameter("traceLevel", porNUMBER,
					    new NumberPortion(0)));
  gsm->AddFunction(FuncObj);

  FuncObj = new FuncDescObj("VertEnum", 1);
  FuncObj->SetFuncInfo(0, gclSignature(GSM_VertEnum,
				       PortionSpec(porNUMBER, 2), 3));
  FuncObj->SetParamInfo(0, 0, gclParameter("A", PortionSpec(porNUMBER,2),
					    REQUIRED, BYVAL));
  FuncObj->SetParamInfo(0, 1, gclParameter("b", PortionSpec(porNUMBER,1),
					    REQUIRED, BYVAL));
  FuncObj->SetParamInfo(0, 2, gclParameter("precision", porPRECISION,
					    new PrecisionPortion(precDOUBLE)));
  gsm->AddFunction(FuncObj);

}


