/*
 * -----------------------------------------------------------------
 * $Revision: 1.2 $
 * $Date: 2006-11-22 00:12:46 $
 * ----------------------------------------------------------------- 
 * Programmer: Radu Serban @ LLNL
 * -----------------------------------------------------------------
 * Copyright (c) 2006, The Regents of the University of California.
 * Produced at the Lawrence Livermore National Laboratory.
 * All rights reserved.
 * For details, see the LICENSE file.
 * -----------------------------------------------------------------
 * This is the header file for the CPBBDPRE module, for a
 * band-block-diagonal preconditioner, i.e. a block-diagonal
 * matrix with banded blocks, for use with one of the CPSPILS
 * linear solvers and the parallel implementation of the NVECTOR 
 * module.
 *
 * Summary:
 *
 * These routines provide a preconditioner matrix that is
 * block-diagonal with banded blocks. The blocking corresponds to the
 * distribution of the dependent variable vector y among the processors. 
 * Each preconditioner block is generated from the Jacobian of the local 
 * part (on the current processor) of a given function g(t,y) approximating 
 * f(t,y) (for explicit-form ODEs) or G(t,y,y') approximating F(t,y,y') 
 * (for implicit-form ODEs). The blocks are generated by a difference 
 * quotient scheme on each processor independently. This scheme utilizes 
 * an assumed banded structure with given half-bandwidths, mudq and mldq.
 * However, the banded Jacobian block kept by the scheme has half-bandwiths 
 * mukeep and mlkeep, which may be smaller.
 *
 * The user's calling program should have the following form:
 *
 *   #include <nvector_parallel.h>
 *   #include <cpodes/cpodes_bbdpre.h>
 *   ...
 *   void *cpode_mem;
 *   void *bbd_data;
 *   ...
 *   Set y0
 *   ...
 *   cpode_mem = CPodeCreate(...);
 *   ier = CPodeMalloc(...);
 *   ...
 *   bbd_data = CPBBDPrecAlloc(cpode_mem, Nlocal, mudq ,mldq,
 *                             mukeep, mlkeep, dqrely, gloc, cfn);
 *   flag = CPBBDSpgmr(cpode_mem, pretype, maxl, bbd_data);
 *      -or-
 *   flag = CPBBDSpbcg(cpode_mem, pretype, maxl, bbd_data);
 *      -or-
 *   flag = CPBBDSptfqmr(cpode_mem, pretype, maxl, bbd_data);
 *   ...
 *   ier = CPode(...);
 *   ...
 *   CPBBDPrecFree(&bbd_data);
 *   ...                                                           
 *   CPodeFree(...);
 * 
 *   Free y0
 *
 * The user-supplied routines required are:
 *
 *   f or F - function defining the ODE right-hand side f(t,y)
 *            or the ODE residual F(t,y,y')
 *
 *   gloc or Gloc - function defining the approximation g(t,y)
 *                  or G(t,y,y')
 *
 *   cfn  - function to perform communication need for gloc.
 *
 * Notes:
 *
 * 1) This header file is included by the user for the definition
 *    of the CPBBDData type and for needed function prototypes.
 *
 * 2) The CPBBDPrecAlloc call includes half-bandwiths mudq and mldq
 *    to be used in the difference quotient calculation of the
 *    approximate Jacobian. They need not be the true
 *    half-bandwidths of the Jacobian of the local block of g,
 *    when smaller values may provide a greater efficiency.
 *    Also, the half-bandwidths mukeep and mlkeep of the retained
 *    banded approximate Jacobian block may be even smaller,
 *    to reduce storage and computation costs further.
 *    For all four half-bandwidths, the values need not be the
 *    same on every processor.
 *
 * 3) The actual name of the user's f (or F) function is passed to
 *    CPodeMalloc, and the names of the user's gloc (or Gloc) and
 *    cfn functions are passed to CPBBDPrecAlloc.
 *
 * 4) The pointer to the user-defined data block f_data, which is
 *    set through CPodeSetFdata is also available to the user in
 *    gloc/Gloc and cfn.
 *
 * 5) For the CPSpgmr solver, the Gram-Schmidt type gstype,
 *    is left to the user to specify through CPSpgmrSetGStype.
 *
 * 6) Optional outputs specific to this module are available by
 *    way of routines listed below. These include work space sizes
 *    and the cumulative number of gloc calls. The costs
 *    associated with this module also include nsetups banded LU
 *    factorizations, nlinsetups cfn calls, and npsolves banded
 *    backsolve calls, where nlinsetups and npsolves are
 *    integrator/CPSPGMR/CPSPBCG/CPSPTFQMR optional outputs.
 * -----------------------------------------------------------------
 */

