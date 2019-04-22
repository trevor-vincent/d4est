#define _GNU_SOURCE
#include <stdio.h>

#include <pXest.h>
#include <d4est_linalg.h>
#include <d4est_util.h>
#include <d4est_elliptic_data.h>
#include <d4est_solver_newton_petsc.h>
#include <d4est_solver_krylov_petsc.h>
#include <d4est_checkpoint.h>
#include <petscsnes.h>
#include <time.h>
#include <ini.h>

static
int d4est_solver_newton_petsc_input_handler
(
 void* user,
 const char* section,
 const char* name,
 const char* value
)
{
  d4est_solver_newton_petsc_params_t* pconfig = (d4est_solver_newton_petsc_params_t*)user;

  if (d4est_util_match_couple(section,"d4est_solver_newton_petsc",name,"snes_type")) {
    D4EST_ASSERT(pconfig->snes_type[0] == '*');
    snprintf (pconfig->snes_type, sizeof(pconfig->snes_type), "%s", value);
  }
  else if (d4est_util_match_couple(section,"d4est_solver_newton_petsc",name,"snes_atol")) {
    D4EST_ASSERT(pconfig->snes_atol[0] == '*');
    snprintf (pconfig->snes_atol, sizeof(pconfig->snes_atol), "%s", value);
  }
  else if (d4est_util_match_couple(section,"d4est_solver_newton_petsc",name,"snes_rtol")) {
    D4EST_ASSERT(pconfig->snes_rtol[0] == '*');
    snprintf (pconfig->snes_rtol, sizeof(pconfig->snes_rtol), "%s", value);
  }
  else if (d4est_util_match_couple(section,"d4est_solver_newton_petsc",name,"snes_stol")) {
    D4EST_ASSERT(pconfig->snes_stol[0] == '*');
    snprintf (pconfig->snes_stol, sizeof(pconfig->snes_stol), "%s", value);
  }
 else if (d4est_util_match_couple(section,"d4est_solver_newton_petsc",name,"snes_trtol")) {
    D4EST_ASSERT(pconfig->snes_trtol[0] == '*');
    snprintf (pconfig->snes_trtol, sizeof(pconfig->snes_trtol), "%s", value);
  } 
  else if (d4est_util_match_couple(section,"d4est_solver_newton_petsc",name,"snes_max_funcs")) {
    D4EST_ASSERT(pconfig->snes_max_funcs[0] == '*');
    snprintf (pconfig->snes_max_funcs, sizeof(pconfig->snes_max_funcs), "%s", value);
  }
  else if (d4est_util_match_couple(section,"d4est_solver_newton_petsc",name,"snes_max_it")) {
    D4EST_ASSERT(pconfig->snes_max_it[0] == '*');
    snprintf (pconfig->snes_max_it, sizeof(pconfig->snes_max_it), "%s", value);
  }
  else if (d4est_util_match_couple(section,"d4est_solver_newton_petsc",name,"snes_linesearch_monitor")) {
    D4EST_ASSERT(pconfig->snes_linesearch_monitor == -1);
    pconfig->snes_linesearch_monitor = atoi(value);
    D4EST_ASSERT(atoi(value) == 0 || atoi(value) == 1);
  }  
  else if (d4est_util_match_couple(section,"d4est_solver_newton_petsc",name,"snes_linesearch_order")) {
    D4EST_ASSERT(pconfig->snes_linesearch_order[0] == '*');
    snprintf (pconfig->snes_linesearch_order, sizeof(pconfig->snes_linesearch_order), "%s", value);
  }
  else if (d4est_util_match_couple(section,"d4est_solver_newton_petsc",name,"snes_monitor")) {
    D4EST_ASSERT(pconfig->snes_monitor == -1);
    pconfig->snes_monitor = atoi(value);
    D4EST_ASSERT(atoi(value) == 0 || atoi(value) == 1);
  }
  else if (d4est_util_match_couple(section,"d4est_solver_newton_petsc",name,"snes_view")) {
    D4EST_ASSERT(pconfig->snes_view == -1);
    pconfig->snes_view = atoi(value);
    D4EST_ASSERT(atoi(value) == 0 || atoi(value) == 1);
  }
  else if (d4est_util_match_couple(section,"d4est_solver_newton_petsc",name,"snes_ksp_ew")) {
    D4EST_ASSERT(pconfig->snes_ksp_ew == -1);
    pconfig->snes_ksp_ew = atoi(value);
    D4EST_ASSERT(atoi(value) == 0 || atoi(value) == 1);
  }
  
  else if (d4est_util_match_couple(section,"d4est_solver_newton_petsc",name,"checkpoint_every_n_newton_its")) {
    D4EST_ASSERT(pconfig->checkpoint_every_n_newton_its == 0);
    pconfig->checkpoint_every_n_newton_its = atoi(value);
  }
  else {
    return 0;  /* unknown section/name, error */
  }
  return 1;
}


