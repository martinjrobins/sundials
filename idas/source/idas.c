/*
 * -----------------------------------------------------------------
 * $Revision: 1.28 $
 * $Date: 2004-10-14 22:17:06 $
 * ----------------------------------------------------------------- 
 * Programmers: Alan C. Hindmarsh, and Radu Serban @ LLNL
 * -----------------------------------------------------------------
 * Copyright (c) 2002, The Regents of the University of California  
 * Produced at the Lawrence Livermore National Laboratory
 * All rights reserved
 * For details, see sundials/idas/LICENSE
 * -----------------------------------------------------------------
 * This is the implementation file for the main IDAS solver.       
 * It is independent of the linear solver in use.                  
 * -----------------------------------------------------------------
 */

/*=================================================================*/
/*BEGIN        Import Header Files                                 */
/*=================================================================*/

#include <stdio.h>
#include <stdlib.h>

#include "idas_impl.h"

#include "sundialsmath.h"

/*=================================================================*/
/*END          Import Header Files                                 */
/*=================================================================*/

/* Macro: loop */
#define loop for(;;)

/*=================================================================*/
/*BEGIN        IDAS Private Constants                              */
/*=================================================================*/

#define ZERO       RCONST(0.0)    /* real 0.0                      */
#define HALF       RCONST(0.5)    /* real 0.5                      */
#define QUARTER    RCONST(0.25)   /* real 0.25                     */
#define TWOTHIRDS  RCONST(0.667)  /* real 2/3 for default steptol  */
#define ONE        RCONST(1.0)    /* real 1.0                      */
#define ONEPT5     RCONST(1.5)    /* real 1.5                      */
#define TWO        RCONST(2.0)    /* real 2.0                      */
#define TWOPT5     RCONST(2.5)    /* real 2.5                      */
#define TEN        RCONST(10.0)   /* real 10.0                     */
#define TWELVE     RCONST(12.0)   /* real 12.0                     */
#define TWENTY     RCONST(20.0)   /* real 20.0                     */
#define HUNDRED    RCONST(100.0)  /* real 100.0                    */
#define PT9        RCONST(0.9)    /* real 0.9                      */
#define PT1        RCONST(0.1)    /* real 0.1                      */
#define PT01       RCONST(0.01)   /* real 0.01                     */
#define PT001      RCONST(0.001)  /* real 0.001                    */
#define PT0001     RCONST(0.0001) /* real 0.0001                   */

/*=================================================================*/
/*END          IDAS Private Constants                              */
/*=================================================================*/

/*=================================================================*/
/*BEGIN        IDAS Default Constants                              */
/*=================================================================*/

#define MXSTEP_DEFAULT   500  /* mxstep default value              */
#define MAXORD_DEFAULT   5    /* maxord default value              */
#define MXNCF    10  /* max number of convergence failures allowed */
#define MXNEF    10  /* max number of error test failures allowed  */
#define EPCON    RCONST(0.33) /* Newton convergence test constant  */
#define MAXNH    5   /* max. number of h tries in IC calc.         */
#define MAXNJ    4   /* max. number of J tries in IC calc.         */
#define MAXNI    10  /* max. Newton iterations in IC calc.         */

/*=================================================================*/
/*END        IDAS Default Constants                                */
/*=================================================================*/

/*=================================================================*/
/*BEGIN        IDAS Routine-Specific Constants                     */
/*=================================================================*/
/* 
   IDASolve return values are defined by an enum statement in the 
   include file ida.h.  They are: 
 
       (successful returns:)
 
       IDA_SUCCESS
       IDA_TSTOP_RETURN
 
       (failed returns:)

       IDA_NO_MEM
       IDA_ILL_INPUT
       IDA_TOO_MUCH_WORK
       IDA_TOO_MUCH_ACC
       IDA_ERR_FAIL
       IDA_CONV_FAIL
       IDA_LSETUP_FAIL
       IDA_LSOLVE_FAIL
       IDA_CONSTR_FAIL   
       IDA_RES_FAIL
       IDA_REP_RES_ERR
*/

/* IDAStep control constants */

#define PREDICT_AGAIN    20

/* IDANewtonIter constants */

#define RATEMAX RCONST(0.9)
#define MAXIT    4

/* Return values from various low-level routines */

#define IDA_RES_RECVR          +1
#define IDA_LSETUP_RECVR       +2
#define IDA_LSOLVE_RECVR       +3
#define IDA_NCONV_RECVR        +4 /* IDANewtonIter, IDANls                   */
#define IDA_CONSTR_RECVR       +5 /* IDANls                                  */
#define CONTINUE_STEPS        +99 /* IDASolve, IDAStopTest1, IDAStopTest2    */

#define UNSET -1    /* IDACompleteStep */
#define LOWER 1     /* IDACompleteStep */
#define RAISE 2     /* IDACompleteStep */
#define MAINTAIN 3  /* IDACompleteStep */

#define ERROR_TEST_FAIL       +7

#define XRATE                RCONST(0.25)        

/* IDASensRes1DQ finite difference methods */

#define CENTERED1  0
#define CENTERED2  1
#define FORWARD1   2
#define FORWARD2   3

/*=================================================================*/
/*END          IDAS Routine-Specific Constants                     */
/*=================================================================*/

/*=================================================================*/
/*BEGIN        IDAS Error Messages                                 */
/*=================================================================*/

/* IDACreate error messages */

#define MSG_IDAMEM_FAIL      "IDACreate-- Allocation of ida_mem failed. \n\n"

/* IDAMalloc/IDAReInit error messages */

#define IDAM               "IDAMalloc/IDAReInit-- "

#define MSG_IDAM_NO_MEM    IDAM "ida_mem = NULL illegal.\n\n"

#define MSG_Y0_NULL        IDAM "y0 = NULL illegal.\n\n"
#define MSG_YP0_NULL       IDAM "yp0 = NULL illegal.\n\n"

#define MSG_BAD_ITOL1      IDAM "itol = %d illegal.\n"
#define MSG_BAD_ITOL2      "The legal values are IDA_SS = %d and IDA_SV = %d.\n\n"
#define MSG_BAD_ITOL       MSG_BAD_ITOL1 MSG_BAD_ITOL2

#define MSG_RES_NULL       IDAM "res = NULL illegal.\n\n"

#define MSG_RELTOL_NULL      IDAM "reltol = NULL illegal.\n\n"

#define MSG_BAD_RELTOL       IDAM "*reltol = %g < 0 illegal.\n\n"

#define MSG_ABSTOL_NULL      IDAM "abstol = NULL illegal.\n\n"

#define MSG_BAD_ABSTOL       IDAM "Some abstol component < 0.0 illegal.\n\n"

#define MSG_BAD_NVECTOR    IDAM "A required vector operation is not implemented.\n\n"

#define MSG_MEM_FAIL       IDAM "A memory request failed.\n\n"

#define MSG_REI_NO_MALLOC  "IDAReInit-- Attempt to call before IDAMalloc. \n\n"

/* IDAQuadMalloc/IDAQuadReInit error messages */

#define QIDAM              "IDAQuadMalloc/IDAQuadReInit-- "

#define MSG_QIDAM_NO_MEM   QIDAM "ida_mem=NULL illegal.\n\n"

#define MSG_QIDAM_MEM_FAIL QIDAM "A memory request failed.\n\n"

#define MSG_BAD_RHSQ       QIDAM "rhsQ=NULL illegal.\n\n"

#define MSG_QREI_QUAD1     "IDAQuadReInit-- Illegal attempt to call before "
#define MSG_QREI_QUAD2     "calling IDAQuadMalloc.\n\n"
#define MSG_QREI_NO_QUAD   MSG_QREI_QUAD1 MSG_QREI_QUAD2

/* IDASensMalloc/ IDASensReInit error messages */

#define SIDAM            "IDASensMalloc/IDASensReInit-- "

#define MSG_SIDAM_NO_MEM SIDAM "ida_mem=NULL illegal.\n\n"

#define MSG_SIDAM_MEM_FAIL SIDAM "A memory request failed.\n\n"

#define MSG_BAD_NS       SIDAM "NS=%d<=0 illegal.\n\n"

#define MSG_P_NULL       SIDAM "p=NULL illegal.\n\n"

#define MSG_YS0_NULL     SIDAM "yS0=NULL illegal.\n\n"
#define MSG_YPS0_NULL    SIDAM "ypS0=NULL illegal.\n\n"

#define MSG_BAD_ISM1     SIDAM "ism=%d illegal.\n"
#define MSG_BAD_ISM2     "The legal values are: "
#define MSG_BAD_ISM3     "IDA_SIMULTANEOUS=%d, IDA_STAGGERED=%d and IDA_STAGGERED1=%d.\n\n"
#define MSG_BAD_ISM      MSG_BAD_ISM1 MSG_BAD_ISM2 MSG_BAD_ISM3

#define MSG_SREI_SENSI1    "IDASensReInit-- Illegal attempt to call before "
#define MSG_SREI_SENSI2    "calling IDASensMalloc.\n\n"
#define MSG_SREI_NO_SENSI  MSG_SREI_SENSI1 MSG_SREI_SENSI2

/* IDAInitialSetup error messages -- called from IDACalcIC or IDASolve */

#define IDAIS              "Initial setup-- "

#define MSG_MISSING_ID      IDAIS "id = NULL but suppressalg option on.\n\n"

#define MSG_BAD_EWT         IDAIS "Some initial ewt component = 0.0 illegal.\n\n"

#define MSG_BAD_CONSTRAINTS IDAIS "illegal values in constraints vector.\n\n"

#define MSG_Y0_FAIL_CONSTR  IDAIS "y0 fails to satisfy constraints.\n\n"

#define MSG_NO_QUADTOL      IDAIS "No quad tolerances set. Illegal for errconQ=TRUE.\n\n"

#define MSG_BAD_RELTOLQ     IDAIS "*reltolQ=%g < 0.0 illegal.\n\n"

#define MSG_BAD_ABSTOLQ     IDAIS "Some abstolQ component < 0.0 illegal.\n\n"  

#define MSG_BAD_EWTQ        IDAIS "Some initial ewtQ component = 0.0 illegal.\n\n"

#define MSG_BAD_ISM_IRESS1  "Illegal use of ism=IDA_STAGGERED1 "
#define MSG_BAD_ISM_IRESS2  "with the provided sensitivity residual function.\n\n"
#define MSG_BAD_ISM_IRESS   IDAIS MSG_BAD_ISM_IRESS1 MSG_BAD_ISM_IRESS2 

#define MSG_PBAR_NULL       IDAIS "pbar is needed, but pbar=NULL illegal.\n\n"

#define MSG_BAD_RELTOLS     IDAIS "*reltolS=%g < 0.0 illegal.\n\n"

#define MSG_BAD_ABSTOLS     IDAIS "Some abstolS component < 0.0 illegal.\n\n"  

#define MSG_IDAIS_MEM_FAIL  IDAIS "A memory request failed (abstolS).\n\n"

#define MSG_BAD_PBAR        IDAIS "Some pbar component = 0.0 illegal.\n\n"

#define MSG_BAD_EWTS        IDAIS "Some initial ewtS component = 0.0 illegal.\n\n"

#define MSG_LSOLVE_NULL     IDAIS "The linear solver's solve routine is NULL.\n\n"

#define MSG_LINIT_FAIL      IDAIS "The linear solver's init routine failed.\n\n"

/* IDASolve error messages */

#define IDASLV             "IDASolve-- "

#define MSG_IDA_NO_MEM     IDASLV "IDA_mem = NULL illegal.\n\n"

#define MSG_NO_MALLOC      IDASLV "Attempt to call before IDAMalloc. \n\n"
 
#define MSG_BAD_HINIT      IDASLV "hinit=%g and tout-t0=%g inconsistent.\n\n"

#define MSG_BAD_TOUT1      IDASLV "Trouble interpolating at tout = %g.\n"
#define MSG_BAD_TOUT2      "tout too far back in direction of integration.\n\n"
#define MSG_BAD_TOUT       MSG_BAD_TOUT1 MSG_BAD_TOUT2

#define MSG_BAD_TSTOP1     IDASLV "tstop = %g is behind  current t = %g \n"
#define MSG_BAD_TSTOP2     "in the direction of integration.\n\n"
#define MSG_BAD_TSTOP      MSG_BAD_TSTOP1 MSG_BAD_TSTOP2


#define MSG_MAX_STEPS1     IDASLV "At t=%g, mxstep=%d steps taken on "
#define MSG_MAX_STEPS2     "this call before\nreaching tout=%g.\n\n"
#define MSG_MAX_STEPS      MSG_MAX_STEPS1 MSG_MAX_STEPS2

#define MSG_EWT_NOW_BAD1   IDASLV "At t=%g, "
#define MSG_EWT_NOW_BAD2   "some ewt component has become <= 0.0.\n\n"
#define MSG_EWT_NOW_BAD    MSG_EWT_NOW_BAD1 MSG_EWT_NOW_BAD2

#define MSG_EWTQ_NOW_BAD1  IDASLV "At t=%g, "
#define MSG_EWTQ_NOW_BAD2  "some ewtQ component has become <= 0.0.\n\n"
#define MSG_EWTQ_NOW_BAD   MSG_EWTQ_NOW_BAD1 MSG_EWTQ_NOW_BAD2

#define MSG_EWTS_NOW_BAD1  IDASLV "At t=%g, "
#define MSG_EWTS_NOW_BAD2  "some ewtS component has become <= 0.0.\n\n"
#define MSG_EWTS_NOW_BAD   MSG_EWTS_NOW_BAD1 MSG_EWTS_NOW_BAD2

#define MSG_TOO_MUCH_ACC   IDASLV "At t=%g, too much accuracy requested.\n\n"

#define MSG_ERR_FAILS1     IDASLV "At t=%g and step size h=%g, the error test\n"
#define MSG_ERR_FAILS2     "failed repeatedly or with |h| = hmin.\n\n"
#define MSG_ERR_FAILS      MSG_ERR_FAILS1 MSG_ERR_FAILS2

#define MSG_CONV_FAILS1    IDASLV "At t=%g and step size h=%g, the corrector\n"
#define MSG_CONV_FAILS2    "convergence failed repeatedly.\n\n"
#define MSG_CONV_FAILS     MSG_CONV_FAILS1 MSG_CONV_FAILS2

#define MSG_SETUP_FAILED1  IDASLV "At t=%g, the linear solver setup routine "
#define MSG_SETUP_FAILED2  "failed in an unrecoverable manner.\n\n"
#define MSG_SETUP_FAILED   MSG_SETUP_FAILED1 MSG_SETUP_FAILED2

#define MSG_SOLVE_FAILED1  IDASLV "At t=%g, the linear solver solve routine "
#define MSG_SOLVE_FAILED2  "failed in an unrecoverable manner.\n\n"
#define MSG_SOLVE_FAILED   MSG_SOLVE_FAILED1 MSG_SOLVE_FAILED2

#define MSG_TOO_CLOSE1     IDASLV "tout=%g too close to t0=%g to start"
#define MSG_TOO_CLOSE2     " integration.\n\n"
#define MSG_TOO_CLOSE      MSG_TOO_CLOSE1 MSG_TOO_CLOSE2

#define MSG_YRET_NULL      IDASLV "yret=NULL illegal.\n\n"
#define MSG_YPRET_NULL     IDASLV "ypret=NULL illegal.\n\n"
#define MSG_TRET_NULL      IDASLV "tret=NULL illegal.\n\n"

#define MSG_BAD_ITASK      IDASLV "itask=%d illegal.\n\n"

#define MSG_NO_TSTOP1      IDASLV "itask = IDA_NORMAL_TSTOP or itask = IDA_ONE_STEP_TSTOP "
#define MSG_NO_TSTOP2      "but tstop was not set.\n\n"
#define MSG_NO_TSTOP       MSG_NO_TSTOP1 MSG_NO_TSTOP2

#define MSG_REP_RES_ERR1   IDASLV "At t = %g, repeated recoverable error \n"
#define MSG_REP_RES_ERR2   "returns from ResFn residual function. \n\n"
#define MSG_REP_RES_ERR    MSG_REP_RES_ERR1 MSG_REP_RES_ERR2

#define MSG_RES_NONRECOV1  IDASLV "At t = %g, nonrecoverable error \n"
#define MSG_RES_NONRECOV2  "return from ResFn residual function. \n\n"
#define MSG_RES_NONRECOV   MSG_RES_NONRECOV1 MSG_RES_NONRECOV2

#define MSG_FAILED_CONSTR1 IDASLV "At t = %g, unable to satisfy \n"
#define MSG_FAILED_CONSTR2 "inequality constraints. \n\n"
#define MSG_FAILED_CONSTR  MSG_FAILED_CONSTR1 MSG_FAILED_CONSTR2

/* IDAGet* Error Messages */

#define MSG_IDAG_NO_MEM    "ida_mem=NULL in an IDAGet routine illegal. \n\n"

#define MSG_BAD_T1         "IDAGetSolution/IDAGetQuad/IDAGetSens-- t=%g illegal.\n"
#define MSG_BAD_T2         "t not in interval tcur-hu=%g to tcur=%g.\n\n"
#define MSG_BAD_T          MSG_BAD_T1 MSG_BAD_T2

#define MSG_IDAG_NO_QUAD1  "IDAGetQuad*-- Illegal attempt to call before "
#define MSG_IDAG_NO_QUAD2  "calling IDAQuadMalloc.\n\n"
#define MSG_IDAG_NO_QUAD   MSG_IDAG_NO_QUAD1 MSG_IDAG_NO_QUAD2

#define MSG_IDAG_NO_SENS1  "IDAGetSens*-- Illegal attempt to call before "
#define MSG_IDAG_NO_SENS2  "calling IDASensMalloc.\n\n"
#define MSG_IDAG_NO_SENS   MSG_IDAG_NO_SENS1 MSG_IDAG_NO_SENS2

#define MSG_BAD_IS         "IDAGetSens1-- is=%d illegal. \n\n"

/*=================================================================*/
/*END          IDAS Error Messages                                 */
/*=================================================================*/

/*=================================================================*/
/*BEGIN        Private Helper Functions Prototypes                 */
/*=================================================================*/

static booleantype IDACheckNvector(N_Vector tmpl);

static booleantype IDAAllocVectors(IDAMem IDA_mem, N_Vector tmpl);
static void IDAFreeVectors(IDAMem IDA_mem);

static booleantype IDAQuadAllocVectors(IDAMem IDA_mem, N_Vector tmpl);
static void IDAQuadFreeVectors(IDAMem IDA_mem);

static booleantype IDASensAllocVectors(IDAMem IDA_mem, N_Vector tmpl);
static void IDASensFreeVectors(IDAMem IDA_mem);

/*------------------*/

int IDAInitialSetup(IDAMem IDA_mem);

/*------------------*/

booleantype IDAEwtSet(IDAMem IDA_mem, N_Vector ycur);
static booleantype IDAEwtSetSS(IDAMem IDA_mem, N_Vector ycur);
static booleantype IDAEwtSetSV(IDAMem IDA_mem, N_Vector ycur);

static booleantype IDAQuadEwtSet(IDAMem IDA_mem, N_Vector qcur);
static booleantype IDAQuadEwtSetSS(IDAMem IDA_mem, N_Vector qcur);
static booleantype IDAQuadEwtSetSV(IDAMem IDA_mem, N_Vector qcur);

static booleantype IDASensTestAtol(IDAMem IDA_mem, void *atolS);

static booleantype IDASensAllocAtol(IDAMem IDA_mem, void **atolSPtr);
static void IDASensFreeAtol(IDAMem IDA_mem, void *atolS);

static booleantype IDASensSetAtol(IDAMem IDA_mem, void *atolS);
static booleantype IDASensSetAtolSS(IDAMem IDA_mem, realtype *atolS);
static booleantype IDASensSetAtolSV(IDAMem IDA_mem, N_Vector *atolS);

booleantype IDASensEwtSet(IDAMem IDA_mem, N_Vector *yScur);
static booleantype IDASensEwtSetSS(IDAMem IDA_mem, N_Vector *yScur);
static booleantype IDASensEwtSetSV(IDAMem IDA_mem, N_Vector *yScur);

/*------------------*/

static int IDAStopTest1(IDAMem IDA_mem, realtype tout,realtype *tret, 
                        N_Vector yret, N_Vector ypret, int itask);
static int IDAStopTest2(IDAMem IDA_mem, realtype tout, realtype *tret, 
                        N_Vector yret, N_Vector ypret, int itask);
static int IDAHandleFailure(IDAMem IDA_mem, int sflag);

/*------------------*/

static int IDAStep(IDAMem IDA_mem);

static void IDASetCoeffs(IDAMem IDA_mem, realtype *ck);

static int IDANls(IDAMem IDA_mem);
static int IDAStgrNls(IDAMem IDA_mem);
static int IDAStgr1Nls(IDAMem IDA_mem, int is);

static void IDASetSS(IDAMem IDA_mem, realtype value);

static int IDANewtonIter(IDAMem IDA_mem);
static int IDAStgrNewtonIter(IDAMem IDA_mem);
static int IDAStgr1NewtonIter(IDAMem IDA_mem, int is);

static void IDAPredict(IDAMem IDA_mem);
static void IDAQuadPredict(IDAMem IDA_mem);
static void IDASensPredict(IDAMem IDA_mem, int is, 
                           N_Vector yySens, N_Vector ypSens);

static int IDATestError(IDAMem IDA_mem, realtype ck, 
                        realtype *est,
                        realtype *erk, realtype *erkm1);
static int IDAQuadTestError(IDAMem IDA_mem, realtype ck, 
                            realtype *estQ,
                            realtype *erQk, realtype *erQkm1);
static int IDAStgrTestError(IDAMem IDA_mem, realtype ck, 
                            realtype *estS, 
                            realtype *erSk, realtype *erSkm1);
static int IDAStgr1TestError(IDAMem IDA_mem, int is, realtype ck, 
                             realtype *estS, 
                             realtype *erSk, realtype *erSkm1);

static void IDARestore(IDAMem IDA_mem, realtype saved_t);

static int IDAHandleNFlag(IDAMem IDA_mem, int nflag,
                          int *ncfPtr, int *nefPtr, realtype est);

static void IDAReset(IDAMem IDA_mem);

static void IDACompleteStep(IDAMem IDA_mem,
                            realtype error_k, realtype error_km1);

/*------------------*/

realtype IDAWrmsNorm(IDAMem IDA_mem, N_Vector x, N_Vector w, 
                     booleantype mask);