#ifndef _CPBBDPRE_H
#define _CPBBDPRE_H

#ifdef __cplusplus  /* wrapper to enable C++ usage */
extern "C" {
#endif

#include <sundials/sundials_nvector.h>

  /* CPBBDPRE return values */

#define CPBBDPRE_SUCCESS            0
#define CPBBDPRE_PDATA_NULL       -11
#define CPBBDPRE_FUNC_UNRECVR     -12

  /*
   * -----------------------------------------------------------------
   * Types: CPBBDLocalRhsFn and CPBBDLocalResFn
   * -----------------------------------------------------------------
   *
   * For ODEs in explicit form, i.e., y' = f(t,y), the user must 
   * supply a function g(t,y) which approximates the right-hand side 
   * function f for the system y'=f(t,y), and which is computed locally 
   * (without interprocess communication). The case where g is 
   * mathematically identical to f is allowed. The implementation of 
   * this function must have type CPBBDLocalRhsFn.
   *
   * This function takes as input the local vector size Nlocal, the
   * independent variable value t, the local real dependent variable
   * vector y, and a pointer to the user-defined data block f_data. 
   * It is to compute the local part of g(t,y) and store this in the 
   * vector gout. Allocation of memory for y and g is handled within
   * the preconditioner module. The f_data parameter is the same as
   * that specified by the user through the CPodeSetFdata routine.
   *
   * A CPBBDLocalRhsFn should return 0 if successful, a positive value
   * if a recoverable error occurred, and a negative value if an 
   * unrecoverable error occurred.
   *
   * -----------------------------------------------------------------
   *
   * For ODEs in implicit form, the user must supply a function 
   * G(t,y,y') which approximates the function F for the system 
   * F(t,y,y') = 0, and which is computed locally (without interprocess 
   * communication. The case where G is mathematically identical to F 
   * is allowed. The implementation of this function must have type 
   * CPBBDLocalResFn.
   *
   * This function takes as input the independent variable value t,
   * the current solution vector y, the current solution derivative 
   * vector yp, and a pointer to the user-defined data block f_data. 
   * It is to compute the local part of G(t,y,y') and store it in the 
   * vector gout. Providing memory for y, yp, and gout is handled within 
   * this preconditioner module. It is expected that this routine will 
   * save communicated data in work space defined by the user and made 
   * available to the preconditioner function for the problem. The f_data
   * parameter is the same as that passed by the user to the CPodeSetFdata
   * routine.
   *
   * A CPBBDLocalResFn Gres is to return an int, defined in the same
   * way as for the residual function: 0 (success), +1 or -1 (fail).
   * -----------------------------------------------------------------
   */

  typedef int (*CPBBDLocalRhsFn)(int Nlocal, realtype t, 
                                 N_Vector y,
                                 N_Vector gout, void *f_data);

  typedef int (*CPBBDLocalResFn)(int Nlocal, realtype t, 
                                 N_Vector y, N_Vector yp, 
                                 N_Vector gout, void *f_data);

  /*
   * -----------------------------------------------------------------
   * Type: CPBBDCommFn
   * -----------------------------------------------------------------
   * The user may supply a function of type CPBBDCommFn which performs
   * all interprocess communication necessary to evaluate the
   * approximate right-hand side function described above.
   *
   * This function takes as input the local vector size Nlocal,
   * the independent variable value t, the dependent variable
   * vector y, and a pointer to the user-defined data block f_data.
   * The f_data parameter is the same as that specified by the user
   * through the CPodeSetFdata routine. A CPBBDCommFn cfn is
   * expected to save communicated data in space defined within the
   * structure f_data. A CPBBDCommFn cfn does not have a return value.
   *
   * Each call to the CPBBDCommFn cfn is preceded by a call to the
   * CPRhsFn f (or CPResFn F) with the same (t, y, y') arguments 
   * (where y'=NULL for explicit-form ODEs). Thus cfn can omit any 
   * communications done by f (or F) if relevant to the evaluation of 
   * the local approximation. If all necessary communication was done 
   * by f (respectively F), the user can pass NULL for cfn in 
   * CPBBDPrecAlloc (see below).
   *
   * A CPBBDCommFn should return 0 if successful, a positive value if 
   * a recoverable error occurred, and a negative value if an 
   * unrecoverable error occurred.
   * -----------------------------------------------------------------
   */

  typedef int (*CPBBDCommFn)(int Nlocal, realtype t, 
                             N_Vector y, N_Vector yp, void *f_data);

  /*
   * -----------------------------------------------------------------
   * Function : CPBBDPrecAlloc
   * -----------------------------------------------------------------
   * CPBBDPrecAlloc allocates and initializes a CPBBDData structure
   * to be passed to CPSp* (and used by CPBBDPrecSetup and
   * CPBBDPrecSolve).
   *
   * The parameters of CPBBDPrecAlloc are as follows:
   *
   * cpode_mem - pointer to the integrator memory.
   *
   * Nlocal - length of the local vectors on the current processor.
   *
   * mudq, mldq - upper and lower half-bandwidths to be used in the
   *   difference quotient computation of the local Jacobian block.
   *
   * mukeep, mlkeep - upper and lower half-bandwidths of the retained 
   *   banded approximation to the local Jacobian block.
   *
   * dqrely - optional input. It is the relative increment in components 
   *   of y used in the difference quotient approximations. To specify 
   *   the default value (square root of the unitroundoff), pass 0.
   *
   * gloc - name of the user-supplied function g(t,y) or G(t,y,y') that
   *   approximates f (respectively F) and whose local Jacobian blocks
   *   are to form the preconditioner.
   *
   * cfn - name of the user-defined function that performs necessary 
   *   interprocess communication for the execution of gloc.
   *
   * CPBBDPrecAlloc returns the storage allocated (type *void), or NULL 
   * if the request for storage cannot be satisfied.
   * -----------------------------------------------------------------
   */

  void *CPBBDPrecAlloc(void *cpode_mem, int Nlocal, 
                       int mudq, int mldq, int mukeep, int mlkeep, 
                       realtype dqrely,
                       void *gloc, CPBBDCommFn cfn);

  /*
   * -----------------------------------------------------------------
   * Function : CPBBDSptfqmr
   * -----------------------------------------------------------------
   * CPBBDSptfqmr links the CPBBDPRE preconditioner to the CPSPTFQMR
   * linear solver. It performs the following actions:
   *  1) Calls the CPSPTFQMR specification routine and attaches the
   *     CPSPTFQMR linear solver to the integrator memory;
   *  2) Sets the preconditioner data structure for CPSPTFQMR
   *  3) Sets the preconditioner setup routine for CPSPTFQMR
   *  4) Sets the preconditioner solve routine for CPSPTFQMR
   *
   * Its first 3 arguments are the same as for CPSptfqmr (see
   * cpsptfqmr.h). The last argument is the pointer to the CPBBDPRE
   * memory block returned by CPBBDPrecAlloc. Note that the user need
   * not call CPSptfqmr.
   *
   * Possible return values are:
   *    CPSPILS_SUCCESS      if successful
   *    CPSPILS_MEM_NULL     if the CPODES memory was NULL
   *    CPSPILS_LMEM_NULL    if the CPSPILS memory was NULL
   *    CPSPILS_MEM_FAIL     if there was a memory allocation failure
   *    CPSPILS_ILL_INPUT    if a required vector operation is missing
   *    CPBBDPRE_PDATA_NULL  if the bbd_data was NULL
   * -----------------------------------------------------------------
   */
  
  int CPBBDSptfqmr(void *cpode_mem, int pretype, int maxl, void *bbd_data);
  
  /*
   * -----------------------------------------------------------------
   * Function : CPBBDSpbcg
   * -----------------------------------------------------------------
   * CPBBDSpbcg links the CPBBDPRE preconditioner to the CPSPBCG
   * linear solver. It performs the following actions:
   *  1) Calls the CPSPBCG specification routine and attaches the
   *     CPSPBCG linear solver to the integrator memory;
   *  2) Sets the preconditioner data structure for CPSPBCG
   *  3) Sets the preconditioner setup routine for CPSPBCG
   *  4) Sets the preconditioner solve routine for CPSPBCG
   *
   * Its first 3 arguments are the same as for CPSpbcg (see
   * cpspbcg.h). The last argument is the pointer to the CPBBDPRE
   * memory block returned by CPBBDPrecAlloc. Note that the user need
   * not call CPSpbcg.
   *
   * Possible return values are:
   *    CPSPILS_SUCCESS      if successful
   *    CPSPILS_MEM_NULL     if the CPODES memory was NULL
   *    CPSPILS_LMEM_NULL    if the CPSPILS memory was NULL
   *    CPSPILS_MEM_FAIL     if there was a memory allocation failure
   *    CPSPILS_ILL_INPUT    if a required vector operation is missing
   *    CPBBDPRE_PDATA_NULL  if the bbd_data was NULL
   * -----------------------------------------------------------------
   */
  
  int CPBBDSpbcg(void *cpode_mem, int pretype, int maxl, void *bbd_data);
  
  /*
   * -----------------------------------------------------------------
   * Function : CPBBDSpgmr
   * -----------------------------------------------------------------
   * CPBBDSpgmr links the CPBBDPRE preconditioner to the CPSPGMR
   * linear solver. It performs the following actions:
   *  1) Calls the CPSPGMR specification routine and attaches the
   *     CPSPGMR linear solver to the integrator memory;
   *  2) Sets the preconditioner data structure for CPSPGMR
   *  3) Sets the preconditioner setup routine for CPSPGMR
   *  4) Sets the preconditioner solve routine for CPSPGMR
   *
   * Its first 3 arguments are the same as for CPSpgmr (see
   * cpspgmr.h). The last argument is the pointer to the CPBBDPRE
   * memory block returned by CPBBDPrecAlloc. Note that the user need
   * not call CPSpgmr.
   *
   * Possible return values are:
   *    CPSPILS_SUCCESS      if successful
   *    CPSPILS_MEM_NULL     if the CPODES memory was NULL
   *    CPSPILS_LMEM_NULL    if the CPSPILS memory was NULL
   *    CPSPILS_MEM_FAIL     if there was a memory allocation failure
   *    CPSPILS_ILL_INPUT    if a required vector operation is missing
   *    CPBBDPRE_PDATA_NULL  if the bbd_data was NULL
   * -----------------------------------------------------------------
   */

  int CPBBDSpgmr(void *cpode_mem, int pretype, int maxl, void *bbd_data);

  /*
   * -----------------------------------------------------------------
   * Function : CPBBDPrecReInit
   * -----------------------------------------------------------------
   * CPBBDPrecReInit re-initializes the BBDPRE module when solving a
   * sequence of problems of the same size with CPSPGMR/CPBBDPRE or
   * CPSPBCG/CPBBDPRE or CPSPTFQMR/CPBBDPRE provided there is no change 
   * in Nlocal, mukeep, or mlkeep. After solving one problem, and after 
   * calling CPodeReInit to re-initialize the integrator for a subsequent 
   * problem, call CPBBDPrecReInit. Then call CPSpgmrSet* or CPSpbcgSet* 
   * or CPSptfqmrSet* functions if necessary for any changes to CPSPGMR, 
   * CPSPBCG, or CPSPTFQMR parameters, before calling CPode.
   *
   * The first argument to CPBBDPrecReInit must be the pointer pdata
   * that was returned by CPBBDPrecAlloc. All other arguments have
   * the same names and meanings as those of CPBBDPrecAlloc.
   *
   * The return value of CPBBDPrecReInit is CPBBDPRE_SUCCESS, indicating
   * success, or CPBBDPRE_PDATA_NULL if bbd_data was NULL.
   * -----------------------------------------------------------------
   */

  int CPBBDPrecReInit(void *bbd_data, int mudq, int mldq,
                      realtype dqrely, void *gloc, CPBBDCommFn cfn);

  /*
   * -----------------------------------------------------------------
   * Function : CPBBDPrecFree
   * -----------------------------------------------------------------
   * CPBBDPrecFree frees the memory block bbd_data allocated by the
   * call to CPBBDAlloc.
   * -----------------------------------------------------------------
   */

  void CPBBDPrecFree(void **bbd_data);

  /*
   * -----------------------------------------------------------------
   * BBDPRE optional output extraction routines
   * -----------------------------------------------------------------
   * CPBBDPrecGetWorkSpace returns the BBDPRE real and integer work space
   *                       sizes.
   * CPBBDPrecGetNumGfnEvals returns the number of calls to gfn.
   *
   * The return value of CPBBDPrecGet* is one of:
   *    CPBBDPRE_SUCCESS    if successful
   *    CPBBDPRE_PDATA_NULL if the bbd_data memory was NULL
   * -----------------------------------------------------------------
   */

  int CPBBDPrecGetWorkSpace(void *bbd_data, long int *lenrwLS, long int *leniwLS);
  int CPBBDPrecGetNumGfnEvals(void *bbd_data, long int *ngevalsBBDP);

  /*
   * -----------------------------------------------------------------
   * The following function returns the name of the constant 
   * associated with a CPBBDPRE return flag
   * -----------------------------------------------------------------
   */
  
  char *CPBBDPrecGetReturnFlagName(int flag);

#ifdef __cplusplus
}
#endif

#endif