void
d4est_solver_newton_petsc_input
(
 p4est_t* p4est,
 const char* input_file,
 d4est_solver_newton_petsc_params_t* input
)
{
  input->snes_atol[0] = '*';
  input->snes_rtol[0] = '*';
  input->snes_stol[0] = '*';
  input->snes_max_funcs[0] = '*';
  input->snes_type[0] = '*';
  input->snes_max_it[0] = '*';
  input->snes_trtol[0] = '*';
  input->snes_linesearch_order[0] = '*';
  input->snes_monitor = -1;
  input->snes_view = -1;
  input->snes_linesearch_monitor = -1;
  input->snes_converged_reason = -1;
  input->snes_ksp_ew = -1;
  input->checkpoint_every_n_newton_its = 0;

  if (ini_parse(input_file, d4est_solver_newton_petsc_input_handler, input) < 0) {
    D4EST_ABORT("Can't load input file");
  }  

  D4EST_CHECK_INPUT("d4est_solver_newton_petsc", input->snes_type[0], '*');
  D4EST_CHECK_INPUT("d4est_solver_newton_petsc", input->snes_atol[0], '*');
  D4EST_CHECK_INPUT("d4est_solver_newton_petsc", input->snes_rtol[0], '*');
  D4EST_CHECK_INPUT("d4est_solver_newton_petsc", input->snes_max_it[0], '*');
  D4EST_CHECK_INPUT("d4est_solver_newton_petsc", input->snes_max_funcs[0], '*');

  if(d4est_util_match(input->snes_type,"newtonls")){
    D4EST_CHECK_INPUT("d4est_solver_newton_petsc", input->snes_linesearch_order[0], '*');
    D4EST_CHECK_INPUT("d4est_solver_newton_petsc", input->snes_linesearch_monitor, -1);
  }
  else if(d4est_util_match(input->snes_type,"newtontr")){
    D4EST_CHECK_INPUT("d4est_solver_newton_petsc", input->snes_trtol[0], '*');
  }  
  
  D4EST_CHECK_INPUT("d4est_solver_newton_petsc", input->snes_view, -1);
  D4EST_CHECK_INPUT("d4est_solver_newton_petsc", input->snes_monitor, -1);

  zlog_category_t* c_default = zlog_get_category("d4est_solver_newton_petsc");
  if(p4est->mpirank == 0){
    zlog_debug(c_default, "snes_type = %s", input->snes_type);
    zlog_debug(c_default, "snes_view = %d", input->snes_view);
    zlog_debug(c_default, "snes_monitor = %d", input->snes_monitor);
    zlog_debug(c_default, "snes_atol = %s", input->snes_atol);
    zlog_debug(c_default, "snes_rtol = %s", input->snes_rtol);
    zlog_debug(c_default, "snes_stol = %s", input->snes_stol);
    zlog_debug(c_default, "snes_maxit = %s", input->snes_max_it);
    zlog_debug(c_default, "snes_maxfuncs = %s", input->snes_max_funcs);
    zlog_debug(c_default, "snes_ksp_ew = %d", input->snes_ksp_ew);
    if(d4est_util_match(input->snes_type,"newtonls")){
      zlog_debug(c_default, "snes_linesearch_order = %s", input->snes_max_funcs);
      zlog_debug(c_default, "snes_linesearch_monitor = %d", input->snes_linesearch_monitor);
    }
    if(d4est_util_match(input->snes_type,"newtontr")){
      zlog_debug(c_default, "snes_trtol = %s", input->snes_trtol);
    }
  } 
}