static realtype IDAQuadWrmsNormUpdate(realtype old_nrm,
                                      N_Vector xQ, N_Vector wQ);

static realtype IDASensWrmsNorm(IDAMem IDA_mem, N_Vector *xS, N_Vector *wS,
                                booleantype mask);
static realtype IDASensWrmsNormUpdate(IDAMem IDA_mem, realtype old_nrm,
                                      N_Vector *xS, N_Vector *wS,
                                      booleantype mask);

/*------------------*/

static int IDASensRes(IDAMem IDA_mem, realtype time,
                      N_Vector yycur, N_Vector ypcur, N_Vector resvalcur,
                      N_Vector *yyScur, N_Vector *ypScur, N_Vector *resvalScur,
                      N_Vector tmp1, N_Vector tmp2, N_Vector tmp3);

static int IDASensRes1(IDAMem IDA_mem, realtype time,
                       N_Vector yycur, N_Vector ypcur, N_Vector resvalcur,
                       int is,
                       N_Vector yyScur, N_Vector ypScur, N_Vector resvalScur,
                       N_Vector tmp1, N_Vector tmp2, N_Vector tmp3);

int IDASensResDQ(int Ns, realtype t, 
                 N_Vector yy, N_Vector yp, N_Vector resval,
                 N_Vector *yyS, N_Vector *ypS, N_Vector *resvalS,
                 void *rdataS,
                 N_Vector ytemp, N_Vector yptemp, N_Vector restemp);

int IDASensRes1DQ(int Ns, realtype t, 
                  N_Vector yy, N_Vector yp, N_Vector resval,
                  int iS,
                  N_Vector yyS, N_Vector ypS, N_Vector resvalS,
                  void *rdataS,
                  N_Vector ytemp, N_Vector yptemp, N_Vector restemp);

/*=================================================================*/
/*END          Private Helper Functions Prototypes                 */
/*=================================================================*/

/*=================================================================*/
/*BEGIN        EXPORTED FUNCTIONS IMPLEMENTATION                   */
/*=================================================================*/

/*------------------     IDACreate     --------------------------*/
/* 
   IDACreate creates an internal memory block for a problem to 
   be solved by IDA.
   If successful, IDACreate returns a pointer to the problem memory. 
   This pointer should be passed to IDAMalloc.  
   If an initialization error occurs, IDACreate prints an error 
   message to standard err and returns NULL. 
*/
/*-----------------------------------------------------------------*/

void *IDACreate(void)
{
  IDAMem IDA_mem;

  IDA_mem = (IDAMem) malloc(sizeof(struct IDAMemRec));
  if (IDA_mem == NULL) {
    fprintf(stderr, MSG_MEM_FAIL);
    return (NULL);
  }

  /* Set unit roundoff in IDA_mem */
  IDA_mem->ida_uround = UNIT_ROUNDOFF;

  /* Set default values for integrator optional inputs */
  IDA_mem->ida_rdata       = NULL;
  IDA_mem->ida_errfp       = stderr;
  IDA_mem->ida_maxord      = MAXORD_DEFAULT;
  IDA_mem->ida_mxstep      = MXSTEP_DEFAULT;
  IDA_mem->ida_hmax_inv    = ZERO;
  IDA_mem->ida_hin         = ZERO;
  IDA_mem->ida_epcon       = EPCON;
  IDA_mem->ida_maxnef      = MXNEF;
  IDA_mem->ida_maxncf      = MXNCF;
  IDA_mem->ida_maxcor      = MAXIT;
  IDA_mem->ida_suppressalg = FALSE;
  IDA_mem->ida_id          = NULL;
  IDA_mem->ida_constraints = NULL;
  IDA_mem->ida_tstopset    = FALSE;

  /* Set default values for quad. optional inputs */
  IDA_mem->ida_rdataQ      = NULL;
  IDA_mem->ida_errconQ     = FALSE;
  IDA_mem->ida_reltolQ     = NULL;
  IDA_mem->ida_abstolQ     = NULL;

  /* Set defaull values for sensi. optional inputs */
  IDA_mem->ida_maxcorS     = MAXIT;
  IDA_mem->ida_rdataS      = (void *)IDA_mem;
  IDA_mem->ida_resS        = IDASensResDQ;
  IDA_mem->ida_resS1       = IDASensRes1DQ;
  IDA_mem->ida_resSDQ      = TRUE;
  IDA_mem->ida_iresS       = IDA_ONESENS;
  IDA_mem->ida_errconS     = TRUE;
  IDA_mem->ida_rhomax      = ZERO;
  IDA_mem->ida_pbar        = NULL;
  IDA_mem->ida_reltolS     = NULL;
  IDA_mem->ida_abstolS     = NULL;
  IDA_mem->ida_abstolSalloc= TRUE;

  /* Set default values for IC optional inputs */
  IDA_mem->ida_epiccon     = PT01 * EPCON;
  IDA_mem->ida_maxnh       = MAXNH;
  IDA_mem->ida_maxnj       = MAXNJ;
  IDA_mem->ida_maxnit      = MAXNI;
  IDA_mem->ida_lsoff       = FALSE;
  IDA_mem->ida_steptol     = RPowerR(IDA_mem->ida_uround, TWOTHIRDS);

  /* Initialize workspace requirements */
  IDA_mem->ida_lrw = 0;
  IDA_mem->ida_liw = 0;

  /* No mallocs have been done yet */
  IDA_mem->ida_MallocDone     = FALSE;
  IDA_mem->ida_quadMallocDone = FALSE;
  IDA_mem->ida_sensMallocDone = FALSE;

  /* Return pointer to IDA memory block */
  return((void *)IDA_mem);
}

#define errfp (IDA_mem->ida_errfp)

/*------------------       IDAMalloc     --------------------------*/
/*
 IDAMalloc allocates and initializes memory for a problem. All
 problem specification inputs are checked for errors. If any
 error occurs during initialization, it is reported to the file
 whose file pointer is errfp and an error flag is returned. 
*/
/*-----------------------------------------------------------------*/

int IDAMalloc(void *ida_mem, ResFn res,
              realtype t0, N_Vector y0, N_Vector yp0, 
              int itol, realtype *reltol, void *abstol)
{
  IDAMem IDA_mem;
  booleantype nvectorOK, allocOK, neg_abstol;
  long int lrw1, liw1;

  /* Check ida_mem */
  if (ida_mem == NULL) {
    fprintf(stderr, MSG_IDAM_NO_MEM);
    return(IDA_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem;
  
  /* Check for legal input parameters */
  
  if (y0 == NULL) { 
    if(errfp!=NULL) fprintf(errfp, MSG_Y0_NULL); 
    return(IDA_ILL_INPUT); 
  }
  
  if (yp0 == NULL) { 
    if(errfp!=NULL) fprintf(errfp, MSG_YP0_NULL); 
    return(IDA_ILL_INPUT); 
  }

  if ((itol != IDA_SS) && (itol != IDA_SV)) {
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_ITOL, itol, IDA_SS, IDA_SV);
    return(IDA_ILL_INPUT);
  }

  if (res == NULL) { 
    if(errfp!=NULL) fprintf(errfp, MSG_RES_NULL); 
    return(IDA_ILL_INPUT); 
  }

  if (reltol == NULL) { 
    if(errfp!=NULL) fprintf(errfp, MSG_RELTOL_NULL); 
    return(IDA_ILL_INPUT); 
  }

  if (*reltol < ZERO) { 
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_RELTOL, *reltol); 
    return(IDA_ILL_INPUT); 
  }
   
  if (abstol == NULL) { 
    if(errfp!=NULL) fprintf(errfp, MSG_ABSTOL_NULL); 
    return(IDA_ILL_INPUT); 
  }

  /* Test if all required vector operations are implemented */
  nvectorOK = IDACheckNvector(y0);
  if(!nvectorOK) {
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_NVECTOR);
    return(IDA_ILL_INPUT);
  }

  /* Test absolute tolerances */
  if (itol == IDA_SS) { 
    neg_abstol = (*((realtype *)abstol) < ZERO); 
  } else { 
    neg_abstol = (N_VMin((N_Vector)abstol) < ZERO); 
  }
  if (neg_abstol) { 
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_ABSTOL); 
    return(IDA_ILL_INPUT); 
  }

  /* Set space requirements for one N_Vector */
  if (y0->ops->nvspace != NULL) {
    N_VSpace(y0, &lrw1, &liw1);
  } else {
    lrw1 = 0;
    liw1 = 0;
  }
  IDA_mem->ida_lrw1 = lrw1;
  IDA_mem->ida_liw1 = liw1;

  /* Allocate the vectors */
  allocOK = IDAAllocVectors(IDA_mem, y0);
  if (!allocOK) {
    if(errfp!=NULL) fprintf(errfp, MSG_MEM_FAIL);
    return(IDA_MEM_FAIL);
  }
 
  /* All error checking is complete at this point */

  /* Copy the input parameters into IDA memory block */
  IDA_mem->ida_res = res;
  IDA_mem->ida_tn = t0;
  IDA_mem->ida_y0  = y0;
  IDA_mem->ida_yp0 = yp0;
  IDA_mem->ida_itol = itol;
  IDA_mem->ida_reltol = reltol;      
  IDA_mem->ida_abstol = abstol;  

  /* Set the linear solver addresses to NULL */
  IDA_mem->ida_linit  = NULL;
  IDA_mem->ida_lsetup = NULL;
  IDA_mem->ida_lsolve = NULL;
  IDA_mem->ida_lperf  = NULL;
  IDA_mem->ida_lfree  = NULL;
  IDA_mem->ida_lmem = NULL;
  IDA_mem->ida_forceSetup = FALSE;

  /* Initialize the phi array */
  N_VScale(ONE, y0, IDA_mem->ida_phi[0]);  
  N_VScale(ONE, yp0, IDA_mem->ida_phi[1]);  
 
  /* Initialize all the counters and other optional output values */
  IDA_mem->ida_nst     = 0;
  IDA_mem->ida_nre     = 0;
  IDA_mem->ida_ncfn    = 0;
  IDA_mem->ida_netf    = 0;
  IDA_mem->ida_nni     = 0;
  IDA_mem->ida_nsetups = 0;
  
  IDA_mem->ida_kused = 0;
  IDA_mem->ida_hused = ZERO;
  IDA_mem->ida_tolsf = ONE;

  /* Initialize quadrature data; default is NO quadratures */
  IDA_mem->ida_quad  = FALSE;

  /* Initialize sensitivity data; default is NO sensitivity */
  IDA_mem->ida_sensi  = FALSE;

  /* Initial setup not done yet */
  IDA_mem->ida_SetupDone = FALSE;

  /* Problem memory has been successfully allocated */
  IDA_mem->ida_MallocDone = TRUE;
  return(IDA_SUCCESS);
}

/*---------------------- IDAReInit --------------------------------*/
/*
 IDAReInit re-initializes IDA's memory for a problem, assuming
 it has already beeen allocated in a prior IDAMalloc call.
 All problem specification inputs are checked for errors.
 The problem size Neq is assumed to be unchaged since the call
 to IDAMalloc, and the maximum order maxord must not be larger.
 If any error occurs during reinitialization, it is reported to
 the file whose file pointer is errfp.
 The return value is IDA_SUCCESS = 0 if no errors occurred, or
 a negative value otherwise.
*/
/*-----------------------------------------------------------------*/

int IDAReInit(void *ida_mem, ResFn res,
              realtype t0, N_Vector y0, N_Vector yp0,
              int itol, realtype *reltol, void *abstol)
{
  IDAMem IDA_mem;
  booleantype neg_abstol;

  /* Check for legal input parameters */
  
  if (ida_mem == NULL) {
    fprintf(stderr, MSG_IDAM_NO_MEM);
    return(IDA_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem;

  /* Check if problem was malloc'ed */
  
  if (IDA_mem->ida_MallocDone == FALSE) {
    if(errfp!=NULL) fprintf(errfp, MSG_REI_NO_MALLOC);
    return(IDA_NO_MALLOC);
  }

  /* Check for legal input parameters */
  
  if (y0 == NULL) { 
    if(errfp!=NULL) fprintf(errfp, MSG_Y0_NULL); 
    return(IDA_ILL_INPUT); 
  }
  
  if (yp0 == NULL) { 
    if(errfp!=NULL) fprintf(errfp, MSG_YP0_NULL); 
    return(IDA_ILL_INPUT); 
  }

  if ((itol != IDA_SS) && (itol != IDA_SV)) {
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_ITOL, itol, IDA_SS, IDA_SV);
    return(IDA_ILL_INPUT);
  }

  if (res == NULL) { 
    if(errfp!=NULL) fprintf(errfp, MSG_RES_NULL); 
    return(IDA_ILL_INPUT); 
  }

  if (reltol == NULL) { 
    if(errfp!=NULL) fprintf(errfp, MSG_RELTOL_NULL); 
    return(IDA_ILL_INPUT); 
  }

  if (*reltol < ZERO) {
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_RELTOL, *reltol); 
    return(IDA_ILL_INPUT); 
  }
   
  if (abstol == NULL) { 
    if(errfp!=NULL) fprintf(errfp, MSG_ABSTOL_NULL); 
    return(IDA_ILL_INPUT); 
  }

  if (itol == IDA_SS) { 
    neg_abstol = (*((realtype *)abstol) < ZERO); 
  } else { 
    neg_abstol = (N_VMin((N_Vector)abstol) < ZERO); 
  }
  if (neg_abstol) { 
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_ABSTOL); 
    return(IDA_ILL_INPUT); 
  }

  /* All error checking is complete at this point */

  /* Copy the input parameters into IDA memory block */
  IDA_mem->ida_res = res;
  IDA_mem->ida_y0  = y0;
  IDA_mem->ida_yp0 = yp0;
  IDA_mem->ida_tn = t0;
  IDA_mem->ida_itol = itol;
  IDA_mem->ida_reltol = reltol;      
  IDA_mem->ida_abstol = abstol;  

  IDA_mem->ida_forceSetup = FALSE;

  /* Initialize the phi array */
  N_VScale(ONE, y0, IDA_mem->ida_phi[0]);  
  N_VScale(ONE, yp0, IDA_mem->ida_phi[1]);  
 
  /* Initialize all the counters and other optional output values */
 
  IDA_mem->ida_nst     = 0;
  IDA_mem->ida_nre     = 0;
  IDA_mem->ida_ncfn    = 0;
  IDA_mem->ida_netf    = 0;
  IDA_mem->ida_nni     = 0;
  IDA_mem->ida_nsetups = 0;
  
  IDA_mem->ida_kused = 0;
  IDA_mem->ida_hused = ZERO;
  IDA_mem->ida_tolsf = ONE;

  /* Re-initialize quadrature data; default is NO quadratures */
  IDA_mem->ida_quad  = FALSE;

  /* Re-initialize sensitivity data; default is NO sensitivity */
  IDA_mem->ida_sensi  = FALSE;

  /* Initial setup not done yet */
  IDA_mem->ida_SetupDone = FALSE;
      
  /* Problem has been successfully re-initialized */

  return(IDA_SUCCESS);
}

/*=================================================================*/
/*END        QUADRATURE OPTIONAL INPUT FUNCTIONS                   */
/*=================================================================*/

/*--------------------   IDAQuadMalloc   --------------------------*/
/*
  IDAQuadMalloc allocates and initializes quadrature related 
  memory for a problem. All problem specification inputs are 
  checked for errors. If any error occurs during initialization, 
  it is reported to the file whose file pointer is errfp. 

  The return value is IDA_SUCCESS = 0 if no errors occurred, or
  a negative value otherwise.
*/
/*-----------------------------------------------------------------*/

