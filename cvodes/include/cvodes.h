/*******************************************************************
 *                                                                 *
 * File          : cvodes.h                                        *
 * Programmers   : Scott D. Cohen, Alan C. Hindmarsh, Radu Serban  *
 *                 and Dan Shumaker @ LLNL                         *
 * Version of    : 28 March 2003                                   *
 *-----------------------------------------------------------------*
 * Copyright (c) 2002, The Regents of the University of California * 
 * Produced at the Lawrence Livermore National Laboratory          *
 * All rights reserved                                             *
 * For details, see sundials/cvodes/LICENSE                        *
 *-----------------------------------------------------------------*
 * This is the interface file for the main CVODES integrator.      *
 *                                                                 *
 *******************************************************************/

#ifdef __cplusplus     /* wrapper to enable C++ usage */
extern "C" {
#endif

#ifndef _cvodes_h
#define _cvodes_h


#include <stdio.h>
#include "sundialstypes.h"
#include "nvector.h"

/******************************************************************
 *                                                                *
 * CVODES is used to solve numerically the ordinary initial value *
 * problem :                                                      *
 *                                                                *
 *                 y' = f(t,y),                                   *
 *                 y(t0) = y0,                                    *
 *                                                                *
 * where t0, y0 in R^N, and f: R x R^N -> R^N are given.          *
 *                                                                *
 * Optionally, CVODES can perform forward sensitivity analysis    *
 * to find sensitivities of the solution y with respect to        *
 * parameters in the right hand side f and/or in the initial      *
 * conditions y0.                                                 * 
 *                                                                *
 ******************************************************************/

/******************************************************************
 *                                                                *
 * Enumerations for inputs to CVodeMalloc, CVodeReInit,           *
 * CVodeSensMalloc, CVodeSensReInit, CvodeQuadMalloc,             *
 * CVodeQuadReInit, and CVode.                                    *
 *----------------------------------------------------------------*
 * Symbolic constants for the lmm, iter, and itol input           *
 * parameters to CVodeMalloc and CVodeReInit, as well as the      *
 * input parameter itask to CVode, are given below.               *
 *                                                                *
 * lmm  : The user of the CVODE package specifies whether to use  *
 *        the ADAMS or BDF (backward differentiation formula)     *
 *        linear multistep method. The BDF method is recommended  *
 *        for stiff problems, and the ADAMS method is recommended *
 *        for nonstiff problems.                                  *
 *                                                                *
 * iter : At each internal time step, a nonlinear equation must   *
 *        be solved. The user can specify either FUNCTIONAL       *
 *        iteration, which does not require linear algebra, or a  *
 *        NEWTON iteration, which requires the solution of linear *
 *        systems. In the NEWTON case, the user also specifies a  *
 *        CVODE linear solver. NEWTON is recommended in case of   *
 *        stiff problems.                                         *
 *                                                                *
 * itol : This parameter specifies the relative and absolute      *
 *        tolerance types to be used. The SS tolerance type means *
 *        a scalar relative and absolute tolerance, while the SV  *
 *        tolerance type means a scalar relative tolerance and a  *
 *        vector absolute tolerance (a potentially different      *
 *        absolute tolerance for each vector component).          *
 *                                                                *
 * ism  : This parameter specifies the sensitivity corrector type *
 *        to be used. In the SIMULTANEOUS case, the nonlinear     *
 *        systems for states and all sensitivities are solved     *
 *        simultaneously. In the STAGGERED case, the nonlinear    *
 *        system for states is solved first and then, the         *
 *        nonlinear systems for all sensitivities are solved      *
 *        at the same time. Finally, in the STAGGERED1 approach   *     
 *        all nonlinear systems are solved in a sequence. Note    *
 *        that the STAGGERED1 approach can be used only if        *
 *        ifS = ONESENS (see bellow).                             *
 *                                                                *
 * ifS  : This parameter specifies the type of the routine that   *
 *        provides the right hand side of the sensitivity         *
 *        equations. The ALLSENS type means that r.h.s. for all   *
 *        sensitivities are provided simultaneously. In this case *
 *        fS (if provided by the user) must be of type SensRhsFn. *
 *        The ONESENS type means that fS (which, if provided by   *
 *        the user, must be of type SensRhs1Fn) computes r.h.s.   *
 *        one sensitivity at a time. Note that ism = STAGGERED1   *
 *        requires ifS=ONESENS. Either value for ifS is valid     *
 *        for ism=SIMULTANEOUS or STAGGERED.                      *
 *                                                                *
 * errcon: This parameter specifies whether sensitivity variables *
 * errconQ (or quadrature variables, for errconQ)                 *
 *         are included in the local error estimation (FULL) or   *
 *         not (PARTIAL). Note that, even with errcon=PARTIAL,    *
 *         sensitivities are included in the convergence test     *
 *         during the nonlinear system solution.                  *
 *                                                                *
 * itask : The itask input parameter to CVode indicates the job   *
 *         of the solver for the next user step. The NORMAL       *
 *         itask is to have the solver take internal steps until  *
 *         it has reached or just passed the user specified tout  *
 *         parameter. The solver then interpolates in order to    *
 *         return an approximate value of y(tout). The ONE_STEP   *
 *         option tells the solver to just take one internal step *
 *         and return the solution at the point reached by that   *
 *         step.                                                  *
 *                                                                *
 ******************************************************************/

enum { ADAMS, BDF };              /* lmm */

enum { FUNCTIONAL, NEWTON };      /* iter */

enum { SS, SV };                  /* itol */

enum { SIMULTANEOUS, STAGGERED, STAGGERED1 }; /* ism */

enum { ALLSENS, ONESENS };        /* ifS */
    
enum { FULL, PARTIAL };           /* errcon */
    
enum { NORMAL, ONE_STEP };        /* itask */

/******************************************************************
 *                                                                *
 * Type : RhsFn                                                   *
 *----------------------------------------------------------------*        
 * The f function which defines the right hand side of the ODE    *
 * system y' = f(t,y) must have type RhsFn.                       *
 * f takes as input the problem size N, the independent variable  *
 * value t, and the dependent variable vector y.  It stores the   * 
 * result of f(t,y) in the vector ydot.  The y and ydot arguments *
 * are of type N_Vector.                                          *
 * (Allocation of memory for ydot is handled within CVODE.)       *
 * The f_data parameter is the same as the f_data                 *
 * parameter passed by the user to the CVodeMalloc routine. This  *
 * user-supplied pointer is passed to the user's f function       *
 * every time it is called.                                       *
 * A RhsFn f does not have a return value.                        *
 *                                                                *
 ******************************************************************/

typedef void (*RhsFn)(realtype t, N_Vector y, 
                      N_Vector ydot, void *f_data);

/******************************************************************
 *                                                                *
 * Type : SensRhsFn                                               *
 *----------------------------------------------------------------*  
 * The fS function which defines the right hand side of the       *
 * sensitivity ODE systems s' = f_y * s + f_p must have type      *
 * SensRhsFn.                                                     *
 *                                                                *
 ******************************************************************/

typedef void (*SensRhsFn)(integertype Ns, realtype t, 
                          N_Vector y, N_Vector ydot, 
                          N_Vector *yS, N_Vector *ySdot, 
                          void *fS_data,  
                          N_Vector tmp1, N_Vector tmp2);

/******************************************************************
 *                                                                *
 * Type : SensRhs1Fn                                              *
 *----------------------------------------------------------------*  
 * The fS function which defines the right hand side of the i-th  *
 * sensitivity ODE system s_i' = f_y * s_i + f_p must have type   *
 * SensRhs1Fn.                                                    *
 *                                                                *
 ******************************************************************/

typedef void (*SensRhs1Fn)(integertype Ns, realtype t, 
                           N_Vector y, N_Vector ydot, 
                           integertype iS, N_Vector yS, N_Vector ySdot, 
                           void *fS_data,
                           N_Vector tmp1, N_Vector tmp2);

/******************************************************************
 *                                                                *
 * Type : QuadRhsFn                                               *
 *----------------------------------------------------------------*        
 *                                                                *
 * A QuadRhsFn f does not have a return value.                    *
 *                                                                *
 ******************************************************************/

typedef void (*QuadRhsFn)(realtype t, N_Vector y, N_Vector qdot, 
                          void *fQ_data);

/******************************************************************
 *                                                                *
 * Function : CVodeMalloc                                         *
 *----------------------------------------------------------------*
 * CVodeMalloc allocates and initializes memory for a problem to  *
 * to be solved by CVODE.                                         *
 *                                                                *
 * f       is the right hand side function in y' = f(t,y).        *          
 *                                                                *
 * t0      is the initial value of t.                             *
 *                                                                *
 * y0      is the initial condition vector y(t0).                 *
 *                                                                *
 * lmm     is the type of linear multistep method to be used.     *
 *            The legal values are ADAMS and BDF (see previous    *
 *            description).                                       *
 *                                                                *
 * iter    is the type of iteration used to solve the nonlinear   *
 *            system that arises during each internal time step.  *
 *            The legal values are FUNCTIONAL and NEWTON.         *
 *                                                                *
 * itol    is the type of tolerances to be used.                  *
 *            The legal values are:                               *
 *               SS (scalar relative and absolute  tolerances),   *
 *               SV (scalar relative tolerance and vector         *
 *                   absolute tolerance).                         *
 *                                                                *
 * reltol  is a pointer to the relative tolerance scalar.         *
 *                                                                *
 * abstol  is a pointer to the absolute tolerance scalar or       *
 *            an N_Vector of absolute tolerances.                 *
 *                                                                *
 * The parameters itol, reltol, and abstol define a vector of     *
 * error weights, ewt, with components                            *
 *   ewt[i] = 1/(reltol*abs(y[i]) + abstol)   (if itol = SS), or  *
 *   ewt[i] = 1/(reltol*abs(y[i]) + abstol[i])   (if itol = SV).  *
 * This vector is used in all error and convergence tests, which  *
 * use a weighted RMS norm on all error-like vectors v:           *
 *    WRMSnorm(v) = sqrt( (1/N) sum(i=1..N) (v[i]*ewt[i])^2 ),    *
 * where N is the problem dimension.                              *
 *                                                                *
 * f_data  is a pointer to user data that will be passed to the   *
 *             user's f function every time f is called.          *
 *                                                                *
 * errfp   is the file pointer for an error file where all CVODE  *
 *            warning and error messages will be written. This    *
 *            parameter can be stdout (standard output), stderr   *
 *            (standard error), a file pointer (corresponding to  *
 *            a user error file opened for writing) returned by   *
 *            fopen, or NULL. If the user passes NULL, then all   *
 *            messages will be written to standard output.        *
 *                                                                *
 * optIn   is a flag indicating whether there are any optional    *
 *            inputs from the user in the arrays iopt and ropt.   *
 *            Pass FALSE to indicate no optional inputs and TRUE  *
 *            to indicate that optional inputs are present.       *
 *                                                                *
 * iopt    is the user-allocated array (of size OPT_SIZE given    *
 *            later) that will hold optional integer inputs and   *
 *            outputs.  The user can pass NULL if he/she does not *
 *            wish to use optional integer inputs or outputs.     *
 *            If optIn is TRUE, the user should preset to 0 those *
 *            locations for which default values are to be used.  *
 *                                                                *
 * ropt    is the user-allocated array (of size OPT_SIZE given    *
 *            later) that will hold optional real inputs and      *
 *            outputs.  The user can pass NULL if he/she does not *
 *            wish to use optional real inputs or outputs.        *
 *            If optIn is TRUE, the user should preset to 0.0 the *
 *            locations for which default values are to be used.  *
 *                                                                *
 * machEnv is a pointer to machine environment-specific           *
 *            information.                                        *
 *                                                                *
 * Note: The tolerance values may be changed in between calls to  *
 *       CVode for the same problem. These values refer to        *
 *       (*reltol) and either (*abstol), for a scalar absolute    *
 *       tolerance, or the components of abstol, for a vector     *
 *       absolute tolerance.                                      *
 *                                                                * 
 * If successful, CVodeMalloc returns a pointer to initialized    *
 * problem memory. This pointer should be passed to CVode. If     *
 * an initialization error occurs, CVodeMalloc prints an error    *
 * message to the file specified by errfp and returns NULL.       *
 *                                                                *
 ******************************************************************/

void *CVodeMalloc(RhsFn f, realtype t0, N_Vector y0, 
                  int lmm, int iter, int itol, realtype *reltol, 
                  void *abstol, void *f_data, FILE *errfp, 
                  booleantype optIn, long int iopt[], realtype ropt[],
                  M_Env machEnv);
 
 /******************************************************************
 *                                                                *
 * Function : CVodeSensMalloc                                     *
 *----------------------------------------------------------------*
 * CVodeSensMalloc allocates and initializes memory related to    *
 * sensitivity computations.                                      *
 *                                                                *
 * cvode_mem is a pointer to CVODE memory returned by CVodeMalloc.*
 *                                                                *
 * Ns        is the number of sensitivities to be computed        *
 *                                                                *
 * ism       is the type of corrector used in sensitivity         *
 *           analysis. The legal values are: SIMULTANEOUS,        *
 *           STAGGERED, and STAGGERED1 (see previous description) *
 *                                                                *
 * p         is a pointer to problem parameters with respect to   *
 *           which sensitivities may be computed (see description *
 *           of plist below). If the right hand sides of the      *
 *           sensitivity equations are to be evaluated by the     *
 *           difference quotient routines provided with CVODES,   *
 *           then p must also be a field in the user data         *
 *           structure pointed to by f_data.                      *
 *                                                                *
 * pbar      is a pointer to scaling factors used in computing    *
 *           sensitivity absolute tolerances as well as by the    *
 *           CVODES difference quotient routines for sensitivty   *
 *           right hand sides. pbar[i] must give the order of     *
 *           magnitude of parameter p[i]. Typically, if p[i] is   *
 *           nonzero, pbar[i]=p[i].                               *
 *                                                                *
 * plist     is a pointer to a list of parameters with respect to *
 *           which sensitivities are to be computed.              *
 *           If plist[j]=i, then sensitivities with respect to    *
 *           the i-th parameter (i.e. p[i-1]) will be computed.   *
 *           A negative plist entry also indicates that the       *
 *           corresponding parameter affects only the initial     *
 *           conditions of the ODE and not its right hand side.   *
 *                                                                *
 * ifS       is the type of sensitivty right hand side. The legal *
 *           values are ALLSENS and ONESENS (see previous         *
 *           description).                                        *
 *                                                                *
 * fS        is the sensitivity right hand side. If fS=NULL is    *
 *           passed, CVODES will use internal differnce quotient  *
 *           routines to evaluate them.                           *
 *                                                                *
 * errcon    is the type of error control. The legal values are   *
 *           FULL and PARTIAL (see previous description).         *
 *                                                                *
 * rhomax    controls the selection of finite difference schemes  *
 *           used in evaluating the sensitivity right hand sides. *
 *           Its value is ignored if fS!=NULL.                    *
 *                                                                *
 * yS0       is the array of initial condition vectors for        *
 *           sensitivity variables.                               * 
 *                                                                *
 * reltolS   is a pointer to the sensitivity relative tolerance   *
 *           scalar. If reltolS=NULL is passed, CVODES will use   *
 *           reltolS = reltol.                                    * 
 *                                                                *
 * abstolS   is a pointer to the array of sensitivity absolute    *
 *           tolerance scalars or a pointer to the array of       *
 *           N_Vector sensitivity absolute tolerances. If         *
 *           abstolS=NULL is passed, CVODES will allocate space   *
 *           for abstolS (based on the value of itol) and set the *
 *           sensitivity absolute tolerances based on the values  *
 *           of abstol and pbar.                                  *
 *                                                                *
 * fS_data   is a pointer to user data that will be passed to the *
 *           user's fS function every time fS is called.          *
 *                                                                *
 * If successful, CVodeSensMalloc returns SUCCESS. If an          *
 * initialization error occurs, CVodeSensMalloc prints an error   *
 * message to the file specified by errfp and returns one of      *
 * the error flags defined below.                                 *
 *                                                                *
 ******************************************************************/

int CVodeSensMalloc(void *cvode_mem, integertype Ns, int ism, 
                    realtype *p, realtype *pbar, integertype *plist, 
                    int ifS, void *fS, int errcon, realtype rhomax, 
                    N_Vector *yS0, realtype *reltolS, void *abstolS,
                    void *fS_data);
    
/* CVodeSensMalloc return values: */

/* SUCCESS = 0  (Defined under CVode return values, but listed
                 here also for completeness)                     */
enum {SCVM_NO_MEM = -1, SCVM_ILL_INPUT = -2, SCVM_MEM_FAIL = -3};

/******************************************************************
 *                                                                *
 * Function : CVodeQuadMalloc                                     *
 *----------------------------------------------------------------*
 * CVodeQuadMalloc allocates and initializes memory related to    *
 * quadrature integration.                                        *
 *                                                                *
 * cvode_mem is a pointer to CVODE memory returned by CVodeMalloc.*
 *                                                                *
 * fQ        is the user-provided integrand routine.              *
 *                                                                *
 * errconQ   is the type of error control. The legal values are   *
 *           FULL and PARTIAL (see previous description).         *
 *                                                                *
 * reltolQ   is a pointer to the quadrature relative tolerance    *
 *           scalar.                                              *
 *                                                                *
 * abstolQ   is a pointer to the absolute tolerance scalar or     *
 *           an N_Vector of absolute tolerances for quadrature    *
 *           variables.                                           *
 *                                                                *
 * fQ_data   is a pointer to user data that will be passed to the *
 *           user's fQ function every time fQ is called.          *
 *                                                                *
 * machEnvQ  is a pointer to machine environment-specific         *
 *           information for Nvectors containing quadrature       *
 *           variables.                                           *
 *                                                                *
 * If successful, CVodeQuadMalloc returns SUCCESS. If an          *
 * initialization error occurs, CVodeQuadMalloc prints an error   *
 * message to the file specified by errfp and returns one of      *
 * the error flags defined below.                                 *
 *                                                                *
 ******************************************************************/

int CVodeQuadMalloc(void *cvode_mem, 
                    QuadRhsFn fQ, int errconQ,
                    realtype *reltolQ, void *abstolQ,
                    void *fQ_data, M_Env machEnvQ);
    
/* CVodeQuadMalloc return values: */

/* SUCCESS = 0  (Defined under CVode return values, but listed
                 here also for completeness)                     */
enum {QCVM_NO_MEM = -1, QCVM_ILL_INPUT = -2, QCVM_MEM_FAIL = -3};

/******************************************************************
 *                                                                *
 * Function : CVodeReInit                                         *
 *----------------------------------------------------------------*
 * CVodeReInit re-initializes CVode for the solution of a problem,*
 * where a prior call to CVodeMalloc has been made with the same  *
 * problem size N. CVodeReInit performs the same input checking   *
 * and initializations that CVodeMalloc does (except for N).      *
 * But it does no memory allocation, assuming that the existing   *
 * internal memory is sufficient for the new problem.             *
 *                                                                *
 * The use of CVodeReInit requires that the maximum method order, *
 * maxord, is no larger for the new problem than for the problem  *
 * specified in the last call to CVodeMalloc.  This condition is  *
 * automatically fulfilled if the multistep method parameter lmm  *
 * is unchanged (or changed from ADAMS to BDF) and the default    *
 * value for maxord is specified.                                 *
 *                                                                *
 * If iter=NEWTON, then following the call to CVodeReInit, a call *
 * to the linear solver specification routine is necessary if a   *
 * different linear solver is chosen, but may not be otherwise.   *
 * If the same linear solver is chosen, and there are no changes  *
 * in the input parameters to the specification routine, then no  *
 * call to that routine is needed.                                *
 * If there are changes in parameters, but they do not increase   *
 * the linear solver memory size, then a call to the corresponding*
 * CVReInit<linsol> routine must made to communicate the new      *
 * parameters; in that case the linear solver memory is reused.   *
 * If the parameter changes do increase the linear solver memory  *
 * size, then the main linear solver specification routine must be*
 * called.  See the linear solver documentation for full details. *
 *                                                                *
 * The first argument to CVodeReInit is:                          *
 *                                                                *
 * cvode_mem = pointer to CVODE memory returned by CVodeMalloc.   *
 *                                                                *
 * All the remaining arguments to CVodeReInit have names and      *
 * meanings identical to those of CVodeMalloc.  Note that the     *
 * problem size N is not passed as an argument to CVodeReInit,    *
 * as that is assumed to be unchanged since the CVodeMalloc call. *
 *                                                                *
 * The return value of CVodeReInit is equal to SUCCESS = 0 if     *
 * there were no errors; otherwise it is a negative int equal to: *
 *   CVREI_NO_MEM     indicating cvode_mem was NULL, or           *
 *   CVREI_ILL_INPUT  indicating an input argument was illegal    *
 *                    (including an attempt to increase maxord).  *
 * In case of an error return, an error message is also printed.  *
 *                                                                *
 * Note: the reported workspace sizes iopt[LENRW] and iopt[LENIW] *
 * are left unchanged from the values computed by CVodeMalloc, and*
 * so may be larger than would be computed for the new problem.   *
 ******************************************************************/

int CVodeReInit(void *cvode_mem, RhsFn f, realtype t0, N_Vector y0, 
                int lmm, int iter, int itol, realtype *reltol, 
                void *abstol, void *f_data, FILE *errfp, 
                booleantype optIn, long int iopt[], 
                realtype ropt[], M_Env machEnv);

/* CVodeReInit return values: */

/* SUCCESS = 0  (Defined under CVode return values, but listed
                 here also for completeness)                     */
enum {CVREI_NO_MEM = -1, CVREI_ILL_INPUT = -2};

/******************************************************************
 *                                                                *
 * Function : CVodeSensReInit                                     *
 *----------------------------------------------------------------*
 * CVodeSensReInit re-initializes CVODE's sensitivity related     *
 * memory for a problem, assuming it has already been allocated   *
 * in prior calls to CVodeMalloc and CvodeSensMalloc.             *
 *                                                                *
 * All problem specification inputs are checked for errors.       *
 * The number of sensitivities Ns is assumed to be unchanged      *
 * since the previous call to CVodeSensMalloc.                    *
 * If any error occurs during initialization, it is reported to   *
 * the file whose file pointer is errfp.                          *
 *                                                                *
 * CVodeSensReInit potentially does some minimal memory allocation*
 * (for the sensitivity absolute tolerance and for arrays of      *
 * counters used by the STAGGERED1 method).                       *
 *                                                                *
 * The return value is equal to SUCCESS = 0 if there were no      *
 * errors; otherwise it is a negative int equal to:               *
 *   SCVREI_NO_MEM    indicating cvode_mem was NULL, or           *
 *   SCVREI_NO_SENSI  indicating there was not a prior call to    *
 *                    CVodeSensMalloc.                            *   
 *   SCVREI_ILL_INPUT indicating an input argument was illegal    *
 *                    (including an attempt to increase maxord).  *
 *   SCVREI_MEM_FAIL  indicating a memory request failed.         *
 * In case of an error return, an error message is also printed.  *
 *                                                                *
 ******************************************************************/

int CVodeSensReInit(void *cvode_mem, int ism, 
                    realtype *p, realtype *pbar, integertype *plist, 
                    int ifS, void *fS, int errcon, realtype rhomax, 
                    N_Vector *yS0, realtype *reltolS, void *abstolS,
                    void *fS_data);
  
/* CVodeSensReInit return values: */

/* SUCCESS = 0  (Defined under CVode return values, but listed
                 here also for completeness)                     */
enum {SCVREI_NO_MEM    = -1, SCVREI_NO_SENSI = -2, 
      SCVREI_ILL_INPUT = -3, SCVREI_MEM_FAIL = -4};

/******************************************************************
 *                                                                *
 * Function : CVodeQuadReInit                                     *
 *----------------------------------------------------------------*
 * CVodeQuadReInit re-initializes CVODE's quadrature related      *
 * memory for a problem, assuming it has already been allocated   *
 * in prior calls to CVodeMalloc and CvodeQuadMalloc.             *
 *                                                                *
 * All problem specification inputs are checked for errors.       *
 * The number of quadratures Nq is assumed to be unchanged        *
 * since the previous call to CVodeQuadMalloc.                    *
 * If any error occurs during initialization, it is reported to   *
 * the file whose file pointer is errfp.                          *
 *                                                                *
 * In case of an error return, an error message is also printed.  *
 *                                                                *
 ******************************************************************/

int CVodeQuadReInit(void *cvode_mem, QuadRhsFn fQ, int errconQ,
                    realtype *reltolQ, void *abstolQ, 
                    void *fQ_data, M_Env machEnvQ);

/* CVodeQuadReInit return values: */

/* SUCCESS = 0  (Defined under CVode return values, but listed
                 here also for completeness)                     */
enum {QCVREI_NO_MEM    = -1, QCVREI_NO_QUAD = -2, 
      QCVREI_ILL_INPUT = -3};

/******************************************************************
 *                                                                *
 * Function : CVode                                               *
 *----------------------------------------------------------------*
 * CVode integrates the ODE over an interval in t.                *
 * If itask is NORMAL, then the solver integrates from its        *
 * current internal t value to a point at or beyond tout, then    *
 * interpolates to t = tout and returns y(tout) in the user-      *
 * allocated vector yout. If itask is ONE_STEP, then the solver   *
 * takes one internal time step and returns in yout the value of  *
 * y at the new internal time. In this case, tout is used only    *
 * during the first call to CVode to determine the direction of   *
 * integration and the rough scale of the problem. In either      *
 * case, the time reached by the solver is placed in (*t). The    *
 * user is responsible for allocating the memory for this value.  *
 *                                                                *
 * cvode_mem is the pointer to CVODE memory returned by           *
 *              CVodeMalloc.                                      *
 *                                                                *
 * tout  is the next time at which a computed solution is desired *
 *                                                                *
 * yout  is the computed solution vector. In NORMAL mode with no  *
 *          errors, yout=y(tout).                                 *
 *                                                                *
 * t     is a pointer to a real location. CVode sets (*t) to the  *
 *          time reached by the solver and returns yout=y(*t).    *
 *                                                                *
 * itask is either NORMAL or ONE_STEP mode. These two modes have  *
 *          described above.                                      *
 *                                                                *
 * The return values for CVode are defined later in this file.    *
 * Here is a brief description of each return value:              *
 *                                                                *
 * SUCCESS       : CVode succeeded.                               *
 *                                                                *
 * CVODE_NO_MEM  : The cvode_mem argument was NULL.               *
 *                                                                *
 * ILL_INPUT     : One of the inputs to CVode is illegal. This    *
 *                 includes the situation when a component of the *
 *                 error weight vectors becomes < 0 during        *
 *                 internal time-stepping. The ILL_INPUT flag     *
 *                 will also be returned if the linear solver     *
 *                 routine CV--- (called by the user after        *
 *                 calling CVodeMalloc) failed to set one of the  *
 *                 linear solver-related fields in cvode_mem or   *
 *                 if the linear solver's init routine failed. In *
 *                 any case, the user should see the printed      *
 *                 error message for more details.                *
 *                                                                *
 * TOO_MUCH_WORK : The solver took mxstep internal steps but      *
 *                 could not reach tout. The default value for    *
 *                 mxstep is MXSTEP_DEFAULT = 500.                *
 *                                                                *
 * TOO_MUCH_ACC  : The solver could not satisfy the accuracy      *
 *                 demanded by the user for some internal step.   *
 *                                                                *
 * ERR_FAILURE   : Error test failures occurred too many times    *
 *                 (= MXNEF = 7) during one internal time step or *
 *                 occurred with |h| = hmin.                      *
 *                                                                *
 * CONV_FAILURE  : Convergence test failures occurred too many    *
 *                 times (= MXNCF = 10) during one internal time  *
 *                 step or occurred with |h| = hmin.              *
 *                                                                *
 * SETUP_FAILURE : The linear solver's setup routine failed in an *
 *                 unrecoverable manner.                          *
 *                                                                *
 * SOLVE_FAILURE : The linear solver's solve routine failed in an *
 *                 unrecoverable manner.                          *
 *                                                                *
 ******************************************************************/

int CVode(void *cvode_mem, realtype tout, N_Vector yout, 
          realtype *t, int itask);

/* CVode return values */

enum { SUCCESS=0, TSTOP_RETURN=1, CVODE_NO_MEM=-1, ILL_INPUT=-2, 
       TOO_MUCH_WORK=-3, TOO_MUCH_ACC=-4, ERR_FAILURE=-5, 
       CONV_FAILURE=-6, SETUP_FAILURE=-7, SOLVE_FAILURE=-8 };

/******************************************************************
 *                                                                *
 * Function : CVodeDky                                            *
 *----------------------------------------------------------------*
 * CVodeDky computes the kth derivative of the y function at      *
 * time t, where tn-hu <= t <= tn, tn denotes the current         *
 * internal time reached, and hu is the last internal step size   *
 * successfully used by the solver. The user may request          *
 * k=0, 1, ..., qu, where qu is the current order. The            *
 * derivative vector is returned in dky. This vector must be      *
 * allocated by the caller. It is only legal to call this         *
 * function after a successful return from CVode.                 *
 *                                                                *
 * cvode_mem is the pointer to CVODE memory returned by           *
 *              CVodeMalloc.                                      *
 *                                                                *
 * t   is the time at which the kth derivative of y is evaluated. *
 *        The legal range for t is [tn-hu,tn] as described above. *
 *                                                                *
 * k   is the order of the derivative of y to be computed. The    *
 *        legal range for k is [0,qu] as described above.         *
 *                                                                *
 * dky is the output derivative vector [(D_k)y](t).               *
 *                                                                *
 * The return values for CVodeDky are defined later in this file. *
 * Here is a brief description of each return value:              *
 *                                                                *
 * OKAY : CVodeDky succeeded.                                     *
 *                                                                *
 * BAD_K : k is not in the range 0, 1, ..., qu.                   *
 *                                                                *
 * BAD_T : t is not in the interval [tn-hu,tn].                   *
 *                                                                *
 * BAD_DKY : The dky argument was NULL.                           *
 *                                                                *
 * DKY_NO_MEM : The cvode_mem argument was NULL.                  *
 *                                                                * 
 ******************************************************************/

int CVodeDky(void *cvode_mem, realtype t, int k, N_Vector dky);

/* CVodeDky return values */

enum { OKAY=0, BAD_K=-1, BAD_T=-2, BAD_DKY=-3, DKY_NO_MEM=-4 };
 
/******************************************************************
 *                                                                *
 * Functions : CVodeSensExtract, CVodeSensDkyAll, CVodeSensDky    *
 *----------------------------------------------------------------*
 * CVodeSensDky computes the kth derivative of the is-th          *
 * sensitivity (is=1, 2, ..., Ns) of the y function at time t,    *
 * where tn-hu <= t <= tn, tn denotes the current internal time   *
 * reached, and hu is the last internal step size successfully    *
 * used by the solver. The user may request k=0, 1, ..., qu,      *
 * where qu is the current order.                                 *
 * The is-th sensitivity derivative vector is returned in dky.    *
 * This vector must be allocated by the caller. It is only legal  *
 * to call this function after a successful return from CVode     *
 * with sensitivty computations enabled.                          *
 * Arguments have the same meaning as in CVodeDky.                *
 *                                                                * 
 * CVodeSensDkyAll computes the k-th derivative of all            *
 * sensitivities of the y function at time t. It repeatedly calls *
 * CVodeSensDky. The argument dkyA must be a pointer to N_Vector  *
 * and must be allocated by the user to hold at least Ns vectors. *
 *                                                                * 
 * CVodeSensExtract returns sensitivities of the y function at    *
 * time t. It calls CVodeSensDkyAll with k=0. The argument ySout  *
 * must be a pointer to N_Vector and must be allocated by the     *
 * user to hold at least Ns vectors.                              *
 *                                                                *
 * Return values are similar to those of CVodeDky. Additionally,  *
 * CVodeSensDky can return DKY_NO_SENSI if sensitivities were     *
 * not computed and BAD_IS if is < 0 or is >= Ns.                 *
 *                                                                * 
 ******************************************************************/

int CVodeSensExtract(void *cvode_mem, realtype t, N_Vector *ySout);

int CVodeSensDkyAll(void *cvode_mem, realtype t, int k, N_Vector *dkyA);
    
int CVodeSensDky(void *cvode_mem, realtype t, int k, integertype is, 
                 N_Vector dky);
    
/* Additional CVodeSensExtract/CVodeSensDky output values */

#define DKY_NO_SENSI -5
#define BAD_IS -6

/******************************************************************
 *                                                                *
 * Functions : CVodeQuadExtract, CVodeQuadDky                     *
 *----------------------------------------------------------------*
 * Return values are similar to those of CVodeDky. Additionally,  *
 * CVodeQuadDky can return DKY_NO_QUAD if quadratures were not    *
 * computed.                                                      *
 *                                                                * 
 ******************************************************************/

int CVodeQuadExtract(void *cvode_mem, realtype t, N_Vector yQout);

int CVodeQuadDky(void *cvode_mem, realtype t, int k, N_Vector dky);

/* Additional CVodeQuadExtract/CVodeQuadDky output values */

#define DKY_NO_QUAD -7

/******************************************************************
 *                                                                *
 * Function : CVodeGetEwt and CVodeGetEwtS                        *
 *----------------------------------------------------------------*
 * This routines return the weight vectors for states and         *
 * sensitivities in weight and weightS, respectively. They are    *
 * provided for use in user-defined sensitivity right hand side   *
 * routines based on finite differences. Note that the user need  *
 * not allocate space for either weight or weightS.               *
 *                                                                *
 ******************************************************************/

int CVodeGetEwt(void *cvode_mem, N_Vector weight);
int CVodeGetEwtS(void *cvode_mem, N_Vector *weightS);

/* CVodeGetEwt/CVodeGetEwtS return values */

/* OKAY = 0  (Defined under CVodeDky return values but listed 
              here also for compleetness)                         */

enum {GEWT_NO_MEM=-1};

/******************************************************************
 *                                                                *
 * Function : CVodeMemExtract                                     *
 *----------------------------------------------------------------*
 * This routine extracts additional diagnostics information from  *
 * the CVODE memory. Arrays that were allocated by the user are   *
 * loaded with the appropriate information from cvode_mem.        *
 *                                                                *
 * cvode_mem is a pointer to CVODE memory returned by CVodeMalloc.*
 *                                                                *
 * n_niS1    is a vector of Ns nonlinear iteration counters for   *
 *           sensitivity variables in the STAGGERED1 method.      *
 *                                                                *
 * n_cfnS1   is a vector of Ns nonlinear solver convergence       *
 *           failure counters for sensitivity variables in the    *
 *           STAGGERED1 method.                                   *
 *                                                                *
 *                                                                *
 ******************************************************************/

int CVodeMemExtract(void *cvode_mem, 
                    long int *n_niS1, long int *n_cfnS1);

/* CVodeMemExtract return values */

/* OKAY = 0  (Defined under CVodeDky return values but listed 
              here also for compleetness)                         */

enum {MEXT_NO_MEM=-1};

/******************************************************************
 *                                                                *
 * Function : CVodeFree                                           *
 *----------------------------------------------------------------*
 * CVodeFree frees the problem memory cvode_mem allocated by      *
 * CVodeMalloc.  Its only argument is the pointer cvode_mem       *
 * returned by CVodeMalloc.                                       *
 *                                                                *
 ******************************************************************/

void CVodeFree(void *cvode_mem);
 
/******************************************************************
 *                                                                *
 * Optional Inputs and Outputs                                    *
 *----------------------------------------------------------------*
 * The user should declare two arrays for optional input and      *
 * output, an iopt array for optional integer input and output    *
 * and an ropt array for optional real input and output. The      *
 * size of both these arrays should be OPT_SIZE.                  *
 * So the user's declaration should look like:                    *
 *                                                                *
 * long int iopt[OPT_SIZE];                                       *
 * realtype ropt[OPT_SIZE];                                       *
 *                                                                *
 * The enumerations below the OPT_SIZE definition                 *
 * are indices into the iopt and ropt arrays. Here is a brief     *
 * description of the contents of these positions:                *
 *                                                                *
 * iopt[MAXORD] : maximum lmm order to be used by the solver.     *
 *                Optional input. (Default = 12 for ADAMS, 5 for  *
 *                BDF).                                           *
 *                                                                *
 * iopt[MXSTEP] : maximum number of internal steps to be taken by *
 *                the solver in its attempt to reach tout.        *
 *                Optional input. (Default = 500).                *
 *                                                                *
 * iopt[MXHNIL] : maximum number of warning messages issued       * 
 *                by the solver that t+h==t on the next internal  *
 *                step. A value of -1 means no such messages are  *
 *                issued.Optional input. (Default = 10).          *
 *                                                                *
 * iopt[NST]    : cumulative number of internal steps taken by    *
 *                the solver (total so far).  Optional output.    *
 *                                                                *
 * iopt[NFE]    : number of calls to the user's f function.       *
 *                Optional output.                                *
 *                                                                *
 * iopt[NSETUPS] : number of calls made to the linear solver's    *
 *                 setup routine. Optional output.                *
 *                                                                *
 * iopt[NNI]     : number of NEWTON iterations performed.         *
 *                 Optional output.                               *
 *                                                                *
 * iopt[NCFN]    : number of nonlinear convergence failures       *
 *                 that have occurred. Optional output.           *
 *                                                                *
 * iopt[NETF]    : number of local error test failures that       *
 *                 have occurred. Optional output.                *
 *                                                                *
 * iopt[QU]      : order used during the last internal step.      *
 *                 Optional output.                               *
 *                                                                *
 * iopt[QCUR]    : order to be used on the next internal step.    *
 *                 Optional output.                               *
 *                                                                *
 * iopt[LENRW]   : size of required CVODE internal real work      *
 *                 space, in realtype words.  Optional output.    *
 *                                                                *
 * iopt[LENIW]   : size of required CVODE internal integer work   *
 *                 space, in integertype words.  Optional output. *
 *                                                                *
 * iopt[SLDET]   : Flag to turn on/off stability limit detection  *
 *                 (1 = on, 0 = off). When BDF is used and order  *
 *                 is 3 or greater, CVsldet is call to detect     *
 *                 stability limit.  If limit is detected, the    *
 *                 order is reduced. Optional input.              *
 *                                                                *
 * iopt[ISTOP]   : Flag to turn on/off testing for tstop (1=on,   *
 *                 0=off). When on, CVODES uses ropt[TSTOP] (see  *
 *                 below) as the value tstop of the independent   *
 *                 variable past which the solution is not to     *
 *                 proceed. Optional input.                       *
 *                                                                *
 * iopt[NOR]     : Number of order reductions due to              *
 *                 stability limit detection.                     *
 *                 Optional output.                               *
 *                                                                *
 * iopt[NFSE]    : number of calls made to the sensitivity        *
 *                 r.h.s. evaluation routine. Optional output.    *
 *                                                                *
 * iopt[NNIS]    : number of Newton iterations performed during   *
 *                 staggered sensitivity corrections (sum over    *
 *                 all sensitivities in the STAGGERED1 case).     *
 *                 Optional output.                               *
 *                                                                *
 * iopt[NCFNS]   : number of nonlinear convergence failures       *
 *                 during the staggered sensitivity corrections   *
 *                 sum over all sensitivities in the STAGGERED1   *
 *                 cse). Optional output.                         *
 *                                                                *
 * iopt[NETFS]   : number of error test failures for sensitivity  *
 *                 variables (staggered approach).                *
 *                 Optional output.                               *
 *                                                                *
 * iopt[NSETUPSS]: number of calls made to the linear solver's    *
 *                 setup routine due only to sensitivities in the *
 *                 staggered approach. Optional output.           *
 *                                                                *
 * iopt[NFQE]    : number of calls made to the quadrature         *
 *                 integrand evaluation routine. Optional output. *
 *                                                                *
 * iopt[NETFQ]   : number of error test failures for quadrature   *
 *                 variables.                                     *
 *                 Optional output.                               *
 *                                                                *
 * ropt[H0]      : initial step size. Optional input.             *
 *                                                                *
 * ropt[HMAX]    : maximum absolute value of step size allowed.   *
 *                 Optional input. (Default is infinity).         *
 *                 Note: If optIn = TRUE, the value of ropt[HMAX] *
 *                 is examined on every call to CVode, and so can *
 *                 be changed between calls.                      *
 *                                                                *
 * ropt[HMIN]    : minimum absolute value of step size allowed.   *
 *                 Optional input. (Default is 0.0).              *
 *                                                                *
 * ropt[TSTOP]   : the independent variable value past which the  *
 *                 solution is not to proceed. Testing for this   *
 *                 condition must be turned on through            *
 *                 iopt[ISTOP] (see above). Optional input.       *
 *                                                                *
 * ropt[H0U]     : actual initial step size used.                 *
 *                 Optional output.                               *
 *                                                                *
 * ropt[HU]      : step size for the last internal step.          *
 *                 Optional output.                               *
 *                                                                *
 * ropt[HCUR]    : step size to be attempted on the next internal *
 *                 step. Optional output.                         *
 *                                                                *
 * ropt[TCUR]    : current internal time reached by the solver.   *
 *                 Optional output.                               *
 *                                                                *
 * ropt[TOLSF]   : a suggested factor by which the user's         *
 *                 tolerances should be scaled when too much      *
 *                 accuracy has been requested for some internal  *
 *                 step. Optional output.                         *
 *                                                                *
 ******************************************************************/

/* iopt, ropt array sizes */

#define OPT_SIZE 40
 

/* iopt and ropt offsets                                          *
 * The constants CVODE_IOPT_SIZE and CVODE_ROPT_SIZE are equal to *
 * the number of integer and real optional inputs and outputs     *
 * actually accessed in cvode.c.  The locations beyond these      *
 * values are used by the linear solvers.                         */

#define CVODE_IOPT_SIZE 23
#define CVODE_ROPT_SIZE  9

/* iopt indices */
enum { MAXORD, MXSTEP, MXHNIL,
       NST, NFE, NSETUPS, NNI, NCFN, NETF, QU, QCUR,
       LENRW, LENIW, SLDET, ISTOP, NOR, 
       NFSE, NNIS, NCFNS, NETFS, NSETUPSS, NFQE, NETFQ};

/* ropt indices */

enum { H0, HMAX, HMIN, TSTOP, H0U,
       HU, HCUR, TCUR, TOLSF };

/* Basic CVODE constants */

#define ADAMS_Q_MAX 12       /* max value of q for lmm == ADAMS   */
#define BDF_Q_MAX    5      /* max value of q for lmm == BDF      */
#define Q_MAX  ADAMS_Q_MAX  /* max value of q for either lmm      */
#define L_MAX  (Q_MAX+1)    /* max value of L for either lmm      */
#define NUM_TESTS    5      /* number of error test quantities    */


/******************************************************************
 *                                                                *
 * Types : struct CVodeMemRec, CVodeMem                           *
 *----------------------------------------------------------------*
 * The type CVodeMem is type pointer to struct CVodeMemRec. This  *
 * structure contains fields to keep track of problem state.      *
 *                                                                *
 ******************************************************************/
  
typedef struct CVodeMemRec {
    
  realtype cv_uround;      /* machine unit roundoff                        */

  /*---------------------------- 
    Problem Specification Data 
  ----------------------------*/

  RhsFn cv_f;              /* y' = f(t,y(t))                               */
  void *cv_f_data;         /* user pointer passed to f                     */
  int cv_lmm;              /* lmm = ADAMS or BDF                           */
  int cv_iter;             /* iter = FUNCTIONAL or NEWTON                  */
  int cv_itol;             /* itol = SS or SV                              */
  realtype *cv_reltol;     /* ptr to relative tolerance                    */
  void *cv_abstol;         /* ptr to absolute tolerance                    */

  /*--------------------------
    Sensitivity Related Data 
  --------------------------*/

  booleantype cv_sensi;    /* TRUE if computing sensitivities              */
  integertype cv_Ns;       /* Number of sensitivities                      */
  SensRhsFn cv_fS;         /* fS = (df/dy)*yS + (df/dp)                    */
  SensRhs1Fn cv_fS1;       /* fS1 = (df/dy)*yS_i + (df/dp)                 */
  int cv_ifS;              /* ifS = ALLSENS or ONESENS                     */
  int cv_ism;              /* ism = SIMULTANEOUS or STAGGERED              */
  realtype *cv_p;          /* parameters in f(t,y,p)                       */
  realtype *cv_pbar;       /* scale factors for parameters                 */
  integertype *cv_plist;   /* list of sensitivities                        */
  realtype *cv_reltolS;    /* ptr to relative tolerance for sensi          */
  void *cv_abstolS;        /* ptr to absolute tolerance for sensi          */
  realtype cv_rhomax;      /* cut-off value for centered/forward finite
                              differences                                  */
  int cv_errcon;           /* errcon = FULL or PARTIAL                     */
  void *cv_fS_data;        /* user pointer passed to fS                    */

  /*--------------------------
    Quadrature Related Data 
  --------------------------*/

  booleantype cv_quad;     /* TRUE if integrating quadratures              */
  QuadRhsFn cv_fQ;
  realtype *cv_reltolQ;    /* ptr to relative tolerance for quad           */
  void *cv_abstolQ;        /* ptr to absolute tolerance for quad           */
  int cv_errconQ;
  void *cv_fQ_data;        /* user pointer passed to fQ                    */

  /*-------------------------
    Nordsieck History Array 
  -------------------------*/

  N_Vector cv_zn[L_MAX];   /* Nordsieck array, of size N x (q+1).
                              zn[j] is a vector of length N (j=0,...,q)
                              zn[j] = [1/factorial(j)] * h^j * 
                              (jth derivative of the interpolating 
                              polynomial                                   */

  /*---------------------
    Vectors of length N 
  ---------------------*/

  N_Vector cv_ewt;         /* error weight vector                          */
  N_Vector cv_y;           /* y is used as temporary storage by the solver
                              The memory is provided by the user to CVode 
                              where the vector is named yout.              */
  N_Vector cv_acor;        /* In the context of the solution of the
                              nonlinear equation, acor = y_n(m) - y_n(0).
                              On return, this vector is scaled to give
                              the estimated local error in y.              */
  N_Vector cv_tempv;       /* temporary storage vector                     */
  N_Vector cv_ftemp;       /* temporary storage vector                     */

  /*-----------------------------
    Sensitivity Related Vectors 
  -----------------------------*/

  N_Vector *cv_znS[L_MAX]; /* Nordsieck arrays for sensitivities           */
  N_Vector *cv_ewtS;       /* error weight vectors for sensitivities       */
  N_Vector *cv_yS;         /* Unlike y, this memory is not provided by
                              the user                                     */
  N_Vector *cv_acorS;      /* acorS = yS_n(m) - yS_n(0)                    */
  N_Vector *cv_tempvS;     /* temporary storage vector (~ tempv)           */
  N_Vector *cv_ftempS;     /* temporary storage vector (~ ftemp)           */

  /*-----------------------------
    Quadrature Related Vectors 
  -----------------------------*/

  N_Vector cv_znQ[L_MAX];
  N_Vector cv_ewtQ;        /* error weight vector for quadratures          */
  N_Vector cv_yQ;          /*                                              */
  N_Vector cv_acorQ;       /*                                              */
  N_Vector cv_tempvQ;      /*                                              */

  /*-------------------------------------------------
    Does CVodeSensMalloc allocate additional space?
  -------------------------------------------------*/  

  booleantype cv_abstolSalloc;   /* Is abstolS allocated by CVODES?        */
  booleantype cv_stgr1alloc;     /* Are ncfS1, ncfnS1, and nniS1 allocated 
                                    by CVODES?                             */

  /*-----------
    Step Data 
  -----------*/

  int cv_q;                /* current order                                */
  int cv_qprime;           /* order to be used on the next step            */ 
                           /* = q-1, q, or q+1                             */
  int cv_qwait;            /* number of internal steps to wait before      */
                           /* considering a change in q                    */
  int cv_L;                /* L = q + 1                                    */

  realtype cv_h;           /* current step size                            */
  realtype cv_hprime;      /* step size to be used on the next step        */ 
  realtype cv_eta;         /* eta = hprime / h                             */
  realtype cv_hscale;      /* value of h used in zn                        */
  realtype cv_tn;          /* current internal value of t                  */

  realtype cv_tau[L_MAX+1];    /* array of previous q+1 successful step
                                  sizes indexed from 1 to q+1              */
  realtype cv_tq[NUM_TESTS+1]; /* array of test quantities indexed from
                                  1 to NUM_TESTS(=5)                       */
  realtype cv_l[L_MAX];        /* coefficients of l(x) (degree q poly)     */

  realtype cv_rl1;             /* 1 / l[1]                                 */
  realtype cv_gamma;           /* gamma = h * rl1                          */
  realtype cv_gammap;          /* gamma at the last setup call             */
  realtype cv_gamrat;          /* gamma / gammap                           */

  realtype cv_crate;           /* est. corrector conv. rate in Nls         */
  realtype cv_crateS;          /* est. corrector conv. rate in NlsStgr     */
  realtype cv_acnrm;           /* | acor |                                 */
  realtype cv_acnrmS;          /* | acorS |                                */
  realtype cv_acnrmQ;          /* | acorQ |                                */
  int  cv_mnewt;               /* Newton iteration counter                 */
  int  *cv_ncfS1;              /* Array of Ns local counters for conv.  
                                  failures (used in CVStep for STAGGERED1) */

  /*--------
    Limits 
  --------*/

  int cv_qmax;             /* q <= qmax                                    */
  int cv_mxstep;           /* maximum number of internal steps for one 
                              user call                                    */
  int cv_maxcor;           /* maximum number of corrector iterations for 
                              the solution of the nonlinear equation       */
  int cv_maxcorS;
  int cv_mxhnil;           /* maximum number of warning messages issued to 
                              the user that t + h == t for the next 
                              internal step                                */

  realtype cv_hmin;        /* |h| >= hmin                                  */
  realtype cv_hmax_inv;    /* |h| <= 1/hmax_inv                            */
  realtype cv_etamax;      /* eta <= etamax                                */

  /*----------
    Counters 
  ----------*/

  long int cv_nst;         /* number of internal steps taken               */
  long int cv_nfe;         /* number of f calls                            */
  long int cv_nfSe;        /* number of fS calls                           */
  long int cv_nfQe;        /* number of fQ calls                           */

  long int cv_ncfn;        /* number of corrector convergence failures     */
  long int cv_ncfnS;       /* number of total sensi. corr. conv. failures  */
  long int *cv_ncfnS1;     /* number of sensi. corrector conv. failures    */

  long int cv_nni;         /* number of nonlinear iterations performed     */
  long int cv_nniS;        /* number of total sensi. nonlinear iterations  */
  long int *cv_nniS1;      /* number of sensi. nonlinear iterations        */

  long int cv_netf;        /* number of error test failures                */
  long int cv_netfS;       /* number of sensi. error test failures         */
  long int cv_netfQ;       /* number of quadr. error test failures         */

  long int cv_nsetups;     /* number of setup calls                        */
  long int cv_nsetupss;    /* number of setup calls due to sensitivities   */

  int      cv_nhnil;       /* number of messages issued to the user that
                              t + h == t for the next iternal step         */

  long int cv_nscon;       /* counter for STALD method                     */

  /*------------------------------- 
    Space requirements for CVODES 
  -------------------------------*/

  long int cv_lrw1;        /* no. of realtype words in 1 N_Vector y           */ 
  long int cv_liw1;        /* no. of integertype words in 1 N_Vector y        */ 
  long int cv_lrw1Q;       /* no. of realtype words in 1 N_Vector yQ          */ 
  long int cv_liw1Q;       /* no. of integertype words in 1 N_Vector yQ       */ 
  long int cv_lrw;         /* no. of realtype words in CVODES work vectors    */
  long int cv_liw;         /* no. of integertype words in CVODES work vectors */

  /*------------------
    Step size ratios
  ------------------*/

  realtype cv_etaqm1;      /* ratio of new to old h for order q-1          */
  realtype cv_etaq;        /* ratio of new to old h for order q            */
  realtype cv_etaqp1;      /* ratio of new to old h for order q+1          */

  realtype cv_ssdat[6][4]; /* scaled data array for STALD                  */

  /*--------------------
    Linear Solver Data 
  --------------------*/

  /* Linear Solver functions to be called */

  int (*cv_linit)(struct CVodeMemRec *cv_mem);

  int (*cv_lsetup)(struct CVodeMemRec *cv_mem, int convfail, 
                   N_Vector ypred, N_Vector fpred, booleantype *jcurPtr, 
                   N_Vector vtemp1, N_Vector vtemp2, N_Vector vtemp3); 

  int (*cv_lsolve)(struct CVodeMemRec *cv_mem, N_Vector b, 
                   N_Vector ycur, N_Vector fcur);

  int (*cv_lsolveS)(struct CVodeMemRec *cv_mem, N_Vector b, 
                    N_Vector ycur, N_Vector fcur, integertype is);

  void (*cv_lfree)(struct CVodeMemRec *cv_mem);

  /* Linear Solver specific memory */

  void *cv_lmem;           

  /* Flag to request a call to the setup routine */

  booleantype cv_forceSetup;

  /*--------------
    Saved Values
  --------------*/

  int cv_qu;               /* last successful q value used                 */
  long int cv_nstlp;       /* step number of last setup call               */
  realtype cv_h0u;         /* actual initial stepsize                      */
  realtype cv_hu;          /* last successful h value used                 */
  realtype cv_saved_tq5;   /* saved value of tq[5]                         */
  booleantype cv_jcur;     /* Is the Jacobian info used by
                              linear solver current?                       */
  realtype cv_tolsf;       /* tolerance scale factor                       */
  booleantype cv_setupNonNull;  /* Does setup do something?                */

  /*----------------------------------------------------------
    Flags turned ON by SensMalloc and QuadMalloc and read by
    SensReInit and QuadReInit
    ---------------------------------------------------------*/

  booleantype cv_sensMallocDone;
  booleantype cv_quadMallocDone;

  /*-----------------------------------------------
    Arrays for Optional Input and Optional Output 
  -----------------------------------------------*/

  long int    *cv_iopt;       /* long int optional input, output           */
  realtype    *cv_ropt;       /* real optional input, output               */
  booleantype  cv_optIn;      /* flag to indicate optional input 
                                 (used by adjoint)                         */

  /*------------
    Error File 
  ------------*/

  FILE *cv_errfp;             /* CVODE error messages are sent to errfp    */

  /*-----------------------------------------------------
    Pointer to Machine Environment-Specific Information 
  -----------------------------------------------------*/

  M_Env cv_machenv;

  /*-----------------------------------------------------
    Pointer to Machine Environment-Specific Information
    for Quadrature N_Vectors
  -----------------------------------------------------*/

  M_Env cv_machenvQ;

  /*----------------------------------------
    Stability Limit Detection control flag 
  ----------------------------------------*/

  booleantype cv_sldeton;     /* Is Stability Limit Detection on?          */

} *CVodeMem;


/******************************************************************
 *                                                                *
 * Communication between user and a CVODE Linear Solver           *
 *----------------------------------------------------------------*
 * Return values of the linear solver specification routine.      *
 * The values of these are given in the enum statement below.     *
 * SUCCESS      : The routine was successful.                     *
 *                                                                *
 * LMEM_FAIL    : A memory allocation failed.                     *
 *                                                                *
 * LIN_ILL_INPUT: Some input was illegal (see message).           *
 *                                                                *
 ******************************************************************/

/* SUCCESS = 0  (Defined under CVode return values, but listed
                 here also for completeness)                      */
enum {LMEM_FAIL = -1, LIN_ILL_INPUT = -2};

/*******************************************************************
 *                                                                 *
 * Communication between cvode.c and a CVODE Linear Solver         *
 *-----------------------------------------------------------------*
 * (1) cv_linit return values                                      *
 *                                                                 *
 * LINIT_OK    : The cv_linit routine succeeded.                   *
 *                                                                 *
 * LINIT_ERR   : The cv_linit routine failed. Each linear solver   *
 *               init routine should print an appropriate error    *
 *               message to (cv_mem->errfp).                       *
 *                                                                 *
 * (2) convfail (input to cv_lsetup)                               *
 *                                                                 *
 * NO_FAILURES : Either this is the first cv_setup call for this   *
 *               step, or the local error test failed on the       *
 *               previous attempt at this step (but the Newton     *
 *               iteration converged).                             * 
 *                                                                 *
 * FAIL_BAD_J  : This value is passed to cv_lsetup if              *
 *                                                                 *
 *               (1) The previous Newton corrector iteration       *
 *                   did not converge and the linear solver's      *
 *                   setup routine indicated that its Jacobian-    *
 *                   related data is not current.                  *
 *                                   or                            *
 *               (2) During the previous Newton corrector          *
 *                   iteration, the linear solver's solve routine  *
 *                   failed in a recoverable manner and the        *
 *                   linear solver's setup routine indicated that  *
 *                   its Jacobian-related data is not current.     *
 *                                                                 *
 * FAIL_OTHER  : During the current internal step try, the         *
 *               previous Newton iteration failed to converge      *
 *               even though the linear solver was using current   *
 *               Jacobian-related data.                            *
 *                                                                 *
 * (3) Parameter documentation, as well as a brief description     *
 *     of purpose, for each CVODE linear solver routine to be      *
 *     called in cvode.c is given below the constant declarations  *
 *     that follow.                                                *
 *                                                                 *
 *******************************************************************/

/* cv_linit return values */

#define LINIT_OK        0
#define LINIT_ERR      -1

/* Constants for convfail (input to cv_lsetup) */

#define NO_FAILURES 0   
#define FAIL_BAD_J  1  
#define FAIL_OTHER  2  


/*******************************************************************
 *                                                                 *
 * int (*cv_linit)(CVodeMem cv_mem);                               *
 *-----------------------------------------------------------------*
 * The purpose of cv_linit is to complete initializations for      *
 * specific linear solver, such as counters and statistics.        *
 * An LInitFn should return LINIT_OK (= 0) if it has successfully  *
 * initialized the CVODE linear solver and LINIT_ERR (= -1)        *
 * otherwise. These constants are defined above.  If an error does *
 * occur, an appropriate message should be sent to (cv_mem->errfp).*
 *                                                                 *
 *******************************************************************/

/*******************************************************************
 *                                                                 *
 * int (*cv_lsetup)(CVodeMem cv_mem, int convfail, N_Vector ypred, *
 *                 N_Vector fpred, booleantype *jcurPtr,           *
 *                 N_Vector vtemp1, N_Vector vtemp2,               *
 *                 N_Vector vtemp3);                               *
 *-----------------------------------------------------------------*
 * The job of cv_lsetup is to prepare the linear solver for        *
 * subsequent calls to cv_lsolve. It may re-compute Jacobian-      *
 * related data is it deems necessary. Its parameters are as       *
 * follows:                                                        *
 *                                                                 *
 * cv_mem - problem memory pointer of type CVodeMem. See the big   *
 *          typedef earlier in this file.                          *
 *                                                                 *
 * convfail - a flag to indicate any problem that occurred during  *
 *            the solution of the nonlinear equation on the        *
 *            current time step for which the linear solver is     *
 *            being used. This flag can be used to help decide     *
 *            whether the Jacobian data kept by a CVODE linear     *
 *            solver needs to be updated or not.                   *
 *            Its possible values have been documented above.      *
 *                                                                 *
 * ypred - the predicted y vector for the current CVODE internal   *
 *         step.                                                   *
 *                                                                 *
 * fpred - f(tn, ypred).                                           *
 *                                                                 *
 * jcurPtr - a pointer to a boolean to be filled in by cv_lsetup.  *
 *           The function should set *jcurPtr=TRUE if its Jacobian *
 *           data is current after the call and should set         *
 *           *jcurPtr=FALSE if its Jacobian data is not current.   *
 *           Note: If cv_lsetup calls for re-evaluation of         *
 *           Jacobian data (based on convfail and CVODE state      *
 *           data), it should return *jcurPtr=TRUE unconditionally;*
 *           otherwise an infinite loop can result.                *
 *                                                                 *
 * vtemp1 - temporary N_Vector provided for use by cv_lsetup.      *
 *                                                                 *
 * vtemp3 - temporary N_Vector provided for use by cv_lsetup.      *
 *                                                                 *
 * vtemp3 - temporary N_Vector provided for use by cv_lsetup.      *
 *                                                                 *
 * The cv_lsetup routine should return 0 if successful,            *
 * a positive value for a recoverable error, and a negative value  *
 * for an unrecoverable error.                                     *
 *                                                                 *
 *******************************************************************/

/*******************************************************************
 *                                                                 *
 * int (*cv_lsolve)(CVodeMem cv_mem, N_Vector b, N_Vector ycur,    *
 *                  N_Vector fcur);                                *
 *-----------------------------------------------------------------*
 * cv_lsolve must solve the linear equation P x = b, where         *
 * P is some approximation to (I - gamma J), J = (df/dy)(tn,ycur)  *
 * and the RHS vector b is input. The N-vector ycur contains       *
 * the solver's current approximation to y(tn) and the vector      *
 * fcur contains the N-vector f(tn,ycur). The solution is to be    *
 * returned in the vector b. cv_lsolve returns a positive value    *
 * for a recoverable error and a negative value for an             *
 * unrecoverable error. Success is indicated by a 0 return value.  *
 *                                                                 *
 *******************************************************************/

/*******************************************************************
 *                                                                 *
 * int (*cv_lsolveS)(CVodeMem cv_mem, N_Vector b, N_Vector ycur,   *
 *                  N_Vector fcur, integertype is);                *
 *-----------------------------------------------------------------*
 *******************************************************************/

/*******************************************************************
 *                                                                 *
 * void (*cv_lfree)(CVodeMem cv_mem);                              *
 *-----------------------------------------------------------------*
 * cv_lfree should free up any memory allocated by the linear      *
 * solver. This routine is called once a problem has been          *
 * completed and the linear solver is no longer needed.            *
 *                                                                 *
 *******************************************************************/

#endif

#ifdef __cplusplus
}
#endif