/** 
 * Required for grabbing x0 
 * in J(x0)dx = -F(x0)
 * 
 * @param MojSnes 
 * @param X 
 * @param Jac 
 * @param Precond 
 * @param flags 
 * @param ctx 
 * 
 * @return 
 */
static
PetscErrorCode d4est_solver_newton_petsc_save_x0
(
 SNES MojSnes,
 Vec x0,
 Mat Jac, 
 Mat Precond,
 void* ctx
) 
{

  PetscErrorCode ierr;
  krylov_ctx_t* petsc_ctx = (krylov_ctx_t*) ctx;
  d4est_elliptic_data_t* vecs = petsc_ctx->vecs;
  const double* px0;
  ierr = VecGetArrayRead( x0, &px0 ); CHKERRQ(ierr);
  int newton_iteration;
  SNESGetIterationNumber(MojSnes,&newton_iteration);
  petsc_ctx->newton_iteration = newton_iteration;
  
  int i;
  for (i = 0; i < vecs->local_nodes; i++){
    vecs->u0[i] = px0[i];
  }
    
  VecRestoreArrayRead
    (
     x0,
     &px0
    );
  
  return 0; 
}


static
PetscErrorCode d4est_solver_newton_petsc_monitor(SNES snes,PetscInt it, PetscReal norm, void *ctx)
{
 krylov_ctx_t* petsc_ctx = (krylov_ctx_t*) ctx;
  zlog_category_t* c_default = zlog_get_category("d4est_solver_newton_petsc");
  zlog_category_t* its_output = zlog_get_category("d4est_solver_newton_petsc_iteration_info");

  int lit;
  SNESGetLinearSolveIterations(snes,&lit);
  petsc_ctx->newton_iteration = it;
  
  if (petsc_ctx->p4est->mpirank == 0){
    double duration_seconds = ((double)(clock() - petsc_ctx->time_start)) / CLOCKS_PER_SEC;
    zlog_info(its_output,"AMR_IT SNES_IT KSP_IT SNES_NORM: %d %d %d %.25f %f", petsc_ctx->amr_level, it, lit, norm, duration_seconds);
  }

  if (petsc_ctx->checkpoint_every_n_newton_its > 0){

     if ( (it - petsc_ctx->last_newton_checkpoint_it) >= petsc_ctx->checkpoint_every_n_newton_its ){

       if (petsc_ctx->p4est->mpirank == 0){
         zlog_info(c_default, "Saving checkpoint at newton iteration %d", it);
       }
      char* output = NULL;
      asprintf(&output,"checkpoint_newton_%d", it);

      Vec x;
      const double* sol;
      SNESGetSolution(snes, &x);
      VecGetArrayRead(x, &sol);
      
      d4est_elliptic_data_t* vecs = petsc_ctx->vecs;
      d4est_checkpoint_save
        (
         petsc_ctx->amr_level,
         output,
         petsc_ctx->p4est,
         NULL,
         NULL,
         (const char * []){"u", NULL},
         (hid_t []){H5T_NATIVE_DOUBLE},
         (int []){vecs->local_nodes},
         (void* []){(void*)sol}
        );

      petsc_ctx->last_newton_checkpoint_it = it;
      free(output);
    }
    else {
      if (petsc_ctx->p4est->mpirank == 0){
        zlog_info(c_default, "No checkpoint at newton iteration %d", it);
        zlog_info(c_default, "petsc_ctx->last_newton_checkpoint_it = %d", petsc_ctx->last_newton_checkpoint_it);
        zlog_info(c_default,"petsc_ctx->checkpoint_every_n_newton_its = %d",petsc_ctx->checkpoint_every_n_newton_its);
      }
    }

  }

  return 0;
}