int IDAQuadMalloc(void *ida_mem, QuadRhsFn rhsQ, N_Vector yQ0)
{
  IDAMem IDA_mem;
  booleantype allocOK;
  long int lrw1Q, liw1Q;

  /* Check ida_mem */
  if (ida_mem == NULL) {
    fprintf(stderr, MSG_IDAM_NO_MEM);
    return(IDA_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem;

  if (rhsQ == NULL) {
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_RHSQ);
    return(IDA_ILL_INPUT);
  }

  /* Set space requirements for one N_Vector */
  N_VSpace(yQ0, &lrw1Q, &liw1Q);
  IDA_mem->ida_lrw1Q = lrw1Q;
  IDA_mem->ida_liw1Q = liw1Q;

  /* Allocate the vectors */
  allocOK = IDAQuadAllocVectors(IDA_mem, yQ0);
  if (!allocOK) {
    if(errfp!=NULL) fprintf(errfp, MSG_QIDAM_MEM_FAIL);
    return(IDA_MEM_FAIL);
  }

  /* Initialize phiQ in the history array */
  N_VScale(ONE, yQ0, IDA_mem->ida_phiQ[0]);

  /* Copy the input parameters into IDAS state */
  IDA_mem->ida_rhsQ = rhsQ;

  /* Initialize counters */
  IDA_mem->ida_nrQe  = 0;
  IDA_mem->ida_netfQ = 0;

  /* Quadrature integration turned ON */
  IDA_mem->ida_quad           = TRUE;
  IDA_mem->ida_quadMallocDone = TRUE;

  /* Quadrature initialization was successfull */
  return(IDA_SUCCESS);
}

/*------------------    IDAQuadReInit    --------------------------*/
/*
  IDAQuadReInit re-initializes IDAS' quadrature related memory 
  for a problem, assuming it has already been allocated in prior 
  calls to IDAMalloc and IDAQuadMalloc. 
  All problem specification inputs are checked for errors.
  If any error occurs during initialization, it is reported to the
  file whose file pointer is errfp.

  The return value is IDA_SUCCESS = 0 if no errors occurred, or
  a negative value otherwise.
*/
/*-----------------------------------------------------------------*/

int IDAQuadReInit(void *ida_mem, QuadRhsFn rhsQ, N_Vector yQ0)
{
  IDAMem IDA_mem;

  /* Check ida_mem */
  if (ida_mem == NULL) {
    fprintf(stderr, MSG_IDAM_NO_MEM);
    return(IDA_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem;

  /* Check if quadrature was initialized */
  if (IDA_mem->ida_quadMallocDone == FALSE) {
    if(errfp!=NULL) fprintf(errfp, MSG_QREI_NO_QUAD);
    return(IDA_NO_QUAD);
  }

  if (rhsQ == NULL) {
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_RHSQ);
    return(IDA_ILL_INPUT);
  }

  /* Initialize phiQ in the history array */
  N_VScale(ONE, yQ0, IDA_mem->ida_phiQ[0]);

  /* Copy the input parameters into IDAS state */
  IDA_mem->ida_rhsQ    = rhsQ;

  /* Initialize counters */
  IDA_mem->ida_nrQe  = 0;
  IDA_mem->ida_netfQ = 0;

  /* Quadrature integration turned ON */
  IDA_mem->ida_quad = TRUE;

  /* Quadrature initialization was successfull */
  return(IDA_SUCCESS);

}

/*=================================================================*/
/*END        SENSITIVITY OPTIONAL INPUT FUNCTIONS                  */
/*=================================================================*/

#define iresS        (IDA_mem->ida_iresS)
#define resSDQ       (IDA_mem->ida_resSDQ)
#define abstolSalloc (IDA_mem->ida_abstolSalloc)
#define stgr1alloc   (IDA_mem->ida_stgr1alloc)
#define ssS1         (IDA_mem->ida_ssS1)
#define netfS1       (IDA_mem->ida_netfS1)
#define nniS1        (IDA_mem->ida_nniS1)
#define ncfnS1       (IDA_mem->ida_ncfnS1)

/*------------------     IDASensMalloc   --------------------------*/
/*
  IDASensMalloc allocates and initializes sensitivity related 
  memory for a problem. All problem specification inputs are 
  checked for errors. If any error occurs during initialization, 
  it is reported to the file whose file pointer is errfp. 
  The return value is IDA_SUCCESS = 0 if no errors occurred, or
  a negative value otherwise.
*/
/*-----------------------------------------------------------------*/

int IDASensMalloc(void *ida_mem, int Ns, int ism, 
                  realtype *p, int *plist, 
                  N_Vector *yS0, N_Vector *ypS0)
{
  IDAMem IDA_mem;
  int is;
  booleantype allocOK;

  /* Check ida_mem */
  if (ida_mem == NULL) {
    fprintf(stderr, MSG_SIDAM_NO_MEM);
    return(IDA_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem;

  /* Check if Ns is legal */
  if (Ns<=0) {
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_NS, Ns);
    return(IDA_ILL_INPUT);
  }
  IDA_mem->ida_Ns = Ns;

  /* Check if ism is legal */
  if ((ism!=IDA_SIMULTANEOUS) && (ism!=IDA_STAGGERED) && (ism!=IDA_STAGGERED1)) {
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_ISM,ism,IDA_SIMULTANEOUS,IDA_STAGGERED,IDA_STAGGERED1);
    return(IDA_ILL_INPUT);
  }
  IDA_mem->ida_ism = ism;

  /* Check if p is non-null */
  if (p==NULL) {
    if(errfp!=NULL) fprintf(errfp, MSG_P_NULL);
    return(IDA_ILL_INPUT);
  }
  IDA_mem->ida_p     = p;
  IDA_mem->ida_plist = plist;

  /* Check if yS0 is non-null */
  if (yS0 == NULL) {
    if(errfp!=NULL) fprintf(errfp, MSG_YS0_NULL);
    return(IDA_ILL_INPUT);
  }
  IDA_mem->ida_yS0 = yS0;

  /* Check if ypS0 is non-null */
  if (ypS0 == NULL) {
    if(errfp!=NULL) fprintf(errfp, MSG_YPS0_NULL);
    return(IDA_ILL_INPUT);
  }
  IDA_mem->ida_ypS0 = ypS0;

  /* Allocate the vectors (using yS0[0] as a template) */
  allocOK = IDASensAllocVectors(IDA_mem, yS0[0]);
  if (!allocOK) {
    if(errfp!=NULL) fprintf(errfp, MSG_SIDAM_MEM_FAIL);
    return(IDA_MEM_FAIL);
  }

  /* Allocate ssS1, netfS1, ncfnS1, and nniS1 if needed */
  stgr1alloc = FALSE;
  if (ism == IDA_STAGGERED1) {
    stgr1alloc = TRUE;
    ssS1   = (realtype*) malloc(Ns*sizeof(realtype));
    netfS1 = (long int*) malloc(Ns*sizeof(long int));
    ncfnS1 = (long int*) malloc(Ns*sizeof(long int));
    nniS1  = (long int*) malloc(Ns*sizeof(long int));
    if ( (ssS1 == NULL) || (ncfnS1 == NULL) || (nniS1 == NULL) ) {
      IDASensFreeVectors(IDA_mem);
      if(errfp!=NULL) fprintf(errfp, MSG_SIDAM_MEM_FAIL);
      return(IDA_MEM_FAIL);
    }
  }

  /*---------------------------------------------- 
     All error checking is complete at this point 
  -----------------------------------------------*/

  /* Initialize the phiS array */
  for (is=0; is<Ns; is++) {
    N_VScale(ONE, yS0[is],  IDA_mem->ida_phiS[0][is]);  
    N_VScale(ONE, ypS0[is], IDA_mem->ida_phiS[1][is]);  
  }

  /* Initialize all sensitivity related counters */
  IDA_mem->ida_nrSe     = 0;
  IDA_mem->ida_nreS     = 0;
  IDA_mem->ida_ncfnS    = 0;
  IDA_mem->ida_netfS    = 0;
  IDA_mem->ida_nniS     = 0;
  IDA_mem->ida_nsetupsS = 0;
  if (ism==IDA_STAGGERED1)
    for (is=0; is<Ns; is++) {
      netfS1[is] = 0;
      ncfnS1[is] = 0;
      nniS1[is]  = 0;
    }

  /* Sensitivities will be computed */
  IDA_mem->ida_sensi = TRUE;
  IDA_mem->ida_sensMallocDone = TRUE;

  /* Sensitivity initialization was successfull */

  return(IDA_SUCCESS);
}

/*------------------   IDASensReInit   --------------------------*/
/*
  IDASensReInit re-initializes IDAS' sensitivity related memory 
  for a problem, assuming it has already been allocated in prior 
  calls to IDAMalloc and IDASensMalloc. 
  All problem specification inputs are checked for errors.
  The number of sensitivities Ns is assumed to be unchanged since
  the previous call to IDASensMalloc.
  If any error occurs during initialization, it is reported to the
  file whose file pointer is errfp.
  The return value is IDA_SUCCESS = 0 if no errors occurred, or
  a negative value otherwise.
*/ 
/*-----------------------------------------------------------------*/

int IDASensReInit(void *ida_mem, int ism,
                  realtype *p, int *plist, 
                  N_Vector *yS0, N_Vector *ypS0)
{
  IDAMem IDA_mem;
  int Ns, is;

  /* Check ida_mem */
  if (ida_mem == NULL) {
    fprintf(stderr, MSG_SIDAM_NO_MEM);
    return(IDA_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem;
  Ns = IDA_mem->ida_Ns;
  
  /* Was sensitivity initialized? */
  if (IDA_mem->ida_sensMallocDone == FALSE) {
    if(errfp!=NULL) fprintf(errfp, MSG_SREI_NO_SENSI);
    return(IDA_NO_SENS);
  } 

  /* Check if ism is legal */
  if ((ism!=IDA_SIMULTANEOUS) && (ism!=IDA_STAGGERED) && (ism!=IDA_STAGGERED1)) {
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_ISM,ism,IDA_SIMULTANEOUS,IDA_STAGGERED,IDA_STAGGERED1);
    return(IDA_ILL_INPUT);
  }
  IDA_mem->ida_ism = ism;

  /* Check if p is non-null */
  if (p==NULL) {
    if(errfp!=NULL) fprintf(errfp, MSG_P_NULL);
    return(IDA_ILL_INPUT);
  }
  IDA_mem->ida_p     = p;
  IDA_mem->ida_plist = plist;

  /* Check if yS0 is non-null */
  if (yS0 == NULL) {
    if(errfp!=NULL) fprintf(errfp, MSG_YS0_NULL);
    return(IDA_ILL_INPUT);
  }
  IDA_mem->ida_yS0 = yS0;

  /* Check if ypS0 is non-null */
  if (ypS0 == NULL) {
    if(errfp!=NULL) fprintf(errfp, MSG_YPS0_NULL);
    return(IDA_ILL_INPUT);
  }
  IDA_mem->ida_ypS0 = ypS0;

  /* Allocate ssS1, netfS1, ncfnS1, and nniS1 if needed */
  if ( (ism == IDA_STAGGERED1) && (!stgr1alloc) ) {
    stgr1alloc = TRUE;
    ssS1   = (realtype*) malloc(Ns*sizeof(realtype));
    netfS1 = (long int*) malloc(Ns*sizeof(long int));
    ncfnS1 = (long int*) malloc(Ns*sizeof(long int));
    nniS1  = (long int*) malloc(Ns*sizeof(long int));
    if ( (ssS1 == NULL) || (ncfnS1 == NULL) || (nniS1 == NULL) ) {
      if(errfp!=NULL) fprintf(errfp, MSG_SIDAM_MEM_FAIL);
      return(IDA_MEM_FAIL);
    }
  }

  /*---------------------------------------------- 
     All error checking is complete at this point 
  -----------------------------------------------*/

  /* Initialize the phiS array */
  for (is=0; is<Ns; is++) {
    N_VScale(ONE, yS0[is],  IDA_mem->ida_phiS[0][is]);  
    N_VScale(ONE, ypS0[is], IDA_mem->ida_phiS[1][is]);  
  }

  /* Initialize all sensitivity related counters */
  IDA_mem->ida_nrSe     = 0;
  IDA_mem->ida_nreS     = 0;
  IDA_mem->ida_ncfnS    = 0;
  IDA_mem->ida_netfS    = 0;
  IDA_mem->ida_nniS     = 0;
  IDA_mem->ida_nsetupsS = 0;
  if (ism==IDA_STAGGERED1)
    for (is=0; is<Ns; is++) {
      netfS1[is] = 0;
      ncfnS1[is] = 0;
      nniS1[is]  = 0;
    }

  /* Sensitivities will be computed */
  IDA_mem->ida_sensi = TRUE;

  /* Sensitivity re-initialization was successfull */
  return(IDA_SUCCESS);
}

/*=================================================================*/
/*BEGIN        Readibility Constants                               */
/*=================================================================*/

#define rdata    (IDA_mem->ida_rdata)
#define maxord   (IDA_mem->ida_maxord)
#define mxstep   (IDA_mem->ida_mxstep)
#define hin      (IDA_mem->ida_hin)
#define hmax_inv (IDA_mem->ida_hmax_inv)
#define tstop    (IDA_mem->ida_tstop)
#define tstopset (IDA_mem->ida_tstopset)
#define epcon    (IDA_mem->ida_epcon)
#define maxnef   (IDA_mem->ida_maxnef)
#define maxncf   (IDA_mem->ida_maxncf)
#define maxcor   (IDA_mem->ida_maxcor)
#define id       (IDA_mem->ida_id)
#define suppressalg (IDA_mem->ida_suppressalg)
#define constraints (IDA_mem->ida_constraints)
#define res      (IDA_mem->ida_res)
#define y0       (IDA_mem->ida_y0)
#define yp0      (IDA_mem->ida_yp0)
#define itol     (IDA_mem->ida_itol)
#define reltol   (IDA_mem->ida_reltol)
#define abstol   (IDA_mem->ida_abstol)
#define uround   (IDA_mem->ida_uround)  
#define maxcol   (IDA_mem->ida_maxcol)
#define phi      (IDA_mem->ida_phi) 
#define ewt      (IDA_mem->ida_ewt)  
#define yy       (IDA_mem->ida_yy)
#define yp       (IDA_mem->ida_yp)
#define delta    (IDA_mem->ida_delta)
#define mm       (IDA_mem->ida_mm)
#define ee       (IDA_mem->ida_ee)
#define savres   (IDA_mem->ida_savres)
#define tempv1   (IDA_mem->ida_tempv1)
#define tempv2   (IDA_mem->ida_tempv2) 
#define kk       (IDA_mem->ida_kk)
#define hh       (IDA_mem->ida_hh)
#define h0u      (IDA_mem->ida_h0u)
#define tn       (IDA_mem->ida_tn)
#define tretp    (IDA_mem->ida_tretp)
#define cj       (IDA_mem->ida_cj)
#define cjold    (IDA_mem->ida_cjold)
#define cjratio  (IDA_mem->ida_cjratio)
#define cjlast   (IDA_mem->ida_cjlast)
#define nst      (IDA_mem->ida_nst)
#define nre      (IDA_mem->ida_nre)
#define ncfn     (IDA_mem->ida_ncfn)
#define netf     (IDA_mem->ida_netf)
#define nni      (IDA_mem->ida_nni)
#define nsetups  (IDA_mem->ida_nsetups)
#define ns       (IDA_mem->ida_ns)
#define lrw1     (IDA_mem->ida_lrw1)
#define liw1     (IDA_mem->ida_liw1)
#define lrw      (IDA_mem->ida_lrw)
#define liw      (IDA_mem->ida_liw)
#define linit    (IDA_mem->ida_linit)
#define lsetup   (IDA_mem->ida_lsetup)
#define lsolve   (IDA_mem->ida_lsolve) 
#define lperf    (IDA_mem->ida_lperf)
#define lfree    (IDA_mem->ida_lfree) 
#define lmem     (IDA_mem->ida_lmem) 
#define knew     (IDA_mem->ida_knew)
#define kused    (IDA_mem->ida_kused)          
#define hused    (IDA_mem->ida_hused)        
#define tolsf    (IDA_mem->ida_tolsf)
#define phase    (IDA_mem->ida_phase)
#define epsNewt  (IDA_mem->ida_epsNewt)
#define toldel   (IDA_mem->ida_toldel)
#define ss       (IDA_mem->ida_ss)
#define rr       (IDA_mem->ida_rr)
#define psi      (IDA_mem->ida_psi)
#define alpha    (IDA_mem->ida_alpha)
#define beta     (IDA_mem->ida_beta)
#define sigma    (IDA_mem->ida_sigma)
#define gamma    (IDA_mem->ida_gamma)
#define forceSetup     (IDA_mem->ida_forceSetup)
#define setupNonNull   (IDA_mem->ida_setupNonNull) 
#define constraintsSet (IDA_mem->ida_constraintsSet)

#define quad         (IDA_mem->ida_quad)
#define errconQ      (IDA_mem->ida_errconQ)
#define itolQ        (IDA_mem->ida_itolQ)
#define reltolQ      (IDA_mem->ida_reltolQ)
#define abstolQ      (IDA_mem->ida_abstolQ)
#define rdataQ       (IDA_mem->ida_rdataQ)
#define rhsQ         (IDA_mem->ida_rhsQ)
#define phiQ         (IDA_mem->ida_phiQ)
#define yyQ          (IDA_mem->ida_yyQ)
#define ypQ          (IDA_mem->ida_ypQ)
#define ewtQ         (IDA_mem->ida_ewtQ)
#define eeQ          (IDA_mem->ida_eeQ)
#define nrQe         (IDA_mem->ida_nrQe)
#define netfQ        (IDA_mem->ida_netfQ)
#define lrw1Q        (IDA_mem->ida_lrw1Q)
#define liw1Q        (IDA_mem->ida_liw1Q)

#define resS         (IDA_mem->ida_resS)
#define resS1        (IDA_mem->ida_resS1)
#define errconS      (IDA_mem->ida_errconS)
#define rhomax       (IDA_mem->ida_rhomax)
#define pbar         (IDA_mem->ida_pbar)
#define itolS        (IDA_mem->ida_itolS)
#define reltolS      (IDA_mem->ida_reltolS)
#define abstolS      (IDA_mem->ida_abstolS)
#define rdataS       (IDA_mem->ida_rdataS)
#define maxcorS      (IDA_mem->ida_maxcorS)
#define Ns           (IDA_mem->ida_Ns)
#define ism          (IDA_mem->ida_ism)
#define p            (IDA_mem->ida_p)
#define plist        (IDA_mem->ida_plist)
#define sensi        (IDA_mem->ida_sensi)
#define phiS         (IDA_mem->ida_phiS)
#define ewtS         (IDA_mem->ida_ewtS)
#define yyS          (IDA_mem->ida_yyS)
#define ypS          (IDA_mem->ida_ypS)
#define yyS1         (IDA_mem->ida_yyS1)
#define ypS1         (IDA_mem->ida_ypS1)
#define deltaS       (IDA_mem->ida_deltaS)
#define deltaS1      (IDA_mem->ida_deltaS1)
#define eeS          (IDA_mem->ida_eeS)
#define tmpS1        (IDA_mem->ida_tmpS1)
#define tmpS2        (IDA_mem->ida_tmpS2)
#define tmpS3        (IDA_mem->ida_tmpS3)
#define ssS          (IDA_mem->ida_ssS)
#define nrSe         (IDA_mem->ida_nrSe)
#define nreS         (IDA_mem->ida_nreS)
#define nniS         (IDA_mem->ida_nniS)
#define ncfnS        (IDA_mem->ida_ncfnS)
#define netfS        (IDA_mem->ida_netfS)
#define nsetupsS     (IDA_mem->ida_nsetupsS)
#define abstolSalloc (IDA_mem->ida_abstolSalloc)
#define stgr1alloc   (IDA_mem->ida_stgr1alloc)

/*=================================================================*/
/*END          Readibility Constants                               */
/*=================================================================*/

/********************* IDASolve ***********************************

 This routine is the main driver of the IDA package. 

 It integrates over an independent variable interval defined by the user, 
 by calling IDAStep to take internal independent variable steps.

 The first time that IDASolve is called for a successfully initialized
 problem, it computes a tentative initial step size.

 IDASolve supports four modes, specified by itask:
    IDA_NORMAL,  IDA_ONE_STEP,  IDA_NORMAL_TSTOP,  and  IDA_ONE_STEP_TSTOP.
 In the IDA_NORMAL and IDA_NORMAL-TSTOP modes, the solver steps until it 
 passes tout and then interpolates to obtain y(tout) and yp(tout).
 In the IDA_ONE_STEP and IDA_ONE_STEP_TSTOP modes, it takes one internal step
 and returns.  In the IDA_NORMAL_TSTOP and IDA_ONE_STEP_TSTOP modes, it also
 takes steps so as to reach tstop exactly and never to go past it.

 IDASolve returns integer values corresponding to success and failure as below:

 successful returns: 
 
  IDA_SUCCESS        
  IDA_TSTOP_RETURN   

 failed returns:
 
  IDA_ILL_INPUT
  IDA_TOO_MUCH_WORK
  IDA_MEM_NULL
  IDA_TOO_MUCH_ACC
  IDA_CONV_FAIL
  IDA_LSETUP_FAIL
  IDA_LSOLVE_FAIL    
  IDA_CONSTR_FAIL
  IDA_ERR_FAIL   
  IDA_REP_RES_ERR
  IDA_RES_FAIL

********************************************************************/

int IDASolve(void *ida_mem, realtype tout, realtype *tret,
             N_Vector yret, N_Vector ypret, int itask)
{
  long int nstloc;
  int is, sflag, istate, ier;
  realtype tdist, troundoff, ypnorm, rh, nrm;
  booleantype istop, ewtsetOK, ewtQsetOK, ewtSsetOK;
  IDAMem IDA_mem;

  /* Check for legal inputs in all cases. */

  if (ida_mem == NULL) {
    fprintf(stderr, MSG_IDA_NO_MEM);
    return(IDA_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem;

  /* Check if problem was malloc'ed */
  
  if (IDA_mem->ida_MallocDone == FALSE) {
    if(errfp!=NULL) fprintf(errfp, MSG_NO_MALLOC);
    return(IDA_NO_MALLOC);
  }

  /* Check for legal arguments */

  if (yret == NULL) {
    if(errfp!=NULL) fprintf(errfp, MSG_YRET_NULL);       
    return(IDA_ILL_INPUT);
  }
  yy = yret;  

  if (ypret == NULL) {
    if(errfp!=NULL) fprintf(errfp, MSG_YPRET_NULL);       
    return(IDA_ILL_INPUT);
  }
  yp = ypret;
  
  if (tret == NULL) {
    if(errfp!=NULL) fprintf(errfp, MSG_TRET_NULL);
    return(IDA_ILL_INPUT);
  }
  *tret = tretp = tn; /* Set tret now in case of illegal-input return. */

  if ((itask != IDA_NORMAL)       && 
      (itask != IDA_ONE_STEP)     &&
      (itask != IDA_NORMAL_TSTOP) &&
      (itask != IDA_ONE_STEP_TSTOP) ) {
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_ITASK, itask);
    return(IDA_ILL_INPUT);
  }
  
  if ( (itask == IDA_NORMAL_TSTOP) || (itask == IDA_ONE_STEP_TSTOP) ) {
    if ( tstopset == FALSE ) {
      if(errfp!=NULL) fprintf(errfp, MSG_NO_TSTOP);
      return(IDA_ILL_INPUT);
    }
    istop = TRUE;
  } else {
    istop = FALSE;
  }

  if (nst == 0) {       /* THIS IS THE FIRST CALL */

    /* Check inputs to IDA for correctness and consistency */

    if (IDA_mem->ida_SetupDone == FALSE) {
      ier = IDAInitialSetup(IDA_mem);
      if(ier != IDA_SUCCESS) return(IDA_ILL_INPUT);
      IDA_mem->ida_SetupDone = TRUE;
    }

    /* Evaluate quadrature rhs and set phiQ[1] */

    if (quad) {
      rhsQ(tn, phi[0], phi[1], phiQ[1], rdataQ);
      nrQe++;
    }

    /* On the first call, check for tout - tn too small,
       set initial hh,
       check for approach to tstop, and scale phi[1] by hh. */

    tdist = ABS(tout - tn);
    troundoff = TWO*uround*(ABS(tn) + ABS(tout));    
    if (tdist < troundoff) {
      if(errfp!=NULL) fprintf(errfp, MSG_TOO_CLOSE, tout, tn);
      return(IDA_ILL_INPUT);
    }

    hh = hin;
    if ( (hh != ZERO) && ((tout-tn)*hh < ZERO) ) {
      if(errfp!=NULL) fprintf(errfp, MSG_BAD_HINIT, hh, tout-tn);
      return(IDA_ILL_INPUT);
    }

    if (hh == ZERO) {
      hh = PT001*tdist;
      ypnorm = IDAWrmsNorm(IDA_mem, phi[1], ewt, suppressalg);
      if (errconQ)
        ypnorm = IDAQuadWrmsNormUpdate(ypnorm, phiQ[1], ewtQ);
      if (errconS)
        ypnorm = IDASensWrmsNormUpdate(IDA_mem, ypnorm, phiS[1], ewtS, suppressalg);
      if (ypnorm > HALF/hh) hh = HALF/ypnorm;
      if(tout < tn) hh = -hh;
    }

    rh = ABS(hh)*hmax_inv;
    if (rh > ONE) hh /= rh;

    /* On first call, check for approach to tstop */

    if(istop) {
      if ( (tstop - tn)*hh < ZERO) {
        if(errfp!=NULL) fprintf(errfp, MSG_BAD_TSTOP, tstop, tn);
        return(IDA_ILL_INPUT);
      }
      if ( (tn + hh - tstop)*hh > ZERO) 
        hh = tstop - tn;
    }

    h0u = hh;

    N_VScale(hh, phi[1], phi[1]);

    if (quad)
      N_VScale(hh, phiQ[1], phiQ[1]);

    if (sensi)
      for (is=0; is<Ns; is++)
        N_VScale(hh, phiS[1][is], phiS[1][is]);

    kk = 0; 
    kused = 0;  /* set in case of an error return before a step */

    /* Set the convergence test constants epsNewt and toldel */

    epsNewt = epcon;
    toldel = PT0001 * epsNewt;

  } /* end of first-call block. */

  /* Call lperf function and set nstloc for later performance testing. */

  if (lperf != NULL) lperf(IDA_mem, 0);
  nstloc = 0;

  /* If not the first call, check for stop conditions. */

  if (nst > 0) {
    istate = IDAStopTest1(IDA_mem, tout, tret, yret, ypret, itask);
    if (istate != CONTINUE_STEPS) return(istate);
  }

  /* Looping point for internal steps. */

  loop {
   
    /* Check for too many steps taken. */
    
    if (nstloc >= mxstep) {
      if(errfp!=NULL) fprintf(errfp, MSG_MAX_STEPS, tn, mxstep, tout);
      istate = IDA_TOO_MUCH_WORK;
      *tret = tretp = tn;
      break; /* Here yy=yret and yp=ypret already have the current solution. */
    }

    /* Call lperf to generate warnings of poor performance. */

    if (lperf != NULL) lperf(IDA_mem, 1);

    /* Reset and check ewt (if not first call). */

    if (nst > 0) {

      ewtsetOK = IDAEwtSet(IDA_mem, phi[0]);

      if (errconQ)
        ewtQsetOK = IDAQuadEwtSet(IDA_mem, phiQ[0]);
      else
        ewtQsetOK = TRUE;

      if (sensi)
        ewtSsetOK = IDASensEwtSet(IDA_mem, phiS[0]);
      else
        ewtSsetOK = TRUE;

      if ( (!ewtsetOK) || (!ewtQsetOK) || (!ewtSsetOK) ) {

        if(!ewtsetOK)  if(errfp!=NULL) fprintf(errfp, MSG_EWT_NOW_BAD, tn);
        if(!ewtQsetOK) if(errfp!=NULL) fprintf(errfp, MSG_EWTQ_NOW_BAD, tn);
        if(!ewtSsetOK) if(errfp!=NULL) fprintf(errfp, MSG_EWTS_NOW_BAD, tn);

        istate = IDA_ILL_INPUT;
        ier = IDAGetSolution(IDA_mem, tn, yret, ypret);
        *tret = tretp = tn;
        break;

      }

    }
    
    /* Check for too much accuracy requested. */
    
    nrm = IDAWrmsNorm(IDA_mem, phi[0], ewt, suppressalg);
    if (errconQ)
      nrm = IDAQuadWrmsNormUpdate(nrm, phiQ[0], ewtQ); 
    if (errconS)
      nrm = IDASensWrmsNormUpdate(IDA_mem, nrm, phiS[0], ewtS, suppressalg);

    tolsf = uround * nrm;

    if (tolsf > ONE) {
      tolsf *= TEN;
      if(errfp!=NULL) fprintf(errfp, MSG_TOO_MUCH_ACC, tn);
      istate = IDA_TOO_MUCH_ACC;
      *tret = tretp = tn;
      if (nst > 0) ier = IDAGetSolution(IDA_mem, tn, yret, ypret);
      break;
    }

    /* Call IDAStep to take a step. */

    sflag = IDAStep(IDA_mem);

    /* Process all failed-step cases, and exit loop. */
   
    if (sflag != IDA_SUCCESS) {
      istate = IDAHandleFailure(IDA_mem, sflag);
      *tret = tretp = tn;
      ier = IDAGetSolution(IDA_mem, tn, yret, ypret);
      break;
    }
    
    nstloc++;

    /* After successful step, check for stop conditions; continue or break. */

    istate = IDAStopTest2(IDA_mem, tout, tret, yret, ypret, itask);
    if (istate != CONTINUE_STEPS) break;

  } /* End of step loop */

  return(istate);    
}

/*------------------  IDAGetSolution  -----------------------------*/
/* 
   This routine evaluates y(t) and y'(t) as the value and derivative of 
   the interpolating polynomial at the independent variable t, and stores
   the results in the vectors yret and ypret.  It uses the current
   independent variable value, tn, and the method order last used, kused.
   This function is called by IDASolve with t = tout, t = tn, or t = tstop.
   
   If kused = 0 (no step has been taken), or if t = tn, then the order used
   here is taken to be 1, giving yret = phi[0], ypret = phi[1]/psi[0].
   
   The return values are:
     IDA_SUCCESS  if t is legal, or
     IDA_BAD_T    if t is not within the interval of the last step taken.
*/
/*-----------------------------------------------------------------*/

int IDAGetSolution(void *ida_mem, realtype t, 
                   N_Vector yret, N_Vector ypret)
{
  IDAMem IDA_mem;
  realtype tfuzz, tp, delt, c, d, gam;
  int j, kord;

  if (ida_mem == NULL) {
    fprintf(stderr, MSG_IDAG_NO_MEM);
    return (IDA_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem; 

  /* Check t for legality.  Here tn - hused is t_{n-1}. */
 
  tfuzz = HUNDRED * uround * (tn + hh);
  tp = tn - hused - tfuzz;
  if ( (t - tp)*hh < ZERO) {
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_T, t, tn-hused, tn);
    return(IDA_BAD_T);
  }

  /* Initialize yret = phi[0], ypret = 0, and kord = (kused or 1). */

  N_VScale (ONE, phi[0], yret);
  N_VConst (ZERO, ypret);
  kord = kused; 
  if (kused == 0) kord = 1;

  /* Accumulate multiples of columns phi[j] into yret and ypret. */
  
  delt = t - tn;
  c = ONE; d = ZERO;
  gam = delt/psi[0];
  for (j=1; j <= kord; j++) {
    d = d*gam + c/psi[j-1];
    c = c*gam;
    gam = (delt + psi[j-1])/psi[j];
    N_VLinearSum(ONE,  yret, c, phi[j], yret);
    N_VLinearSum(ONE, ypret, d, phi[j], ypret);
  }

  return(IDA_SUCCESS);
}

/*-----------------------------------------------------------------*/
/*
  The following function can be called to obtain the quadrature 
  variables after a successful integration step.                 
  
  Return values are similar to those of IDAGetSolution.         
  Additionally, IDAGetQuad can return IDAG_NO_QUAD if quadratures
  were not computed.                                             
*/
/*-----------------------------------------------------------------*/

int IDAGetQuad(void *ida_mem, realtype t, N_Vector yretQ)
{
  IDAMem IDA_mem;
  realtype tfuzz, tp, delt, c, gam;
  int j, kord;

  if (ida_mem == NULL) {
    fprintf(stderr, MSG_IDAG_NO_MEM);
    return (IDA_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem; 

  if(quad != TRUE) {
    if(errfp!=NULL) fprintf(errfp, MSG_IDAG_NO_QUAD);
    return (IDA_NO_QUAD);
  }

  /* Check t for legality.  Here tn - hused is t_{n-1}. */
 
  tfuzz = HUNDRED * uround * (tn + hh);
  tp = tn - hused - tfuzz;
  if ( (t - tp)*hh < ZERO) {
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_T, t, tn-hused, tn);
    return(IDA_BAD_T);
  }

  /* Initialize yretQ = phiQ[0] and kord = (kused or 1). */

  N_VScale (ONE, phiQ[0], yretQ);
  kord = kused; 
  if (kused == 0) kord = 1;
  /*if (kused == 0 || t == tn) kord = 1;*/

  /* Accumulate multiples of columns phiQ[j] into yretQ */

  delt = t - tn;
  c = ONE;
  gam = delt/psi[0];
  for (j=1; j <= kord; j++) {
    c = c*gam;
    gam = (delt + psi[j-1])/psi[j];
    N_VLinearSum(ONE,  yretQ, c, phiQ[j], yretQ);
  }

  return(IDA_SUCCESS);
}

/*-----------------------------------------------------------------*/
/*
  IDAGetSens1 returns sensitivities of the y function at
  the time t. The arguments yretS and ypretS must be pointers to 
  arrays of N_Vector's and must be allocated by the user.
  
  Return values are similar to those of IDAGetSolution.
  Additionally, IDAGetSens can return IDAG_NO_SENS if
  sensitivities were not computed.
*/
/*-----------------------------------------------------------------*/

int IDAGetSens(void *ida_mem, realtype t, 
               N_Vector *yretS, N_Vector *ypretS)
{
  IDAMem IDA_mem;
  int is, retval;

  if (ida_mem == NULL) {
    fprintf(stderr, MSG_IDAG_NO_MEM);
    return (IDA_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem; 

  for (is=0; is<Ns; is++) {
    retval = IDAGetSens1(ida_mem, t, is, yretS[is], ypretS[is]);
    if (retval != IDA_SUCCESS) return(retval);
  }
  return(IDA_SUCCESS);
}

/*-----------------------------------------------------------------*/
/*
  IDAGetSens1 returns the is-th sensitivity of the y function at
  the time t. The arguments yretS and ypretS must be pointers to 
  N_Vector's and must be allocated by the user.
  
  Return values are similar to those of IDAGetSolution.
  Additionally, IDAGetSens1 can return IDA_NO_SENS if
  sensitivities were not computed and IDA_BAD_IS if 
  is < 0 or is >= Ns.
*/
/*-----------------------------------------------------------------*/

int IDAGetSens1(void *ida_mem, realtype t, int is, 
                N_Vector yretS, N_Vector ypretS)
{
  IDAMem IDA_mem;
  realtype tfuzz, tp, delt, c, d, gam;
  int j, kord;

  if (ida_mem == NULL) {
    fprintf(stderr, MSG_IDAG_NO_MEM);
    return (IDA_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem; 

  if(sensi != TRUE) {
    if(errfp!=NULL) fprintf(errfp, MSG_IDAG_NO_SENS);
    return (IDA_NO_SENS);
  }

  if ( (is<0) || (is>=Ns) ) {
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_IS, is);
    return(IDA_BAD_IS);
  }

  /* Check t for legality.  Here tn - hused is t_{n-1}. */
 
  tfuzz = HUNDRED * uround * (tn + hh);
  tp = tn - hused - tfuzz;
  if ( (t - tp)*hh < ZERO) {
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_T, t, tn-hused, tn);
    return(IDA_BAD_T);
  }

  /* Initialize yretS = phiS[0][is], ypretS = 0, and kord = (kused or 1). */

  N_VScale(ONE, phiS[0][is], yretS);
  N_VConst(ZERO, ypretS);
  kord = kused; 
  if (kused == 0) kord = 1;
  /*if (kused == 0 || t == tn) kord = 1;*/

  /* Accumulate multiples of columns phiS[j][is] into yretS and ypretS. */

  delt = t - tn;
  c = ONE; d = ZERO;
  gam = delt/psi[0];
  for (j=1; j <= kord; j++) {
    d = d*gam + c/psi[j-1];
    c = c*gam;
    gam = (delt + psi[j-1])/psi[j];
    N_VLinearSum(ONE,  yretS, c, phiS[j][is], yretS);
    N_VLinearSum(ONE, ypretS, d, phiS[j][is], ypretS);
  }

  return(IDA_SUCCESS);  
}

/********************* IDAFree **********************************

 This routine frees the problem memory allocated by IDAMalloc
 Such memory includes all the vectors allocated by IDAAllocVectors,
 and the memory lmem for the linear solver (deallocated by a call
 to lfree).

*******************************************************************/

void IDAFree(void *ida_mem)
{
  IDAMem IDA_mem;

  if (ida_mem == NULL) return;

  IDA_mem = (IDAMem) ida_mem;
  
  IDAFreeVectors(IDA_mem);

  IDAQuadFree(ida_mem);

  IDASensFree(ida_mem);

  if (lfree != NULL) lfree(IDA_mem);

  free(IDA_mem);
}

/*------------------  IDAQuadFree      --------------------------*/
/*
  IDAQuadFree frees the problem memory in ida_mem allocated
  for quadrature integration. Its only argument is the pointer
  ida_mem returned by IDACreate. 
*/
/*-----------------------------------------------------------------*/

void IDAQuadFree(void *ida_mem)
{
  IDAMem IDA_mem;
  
  if (ida_mem == NULL) return;
  IDA_mem = (IDAMem) ida_mem;

  if(quad) {
    IDAQuadFreeVectors(IDA_mem);
    quad = FALSE;
  }
}

/*------------------  IDASensFree      --------------------------*/
/*
  IDASensFree frees the problem memory in ida_mem allocated
  for sensitivity analysis. Its only argument is the pointer
  ida_mem returned by IDACreate. 
*/
/*-----------------------------------------------------------------*/

void IDASensFree(void *ida_mem)
{
  IDAMem IDA_mem;
  
  if (ida_mem == NULL) return;
  IDA_mem = (IDAMem) ida_mem;

  if(sensi) {
    if (abstolSalloc) 
      IDASensFreeAtol(IDA_mem, abstolS);
    if (stgr1alloc) {
      free(netfS1);
      free(ncfnS1);
      free(nniS1);
    }
    IDASensFreeVectors(IDA_mem);
    sensi = FALSE;
  }
}

/*=================================================================*/
/*END          EXPORTED FUNCTIONS IMPLEMENTATION                   */
/*=================================================================*/

/*=================================================================*/
/*BEGIN        PRIVATE FUNCTIONS IMPLEMENTATION                    */
/*=================================================================*/
 
/****************** IDACheckNvector **********************************
 This routine checks if all required vector operations are present.
 If any of them is missing it returns FALSE.
**********************************************************************/

static booleantype IDACheckNvector(N_Vector tmpl)
{
  if((tmpl->ops->nvclone        == NULL) ||
     (tmpl->ops->nvdestroy      == NULL) ||
     (tmpl->ops->nvlinearsum    == NULL) ||
     (tmpl->ops->nvconst        == NULL) ||
     (tmpl->ops->nvprod         == NULL) ||
     (tmpl->ops->nvscale        == NULL) ||
     (tmpl->ops->nvabs          == NULL) ||
     (tmpl->ops->nvinv          == NULL) ||
     (tmpl->ops->nvaddconst     == NULL) ||
     (tmpl->ops->nvwrmsnorm     == NULL) ||
     (tmpl->ops->nvmin          == NULL))
    return(FALSE);
  else
    return(TRUE);
}

/****************** IDAAllocVectors ***********************************

 This routine allocates the IDA vectors ewt, tempv1, tempv2, and
 phi[0], ..., phi[maxord]. If all memory allocations are successful,
 IDAAllocVectors returns TRUE. Otherwise all allocated memory is freed
 and IDAAllocVectors returns FALSE.
 This routine also updates the optional outputs lrw and liw, which are
 (respectively) the lengths of the real and integer work spaces
 allocated here.

**********************************************************************/

static booleantype IDAAllocVectors(IDAMem IDA_mem, N_Vector tmpl)
{
  int i, j;

  /* Allocate ewt, ee, delta, tempv1, tempv2 */
  
  ewt = N_VClone(tmpl);
  if (ewt == NULL) return(FALSE);

  ee = N_VClone(tmpl);
  if (ee == NULL) {
    N_VDestroy(ewt);
    return(FALSE);
  }
  delta = N_VClone(tmpl);
  if (delta == NULL) {
    N_VDestroy(ewt);
    N_VDestroy(ee);
    return(FALSE);
  }
  tempv1 = N_VClone(tmpl);
  if (tempv1 == NULL) {
    N_VDestroy(ewt);
    N_VDestroy(ee);
    N_VDestroy(delta);
    return(FALSE);
  }
  tempv2= N_VClone(tmpl);
  if (tempv2 == NULL) {
    N_VDestroy(ewt);
    N_VDestroy(ee);
    N_VDestroy(delta);
    N_VDestroy(tempv1);
    return(FALSE);
  }

  savres = tempv1;

  /* Allocate phi[0] ... phi[maxord].  Make sure phi[2] and phi[3] are
  allocated (for use as temporary vectors), regardless of maxord.       */

  maxcol = MAX(maxord,3);
  for (j=0; j <= maxcol; j++) {
    phi[j] = N_VClone(tmpl);
    if (phi[j] == NULL) {
      N_VDestroy(ewt);
      N_VDestroy(ee);
      N_VDestroy(delta);
      N_VDestroy(tempv1);
      N_VDestroy(tempv2);
      for (i=0; i < j; i++) N_VDestroy(phi[i]);
      return(FALSE);
    }
  }

  /* Set solver workspace lengths  */

  lrw += (maxcol + 6) * lrw1;
  liw += (maxcol + 6) * liw1;

  return(TRUE);
}


/*---------------------- IDAFreeVectors  --------------------------*/
/*
  This routine frees the IDA vectors allocated in IDAAllocVectors.
*/
/*-----------------------------------------------------------------*/

static void IDAFreeVectors(IDAMem IDA_mem)
{
  int j;
  
  N_VDestroy(ewt);
  N_VDestroy(ee);
  N_VDestroy(delta);
  N_VDestroy(tempv1);
  N_VDestroy(tempv2);
  for(j=0; j <= maxcol; j++) N_VDestroy(phi[j]);
}

/*------------------ IDAQuadAllocVectors  --------------------------*/
/*
  NOTE: Space for ewtQ is allocated even when errconQ=FALSE, 
  although in this case, ewtQ is never used. The reason for this
  decision is to allow the user to re-initialize the quadrature
  computation with errconQ=TRUE, after an initialization with
  errconQ=FALSE, without new memory allocation within 
  IDAQuadReInit.
*/
/*-----------------------------------------------------------------*/

static booleantype IDAQuadAllocVectors(IDAMem IDA_mem, N_Vector tmpl)
{
  int i, j;

  /* Allocate yyQ */
  yyQ = N_VClone(tmpl);
  if (yyQ == NULL) {
    return (FALSE);
  }

  /* Allocate ypQ */
  ypQ = N_VClone(tmpl);
  if (ypQ == NULL) {
    N_VDestroy(yyQ);
    return (FALSE);
  }

  /* Allocate ewtQ */
  ewtQ = N_VClone(tmpl);
  if (ewtQ == NULL) {
    N_VDestroy(yyQ);
    N_VDestroy(ypQ);
    return (FALSE);
  }

  /* Allocate eeQ */
  eeQ = N_VClone(tmpl);
  if (eeQ == NULL) {
    N_VDestroy(yyQ);
    N_VDestroy(ypQ);
    N_VDestroy(ewtQ);
    return (FALSE);
  }

  for (j=0; j <= maxord; j++) {
    phiQ[j] = N_VClone(tmpl);
    if (phiQ[j] == NULL) {
      N_VDestroy(yyQ);
      N_VDestroy(ypQ);
      N_VDestroy(ewtQ);
      N_VDestroy(eeQ);
      for (i=0; i < j; i++) N_VDestroy(phiQ[i]);
      return(FALSE);
    }
  }

  lrw += (maxord+5)*lrw1Q;
  liw += (maxord+5)*liw1Q;

  return(TRUE);
}

/*------------------ IDAQuadFreeVectors  --------------------------*/
/*-----------------------------------------------------------------*/

static void IDAQuadFreeVectors(IDAMem IDA_mem)
{
  int j;

  N_VDestroy(yyQ);
  N_VDestroy(ypQ);
  N_VDestroy(ewtQ);
  N_VDestroy(eeQ);
  for(j=0; j <= maxord; j++) N_VDestroy(phiQ[j]);
}

/*------------------ IDASensAllocVectors  -------------------------*/
/*-----------------------------------------------------------------*/

static booleantype IDASensAllocVectors(IDAMem IDA_mem, N_Vector tmpl)
{
  int i, j;
  
  tmpS1 = tempv1;
  tmpS2 = tempv2;

  tmpS3 = N_VClone(tmpl);
  if (tmpS3==NULL) {
    return(FALSE);
  }
  
  ewtS = N_VCloneVectorArray(Ns, tmpl);
  if (ewtS==NULL) {
    N_VDestroy(tmpS3);
    return(FALSE);
  }

  eeS = N_VCloneVectorArray(Ns, tmpl);
  if (eeS==NULL) {
    N_VDestroy(tmpS3);
    N_VDestroyVectorArray(ewtS, Ns);
    return(FALSE);
  }

  for (j=0; j <= maxcol; j++) {
    phiS[j] = N_VCloneVectorArray(Ns, tmpl);
    if (phiS[j] == NULL) {
      N_VDestroy(tmpS3);
      N_VDestroyVectorArray(ewtS, Ns);
      N_VDestroyVectorArray(eeS, Ns);
      for (i=0; i < j; i++) N_VDestroyVectorArray(phiS[i], Ns);
      return(FALSE);
    }
  }

  lrw += (maxcol+3)*Ns*lrw1 + lrw1;
  liw += (maxcol+3)*Ns*liw1 + liw1;
  
  switch (ism) {

  case IDA_SIMULTANEOUS:
  case IDA_STAGGERED:

    yyS = N_VCloneVectorArray(Ns, tmpl);
    if (yyS==NULL) {
      for (j=0; j<=maxcol; j++) N_VDestroyVectorArray(phiS[j], Ns);
      N_VDestroyVectorArray(eeS, Ns);
      N_VDestroyVectorArray(ewtS, Ns);
      N_VDestroy(tmpS3);
      return(FALSE);
    }

    ypS = N_VCloneVectorArray(Ns, tmpl);
    if (ypS==NULL) {
      N_VDestroyVectorArray(yyS, Ns);
      for (j=0; j<=maxcol; j++) N_VDestroyVectorArray(phiS[j], Ns);
      N_VDestroyVectorArray(eeS, Ns);
      N_VDestroyVectorArray(ewtS, Ns);
      N_VDestroy(tmpS3);
      return(FALSE);
    }

    deltaS = N_VCloneVectorArray(Ns, tmpl);
    if (deltaS==NULL) {
      N_VDestroyVectorArray(ypS, Ns);
      N_VDestroyVectorArray(yyS, Ns);
      for (j=0; j<=maxcol; j++) N_VDestroyVectorArray(phiS[j], Ns);
      N_VDestroyVectorArray(eeS, Ns);
      N_VDestroyVectorArray(ewtS, Ns);
      N_VDestroy(tmpS3);
      return(FALSE);
    }

    lrw += 3*Ns*lrw1;
    liw += 3*Ns*liw1;

    break;

  case IDA_STAGGERED1:

    yyS1 = N_VClone(tmpl);
    if (yyS1==NULL) {
      for (j=0; j<=maxcol; j++) N_VDestroyVectorArray(phiS[j], Ns);
      N_VDestroyVectorArray(eeS, Ns);
      N_VDestroyVectorArray(ewtS, Ns);
      N_VDestroy(tmpS3);
      return(FALSE);
    }

    ypS1 = N_VClone(tmpl);
    if (ypS1==NULL) {
      N_VDestroy(yyS1);
      for (j=0; j<=maxcol; j++) N_VDestroyVectorArray(phiS[j], Ns);
      N_VDestroyVectorArray(eeS, Ns);
      N_VDestroyVectorArray(ewtS, Ns);
      N_VDestroy(tmpS3);
      return(FALSE);
    }

    deltaS1 = N_VClone(tmpl);
    if (deltaS1==NULL) {
      N_VDestroy(ypS1);
      N_VDestroy(yyS1);
      for (j=0; j<=maxcol; j++) N_VDestroyVectorArray(phiS[j], Ns);
      N_VDestroyVectorArray(eeS, Ns);
      N_VDestroyVectorArray(ewtS, Ns);
      N_VDestroy(tmpS3);
      return(FALSE);
    }

    lrw += 3*lrw1;
    liw += 3*liw1;

    break;

  }

  return(TRUE);
}

/*------------------ IDASensFreeVectors  --------------------------*/
/*-----------------------------------------------------------------*/

static void IDASensFreeVectors(IDAMem IDA_mem)
{
  int j;

  switch (ism) {
  case IDA_SIMULTANEOUS:
  case IDA_STAGGERED:
    N_VDestroyVectorArray(deltaS, Ns);
    N_VDestroyVectorArray(ypS, Ns);
    N_VDestroyVectorArray(yyS, Ns);
    break;
  case IDA_STAGGERED1:
    N_VDestroy(deltaS1);
    N_VDestroy(ypS1);
    N_VDestroy(yyS1);
    break;
  }
  for (j=0; j<=maxcol; j++) N_VDestroyVectorArray(phiS[j], Ns);
  N_VDestroyVectorArray(eeS, Ns);
  N_VDestroyVectorArray(ewtS, Ns);
  N_VDestroy(tmpS3);
}

/********************** IDAInitialSetup *********************************

 This routine is called by IDASolve once at the first step. It performs
 all checks on optional inputs and inputs to IDAMalloc/IDAReInit that
 could not be done before.

 If no error is encountered, IDAInitialSetup returns IDA_SUCCESS. Otherwise,
 it returns an error flag and prints a message to errfp.
*************************************************************************/

int IDAInitialSetup(IDAMem IDA_mem)
{
  realtype temptest;
  booleantype allocOK, ewtsetOK, conOK, neg_abstol, tolsetOK;
  int ier;
  
  /* Test for more vector operations, depending on options */
  
  if (suppressalg)
    if (id->ops->nvwrmsnormmask == NULL) {
      if(errfp!=NULL) fprintf(errfp, MSG_BAD_NVECTOR);
      return(IDA_ILL_INPUT);
    }
  
  if (constraints != NULL)
    if (constraints->ops->nvdiv         == NULL ||
        constraints->ops->nvmaxnorm     == NULL ||
        constraints->ops->nvcompare     == NULL ||
        constraints->ops->nvconstrmask  == NULL ||
        constraints->ops->nvminquotient == NULL) {
      if(errfp!=NULL) fprintf(errfp, MSG_BAD_NVECTOR);
      return(IDA_ILL_INPUT);
    }

  /* Test id vector for legality */
  if(suppressalg && (id==NULL)){ 
    if(errfp!=NULL) fprintf(errfp, MSG_MISSING_ID); 
    return(IDA_ILL_INPUT); 
  }

  /* Load ewt */
  ewtsetOK = IDAEwtSet(IDA_mem, y0);
  if (!ewtsetOK) {
    if(errfp!=NULL) fprintf(errfp, MSG_BAD_EWT);
    return(IDA_ILL_INPUT);
  }

  /*  Check the constraints pointer and vector */
  if (constraints == NULL) constraintsSet = FALSE;
  else {
    constraintsSet = TRUE;
    temptest = N_VMaxNorm(constraints);
    if(temptest > TWOPT5){ 
      if(errfp!=NULL) fprintf(errfp, MSG_BAD_CONSTRAINTS); 
      return(IDA_ILL_INPUT); 
    } else if(temptest < HALF) constraintsSet = FALSE; /* constraints empty */
  }

  /* Check to see if y0 satisfies constraints. */
  if (constraintsSet) {
    conOK = N_VConstrMask(constraints, y0, tempv2);
    if (!conOK) { 
      if(errfp!=NULL) fprintf(errfp, MSG_Y0_FAIL_CONSTR); 
      return(IDA_ILL_INPUT); 
    }
  }

  /* Quadrature related set-up */
  if (quad && errconQ) {

    if ( (reltolQ == NULL) || (abstolQ == NULL) ) {
      if(errfp!=NULL) fprintf(errfp, MSG_NO_QUADTOL);
      return(IDA_ILL_INPUT);
    }

    if (*reltolQ < ZERO) {
      if(errfp!=NULL) fprintf(errfp, MSG_BAD_RELTOLQ, *reltolQ);
      return(IDA_ILL_INPUT);
    }

    if (itolQ == IDA_SS) {
      neg_abstol = (*((realtype *)abstolQ) < ZERO);
    } else {
      neg_abstol = (N_VMin((N_Vector)abstolQ) < ZERO);
    }
    if (neg_abstol) {
      if(errfp!=NULL) fprintf(errfp, MSG_BAD_ABSTOLQ);
      return(IDA_ILL_INPUT);
    }

    ewtsetOK = IDAQuadEwtSet(IDA_mem, phiQ[0]);
    if (!ewtsetOK) {
      if(errfp!=NULL) fprintf(errfp, MSG_BAD_EWTQ);
      return (IDA_ILL_INPUT);
    }
  }

  if (!quad) errconQ = FALSE;

  /* Sensitivity related set-up */
  if (sensi) {

    /* Check if ism and iresS agree */
    if ((ism==IDA_STAGGERED1) && (iresS==IDA_ALLSENS)) {
      if(errfp!=NULL) fprintf(errfp, MSG_BAD_ISM_IRESS);
      return (IDA_ILL_INPUT);
    } 

    /* If pbar is needed, check if it is NULL */
    if( (abstolS==NULL) || (resSDQ==TRUE) )
      if (pbar==NULL) {
        if(errfp!=NULL) fprintf(errfp, MSG_PBAR_NULL);
        return(IDA_ILL_INPUT);
      }

    /* Check if reltolS is non-null and legal */
    if (reltolS != NULL) {
      if (*reltolS<ZERO) {
        if(errfp!=NULL) fprintf(errfp, MSG_BAD_RELTOLS, *reltolS);
        return(IDA_ILL_INPUT);
      }
    } else {
      reltolS = reltol;
    }
    
    /* Check if abstolS is non-null and legal */
    if (abstolS != NULL) {
      abstolSalloc = FALSE;
      neg_abstol = IDASensTestAtol(IDA_mem, abstolS);
      if (neg_abstol) {
        if(errfp!=NULL) fprintf(errfp, MSG_BAD_ABSTOLS);
        return(IDA_ILL_INPUT);
      }
    } else {
      abstolSalloc = TRUE;
      itolS = itol;
      allocOK = IDASensAllocAtol(IDA_mem, &abstolS);
      if (!allocOK) {
        if(errfp!=NULL) fprintf(errfp, MSG_IDAIS_MEM_FAIL);
        return(IDA_ILL_INPUT);
      }
      tolsetOK = IDASensSetAtol(IDA_mem, abstolS);
      if (!tolsetOK) {
        if(errfp!=NULL) fprintf(errfp, MSG_BAD_PBAR);
        return(IDA_ILL_INPUT);
      }
    }

    /* Load ewtS */
    ewtsetOK = IDASensEwtSet(IDA_mem, phiS[0]);
    if (!ewtsetOK) {
      if(errfp!=NULL) fprintf(errfp, MSG_BAD_EWTS);
      return (IDA_ILL_INPUT);
    }

  }

  if (!sensi) errconS = FALSE;
  
  /* Check that lsolve exists and call linit function if it exists. */

  if (lsolve == NULL) {
    if(errfp!=NULL) fprintf(errfp, MSG_LSOLVE_NULL);
    return(IDA_ILL_INPUT);
  }

  if (linit != NULL) {
    ier = linit(IDA_mem);
    if (ier < 0) {
      if(errfp!=NULL) fprintf(errfp, MSG_LINIT_FAIL);
      return(IDA_LINIT_FAIL);
    }
  }

  return(IDA_SUCCESS);

}

/*********************** IDAEwtSet **************************************
  
 This routine is responsible for loading the error weight vector
 ewt, according to itol, as follows:
 (1) ewt[i] = 1 / (*rtol * ABS(ycur[i]) + *atol), i=0,...,Neq-1
     if itol = IDA_SS
 (2) ewt[i] = 1 / (*rtol * ABS(ycur[i]) + atol[i]), i=0,...,Neq-1
     if itol = IDA_SV

  IDAEwtSet returns TRUE if ewt is successfully set as above to a
  positive vector and FALSE otherwise. In the latter case, ewt is
  considered undefined after the FALSE return from IDAEwtSet.

  All the real work is done in the routines IDAEwtSetSS, IDAEwtSetSV.
 
***********************************************************************/

booleantype IDAEwtSet(IDAMem IDA_mem, N_Vector ycur)
{
  booleantype ewtsetOK=TRUE;

  switch(itol) {
  case IDA_SS: 
    ewtsetOK = IDAEwtSetSS(IDA_mem, ycur); 
    break;
  case IDA_SV: 
    ewtsetOK = IDAEwtSetSV(IDA_mem, ycur); 
    break;
  }
  return(ewtsetOK);
}

/*-------------------- IDAEwtSetSS --------------------------------*/
/*
  This routine sets ewt as decribed above in the case itol=IDA_SS.
  It tests for non-positive components before inverting. IDAEwtSetSS
  returns TRUE if ewt is successfully set to a positive vector
  and FALSE otherwise. In the latter case, ewt is considered
  undefined after the FALSE return from IDAEwtSetSS.
*/
/*-----------------------------------------------------------------*/

static booleantype IDAEwtSetSS(IDAMem IDA_mem, N_Vector ycur)
{
  realtype rtoli, *atoli;
  
  rtoli = *reltol;
  atoli = (realtype *)abstol;

  N_VAbs(ycur, tempv1);
  N_VScale(rtoli, tempv1, tempv1);
  N_VAddConst(tempv1, *atoli, tempv1);
  if (N_VMin(tempv1) <= ZERO) return(FALSE);
  N_VInv(tempv1, ewt);

  return(TRUE);
}

/*-------------------- IDAEwtSetSV --------------------------------*/
/*
  This routine sets ewt as decribed above in the case itol=IDA_SV.
  It tests for non-positive components before inverting. IDAEwtSetSV
  returns TRUE if ewt is successfully set to a positive vector
  and FALSE otherwise. In the latter case, ewt is considered
  undefined after the FALSE return from IDAEwtSetSV.
*/
/*-----------------------------------------------------------------*/

static booleantype IDAEwtSetSV(IDAMem IDA_mem, N_Vector ycur)
{
  realtype rtoli;
  N_Vector atoli;
  
  rtoli = *reltol;

  atoli = (N_Vector)abstol;
  N_VAbs(ycur, tempv1);
  N_VLinearSum(rtoli, tempv1, ONE, atoli, tempv1);
  if (N_VMin(tempv1) <= ZERO) return(FALSE);
  N_VInv(tempv1, ewt);

  return(TRUE);
}

/*------------------  IDAQuadEwtSet      --------------------------*/
/*-----------------------------------------------------------------*/

static booleantype IDAQuadEwtSet(IDAMem IDA_mem, N_Vector qcur)
{
  booleantype ewtsetOK = TRUE;

  switch (itolQ) {
  case IDA_SS: 
    ewtsetOK = IDAQuadEwtSetSS(IDA_mem, qcur);
    break;
  case IDA_SV: 
    ewtsetOK = IDAQuadEwtSetSV(IDA_mem, qcur);
    break;
  }
  return(ewtsetOK);
}

/*------------------  IDAQuadEwtSetSS    --------------------------*/
/*-----------------------------------------------------------------*/

static booleantype IDAQuadEwtSetSS(IDAMem IDA_mem, N_Vector qcur)
{
  realtype rtoli, atoli;
  N_Vector tempv;
  
  rtoli = *reltolQ;
  atoli = *((realtype *)abstolQ);

  /* Use ypQ as temporary storage */
  tempv = ypQ;

  N_VAbs(qcur, tempv);
  N_VScale(rtoli, tempv, tempv);
  N_VAddConst(tempv, atoli, tempv);
  if (N_VMin(tempv) <= ZERO) return (FALSE);
  N_VInv(tempv, ewtQ);

  return(TRUE);
}

/*------------------  IDAQuadEwtSetSV    --------------------------*/
/*-----------------------------------------------------------------*/

static booleantype IDAQuadEwtSetSV(IDAMem IDA_mem, N_Vector qcur)
{
  realtype rtoli;
  N_Vector tempv;
  
  rtoli = *reltolQ;

  /* Use ypQ as temporary storage */
  tempv = ypQ;

  N_VAbs(qcur, tempv);
  N_VLinearSum(rtoli, tempv, ONE, (N_Vector)abstolQ, tempv);
  if (N_VMin(tempv) <= ZERO) return (FALSE);
  N_VInv(tempv, ewtQ);

  return(TRUE);
}

/*-------------------- IDASensTestAtol  --------------------------*/
/*-----------------------------------------------------------------*/

static booleantype IDASensTestAtol(IDAMem IDA_mem, void *atolS)
{
  int is;
  realtype *atolSS;
  N_Vector *atolSV;
  
  switch (itolS) {
  case IDA_SS:
    atolSS = (realtype *)atolS;
    for (is=0; is<Ns; is++)
      if (atolSS[is] < ZERO) return (TRUE);
    break;
  case IDA_SV:
    atolSV = (N_Vector *)atolS;
    for (is=0; is<Ns; is++) 
      if (N_VMin(atolSV[is]) < ZERO) return (TRUE);
    break;
  }

  return(FALSE);
}

/* 
 * IDASensAllocAtol
 * Allocate space for the forward sensitivity absolute tolerances
 * If needed, use the N_Vector 'tempv1' as a template
 */

static booleantype IDASensAllocAtol(IDAMem IDA_mem, void **atolSPtr)
{
  switch (itolS) {
  case IDA_SS:
    *atolSPtr = (void *)malloc(Ns*sizeof(realtype));
    break;
  case IDA_SV:
    *atolSPtr = (void *)N_VCloneVectorArray(Ns, tempv1);
    break;
  }
  
  if (*atolSPtr==NULL) return (FALSE);
  else                 return (TRUE);
}

/*--------------------- IDASensFreeAtol  --------------------------*/
/*-----------------------------------------------------------------*/

static void IDASensFreeAtol(IDAMem IDA_mem, void *atolS)
{
  switch (itolS) {
  case IDA_SS:
    free((realtype*)atolS);
    break;
  case IDA_SV:
    N_VDestroyVectorArray((N_Vector *)atolS, Ns);
    break;
  }
}

/*--------------------- IDASensSetAtol   --------------------------*/
/*-----------------------------------------------------------------*/

static booleantype IDASensSetAtol(IDAMem IDA_mem, void *atolS)
{
  booleantype flag=TRUE;
  
  switch (itolS) {
  case IDA_SS: 
    flag = IDASensSetAtolSS(IDA_mem, (realtype *)atolS);
    break;
  case IDA_SV: 
    flag = IDASensSetAtolSV(IDA_mem, (N_Vector *)atolS);
    break;
  }
  
  return(flag);
}

/*--------------------- IDASensSetAtolSS --------------------------*/
/*-----------------------------------------------------------------*/

static booleantype IDASensSetAtolSS(IDAMem IDA_mem, realtype *atolS)
{
  int is, which;
  realtype rpbar;
  
  for (is=0; is<Ns; is++) {
    if (plist!=NULL) which = abs(plist[is]) - 1; 
    else             which = is;
    if (pbar[which]==ZERO) return (FALSE);
    rpbar = ONE/ABS(pbar[which]);
    atolS[is] = *((realtype *)abstol) * rpbar;
  }
  
  return (TRUE);
}

/*--------------------- IDASensSetAtolSV --------------------------*/
/*-----------------------------------------------------------------*/

static booleantype IDASensSetAtolSV(IDAMem IDA_mem, N_Vector *atolS)
{
  int is, which;
  realtype rpbar;
  
  for (is=0; is<Ns; is++) {
    if (plist!=NULL) which = abs(plist[is]) - 1;
    else             which = is;
    if (pbar[which]==ZERO) return (FALSE);
    rpbar = ONE/ABS(pbar[which]);
    N_VScale(rpbar, (N_Vector)abstol, atolS[is]);
  }
  
  return (TRUE);
}

/*--------------------- IDASensEwtSet    --------------------------*/
/*-----------------------------------------------------------------*/

booleantype IDASensEwtSet(IDAMem IDA_mem, N_Vector *yScur)
{
  booleantype ewtsetOK=TRUE;

  switch(itolS) {
  case IDA_SS: 
    ewtsetOK = IDASensEwtSetSS(IDA_mem, yScur); 
    break;
  case IDA_SV: 
    ewtsetOK = IDASensEwtSetSV(IDA_mem, yScur); 
    break;
  }
  return(ewtsetOK);
}

/*--------------------- IDASensEwtSetSS  --------------------------*/
/*-----------------------------------------------------------------*/

static booleantype IDASensEwtSetSS(IDAMem IDA_mem, N_Vector *yScur)
{
  int is;
  realtype rtoli, atoli;
  
  for (is=0; is<Ns; is++) {
    rtoli = *reltolS;
    atoli = ((realtype *)abstolS)[is];
    N_VAbs(yScur[is], tempv1);
    N_VScale(rtoli, tempv1, tempv1);
    N_VAddConst(tempv1, atoli, tempv1);
    if (N_VMin(tempv1) <= ZERO) return (FALSE);
    N_VInv(tempv1, ewtS[is]);
  }
  return (TRUE);
}

/*--------------------- IDASensEwtSetSV  --------------------------*/
/*-----------------------------------------------------------------*/

static booleantype IDASensEwtSetSV(IDAMem IDA_mem, N_Vector *yScur)
{
  int is;
  realtype rtoli;
  
  for (is=0; is<Ns; is++) {
    rtoli = *reltolS;
    N_VAbs(yScur[is], tempv1);
    N_VLinearSum(rtoli, tempv1, ONE, ((N_Vector *)abstolS)[is], tempv1);
    if (N_VMin(tempv1) <= ZERO) return (FALSE);
    N_VInv(tempv1, ewtS[is]);
  }
  return (TRUE);
}


/********************* IDAStopTest1 ********************************

 This routine tests for stop conditions before taking a step.
 The tests depend on the value of itask.
 The variable tretp is the previously returned value of tret.

 The return values are:
   CONTINUE_STEPS     if no stop conditions were found
   IDA_SUCCESS        for a normal return to the user
   IDA_TSTOP_RETURN   for a tstop-reached return to the user
   IDA_ILL_INPUT      for an illegal-input return to the user 

 In the tstop cases, this routine may adjust the stepsize hh to cause
 the next step to reach tstop exactly.

********************************************************************/

static int IDAStopTest1(IDAMem IDA_mem, realtype tout, realtype *tret, 
                        N_Vector yret, N_Vector ypret, int itask)
{

  int ier;
  realtype troundoff;

  switch (itask) {
    
  case IDA_NORMAL:  
    /* Test for tout = tretp, and for tn past tout. */
    if (tout == tretp) {
      *tret = tretp = tout;
      return(IDA_SUCCESS);
    }
    if ( (tn - tout)*hh >= ZERO) {
      ier = IDAGetSolution(IDA_mem, tout, yret, ypret);
      if (ier != IDA_SUCCESS) {
        if(errfp!=NULL) fprintf(errfp,MSG_BAD_TOUT, tout);
        return(IDA_ILL_INPUT);
      }
      *tret = tretp = tout;
      return(IDA_SUCCESS);
    }
    return(CONTINUE_STEPS);
    
  case IDA_ONE_STEP:
    /* Test for tn past tretp. */
    if ( (tn - tretp)*hh > ZERO) {
      ier = IDAGetSolution(IDA_mem, tn, yret, ypret);
      *tret = tretp = tn;
      return(IDA_SUCCESS);
    }
    return(CONTINUE_STEPS);
    
  case IDA_NORMAL_TSTOP:
    /* Test for tn past tstop, tn = tretp, tn past tout, tn near tstop. */
    if ( (tn - tstop)*hh > ZERO) {
      if(errfp!=NULL) fprintf(errfp, MSG_BAD_TSTOP, tstop, tn);
      return(IDA_ILL_INPUT);
    }
    if (tout == tretp) {
      *tret = tretp = tout;
      return(IDA_SUCCESS);
    }
    if ( (tn - tout)*hh >= ZERO) {
      ier = IDAGetSolution(IDA_mem, tout, yret, ypret);
      if (ier != IDA_SUCCESS) {
        if(errfp!=NULL) fprintf(errfp, MSG_BAD_TOUT, tout);
        return(IDA_ILL_INPUT);
      }
      *tret = tretp = tout;
      return(IDA_SUCCESS);
    }
    troundoff = HUNDRED*uround*(ABS(tn) + ABS(hh));
    if ( ABS(tn - tstop) <= troundoff) {
      ier = IDAGetSolution(IDA_mem, tstop, yret, ypret);
      if (ier != IDA_SUCCESS) {
        if(errfp!=NULL) fprintf(errfp, MSG_BAD_TSTOP, tstop, tn);
        return(IDA_ILL_INPUT);
      }
      *tret = tretp = tstop;
      return(IDA_TSTOP_RETURN);
    }
    if ( (tn + hh - tstop)*hh > ZERO) hh = tstop - tn;
    return(CONTINUE_STEPS);
    
  case IDA_ONE_STEP_TSTOP:
    /* Test for tn past tstop, tn past tretp, and tn near tstop. */
    if ( (tn - tstop)*hh > ZERO) {
      if(errfp!=NULL) fprintf(errfp, MSG_BAD_TSTOP, tstop, tn);
      return(IDA_ILL_INPUT);
    }
    if ( (tn - tretp)*hh > ZERO) {
      ier = IDAGetSolution(IDA_mem, tn, yret, ypret);
      *tret = tretp = tn;
      return(IDA_SUCCESS);
    }
    troundoff = HUNDRED*uround*(ABS(tn) + ABS(hh));
    if ( ABS(tn - tstop) <= troundoff) {
      ier = IDAGetSolution(IDA_mem, tstop, yret, ypret);
      if (ier != IDA_SUCCESS) {
        if(errfp!=NULL) fprintf(errfp, MSG_BAD_TSTOP, tstop, tn);
        return(IDA_ILL_INPUT);
      }
      *tret = tretp = tstop;
      return(IDA_TSTOP_RETURN);
    }
    if ( (tn + hh - tstop)*hh > ZERO) hh = tstop - tn;
    return(CONTINUE_STEPS);
    
  }
  return -99;
}

/********************* IDAStopTest2 ********************************

 This routine tests for stop conditions after taking a step.
 The tests depend on the value of itask.

 The return values are:
   CONTINUE_STEPS     if no stop conditions were found
   IDA_SUCCESS        for a normal return to the user
   IDA_TSTOP_RETURN   for a tstop-reached return to the user

 In the two cases with tstop, this routine may reset the stepsize hh
 to cause the next step to reach tstop exactly.

 In the two cases with IDA_ONE_STEP mode, no interpolation to tn is needed
 because yret and ypret already contain the current y and y' values.

 Note: No test is made for an error return from IDAGetSolution here,
 because the same test was made prior to the step.

********************************************************************/

static int IDAStopTest2(IDAMem IDA_mem, realtype tout, realtype *tret, 
                        N_Vector yret, N_Vector ypret, int itask)
{

  int ier;
  realtype troundoff;

  switch (itask) {

    case IDA_NORMAL:  
      /* Test for tn past tout. */
      if ( (tn - tout)*hh >= ZERO) {
        ier = IDAGetSolution(IDA_mem, tout, yret, ypret);
        *tret = tretp = tout;
        return(IDA_SUCCESS);
      }
      return(CONTINUE_STEPS);

    case IDA_ONE_STEP:
      *tret = tretp = tn;
      return(IDA_SUCCESS);

    case IDA_NORMAL_TSTOP:
      /* Test for tn at tstop, for tn past tout, and for tn near tstop. */
      troundoff = HUNDRED*uround*(ABS(tn) + ABS(hh));
      if ( ABS(tn - tstop) <= troundoff) {
        ier = IDAGetSolution(IDA_mem, tstop, yret, ypret);
        *tret = tretp = tstop;
        return(IDA_TSTOP_RETURN);
      }
      if ( (tn - tout)*hh >= ZERO) {
        ier = IDAGetSolution(IDA_mem, tout, yret, ypret);
        *tret = tretp = tout;
        return(IDA_SUCCESS);
      }
      if ( (tn + hh - tstop)*hh > ZERO) hh = tstop - tn;
      return(CONTINUE_STEPS);

    case IDA_ONE_STEP_TSTOP:
      /* Test for tn at tstop. */
      troundoff = HUNDRED*uround*(ABS(tn) + ABS(hh));
      if ( ABS(tn - tstop) <= troundoff) {
        ier = IDAGetSolution(IDA_mem, tstop, yret, ypret);
        *tret = tretp = tstop;
        return(IDA_TSTOP_RETURN);
      }
      if ( (tn + hh - tstop)*hh > ZERO) hh = tstop - tn;
      *tret = tretp = tn;
      return(IDA_SUCCESS);

  }
  return -99;
}


/****************** IDAHandleFailure ******************************

 This routine prints error messages for all cases of failure by
 IDAStep.  It returns to IDASolve the value that it is to return to
 the user.

*****************************************************************/

static int IDAHandleFailure(IDAMem IDA_mem, int sflag)
{

  /* Depending on sflag, print error message and return error flag */
  switch (sflag) {

  case IDA_ERR_FAIL: 
    if(errfp!=NULL) fprintf(errfp, MSG_ERR_FAILS, tn, hh);
    return(IDA_ERR_FAIL);
    
  case IDA_CONV_FAIL:
    if(errfp!=NULL) fprintf(errfp, MSG_CONV_FAILS, tn, hh);
    return(IDA_CONV_FAIL);
    
  case IDA_LSETUP_FAIL:
    if(errfp!=NULL) fprintf(errfp, MSG_SETUP_FAILED, tn);
    return(IDA_LSETUP_FAIL);
    
  case IDA_LSOLVE_FAIL:
    if(errfp!=NULL) fprintf(errfp, MSG_SOLVE_FAILED, tn);
    return(IDA_LSOLVE_FAIL);
    
  case IDA_REP_RES_ERR:
    if(errfp!=NULL) fprintf(errfp, MSG_REP_RES_ERR, tn);
    return(IDA_REP_RES_ERR);
    
  case IDA_RES_FAIL:
    if(errfp!=NULL) fprintf(errfp, MSG_RES_NONRECOV, tn);
    return(IDA_RES_FAIL);
    
  case IDA_CONSTR_FAIL:
    if(errfp!=NULL) fprintf(errfp, MSG_FAILED_CONSTR, tn);
    return(IDA_CONSTR_FAIL);
    
  }
  return -99;
}

/********************* IDAStep **************************************
 
 This routine performs one internal IDA step, from tn to tn + hh.
 It calls other routines to do all the work.

 It solves a system of differential/algebraic equations of the form
       F(t,y,y') = 0, for one step. In IDA, tt is used for t,
 yy is used for y, and yp is used for y'. The function F is supplied as 'res'
 by the user.

 The methods used are modified divided difference, fixed leading 
 coefficient forms of backward differentiation formulas.
 The code adjusts the stepsize and order to control the local error per step.

 The main operations done here are as follows:
  * initialize various quantities;
  * setting of multistep method coefficients;
  * solution of the nonlinear system for yy at t = tn + hh;
  * deciding on order reduction and testing the local error;
  * attempting to recover from failure in nonlinear solver or error test;
  * resetting stepsize and order for the next step.
  * updating phi and other state data if successful;

 On a failure in the nonlinear system solution or error test, the
 step may be reattempted, depending on the nature of the failure.

 Variables or arrays (all in the IDAMem structure) used in IDAStep are:

 tt -- Independent variable.
 yy -- Solution vector at tt.
 yp -- Derivative of solution vector after successful stelp.
 res -- User-supplied function to evaluate the residual. See the 
        description given in file ida.h .
 lsetup -- Routine to prepare for the linear solver call. It may either
        save or recalculate quantities used by lsolve. (Optional)
 lsolve -- Routine to solve a linear system. A prior call to lsetup
        may be required. 
 hh  -- Appropriate step size for next step.
 ewt -- Vector of weights used in all convergence tests.
 phi -- Array of divided differences used by IDAStep. This array is composed 
       of  (maxord+1) nvectors (each of size Neq). (maxord+1) is the maximum 
       order for the problem, maxord, plus 1.
 
       Return values are:
       IDA_SUCCESS   IDA_RES_FAIL        IDA_LSETUP_FAIL
                     IDA_LSOLVE_FAIL     IDA_ERR_FAIL            
                     IDA_CONSTR_FAIL     IDA_CONV_FAIL          
                     IDA_REP_RES_ERR            

********************************************************************/

static int IDAStep(IDAMem IDA_mem)
{
  realtype saved_t, ck;
  realtype est, erk, erkm1;
  realtype estQ, erQk, erQkm1;
  realtype estS, erSk, erSkm1;
  realtype error_k, error_km1;
  int ncf, nef;
  int nflag, kflag;
  booleantype sensi_stgr, sensi_stgr1;
  int is;

  saved_t = tn;

  sensi_stgr  = (sensi && (ism==IDA_STAGGERED));
  sensi_stgr1 = (sensi && (ism==IDA_STAGGERED1));

  ncf = nef = 0;

  if(nst == ZERO){
    kk = 1;
    kused = 0;
    hused = ZERO;
    psi[0] = hh;
    cj = ONE/hh;
    phase = 0;
    ns = 0;
  }
  
  /* Looping point for attempts to take a step */

  loop {  

    /*-----------------------
      Set method coefficients
      -----------------------*/

    IDASetCoeffs(IDA_mem, &ck);

    kflag = IDA_SUCCESS;
    
    /*-----------------------
      Advance state variables
      -----------------------*/

    nflag = IDANls(IDA_mem);

    if (nflag == IDA_SUCCESS) {
      nflag = IDATestError(IDA_mem, ck, &est, &erk, &erkm1);
      error_k   = erk;
      error_km1 = erkm1;
    }

    if (nflag != IDA_SUCCESS) {

      /* there was a failure; restore and decide what to do */
      IDARestore(IDA_mem, saved_t);
      kflag = IDAHandleNFlag(IDA_mem, nflag, &ncf, &nef, est);

      /* exit on nonrecoverable failure */ 
      if (kflag != PREDICT_AGAIN) return(kflag);

      /* recoverable failure; predict again */
      if(nst==0) IDAReset(IDA_mem);
      continue;

    } /* end nflag != IDA_SUCCESS */

    /*----------------------------
      Advance quadrature variables 
      ----------------------------*/

    if (quad) {

      /* Predict: load yyQ and ypQ */
      IDAQuadPredict(IDA_mem);

      /* Compute correction eeQ */
      rhsQ(tn, yy, yp, eeQ, rdataQ);
      nrQe++;
      N_VLinearSum(ONE, eeQ, -ONE, ypQ, eeQ);
      N_VScale(ONE/cj, eeQ, eeQ);

      /* Apply correction: yyQ = yyQ + eeQ */
      N_VLinearSum(ONE, yyQ, ONE, eeQ, yyQ);

      nflag = IDA_SUCCESS;

      if (errconQ) {
        nflag = IDAQuadTestError(IDA_mem, ck, &estQ, &erQk, &erQkm1);
        if (erQk   > error_k)   error_k   = erQk;
        if (erQkm1 > error_km1) error_km1 = erQkm1;
      }

      if (nflag != IDA_SUCCESS) {

        /* there was a failure; restore and decide what to do */
        IDARestore(IDA_mem, saved_t);
        kflag = IDAHandleNFlag(IDA_mem, nflag, &ncf, &nef, estQ);
        
        /* exit on nonrecoverable failure */ 
        if (kflag != PREDICT_AGAIN) return(kflag);

        /* recoverable failure; predict again */
        if(nst==0) IDAReset(IDA_mem);
        continue;

      } /* end nflag != IDA_SUCCESS */

    } /* end quad */

    /*---------------------------------
      Advance sensitivities (IDA_STAGGERED)
      --------------------------------*/

    if (sensi_stgr) {

      
      res(tn, yy, yp, delta, rdata);
      nre++;
      nreS++;

      nflag = IDAStgrNls(IDA_mem);

      if ( (nflag == IDA_SUCCESS) && errconS ) {
        nflag = IDAStgrTestError(IDA_mem, ck, &estS, &erSk, &erSkm1);
        if (erSk   > error_k)   error_k   = erSk;
        if (erSkm1 > error_km1) error_km1 = erSkm1;
      }

      if (nflag != IDA_SUCCESS) {

        /* there was a failure; restore and decide what to do */
        IDARestore(IDA_mem, saved_t);
        kflag = IDAHandleNFlag(IDA_mem, nflag, &ncf, &nef, estS);

        /* exit on nonrecoverable failure */ 
        if (kflag != PREDICT_AGAIN) return(kflag);

        /* recoverable failure; predict again */
        if(nst==0) IDAReset(IDA_mem);
        continue;

      } /* end nflag != IDA_SUCCESS */

    } /* end sensi_stgr */

    /*----------------------------------
      Advance sensitivities (IDA_STAGGERED1) 
      ----------------------------------*/

    if (sensi_stgr1) {

      res(tn, yy, yp, delta, rdata);
      nre++;
      nreS++;

      for ( is=0; is<Ns; is++ ) {
        
        nflag = IDAStgr1Nls(IDA_mem, is);

        if ( (nflag == IDA_SUCCESS) && errconS ) {
          nflag = IDAStgr1TestError(IDA_mem, is, ck, &estS, &erSk, &erSkm1);
          if (erSk   > error_k)   error_k   = erSk;
          if (erSkm1 > error_km1) error_km1 = erSkm1;
        }

        if (nflag != IDA_SUCCESS) {
          
          /* there was a failure; restore and decide what to do */
          IDARestore(IDA_mem, saved_t);
          kflag = IDAHandleNFlag(IDA_mem, nflag, &ncf, &nef, estS);
          
          /* exit on nonrecoverable failure */ 
          if (kflag != PREDICT_AGAIN) return(kflag);
          
          /* recoverable failure; exit the for loop and predict again */
          break;
          
        } /* end nflag != IDA_SUCCESS */
        
      } /* end for is */

      if ((nflag != IDA_SUCCESS) && (kflag == PREDICT_AGAIN)) {
        if(nst==0) IDAReset(IDA_mem);
        continue;
      }

    } /* end sensi_stgr1 */

    /*-----------------------------------
      Everything went fine; exit the loop 
      -----------------------------------*/

    break;
  }

  /* Nonlinear system solve and error test were both successful;
     update data, and consider change of step and/or order       */

  IDACompleteStep(IDA_mem, error_k, error_km1);

  return(IDA_SUCCESS);
}


/*-------------------- IDASetCoeffs -------------------------------*/
/*
  This routine computes the coefficients relevant to the current step.
  The counter ns counts the number of consecutive steps taken at
  constant stepsize h and order k, up to a maximum of k + 2.
  Then the first ns components of beta will be one, and on a step  
  with ns = k + 2, the coefficients alpha, etc. need not be reset here.
  Also, IDACompleteStep prohibits an order increase until ns = k + 2.
*/
/*-----------------------------------------------------------------*/

static void IDASetCoeffs(IDAMem IDA_mem, realtype *ck)
{
  int i, is;
  realtype temp1, temp2, alpha0, alphas;
  
  /* Set coefficients for the current stepsize h */
  
  if(hh != hused || kk != kused) ns = 0;
  ns = MIN(ns+1,kused+2);
  if(kk+1 >= ns){
    beta[0] = ONE;
    alpha[0] = ONE;
    temp1 = hh;
    gamma[0] = ZERO;
    sigma[0] = ONE;
    for(i=1;i<=kk;i++){
      temp2 = psi[i-1];
      psi[i-1] = temp1;
      beta[i] = beta[i-1] * psi[i-1] / temp2;
      temp1 = temp2 + hh;
      alpha[i] = hh / temp1;
      sigma[i] = i * sigma[i-1] * alpha[i]; 
      gamma[i] = gamma[i-1] + alpha[i-1] / hh;
    }
    psi[kk] = temp1;
  }

  /* compute alphas, alpha0 */
  alphas = ZERO;
  alpha0 = ZERO;
  for(i=0;i<kk;i++){
    alphas = alphas - ONE/(i+1);
    alpha0 = alpha0 - alpha[i];
  }
  
  /* compute leading coefficient cj  */
  cjlast = cj;
  cj = -alphas/hh;
  
  /* compute variable stepsize error coefficient ck */
  
  *ck = ABS(alpha[kk] + alphas - alpha0);
  *ck = MAX(*ck, alpha[kk]);
  
  /* change phi to phi-star  */
  
  for(i=ns;i<=kk;i++) 
    N_VScale(beta[i], phi[i], phi[i]);

  if (quad)
    for(i=ns;i<=kk;i++)
      N_VScale(beta[i], phiQ[i], phiQ[i]);

  if (sensi)
    for(is=0;is<Ns;is++)
      for(i=ns;i<=kk;i++)
        N_VScale(beta[i], phiS[i][is], phiS[i][is]);

  /* update independent variable */
  
  tn = tn + hh;
}


/*----------------------------- IDANls ----------------------------*/
/*
  This routine attempts to solve the nonlinear system using the linear
  solver specified. NOTE: this routine uses N_Vector ee as the scratch
  vector tempv3 passed to lsetup.
  
  Possible return values:

  IDA_SUCCESS

  IDA_RES_RECVR       IDA_RES_FAIL
  IDA_LSETUP_RECVR    IDA_LSETUP_FAIL
  IDA_LSOLVE_RECVR    IDA_LSOLVE_FAIL

  IDA_CONSTR_RECVR
  IDA_NCONV_RECVR
*/
/*-----------------------------------------------------------------*/

static int IDANls(IDAMem IDA_mem)
{
  int retval, is;
  booleantype constraintsPassed, callSetup, tryAgain;
  realtype temp1, temp2, vnorm;
  N_Vector tempv3;
  booleantype sensi_sim;

  sensi_sim = (sensi && (ism==IDA_SIMULTANEOUS));

  callSetup = FALSE;

  /* Initialize if the first time called */

  if(nst == 0){
    cjold = cj;
    IDASetSS(IDA_mem, TWENTY);
    if(setupNonNull) callSetup = TRUE;
  }

  mm = tempv2;
  tempv3 = ee;

  /* Decide if lsetup is to be called */

  if(setupNonNull){
    cjratio = cj / cjold;
    temp1 = (ONE - XRATE) / (ONE + XRATE);
    temp2 = ONE/temp1;
    if(cjratio < temp1 || cjratio > temp2) callSetup = TRUE;
    if(forceSetup) callSetup = TRUE;
    if(cj != cjlast) IDASetSS(IDA_mem, HUNDRED);
  }

  /* Begin the main loop. This loop is traversed at most twice. 
     The second pass only occurs when the first pass had a recoverable
     failure with old Jacobian data */

  loop{

    /* Compute predicted values for yy and yp, and compute residual there. */
    IDAPredict(IDA_mem);
    retval = res(tn, yy, yp, delta, rdata);
    nre++;
    if(retval != IDA_SUCCESS) break;

    if (sensi_sim) {
      for(is=0;is<Ns;is++)
        IDASensPredict(IDA_mem, is, yyS[is], ypS[is]);
      retval = IDASensRes(IDA_mem, tn, yy, yp, delta, 
                          yyS, ypS, deltaS,
                          tmpS1, tmpS2, tmpS3);
      if(retval != IDA_SUCCESS) break;
    }

    /* If indicated, call linear solver setup function and reset parameters. */
    if(callSetup){
      retval = lsetup(IDA_mem, yy, yp, delta, tempv1, tempv2, tempv3);
      nsetups++;
      forceSetup = FALSE;
      cjold = cj;
      cjratio = ONE;
      IDASetSS(IDA_mem, TWENTY);
      if (retval < 0) return(IDA_LSETUP_FAIL);
      if (retval > 0) return(IDA_LSETUP_RECVR);
    }

    /* Call the Newton iteration routine.  */

    retval = IDANewtonIter(IDA_mem);


    /* Retry the current step on recoverable failure with old Jacobian data. */

    tryAgain = (retval>0) && (setupNonNull) && (!callSetup);

    if(tryAgain){
      callSetup = TRUE;
      continue;
    }
    else break;

  }  /* end of loop */

  if(retval != IDA_SUCCESS) return(retval);

  /* If otherwise successful, check and enforce inequality constraints. */

  if(constraintsSet){  /* Check constraints and get mask vector mm, 
                          set where constraints failed */
    constraintsPassed = N_VConstrMask(constraints,yy,mm);
    if(constraintsPassed) return(IDA_SUCCESS);
    else {
      N_VCompare(ONEPT5, constraints, tempv1);  
      /* a , where a[i] =1. when |c[i]| = 2 ,  c the vector of constraints */
      N_VProd(tempv1, constraints, tempv1);             /* a * c                 */
      N_VDiv(tempv1, ewt, tempv1);                      /* a * c * wt            */
      N_VLinearSum(ONE, yy, -PT1, tempv1, tempv1);      /* y - 0.1 * a * c * wt  */
      N_VProd(tempv1, mm, tempv1);                      /*  v = mm*(y-.1*a*c*wt) */
      vnorm = IDAWrmsNorm(IDA_mem, tempv1, ewt, FALSE); /* ||v||                 */
      
      /* If vector v of constraint corrections is small
         in norm, correct and accept this step */      
      if(vnorm <= epsNewt){  
        N_VLinearSum(ONE, ee, -ONE, tempv1, ee);  /* ee <- ee - v */
        return(IDA_SUCCESS);
      }
      else {
        /* Constraints not met -- reduce h by computing rr = h'/h */
        N_VLinearSum(ONE, phi[0], -ONE, yy, tempv1);
        N_VProd(mm, tempv1, tempv1);
        rr = PT9*N_VMinQuotient(phi[0], tempv1);
        rr = MAX(rr,PT1);
        return(IDA_CONSTR_RECVR);
      }
    }
  }

  return(IDA_SUCCESS);
}

/*-------------------- IDAStgrNls ---------------------------------*/
/*
 This routine attempts to solve, one by one, all the sensitivity 
 linear systems using nonlinear iterations and the linear solver 
 specified (IDA_STAGGERED approach).

  Possible return values:

  IDA_SUCCESS

  IDA_RES_RECVR  IDA_RES_FAIL
  IDA_LSETUP_RECVR  IDA_LSETUP_FAIL
  IDA_LSOLVE_RECVR  IDA_LSOLVE_FAIL

  IDA_NCONV_RECVR
*/
/*-----------------------------------------------------------------*/

static int IDAStgrNls(IDAMem IDA_mem)
{
  booleantype callSetup, tryAgain;
  int is, retval;

  callSetup = FALSE;

  /* Begin the main loop. This loop is traversed at most twice. 
     The second pass only occurs when the first pass had a recoverable
     failure with old Jacobian data */
  loop{

    for(is=0;is<Ns;is++)
      IDASensPredict(IDA_mem, is, yyS[is], ypS[is]);

    /* Sensitivity residuals at predicted sensitivities -> in deltaS */
    retval = IDASensRes(IDA_mem, tn, yy, yp, delta, 
                        yyS, ypS, deltaS,
                        tmpS1, tmpS2, tmpS3);
    if (retval != IDA_SUCCESS) break;

    /* If indicated, call the linear solver setup function */
    if (callSetup) {
      retval = lsetup(IDA_mem, yy, yp, delta, tmpS1, tmpS2, tmpS3);
      nsetupsS++;
      cjold = cj;
      cjratio = ONE;
      IDASetSS(IDA_mem, TWENTY);
      if(retval != IDA_SUCCESS) break;
    }

    /* Call the Newton iteration routine */
    retval = IDAStgrNewtonIter(IDA_mem);

    /* Retry the current step on recoverable failure with old Jacobian data */
    tryAgain = (retval>0) && (setupNonNull) && (!callSetup);

    if (tryAgain) {
      callSetup = TRUE;
      continue;
    }
    else break;

  }

  if (retval != IDA_SUCCESS) 
    ncfnS++;

  return(retval);

}

/*-------------------- IDAStgr1Nls --------------------------------*/
/*
 This routine attempts to solve the is-th sensitivity linear system
 using nonlinear iterations and the linear solver specified 
 (IDA_STAGGERED1 approach).

  Possible return values:

  IDA_SUCCESS

  IDA_RES_RECVR  IDA_RES_FAIL
  IDA_LSETUP_RECVR  IDA_LSETUP_FAIL
  IDA_LSOLVE_RECVR  IDA_LSOLVE_FAIL

  IDA_NCONV_RECVR
*/
/*-----------------------------------------------------------------*/

static int IDAStgr1Nls(IDAMem IDA_mem, int is)
{
  booleantype callSetup, tryAgain;
  int retval;

  callSetup = FALSE;

  /* Begin the main loop. This loop is traversed at most twice. 
     The second pass only occurs when the first pass had a recoverable
     failure with old Jacobian data */
  loop{

    IDASensPredict(IDA_mem, is, yyS1, ypS1);

    /* Sensitivity residual at predicted sensitivities -> in deltaS1 */
    retval = IDASensRes1(IDA_mem, tn, yy, yp, delta, 
                         is,
                         yyS1, ypS1, deltaS1,
                         tmpS1, tmpS2, tmpS3);
    if(retval != IDA_SUCCESS) break;

    /* If indicated, call the linear solver setup function */
    if (callSetup) {
      retval = lsetup(IDA_mem, yy, yp, delta, tmpS1, tmpS2, tmpS3);
      nsetupsS++;
      cjold = cj;
      cjratio = ONE;
      IDASetSS(IDA_mem, TWENTY);
      if(retval != IDA_SUCCESS) break;
    }
    

    /* Call the Newton iteration routine */
    retval = IDAStgr1NewtonIter(IDA_mem, is);

    /* Retry the current step on recoverable failure with old Jacobian data */
    tryAgain = (retval>0) && (setupNonNull) && (!callSetup);

    if (tryAgain) {
      callSetup = TRUE;
      continue;
    }
    else break;

  }

  if (retval != IDA_SUCCESS) {
    ncfnS++;
    ncfnS1[is]++;
  }

  return(retval);

}

/*-------------------- IDASetSS -----------------------------------*/
/*
  This routine sets the 'ss' scalars used in the Newton convergence 
  tests. Value is TWENTY (at the first step or after Jacobian data
  update) or HUNDRED (on a step with changed cj).
*/
/*-----------------------------------------------------------------*/

static void IDASetSS(IDAMem IDA_mem, realtype value)
{
  int is;

  ss = value;
  if (sensi && (ism==IDA_STAGGERED)) 
    ssS = value;
  if (sensi && (ism==IDA_STAGGERED1))
    for(is=0;is<Ns;is++)
      ssS1[is] = value;
}

/*-------------------- IDANewtonIter ------------------------------*/
/*
  This routine performs the Newton iteration.  
  It assumes that delta contains the initial residual vector on entry.
  If the iteration succeeds, it returns the value IDA_SUCCESS = 0.
  If not, it returns either:
    a positive value (for a recoverable failure), namely one of:
       IDA_RES_RECVR
       IDA_LSOLVE_RECVR
       IDA_NCONV_RECVR
  or
    a negative value (for a nonrecoverable failure), namely one of:
       IDA_RES_FAIL
       IDA_LSOLVE_FAIL

  NOTE: This routine uses N_Vector savres, which is preset to tempv1.
*/
/*-----------------------------------------------------------------*/

static int IDANewtonIter(IDAMem IDA_mem)
{
  int mnewt, retval, is;
  realtype delnrm, oldnrm, rate;
  booleantype sensi_sim;

  sensi_sim = (sensi && (ism==IDA_SIMULTANEOUS));

  /* Initialize counter mnewt and cumulative correction vector ee. */
  mnewt = 0;
  N_VConst (ZERO, ee);

  if (sensi_sim)
    for(is=0;is<Ns;is++)
      N_VConst(ZERO, eeS[is]);

  /* Looping point for Newton iteration.  Break out on any error. */
  loop {

    nni++;

    /* Save a copy of the residual vector in savres. */
    N_VScale(ONE, delta, savres);

    /* Call the lsolve function to get correction vector delta. */
    retval = lsolve(IDA_mem, delta, ewt, yy, yp, savres);
    if(retval < 0) return(IDA_LSOLVE_FAIL);
    if(retval > 0) return(IDA_LSOLVE_RECVR);

    if (sensi_sim) {
      for(is=0;is<Ns;is++) {
        retval = lsolve(IDA_mem, deltaS[is], ewtS[is], yy, yp, savres);
        if(retval < 0) return(IDA_LSOLVE_FAIL);
        if(retval > 0) return(IDA_LSOLVE_RECVR);
      }
    }

    /* Apply delta to yy, yp, and ee, and get norm(delta). */
    N_VLinearSum(ONE, yy, -ONE, delta, yy);
    N_VLinearSum(ONE, ee, -ONE, delta, ee);
    N_VLinearSum(ONE, yp, -cj,  delta, yp);
    delnrm = IDAWrmsNorm(IDA_mem, delta, ewt, FALSE);

    if (sensi_sim) {
      for(is=0;is<Ns;is++) {
        N_VLinearSum(ONE, eeS[is], -ONE, deltaS[is], eeS[is]);
        N_VLinearSum(ONE, yyS[is], -ONE, deltaS[is], yyS[is]);
        N_VLinearSum(ONE, ypS[is], -cj,  deltaS[is], ypS[is]);
      }
      delnrm = IDASensWrmsNormUpdate(IDA_mem, delnrm, deltaS, ewtS, FALSE);
    }

    /* Test for convergence, first directly, then with rate estimate. */

    if (mnewt == 0) { 
       oldnrm = delnrm;
       if (delnrm <= toldel) {
         return(IDA_SUCCESS);
       }
    }
    else {
      rate = RPowerR( delnrm/oldnrm, ONE/mnewt );
      if (rate > RATEMAX) return(IDA_NCONV_RECVR);
      ss = rate/(ONE - rate);
    }

    if (ss*delnrm <= epsNewt) {
      return(IDA_SUCCESS);
    }

    /* Not yet converged.  Increment mnewt and test for max allowed. */
    mnewt++;
    if (mnewt >= maxcor) return(IDA_NCONV_RECVR);

    /* Call res for new residual and check error flag from res. */
    retval = res(tn, yy, yp, delta, rdata);
    nre++;
    if(retval < 0) return(IDA_RES_FAIL);
    if(retval > 0) return(IDA_RES_RECVR);

    if (sensi_sim) {
      retval = IDASensRes(IDA_mem, tn, yy, yp, delta, 
                          yyS, ypS, deltaS,
                          tmpS1, tmpS2, tmpS3);
      if(retval < 0) return(IDA_RES_FAIL);
      if(retval > 0) return(IDA_RES_RECVR);
    }

    /* Loop for next iteration. */

  } /* end of Newton iteration loop */

  return(retval);

}

/*-------------------- IDAStgrNewtonIter --------------------------*/
/*
  This routine performs the Newton iteration for sensitivity variables
  int the IDA_STAGGERED case.
  It assumes that deltaS contains the initial sensitivity residual 
  vectors on entry.

  If the iteration succeeds, it returns the value IDA_SUCCESS = 0.
  If not, it returns either:
    a positive value (for a recoverable failure), namely one of:
       IDA_RES_RECVR
       IDA_LSOLVE_RECVR
       IDA_NCONV_RECVR
  or
    a negative value (for a nonrecoverable failure), namely one of:
       IDA_RES_FAIL
       IDA_LSOLVE_FAIL
*/
/*-----------------------------------------------------------------*/

static int IDAStgrNewtonIter(IDAMem IDA_mem)
{
  int mnewt, is, retval;
  realtype delSnrm, delSnrm1, rateS;

  mnewt = 0;                  /* local Newton iteration counter */
  for(is=0;is<Ns;is++)
    N_VConst(ZERO, eeS[is]);  /* cumulative sensi. corrections  */

  loop {

    nniS++;

    for(is=0;is<Ns;is++) {

      retval = lsolve(IDA_mem, deltaS[is], ewtS[is], yy, yp, delta);
      if(retval < 0) return(IDA_LSOLVE_FAIL);
      if(retval > 0) return(IDA_LSOLVE_RECVR);

      N_VLinearSum(ONE, eeS[is], -ONE, deltaS[is], eeS[is]);
      N_VLinearSum(ONE, yyS[is], -ONE, deltaS[is], yyS[is]);
      N_VLinearSum(ONE, ypS[is], -cj,  deltaS[is], ypS[is]);

    }

    delSnrm = IDASensWrmsNorm(IDA_mem, deltaS, ewtS, FALSE);

    if (mnewt == 0) {
      if (delSnrm <= toldel) return(IDA_SUCCESS);
      delSnrm1 = delSnrm;
    } else {
      rateS = RPowerR(delSnrm/delSnrm1, ONE/mnewt);
      if (rateS > RATEMAX) return(IDA_NCONV_RECVR);
      ssS = rateS/(ONE - rateS);
    }

    if (ssS * delSnrm <= epsNewt) return(IDA_SUCCESS);

    mnewt++;
    if(mnewt >= maxcorS) return(IDA_NCONV_RECVR);

    retval = IDASensRes(IDA_mem, tn, yy, yp, delta, 
                        yyS, ypS, deltaS,
                        tmpS1, tmpS2, tmpS3);
    if (retval != IDA_SUCCESS) return(retval);

  }

  return(retval);
}

/*-------------------- IDAStgr1NewtonIter -------------------------*/
/*
  This routine performs the Newton iteration for the is-th sensitivity 
  variable in the IDA_STAGGERED1 case.
  It assumes that deltaS1 contains the initial sensitivity residual 
  vector on entry.

  If the iteration succeeds, it returns the value IDA_SUCCESS = 0.
  If not, it returns either:
    a positive value (for a recoverable failure), namely one of:
       IDA_RES_RECVR
       IDA_LSOLVE_RECVR
       IDA_NCONV_RECVR
  or
    a negative value (for a nonrecoverable failure), namely one of:
       IDA_RES_FAIL
       IDA_LSOLVE_FAIL
*/
/*-----------------------------------------------------------------*/

static int IDAStgr1NewtonIter(IDAMem IDA_mem, int is)
{
  int mnewt, retval;
  realtype delSnrm, delSnrm1, rateS;

  mnewt = 0;                /* local Newton iteration counter */
  N_VConst(ZERO, eeS[is]);  /* cumulative sensi. corrections  */

  loop {

    nniS++;
    nniS1[is]++;

    retval = lsolve(IDA_mem, deltaS1, ewtS[is], yy, yp, delta);
    if(retval < 0) return(IDA_LSOLVE_FAIL);
    if(retval > 0) return(IDA_LSOLVE_RECVR);

    N_VLinearSum(ONE, eeS[is], -ONE, deltaS1, eeS[is]);
    N_VLinearSum(ONE, yyS1,    -ONE, deltaS1, yyS1   );
    N_VLinearSum(ONE, ypS1,    -cj,  deltaS1, ypS1   );

    delSnrm = IDAWrmsNorm(IDA_mem, deltaS1, ewtS[is], FALSE);

    if (mnewt == 0) {
      if (delSnrm <= toldel) return(IDA_SUCCESS);
      delSnrm1 = delSnrm;
    } else {
      rateS = RPowerR(delSnrm/delSnrm1, ONE/mnewt);
      if (rateS > RATEMAX) return(IDA_NCONV_RECVR);
      ssS1[is] = rateS/(ONE - rateS);
    }

    if (ssS1[is] * delSnrm <= epsNewt) return(IDA_SUCCESS);

    mnewt++;
    if(mnewt >= maxcorS) return(IDA_NCONV_RECVR);

    retval = IDASensRes1(IDA_mem, tn, yy, yp, delta, 
                         is, 
                         yyS1, ypS1, deltaS1,
                         tmpS1, tmpS2, tmpS3);
    if (retval != IDA_SUCCESS) return(retval);

  }

  return(retval);
}

/*---------------------- IDAPredict -------------------------------*/
/*
  This routine predicts the new values for vectors yy and yp.
*/
/*-----------------------------------------------------------------*/

static void IDAPredict(IDAMem IDA_mem)
{
  int j;

  N_VScale(ONE, phi[0], yy);
  N_VConst(ZERO, yp);
  
  for(j=1; j<=kk; j++) {
    N_VLinearSum(ONE,      phi[j], ONE, yy, yy);
    N_VLinearSum(gamma[j], phi[j], ONE, yp, yp);
  }
}

/*------------------ IDAQuadPredict -------------------------------*/
/*
  This routine predicts the new value for vectors yyQ and ypQ
*/
/*-----------------------------------------------------------------*/

static void IDAQuadPredict(IDAMem IDA_mem)
{
  int j;

  N_VScale(ONE, phiQ[0], yyQ);
  N_VConst(ZERO, ypQ);

  for(j=1; j<=kk; j++) {
    N_VLinearSum(ONE,      phiQ[j], ONE, yyQ, yyQ);
    N_VLinearSum(gamma[j], phiQ[j], ONE, ypQ, ypQ);
  }

}

/*-------------------- IDASensPredict -----------------------------*/
/*
  This routine loads the predicted values for the is-th sensitivity 
  in the vectors yySens and ypSens.

  When ism=IDA_STAGGERED,  yySens = yyS[is] and ypSens = ypS[is]
  When ism=IDA_STAGGERED1, yySens = yyS1    and ypSens = ypS1
*/
/*-----------------------------------------------------------------*/

static void IDASensPredict(IDAMem IDA_mem, int is, 
                           N_Vector yySens, N_Vector ypSens)
{
  int j;

  N_VScale(ONE, phiS[0][is], yySens);
  N_VConst(ZERO, ypSens);
  
  for(j=1; j<=kk; j++) {
    N_VLinearSum(ONE,      phiS[j][is], ONE, yySens, yySens);
    N_VLinearSum(gamma[j], phiS[j][is], ONE, ypSens, ypSens);
  }

}

/*------------------ IDATestError ---------------------------------*/
/*
  This routine estimates errors at orders k, k-1, k-2, decides whether 
  or not to reduce order, and performs the local error test. 
  
  On return, *est = *erk    if knew = kk
             *est = *erkm1  if knew = kk - 1

  Note that 'est' is only used in IDAHandleNFlag and is recomputed 
  in IDACompleteStep.

  IDATestError returns either IDA_SUCCESS or ERROR_TEST_FAIL
*/
/*-----------------------------------------------------------------*/

static int IDATestError(IDAMem IDA_mem, realtype ck, 
                        realtype *est, 
                        realtype *erk, realtype *erkm1)
{
  int is;
  realtype enorm_k, enorm_km1, enorm_km2;
  realtype erkm2;
  realtype terk, terkm1, terkm2;
  booleantype sensi_sim;

  sensi_sim = (sensi && (ism==IDA_SIMULTANEOUS) && errconS);

  /* Compute error for order k. */
  enorm_k = IDAWrmsNorm(IDA_mem, ee, ewt, suppressalg);
  if (sensi_sim)
    enorm_k = IDASensWrmsNormUpdate(IDA_mem, enorm_k, eeS, ewtS, suppressalg);
  *erk  = sigma[kk] * enorm_k;
  terk = (kk+1) * (*erk);

  knew = kk;
  *est = *erk;

  /* Now compute the errors for orders k-1 and k-2, 
     and decide whether to reduce the order k to k-1 */
  
  if ( kk > 1 ) {

    N_VLinearSum(ONE, phi[kk], ONE, ee, delta);
    enorm_km1 = IDAWrmsNorm(IDA_mem, delta, ewt, suppressalg);
    if(sensi_sim) {
      for(is=0;is<Ns;is++)
        N_VLinearSum(ONE, phiS[kk][is], ONE, eeS[is], deltaS[is]);
      enorm_km1 = IDASensWrmsNormUpdate(IDA_mem, enorm_km1, deltaS, ewtS, suppressalg);
    }
    *erkm1 = sigma[kk-1] * enorm_km1;
    terkm1 = kk * (*erkm1);

    if ( (kk==2) && (terkm1 <= 0.5*terk) ) { knew = kk - 1; *est = *erkm1; } 

    if ( kk > 2 ) {
      N_VLinearSum(ONE, phi[kk-1], ONE, delta, delta);
      enorm_km2 = IDAWrmsNorm(IDA_mem, delta, ewt, suppressalg);
      if (sensi_sim) {
        for(is=0;is<Ns;is++)
          N_VLinearSum(ONE, phiS[kk-1][is], ONE, deltaS[is], deltaS[is]);
        enorm_km2 = IDASensWrmsNormUpdate(IDA_mem, enorm_km2, deltaS, ewtS, suppressalg);
      }
      erkm2 = sigma[kk-2] * enorm_km2;
      terkm2 = (kk-1) * erkm2;
      if (MAX(terkm1, terkm2) <= terk) { knew = kk - 1; *est = *erkm1; }
    }

  }
  
  if (ck * enorm_k > ONE) return(ERROR_TEST_FAIL);
  else                    return(IDA_SUCCESS);

}

/*------------------ IDAQuadTestError -----------------------------*/
/*
  This routine estimates estimates the quadrature errors at the 
  current order k. If a decision to reduce the order was not
  taken in IDATestError, we estimate quadrature errors at orders
  k-1 and k-2 and decide if the order should be reduced due to
  these. 

  Finally, we perform the local error test on the quadrature variables.
  If it fails we take appropriate action.

  IDAQuadTestError returns either IDA_SUCCESS or ERROR_TEST_FAIL
*/
/*-----------------------------------------------------------------*/

static int IDAQuadTestError(IDAMem IDA_mem, realtype ck, 
                            realtype *estQ,
                            realtype *erQk, realtype *erQkm1)
{
  realtype enormQ, terQk, terQkm1, terQkm2, erQkm2;
  N_Vector tempv;

  /* Compute error for order k. */
  enormQ = N_VWrmsNorm(eeQ, ewtQ);
  *erQk  = sigma[kk] * enormQ;
  terQk = (kk+1) * (*erQk);
  
  *estQ = *erQk;

  /* Now compute the errors for orders k-1 and k-2, 
     and decide whether to reduce the order k to k-1 */

  if ( kk > 1 ) {
    
    /* Estimate error at order k-1 */
    tempv = ypQ;
    N_VLinearSum(ONE, phiQ[kk], ONE, eeQ, tempv);
    *erQkm1 = sigma[kk-1] * N_VWrmsNorm(tempv, ewtQ);
    terQkm1 = kk * (*erQkm1);

    if ( knew != kk ) {
      
      /* The decision to reduce the order has already been made.
         Update the quadrature error estimate */
      
      *estQ = *erQkm1;
    }

    else {

      /* See whether the order should be reduced due to quadratures */

      if ( (kk==2) && (terQkm1 <= HALF*terQk) ) { knew = kk - 1; *estQ = *erQkm1; }

      if ( kk > 2 ) {
        N_VLinearSum(ONE, phiQ[kk-1], ONE, tempv, tempv);
        erQkm2 = sigma[kk-2] * N_VWrmsNorm(tempv, ewtQ);
        terQkm2 = (kk-1) * erQkm2;
        if (MAX(terQkm1, terQkm2) <= terQk) { knew = kk - 1; *estQ = *erQkm1; }
      }

    }

  }

  if (ck * enormQ > ONE) {
    netfQ++;
    return(ERROR_TEST_FAIL);
  } else {
    return(IDA_SUCCESS);
  }

}

/*-------------------- IDAStgrTestError ---------------------------*/
/*
*/
/*-----------------------------------------------------------------*/

static int IDAStgrTestError(IDAMem IDA_mem, realtype ck, 
                            realtype *estS, 
                            realtype *erSk, realtype *erSkm1)
{
  int is;
  realtype enormS, terSk, terSkm1, terSkm2, erSkm2;

  /* Compute error for order k. */
  enormS = IDASensWrmsNorm(IDA_mem, eeS, ewtS, suppressalg);
  *erSk = sigma[kk] * enormS;
  terSk = (kk+1) * (*erSk);

  *estS = *erSk;

  /* Now compute the errors for orders k-1 and k-2, 
     and decide whether to reduce the order k to k-1 */
  
  if ( kk > 1 ) {
    
    /* Estimate error at order k-1 */
    for(is=0;is<Ns;is++)
      N_VLinearSum(ONE, phiS[kk][is], ONE, eeS[is], deltaS[is]);
    *erSkm1 = sigma[kk-1] * IDASensWrmsNorm(IDA_mem, deltaS, ewtS, suppressalg);
    terSkm1 = kk * (*erSkm1);

    if ( knew != kk ) {
      
      /* The decision to reduce the order has already been made.
         Update the sensitivity error estimate */
      
      *estS = *erSkm1;
    }

    else {

      /* See whether the order should be reduced due to sensitivities */

      if ( (kk==2) && (terSkm1 <= HALF*terSk) ) { knew = kk - 1; *estS = *erSkm1; }

      if ( kk > 2 ) {
        for(is=0;is<Ns;is++)
          N_VLinearSum(ONE, phiS[kk-1][is], ONE, deltaS[is], deltaS[is]);
        erSkm2 = sigma[kk-2] * IDASensWrmsNorm(IDA_mem, deltaS, ewtS, suppressalg);
        terSkm2 = (kk-1) * erSkm2;
        if (MAX(terSkm1, terSkm2) <= terSk) { knew = kk - 1; *estS = *erSkm1; }
      }

    }

  }

  if (ck * enormS > ONE) {
    netfS++;
    return(ERROR_TEST_FAIL);
  } else {
    return(IDA_SUCCESS);
  }

}

/*-------------------- IDAStgr1TestError --------------------------*/
/*
*/
/*-----------------------------------------------------------------*/

static int IDAStgr1TestError(IDAMem IDA_mem, int is, realtype ck, 
                             realtype *estS, 
                             realtype *erSk, realtype *erSkm1)
{
  realtype enormS, terSk, terSkm1, terSkm2, erSkm2;

  /* Compute error for order k. */
  enormS = IDAWrmsNorm(IDA_mem, eeS[is], ewtS[is], suppressalg);
  *erSk = sigma[kk] * enormS;
  terSk = (kk+1) * (*erSk);

  *estS = *erSk;

  /* Now compute the errors for orders k-1 and k-2, 
     and decide whether to reduce the order k to k-1 */
  
  if ( kk > 1 ) {
    
    /* Estimate error at order k-1 */
    N_VLinearSum(ONE, phiS[kk][is], ONE, eeS[is], deltaS1);
    *erSkm1 = sigma[kk-1] * IDAWrmsNorm(IDA_mem, deltaS1, ewtS[is], suppressalg);
    terSkm1 = kk * (*erSkm1);

    if ( knew != kk ) {
      
      /* The decision to reduce the order has already been made.
         Update the sensitivity error estimate */
      
      *estS = *erSkm1;
    }

    else {

      /* See whether the order should be reduced due to sensitivities */

      if ( (kk==2) && (terSkm1 <= HALF*terSk) ) { knew = kk - 1; *estS = *erSkm1; }

      if ( kk > 2 ) {
        N_VLinearSum(ONE, phiS[kk-1][is], ONE, deltaS1, deltaS1);
        erSkm2 = sigma[kk-2] * IDAWrmsNorm(IDA_mem, deltaS1, ewtS[is], suppressalg);
        terSkm2 = (kk-1) * erSkm2;
        if (MAX(terSkm1, terSkm2) <= terSk) { knew = kk - 1; *estS = *erSkm1; }
      }

    }

  }

  if (ck * enormS > ONE) {
    netfS++;
    netfS1[is]++;
    return(ERROR_TEST_FAIL);
  } else {
    return(IDA_SUCCESS);
  }

}

/*------------------ IDARestore -----------------------------------*/
/*
  This routine restores tn, psi, and phi in the event of a failure.
  It changes back phi-star to phi (changed in IDASetCoeffs)
*/
/*-----------------------------------------------------------------*/

static void IDARestore(IDAMem IDA_mem, realtype saved_t)
{
  int j, is;

  tn = saved_t;
  
  for (j = 1; j <= kk; j++) 
    psi[j-1] = psi[j] - hh;

  for (j = ns; j<=kk; j++) 
    N_VScale(ONE/beta[j], phi[j], phi[j]);

  if (quad)
    for (j = ns; j<=kk; j++) 
      N_VScale(ONE/beta[j], phiQ[j], phiQ[j]);

  if (sensi)
    for (is=0; is<Ns; is++)
      for (j = ns; j<=kk; j++) 
        N_VScale(ONE/beta[j], phiS[j][is], phiS[j][is]);

}

/*------------------ IDAHandleNFlag -------------------------------*/
/*
  This routine handles failures indicated by the input variable nflag. 
  Positive values indicate various recoverable failures while negative
  values indicate nonrecoverable failures. This routine adjusts the
  step size for recoverable failures. The possible return values are:

  Failures due to state variables
  -------------------------------

  Possible nflag values (input):

     --convergence failures--
     IDA_RES_RECVR
     IDA_CONSTR_RECVR
     IDA_LSOLVE_RECVR
     IDA_NCONV_RECVR
     IDA_RES_FAIL
     IDA_LSOLVE_FAIL

     --error test failure--
     ERROR_TEST_FAIL

  Possible kflag values (output):

     --recoverable--
     PREDICT_AGAIN

     --nonrecoverable--
     IDA_CONSTR_FAIL   
     IDA_REP_RES_ERR    
     IDA_ERR_FAIL  
     IDA_CONV_FAIL 
     IDA_RES_FAIL
     IDA_LSETUP_FAIL
     IDA_LSOLVE_FAIL

  Failures due to quadrature variables
  ------------------------------------

  Possible nflag values (input):

     ---error test failure---
     ERROR_TEST_FAIL

  Possible kflag values (output):

     --recoverable---
     PREDICT_AGAIN

     --nonrecoverable--
     IDA_ERR_FAIL

  Failures due to sensitivity variables
  -------------------------------------

  Possible nflag values (input):

     --convergence failures--
     IDA_RES_RECVR
     IDA_CONSTR_RECVR
     IDA_LSOLVE_RECVR
     IDA_NCONV_RECVR
     IDA_RES_FAIL
     IDA_LSOLVE_FAIL

     --error test failure--
     ERROR_TEST_FAIL

  Possible kflag values (output):

     --recoverable--
     PREDICT_AGAIN

     --nonrecoverable--
     IDA_CONSTR_FAIL   
     IDA_REP_RES_ERR    
     IDA_ERR_FAIL  
     IDA_CONV_FAIL 
     IDA_RES_FAIL
     IDA_LSETUP_FAIL
     IDA_LSOLVE_FAIL
*/
/*-----------------------------------------------------------------*/

static int IDAHandleNFlag(IDAMem IDA_mem, int nflag,
                          int *ncfPtr, int *nefPtr, realtype est)
{
  int *ncf, *nef;
  
  ncf = ncfPtr; 
  nef = nefPtr;
  phase = 1;
  
    
  /*-----------------------
    Nonlinear solver failed 
    -----------------------*/

  if (nflag != ERROR_TEST_FAIL) {
    
    ncfn++;
    
    if (nflag < 0) {  /* nonrecoverable failure */
      return(nflag);
    }
    else {            /* recoverable failures   */
      (*ncf)++; 
      
      if (nflag != IDA_CONSTR_RECVR) rr = QUARTER;
      hh *= rr;
      
      if (*ncf < maxncf)                  return(PREDICT_AGAIN);
      else if (nflag == IDA_RES_RECVR)    return(IDA_REP_RES_ERR);
      else if (nflag == IDA_CONSTR_RECVR) return(IDA_CONSTR_FAIL);
      else                                return(IDA_CONV_FAIL);
    }
    
  }
  
  /*-----------------
    Error Test failed 
    -----------------*/

  else {

    (*nef)++; 
    netf++;
    
    if (*nef == 1) {
      
      /* On first error test failure, keep current order or lower order by one. 
         Compute new stepsize based on differences of the solution. */
      kk = knew;
      
      rr = PT9 * RPowerR( TWO*est + PT0001,(-ONE/(kk+1)) );
      rr = MAX(QUARTER, MIN(PT9,rr));
      hh *=rr;  /* adjust step size */
      
      return(PREDICT_AGAIN);
      
    } 
    
    else if (*nef == 2) {
      
      /* On second error test failure, use current order or decrease order by one. 
         Reduce stepsize by factor of 1/4. */
      kk = knew;
      rr = QUARTER;
      hh *= rr;
      
      return(PREDICT_AGAIN);
      
    } 
    
    else if (*nef < maxnef) {
      
      /* On third and subsequent error test failures, set order to 1.
         Reduce stepsize by factor of 1/4. */
        kk = 1;
        rr = QUARTER;
        hh *= rr;
        return(PREDICT_AGAIN);

    } 

    else {

      /* Return Repeated Error Failures */
      return(IDA_ERR_FAIL);

    }
    
  }
  
}

/*-------------------- IDAReset ----------------------------------*/
/*
  This routine is called only if we need to predict again at the 
  very first step. In such a case, reset phi[1] and psi[0].
*/
/*-----------------------------------------------------------------*/

static void IDAReset(IDAMem IDA_mem)
{
  int is;

  psi[0] = hh;

  N_VScale(rr, phi[1], phi[1]);

  if (quad)
    N_VScale(rr, phiQ[1], phiQ[1]);

  if (sensi)
    for(is=0;is<Ns;is++) 
      N_VScale(rr, phiS[1][is], phiS[1][is]);
}

/*-------------------- IDACompleteStep ----------------------------*/
/*
  This routine completes a successful step.  It increments nst,
  saves the stepsize and order used, makes the final selection of
  stepsize and order for the next step, and updates the phi array.
*/
/*-----------------------------------------------------------------*/

static void IDACompleteStep(IDAMem IDA_mem,
                            realtype error_k, realtype error_km1)
{
  int j, is, kdiff, action;
  realtype terk, terkm1, terkp1;
  realtype erkp1, erQkp1, erSkp1;
  realtype error_kp1, est;
  realtype temp, hnew;
  N_Vector tempvQ;

  nst++;
  kdiff = kk - kused;
  kused = kk;
  hused = hh;

  if ( (knew == kk-1) || (kk == maxord) ) phase = 1;

  /* For the first few steps, until either a step fails, or the order is 
  reduced, or the order reaches its maximum, we raise the order and double 
  the stepsize. During these steps, phase = 0. Thereafter, phase = 1, and
  stepsize and order are set by the usual local error algorithm.         

  Note that, after the first step, the order is not increased, as not all 
  of the neccessary information is available yet.
  */
  
  if (phase == 0) {

    if(nst > 1) {
      kk++;
      hnew = TWO * hh;
      hh = hnew;
    }

  } else {

    action = UNSET;
    
    /* Set action = LOWER/MAINTAIN/RAISE to specify order decision */
    
    if (knew == kk-1)                   {action = LOWER;    goto takeaction;}
    if (kk == maxord)                   {action = MAINTAIN; goto takeaction;}
    if ( (kk+1 >= ns ) || (kdiff == 1)) {action = MAINTAIN; goto takeaction;}
    
    /* Estimate the error at order k+1, unless already decided to
       reduce order, or already using maximum order, or stepsize has not
       been constant, or order was just raised. */
    
    N_VLinearSum (ONE, ee, -ONE, phi[kk+1], tempv1);
    temp = IDAWrmsNorm(IDA_mem, tempv1, ewt, suppressalg);
    erkp1= temp/(kk+2);
    error_kp1 = erkp1;

    if (errconQ) {
      tempvQ = ypQ;
      N_VLinearSum (ONE, eeQ, -ONE, phiQ[kk+1], tempvQ);
      temp = N_VWrmsNorm(tempvQ, ewtQ);
      erQkp1= temp/(kk+2);
      if (erQkp1 > error_kp1) error_kp1 = erQkp1;
    }

    if (errconS) {
      for (is=0; is<Ns; is++) {
        N_VLinearSum (ONE, eeS[is], -ONE, phiS[kk+1][is], tempv1);
        temp = IDAWrmsNorm(IDA_mem, tempv1, ewtS[is], suppressalg);
        erSkp1 = temp/(kk+2);
        if (erSkp1 > error_kp1) error_kp1 = erSkp1;
      }
    }

    /* Choose among orders k-1, k, k+1 using local truncation error norms. */

    terk   = (kk+1) * error_k;
    terkp1 = (kk+2) * error_kp1;

    if (kk == 1) {
      if (terkp1 >= HALF * terk)       {action = MAINTAIN; goto takeaction;}
      else                             {action = RAISE;    goto takeaction;}
    } else {
      terkm1 =  kk    * error_km1;
      if (terkm1 <= MIN(terk, terkp1)) {action = LOWER;    goto takeaction;}
      else if (terkp1  >= terk)        {action = MAINTAIN; goto takeaction;}
      else                             {action = RAISE;    goto takeaction;}
    }
    
  takeaction:
    
    /* Set the estimated error norm and, on change of order, reset kk. */
    if      (action == RAISE) { kk++; est = error_kp1; }
    else if (action == LOWER) { kk--; est = error_km1; }
    else                      {       est = error_k;   }  

    /* Compute rr = tentative ratio hnew/hh from error norm.
       Reduce hh if rr <= 1, double hh if rr >= 2, else leave hh as is.
       If hh is reduced, hnew/hh is restricted to be between .5 and .9. */
    
    hnew = hh;
    rr = RPowerR( (TWO * est + PT0001) , (-ONE/(kk+1) ) );
    
    if (rr >= TWO) {
      hnew = TWO * hh;
      if( (temp = ABS(hnew)*hmax_inv) > ONE ) hnew /= temp;
    } else if (rr <= ONE ) { 
      rr = MAX(HALF, MIN(PT9,rr));
      hnew = hh * rr;
    }
    
    hh = hnew;
    
  } /* end of phase if block */
  
  /* Save ee for possible order increase on next step */
  
  if (kused < maxord) {

    N_VScale(ONE, ee, phi[kused+1]);

    if (errconQ)
      N_VScale(ONE, eeQ, phiQ[kused+1]);

    if (errconS)
      for (is=0; is<Ns; is++)
        N_VScale(ONE, eeS[is], phiS[kused+1][is]);

  }

  /* Update phi arrays */

  N_VLinearSum(ONE, ee, ONE, phi[kused], phi[kused]);
  for (j= kused-1; j>=0; j--)
    N_VLinearSum(ONE, phi[j], ONE, phi[j+1], phi[j]);

  if (quad) {
    
    N_VLinearSum(ONE, eeQ, ONE, phiQ[kused], phiQ[kused]);
    for (j= kused-1; j>=0; j--)
      N_VLinearSum(ONE, phiQ[j], ONE, phiQ[j+1], phiQ[j]);

  }

  if (sensi) {

    for (is=0; is<Ns; is++) {
      N_VLinearSum(ONE, eeS[is], ONE, phiS[kused][is], phiS[kused][is]);
      for (j=kused-1; j>=0; j--)
        N_VLinearSum(ONE, phiS[j][is], ONE, phiS[j+1][is], phiS[j][is]);
    }

  }


}

/*=================================================================*/
/*BEGIN        Combined norms                                      */
/*=================================================================*/

/*-----------------------  IDAWrmsNorm   --------------------------*/
/*
  Returns the WRMS norm of vector x with weights w.
  If mask = TRUE, the weight vector w is masked by id, i.e.,
      nrm = N_VWrmsNormMask(x,w,id);
  Otherwise,
      nrm = N_VWrmsNorm(x,w);
  
  mask = FALSE       when the call is made from the nonlinear solver.
  mask = suppressalg otherwise.
*/
/*-----------------------------------------------------------------*/

realtype IDAWrmsNorm(IDAMem IDA_mem, 
                     N_Vector x, N_Vector w, 
                     booleantype mask)
{
  realtype nrm;

  if (mask) nrm = N_VWrmsNormMask(x, w, id);
  else      nrm = N_VWrmsNorm(x, w);

  return(nrm);
}

/*------------------  IDAQuadWrmsNormUpdate   --------------------------*/
/*
  Updates the norm old_nrm to account for all quadratures.
*/
/*-----------------------------------------------------------------*/

static realtype IDAQuadWrmsNormUpdate(realtype old_nrm,
                                      N_Vector xQ, N_Vector wQ)
{
  realtype qnrm;

  qnrm = N_VWrmsNorm(xQ, wQ);
  if (old_nrm > qnrm) return(old_nrm);
  else                return(qnrm);
}

/*------------------   IDASensWrmsNorm        ----------------------*/
/*
  This routine returns the maximum over the weighted root mean 
  square norm of xS with weight vectors wS:

    max { wrms(xS[0],wS[0]) ... wrms(xS[Ns-1],wS[Ns-1]) }    

  Called by IDASensUpdateNorm or directly in the IDA_STAGGERED approach 
  during the NLS solution and before the error test.
*/
/*-----------------------------------------------------------------*/

static realtype IDASensWrmsNorm(IDAMem IDA_mem, 
                                N_Vector *xS, N_Vector *wS,
                                booleantype mask)
{
  int is;
  realtype nrm, snrm;

  nrm = IDAWrmsNorm(IDA_mem, xS[0], wS[0], mask);
  for (is=1; is<Ns; is++) {
    snrm = IDAWrmsNorm(IDA_mem, xS[is], wS[is], mask);
    if ( snrm > nrm ) nrm = snrm;
  }

  return (nrm);
}

/*------------------  IDASensWrmsNormUpdate   ---------------------*/
/*
  Updates the norm old_nrm to account for all sensitivities.
*/
/*-----------------------------------------------------------------*/

static realtype IDASensWrmsNormUpdate(IDAMem IDA_mem, realtype old_nrm,
                                      N_Vector *xS, N_Vector *wS,
                                      booleantype mask)
{
  realtype snrm;
  
  snrm = IDASensWrmsNorm(IDA_mem, xS, wS, mask);
  if (old_nrm > snrm) return(old_nrm);
  else                return(snrm);
}

/*=================================================================*/
/*END          Combined norms                                      */
/*=================================================================*/

/*=================================================================*/
/*BEGIN        Sensitivity RHS Routines                            */
/*=================================================================*/

/*------------------    IDASensRes       --------------------------*/
/*
  IDASensRes is a high level routine that returns the residuals
  of sensitivity equations. Depending on the 'iresS' flag, it either 
  calls directly the resS routine (if iresS=IDA_ALLSENS) or calls the
  resS1 routine in a loop over all sensitivities (if iresS=IDA_ONESENS).
*/
/*-----------------------------------------------------------------*/

static int IDASensRes(IDAMem IDA_mem, realtype time,
                      N_Vector yycur, N_Vector ypcur, N_Vector resvalcur,
                      N_Vector *yyScur, N_Vector *ypScur, N_Vector *resvalScur,
                      N_Vector tmp1, N_Vector tmp2, N_Vector tmp3)
{
  int ier, is;

  if (iresS==IDA_ALLSENS) {
    ier = resS(Ns, time, 
               yycur, ypcur, resvalcur, 
               yyScur, ypScur, resvalScur, 
               rdataS,
               tmp1, tmp2, tmp3);
    nrSe++;
  } else {
    for (is=0; is<Ns; is++) {
      ier = resS1(Ns, time, 
                  yycur, ypcur, resvalcur,
                  is, yyScur[is], ypScur[is], resvalScur[is], 
                  rdataS,
                  tmp1, tmp2, tmp3);
      nrSe++;
      if (ier != 0) break;
    }
  }
  return(ier);
}

/*------------------   IDASensRes1       --------------------------*/
/*
  IDASensRes1 is a high level routine that returns the residual
  of the is-th sensitivity equation. 
  
  IDASensRes1 is called only during the IDA_STAGGERED1 corrector loop
  (iresS must be IDA_ONESENS, otherwise IDASensMalloc would have 
  issued an error message).
*/
/*-----------------------------------------------------------------*/

static int IDASensRes1(IDAMem IDA_mem, realtype time,
                       N_Vector yycur, N_Vector ypcur, N_Vector resvalcur,
                       int is,
                       N_Vector yyScur, N_Vector ypScur, N_Vector resvalScur,
                       N_Vector tmp1, N_Vector tmp2, N_Vector tmp3)
{
  int ier;

  ier = resS1(Ns, time, 
              yycur, ypcur, resvalcur, 
              is, yyScur, ypScur, resvalScur, 
              rdataS,
              tmp1, tmp2, tmp3);
  nrSe++;
  return(ier);
}

/*=================================================================*/
/*BEGIN        DQ Approximations for Sensitivity RHS Routines      */
/*=================================================================*/

#undef Ns
#undef yy
#undef yp
#undef yyS
#undef ypS
#undef rdataS

/*------------------   IDASensRhsDQ      --------------------------*/
/*
  IDASensRhsDQ computes the residuals of all sensitivity equations
  by finite differences
*/
/*-----------------------------------------------------------------*/

int IDASensResDQ(int Ns, realtype t, 
                 N_Vector yy, N_Vector yp, N_Vector resval,
                 N_Vector *yyS, N_Vector *ypS, N_Vector *resvalS,
                 void *rdataS,
                 N_Vector ytemp, N_Vector yptemp, N_Vector restemp)
{
  int ier, is;
  
  for (is=0; is<Ns; is++) {
    ier = IDASensRes1DQ(Ns, t, 
                        yy, yp, resval, 
                        is, yyS[is], ypS[is], resvalS[is], 
                        rdataS,
                        ytemp, yptemp, restemp);
    if (ier != 0) break;
  }
  return(ier);
}

/*------------------   IDASensRes1DQ     --------------------------*/
/*
  IDASensRes1DQ computes the residual of the is-th sensitivity 
  equation by finite differences
*/
/*-----------------------------------------------------------------*/

int IDASensRes1DQ(int Ns, realtype t, 
                  N_Vector yy, N_Vector yp, N_Vector resval,
                  int is,
                  N_Vector yyS, N_Vector ypS, N_Vector resvalS,
                  void *rdataS,
                  N_Vector ytemp, N_Vector yptemp, N_Vector restemp)
{
  IDAMem IDA_mem;
  int method;
  int which;
  int ier=IDA_SUCCESS;
  booleantype skipFP;
  realtype psave, pbari;
  realtype del , rdel;
  realtype Delp, rDelp, r2Delp;
  realtype Dely, rDely, r2Dely;
  realtype Del , rDel , r2Del ;
  realtype norms, ratio;

  /* rdataS points to IDA_mem */
  IDA_mem = (IDAMem) rdataS;

  /* Set base perturbation del */
  del  = RSqrt(MAX(*reltol, uround));
  rdel = ONE/del;

  if (plist!=NULL) {
    which   = abs(plist[is]) - 1;
    skipFP  = (plist[is] < 0);
  } else {
    which  = is;
    skipFP = FALSE;
  }
  psave   = p[which];
  pbari   = ABS(pbar[which]);

  Delp  = pbari * del;
  rDelp = ONE/Delp;
  norms = N_VWrmsNorm(yyS, ewt) * pbari;
  rDely = MAX(norms, rdel) / pbari;
  Dely  = ONE/rDely;

  ratio = Dely * rDelp;

  if ((MAX(ONE/ratio, ratio) <= ABS(rhomax)) || rhomax == ZERO)
    method = (rhomax >= ZERO) ? CENTERED1 : FORWARD1; 
  else
    method = (rhomax > ZERO) ? CENTERED2 : FORWARD2;


  switch (method) {

  case CENTERED1:

    Del = MIN(Dely, Delp);
    r2Del = HALF/Del;

    /* Forward perturb y, y' and parameter */
    N_VLinearSum(Del, yyS, ONE, yy, ytemp);
    N_VLinearSum(Del, ypS, ONE, yp, yptemp);
    p[which] = psave + Del;

    /* Save residual in resvalS */
    ier = res(t, ytemp, yptemp, resvalS, rdata);
    nre++;
    nreS++;
    if (ier != 0) return(ier);
    
    /* Backward perturb y, y' and parameter */
    N_VLinearSum(-Del, yyS, ONE, yy, ytemp);
    N_VLinearSum(-Del, ypS, ONE, yp, yptemp);
    p[which] = psave - Del;

    /* Save residual in restemp */
    ier = res(t, ytemp, yptemp, restemp, rdata);
    nre++;
    nreS++;
    if (ier != 0) return(ier);

    /* Estimate the residual for the i-th sensitivity equation */
    N_VLinearSum(r2Del, resvalS, -r2Del, restemp, resvalS);
    
    break;

  case CENTERED2:

    r2Delp = HALF/Delp;
    r2Dely = HALF/Dely;

    /* Forward perturb y and y' */
    N_VLinearSum(Dely, yyS, ONE, yy, ytemp);
    N_VLinearSum(Dely, ypS, ONE, yp, yptemp);
    
    /* Save residual in resvalS */
    ier = res(t, ytemp, yptemp, resvalS, rdata);
    nre++;
    nreS++;
    if (ier != 0) return(ier);
    
    /* Backward perturb y and y' */
    N_VLinearSum(-Dely, yyS, ONE, yy, ytemp);
    N_VLinearSum(-Dely, ypS, ONE, yp, yptemp);

    /* Save residual in restemp */
    ier = res(t, ytemp, yptemp, restemp, rdata);
    nre++;
    nreS++;
    if (ier != 0) return(ier);

    /* Save the first difference quotient in resvalS */
    N_VLinearSum(r2Dely, resvalS, -r2Dely, restemp, resvalS);

    if (!skipFP) {

      /* Forward perturb parameter */
      p[which] = psave + Delp;

      /* Save residual in ytemp */
      ier = res(t, yy, yp, ytemp, rdata);
      nre++;
      nreS++;
      if (ier != 0) return(ier);

      /* Backward perturb parameter */
      p[which] = psave - Delp;

      /* Save residual in yptemp */
      ier = res(t, yy, yp, yptemp, rdata);
      nre++;
      nreS++;
      if (ier != 0) return(ier);

      /* Save the second difference quotient in restemp */
      N_VLinearSum(r2Delp, ytemp, -r2Delp, yptemp, restemp);

      /* Add the difference quotients for the sensitivity residual */
      N_VLinearSum(ONE, resvalS, ONE, restemp, resvalS);

    }

    break;

  case FORWARD1:

    Del = MIN(Dely, Delp);
    rDel = ONE/Del;

    /* Forward perturb y, y' and parameter */
    N_VLinearSum(Del, yyS, ONE, yy, ytemp);
    N_VLinearSum(Del, ypS, ONE, yp, yptemp);
    p[which] = psave + Del;

    /* Save residual in resvalS */
    ier = res(t, ytemp, yptemp, resvalS, rdata);
    nre++;
    nreS++;
    if (ier != 0) return(ier);

    /* Estimate the residual for the i-th sensitivity equation */
    N_VLinearSum(rDel, resvalS, -rDel, resval, resvalS);

    break;

  case FORWARD2:

    /* Forward perturb y and y' */
    N_VLinearSum(Dely, yyS, ONE, yy, ytemp);
    N_VLinearSum(Dely, ypS, ONE, yp, yptemp);

    /* Save residual in resvalS */
    ier = res(t, ytemp, yptemp, resvalS, rdata);
    nre++;
    nreS++;
    if (ier != 0) return(ier);

    /* Save the first difference quotient in resvalS */
    N_VLinearSum(rDely, resvalS, -rDely, resval, resvalS);

    if (!skipFP) {

      /* Forward perturb parameter */
      p[which] = psave + Delp;

      /* Save residual in restemp */
      ier = res(t, yy, yp, restemp, rdata);
      nre++;
      nreS++;
      if (ier != 0) return(ier);

      /* Save the second difference quotient in restemp */
      N_VLinearSum(rDelp, restemp, -rDelp, resval, restemp);

      /* Add the difference quotients for the sensitivity residual */
      N_VLinearSum(ONE, resvalS, ONE, restemp, resvalS);

    }

    break;

  }

  /* Restore original value of parameter */
  p[which] = psave;
  
  return(0);

}

/*=================================================================*/
/*END          DQ Approximations for Sensitivity RES Routines      */
/*=================================================================*/

/*=================================================================*/
/*END          Sensitivity RES Routines                            */
/*=================================================================*/

/*=================================================================*/
/*END          PRIVATE FUNCTIONS IMPLEMENTATION                    */
/*=================================================================*/