static
PetscErrorCode d4est_solver_newton_petsc_get_residual(SNES snes, Vec x, Vec f, void *ctx){

  const double* xx;
  krylov_ctx_t* petsc_ctx = (krylov_ctx_t*) ctx;

  double* ftemp;

  VecGetArray(f,&ftemp); 
  VecGetArrayRead(x,&xx);

  d4est_elliptic_data_t* vecs = petsc_ctx->vecs;
  d4est_elliptic_data_t vecs_for_res_build;
  d4est_elliptic_data_copy_ptrs(vecs, &vecs_for_res_build);
  vecs_for_res_build.u = (double*)xx;//x_temp;
  vecs_for_res_build.u0 = (double*)xx;//x_temp;
  vecs_for_res_build.Au = ftemp;

  d4est_elliptic_eqns_build_residual
    (
     petsc_ctx->p4est,
     *petsc_ctx->ghost,
     *petsc_ctx->ghost_data,
     petsc_ctx->fcns,
     &vecs_for_res_build,
     petsc_ctx->d4est_ops,
     petsc_ctx->d4est_geom,
     petsc_ctx->d4est_quad,
     petsc_ctx->d4est_factors
    );


  VecRestoreArray(f,&ftemp);
  VecRestoreArrayRead(x,&xx);
  return 0;
}

static
PetscErrorCode d4est_solver_newton_petsc_apply_jacobian( Mat jac, Vec x, Vec y )
{
  void           *ctx;
  PetscErrorCode ierr;

  krylov_ctx_t* petsc_ctx;
  const double* px;
  double* py;

  /* PetscFunctionBegin; */
  ierr = MatShellGetContext( jac, &ctx ); CHKERRQ(ierr);  
  petsc_ctx = (krylov_ctx_t *)ctx;
  ierr = VecGetArrayRead( x, &px ); CHKERRQ(ierr);
  ierr = VecGetArray( y, &py ); CHKERRQ(ierr);

  d4est_elliptic_data_t* vecs = petsc_ctx->vecs;
  d4est_elliptic_data_t vecs_for_jac;
  d4est_elliptic_data_copy_ptrs(vecs, &vecs_for_jac);

  vecs_for_jac.u = (double*)px;
  vecs_for_jac.Au = py;
 
  d4est_elliptic_eqns_apply_lhs
    (
     petsc_ctx->p4est,
     *petsc_ctx->ghost,
     *petsc_ctx->ghost_data,
     petsc_ctx->fcns,
     &vecs_for_jac,
     petsc_ctx->d4est_ops,
     petsc_ctx->d4est_geom,
     petsc_ctx->d4est_quad,
     petsc_ctx->d4est_factors
    );
  
  ierr = VecRestoreArrayRead( x, &px ); CHKERRQ(ierr);
  ierr = VecRestoreArray( y, &py ); CHKERRQ(ierr);
  return ierr;
}

void
d4est_solver_newton_petsc_set_options_database_from_params
(
 d4est_solver_newton_petsc_params_t* input
)
{
  if(input->snes_monitor)
    PetscOptionsSetValue(NULL,"-snes_monitor","");
  else
    PetscOptionsClearValue(NULL,"-snes_monitor");

  if(input->snes_view)
    PetscOptionsSetValue(NULL,"-snes_view","");
  else
    PetscOptionsClearValue(NULL,"-snes_view");

  if(input->snes_converged_reason)
     PetscOptionsSetValue(NULL,"-snes_converged_reason","");
  else
    PetscOptionsClearValue(NULL,"-snes_converged_reason");

  if (input->snes_ksp_ew == 1)
    PetscOptionsSetValue(NULL,"-snes_ksp_ew","");
  else
    PetscOptionsClearValue(NULL,"-snes_ksp_ew");
  
  PetscOptionsClearValue(NULL,"-snes_type");
  PetscOptionsSetValue(NULL,"-snes_type",input->snes_type);
  
  PetscOptionsClearValue(NULL,"-snes_atol");
  PetscOptionsSetValue(NULL,"-snes_atol",input->snes_atol);
  
  PetscOptionsClearValue(NULL,"-snes_rtol");
  PetscOptionsSetValue(NULL,"-snes_rtol",input->snes_rtol);
  
  PetscOptionsClearValue(NULL,"-snes_max_it");
  PetscOptionsSetValue(NULL,"-snes_max_it", input->snes_max_it);

  PetscOptionsClearValue(NULL,"-snes_max_funcs");
  PetscOptionsSetValue(NULL,"-snes_max_funcs", input->snes_max_funcs);

  if(input->snes_stol[0] != '*'){
    PetscOptionsClearValue(NULL,"-snes_stol");
    PetscOptionsSetValue(NULL,"-snes_stol", input->snes_stol);
  }
    
  if(d4est_util_match(input->snes_type,"newtonls")){
    PetscOptionsClearValue(NULL,"-snes_linesearch_order");
    PetscOptionsSetValue(NULL,"-snes_linesearch_order", input->snes_linesearch_order);
    if(input->snes_linesearch_monitor){
      PetscOptionsSetValue(NULL,"-snes_linesearch_monitor", "");
    }
    else {
      PetscOptionsClearValue(NULL,"-snes_linesearch_monitor");
    }
  }
  else if(d4est_util_match(input->snes_type,"newtontr")){
    PetscOptionsClearValue(NULL,"-snes_trtol");
    PetscOptionsSetValue(NULL,"-snes_trtol", input->snes_trtol);
  }  
}

d4est_solver_newton_petsc_info_t
d4est_solver_newton_petsc_solve
(
 p4est_t* p4est,
 d4est_elliptic_data_t* vecs,
 d4est_elliptic_eqns_t* fcns,
 d4est_ghost_t** ghost,
 d4est_ghost_data_t** ghost_data, 
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 d4est_mesh_data_t* d4est_factors,
 d4est_solver_krylov_petsc_params_t* krylov_options,
 d4est_solver_newton_petsc_params_t* newton_options,
 d4est_krylov_pc_t* d4est_krylov_pc,
 int amr_level
)
{
  zlog_category_t* c_default = zlog_get_category("d4est_solver_newton_petsc");
  clock_t start = clock();
  d4est_solver_krylov_petsc_set_options_database_from_params(krylov_options);
  d4est_solver_newton_petsc_set_options_database_from_params(newton_options);
  
  SNES snes;
  KSP ksp;
  Vec x,r;
  SNESCreate(PETSC_COMM_WORLD,&snes);//CHKERRQ(ierr);
  VecCreate(PETSC_COMM_WORLD,&x);//CHKERRQ(ierr);
  VecSetSizes(x, vecs->local_nodes, PETSC_DECIDE);//CHKERRQ(ierr);
  VecSetFromOptions(x);//CHKERRQ(ierr);
  VecDuplicate(x,&r);//CHKERRQ(ierr);
  
  krylov_ctx_t petsc_ctx;
  petsc_ctx.p4est = p4est;
  petsc_ctx.vecs = vecs;
  petsc_ctx.fcns = fcns;
  petsc_ctx.ghost = ghost;
  petsc_ctx.ghost_data = ghost_data;
  petsc_ctx.d4est_ops = d4est_ops;
  petsc_ctx.d4est_geom = d4est_geom;
  petsc_ctx.d4est_quad = d4est_quad;
  petsc_ctx.d4est_factors = d4est_factors;
  petsc_ctx.checkpoint_every_n_newton_its = newton_options->checkpoint_every_n_newton_its;
  petsc_ctx.last_newton_checkpoint_it = 0;
  petsc_ctx.amr_level = amr_level;
  petsc_ctx.time_start = start;
  SNESSetFunction(snes,r,d4est_solver_newton_petsc_get_residual,(void*)&petsc_ctx);//CHKERRQ(ierr);
  SNESGetKSP(snes,&ksp);
  petsc_ctx.ksp = &ksp;
  petsc_ctx.using_newton = 1;
  petsc_ctx.newton_iteration = -1;
  
  PC pc;
  KSPGetPC(ksp,&pc);
  if (d4est_krylov_pc != NULL && krylov_options->ksp_do_not_use_preconditioner == 0) {
    PCSetType(pc,PCSHELL);//CHKERRQ(ierr);
    d4est_krylov_pc->pc_ctx = &petsc_ctx;
    PCShellSetApply(pc, d4est_solver_krylov_petsc_pc_apply);//CHKERRQ(ierr);
    if (d4est_krylov_pc->pc_setup != NULL){
      PCShellSetSetUp(pc, d4est_solver_krylov_petsc_pc_setup);
    }
    PCShellSetContext(pc, d4est_krylov_pc);//CHKERRQ(ierr);
  }
  else {
    PCSetType(pc,PCNONE);//CHKERRQ(ierr);
  }
  
  KSPSetFromOptions(ksp);
  KSPSetResidualHistory(ksp,
                        PETSC_NULL,
                        PETSC_DECIDE, // size of the array holding the history
                        PETSC_TRUE);  // Whether or not to reset the history for each solve.
  
  
  SNESSetFromOptions(snes);//CHKERRQ(ierr);

  /* Set the counters to not reset every linear step */
  SNESSetCountersReset(snes,PETSC_FALSE);
  
  double* u0 = P4EST_ALLOC(double, vecs->local_nodes);
  vecs->u0 = u0;
  
  Mat J;
  MatCreateShell
    (
     PETSC_COMM_WORLD,
     vecs->local_nodes,
     vecs->local_nodes,
     PETSC_DETERMINE,
     PETSC_DETERMINE,
     (void*)&petsc_ctx,
     &J
    );
  
  MatShellSetOperation(J,MATOP_MULT,(void(*)())d4est_solver_newton_petsc_apply_jacobian);
  SNESSetJacobian(snes,J,J,d4est_solver_newton_petsc_save_x0,(void*)&petsc_ctx);
  VecPlaceArray(x, vecs->u);
 
  PetscReal res = 0.0;
  SNESComputeFunction(snes, x, r);
  VecNorm(r, NORM_2, &res);
  if (p4est->mpirank == 0){
    zlog_info(c_default, "Initial residual = %.25f", res);
  }

  SNESMonitorSet(snes, d4est_solver_newton_petsc_monitor, &petsc_ctx, NULL);
  KSPMonitorSet(ksp, d4est_solver_krylov_petsc_monitor, &petsc_ctx, NULL);
  SNESSolve(snes,NULL,x);

  SNESComputeFunction(snes, x, r);
  VecNorm(r, NORM_2, &res);
  if (p4est->mpirank == 0){
    zlog_info(c_default, "Final residual = %.25f", res);
  }
  
  clock_t end = clock();
  double time_spent = (double)(end - start) / CLOCKS_PER_SEC;

  d4est_solver_newton_petsc_info_t info;
  SNESGetIterationNumber(snes,&info.total_newton_iterations);
  SNESGetLinearSolveIterations(snes,&info.total_krylov_iterations);
  SNESGetFunctionNorm(snes, &info.residual_norm);
    
  if (p4est->mpirank == 0){
    zlog_info(c_default, "Finished in %f seconds", time_spent);
  }

  P4EST_FREE(u0);
  VecResetArray(x);
  VecDestroy(&x);//CHKERRQ(ierr);  
  VecDestroy(&r);//CHKERRQ(ierr);


  /* Check Jacobian */
  
  /* Vec sol; */
  /* double* sol_dbl; */
  /* SNESGetSolution(snes,&sol); */
  /* VecGetArray(sol, &sol_dbl); */
  /* for (int i = 0; i < vecs->local_nodes; i++){ */
    /* vecs->u[i] = sol_dbl[i]; */
  /* } */
  
  SNESDestroy(&snes);//CHKERRQ(ierr);

  return info;
}
