#define _GNU_SOURCE
#include <stdio.h>

#include <sc_reduce.h>
#include <pXest.h>
#include <d4est_util.h>
#include <d4est_linalg.h>
#include <problem.h>
#include <d4est_elliptic_data.h>
#include <d4est_elliptic_eqns.h>
#include <d4est_estimator_bi.h>
#include <d4est_solver_cg.h>
#include <d4est_amr.h>
#include <d4est_amr_smooth_pred.h>
#include <d4est_estimator_residual.h>
#include <d4est_geometry.h>
#include <d4est_geometry_brick.h>
#include <d4est_vtk.h>
#include <d4est_norms.h>
#include <d4est_mesh.h>
#include <ini.h>
#include <d4est_element_data.h>
#include <d4est_estimator_stats.h>
#include <d4est_laplacian.h>
#include <d4est_laplacian_flux_sipg.h>
#include <d4est_solver_newton.h>
#include <multigrid.h>
#include <d4est_krylov_pc_multigrid.h>
#include <multigrid_logger_residual.h>
#include <multigrid_element_data_updater.h>
#include <multigrid_matrix_operator.h>
#include <d4est_solver_krylov_petsc.h>
#include <d4est_solver_newton_petsc.h>
#include <d4est_util.h>
#include <d4est_h5.h>
#include <d4est_checkpoint.h>
#include <time.h>
#include "multi_puncture_fcns.h"

int
skip_curved_elements
(
 d4est_element_data_t* elem
)
{
  if (elem->tree == 6)
    return 0;
  else
    return 1;
}

void
save_u_fcn
(
 p4est_t* p4est,
 d4est_elliptic_data_t* edata,
 const char* checkpoint_prefix,
 void* user
)
{
  d4est_h5_create_dataset(p4est->mpirank, checkpoint_prefix, "u", H5T_NATIVE_DOUBLE, edata->local_nodes);
  d4est_h5_write_dataset(p4est->mpirank, checkpoint_prefix, "u", H5T_NATIVE_DOUBLE, edata->u);
  
}


typedef struct {
  
  int do_not_solve;
  int use_puncture_finder;
  int amr_level_for_uniform_p;
  
} multi_puncture_init_params_t;


static
int multi_puncture_init_params_handler
(
 void* user,
 const char* section,
 const char* name,
 const char* value
)
{
  multi_puncture_init_params_t* pconfig = (multi_puncture_init_params_t*)user;
  if (d4est_util_match_couple(section,"problem",name,"do_not_solve")) {
    D4EST_ASSERT(pconfig->do_not_solve == -1);
    pconfig->do_not_solve = atoi(value);
  }
  else if (d4est_util_match_couple(section,"amr",name,"use_puncture_finder")) {
    D4EST_ASSERT(pconfig->use_puncture_finder == -1);
    pconfig->use_puncture_finder = atoi(value);
  }
  else if (d4est_util_match_couple(section,"amr",name,"amr_level_for_uniform_p")) {
    D4EST_ASSERT(pconfig->amr_level_for_uniform_p == -1);
    pconfig->amr_level_for_uniform_p = atoi(value);
  }
  else {
    return 0;  /* unknown section/name, error */
  }
  return 1;
}


static
multi_puncture_init_params_t
multi_puncture_init_params_input
(
 const char* input_file
)
{
  multi_puncture_init_params_t input;
  input.do_not_solve = -1;
  /* input.deg_quad_inc_inner = -1; */
  /* input.deg_quad_inc_outer = -1; */
  input.amr_level_for_uniform_p = -1;
  input.use_puncture_finder = -1;

  if (ini_parse(input_file, multi_puncture_init_params_handler, &input) < 0) {
    D4EST_ABORT("Can't load input file");
  }

  D4EST_CHECK_INPUT("problem", input.do_not_solve, -1);
  D4EST_CHECK_INPUT("amr", input.use_puncture_finder, -1);
  /* D4EST_CHECK_INPUT("problem", input.deg_quad_inc_inner, -1); */
  /* D4EST_CHECK_INPUT("problem", input.deg_quad_inc_outer, -1); */
  D4EST_CHECK_INPUT("amr", input.amr_level_for_uniform_p, -1);
  
  return input;
}



static
int
amr_mark_element
(
 p4est_t* p4est,
 double eta2,
 d4est_estimator_stats_t* stats,
 d4est_element_data_t* elem_data,
 void* user
)
{
  problem_ctx_t* ctx = user;
  d4est_amr_smooth_pred_params_t* params = ctx->smooth_pred_params;

  double eta2_percentile
    = d4est_estimator_stats_get_percentile(stats,params->percentile);
  return ((eta2 >= eta2_percentile) || fabs(eta2 - eta2_percentile) < eta2*1e-4) && (elem_data->tree == 6);
}

static
gamma_params_t
amr_set_element_gamma
(
 p4est_t* p4est,
 d4est_estimator_stats_t* stats,
 d4est_element_data_t* elem_data,
 void* user
)
{
  problem_ctx_t* ctx = user;
  d4est_amr_smooth_pred_params_t* params = ctx->smooth_pred_params;
  
  gamma_params_t gamma_hpn;

  gamma_hpn.gamma_h = params->gamma_h;
  gamma_hpn.gamma_p = params->gamma_p;
  gamma_hpn.gamma_n = params->gamma_n;

  return gamma_hpn;
}

static void
multi_puncture_only_p_refine_in_center_cube
(
  p4est_iter_volume_info_t* info,
  void* user_data
){
  d4est_amr_t* d4est_amr = (d4est_amr_t*) info->p4est->user_pointer;
  d4est_element_data_t* ed = (d4est_element_data_t*) info->quad->p.user_data;

  if (ed->tree == 6){
    d4est_amr->refinement_log[ed->id] = ed->deg + 1;
  }
  else {
    d4est_amr->refinement_log[ed->id] = ed->deg;
  }
}

static void
multi_puncture_p_refine_everywhere
(
  p4est_iter_volume_info_t* info,
  void* user_data
){
  d4est_amr_t* d4est_amr = (d4est_amr_t*) info->p4est->user_pointer;
  d4est_element_data_t* ed = (d4est_element_data_t*) info->quad->p.user_data;
  d4est_amr->refinement_log[ed->id] = ed->deg + 1;
}

/* static void */
/* multi_puncture_find_punctures_and_prefine_outside_cube_element_marker */
/* ( */
/*   p4est_iter_volume_info_t* info, */
/*   void* user_data */
/* ){ */
/*   d4est_amr_t* d4est_amr = (d4est_amr_t*) info->p4est->user_pointer; */
/*   d4est_element_data_t* ed = (d4est_element_data_t*) info->quad->p.user_data; */
/*   multi_puncture_params_t* params = d4est_amr->scheme->amr_scheme_data; */
  
/*   double xi [(P4EST_DIM)]; */
/*   double xf [(P4EST_DIM)]; */
  
/*   if (ed->region == 1){ */
/*     d4est_geometry_compute_bounds(md_on_e.xyz, ed->deg, xi, xf); */
/*     d4est_amr->refinement_log[ed->id] = ed->deg; */

/*     for (int i = 0; i < params->number_of_punctures; i++){ */
/*       if ((params->bh[i].XYZ[0] <= xf[0]) && */
/*           (params->bh[i].XYZ[1] <= xf[1]) && */
/*           (params->bh[i].XYZ[2] <= xf[2]) && */
/*           (params->bh[i].XYZ[0] >= xi[0]) && */
/*           (params->bh[i].XYZ[1] >= xi[1]) && */
/*           (params->bh[i].XYZ[2] >= xi[2])){ */
/*         d4est_amr->refinement_log[ed->id] = -ed->deg; */
/*         return; */
/*       } */
/*     } */
/*   } */
/*   else { */
/*     d4est_amr->refinement_log[ed->id] = ed->deg + 1; */
/*   } */
/* } */

/* static void */
/* multi_puncture_find_punctures_element_marker */
/* ( */
/*   p4est_iter_volume_info_t* info, */
/*   void* user_data */
/* ){ */
/*   d4est_amr_t* d4est_amr = (d4est_amr_t*) info->p4est->user_pointer; */
/*   d4est_element_data_t* ed = (d4est_element_data_t*) info->quad->p.user_data; */
/*   multi_puncture_params_t* params = d4est_amr->scheme->amr_scheme_data; */
  
/*   double xi [(P4EST_DIM)]; */
/*   double xf [(P4EST_DIM)]; */
  
/*   d4est_geometry_compute_bounds(ed->xyz, ed->deg, xi, xf); */
/*   d4est_amr->refinement_log[ed->id] = ed->deg; */

/*   for (int i = 0; i < params->number_of_punctures; i++){ */
/*     if ((params->bh[i].XYZ[0] <= xf[0]) && */
/*         (params->bh[i].XYZ[1] <= xf[1]) && */
/*         (params->bh[i].XYZ[2] <= xf[2]) && */
/*         (params->bh[i].XYZ[0] >= xi[0]) && */
/*         (params->bh[i].XYZ[1] >= xi[1]) && */
/*         (params->bh[i].XYZ[2] >= xi[2])){ */
/*       d4est_amr->refinement_log[ed->id] = -ed->deg; */
/*       return; */
/*     } */
/*   } */
/* } */

int
problem_set_mortar_degree
(
 d4est_element_data_t* elem_data,
 void* user_ctx
)
{
  return elem_data->deg_quad;
}

void
problem_init
(
 p4est_t* p4est,
 p4est_ghost_t** ghost,
 d4est_element_data_t** ghost_data,
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 d4est_mesh_data_t* d4est_factors,
 d4est_mesh_initial_extents_t* initial_extents,
 const char* input_file,
 sc_MPI_Comm mpicomm
)
{
  /* init_random_puncture_data */
  /*   ( */
  /*    p4est, */
  /*    3 */
  /*   ); */

  int initial_nodes = initial_extents->initial_nodes;
  multi_puncture_init_params_t init_params
    = multi_puncture_init_params_input(input_file);
  
  multi_puncture_params_t* multi_puncture_params = multi_puncture_params_init(p4est, input_file);
  
  d4est_amr_smooth_pred_params_t smooth_pred_params = d4est_amr_smooth_pred_params_input(input_file);
  d4est_laplacian_robin_bc_t bc_data_for_jac;
  bc_data_for_jac.robin_coeff = multi_puncture_robin_coeff_sphere_fcn;
  bc_data_for_jac.robin_rhs = multi_puncture_robin_bc_rhs_fcn;

  d4est_laplacian_robin_bc_t bc_data_for_res;
   bc_data_for_res.robin_coeff = multi_puncture_robin_coeff_sphere_fcn;
  bc_data_for_res.robin_rhs = multi_puncture_robin_bc_rhs_fcn;
  
  d4est_laplacian_dirichlet_bc_t bc_data_for_bi;
  bc_data_for_bi.dirichlet_fcn = zero_fcn;
  bc_data_for_bi.eval_method = EVAL_BNDRY_FCN_ON_LOBATTO;

  d4est_laplacian_flux_data_t* flux_data_for_bi = d4est_laplacian_flux_new(p4est, input_file, BC_DIRICHLET, &bc_data_for_bi);
  
  d4est_laplacian_flux_data_t* flux_data_for_jac = d4est_laplacian_flux_new(p4est, input_file, BC_ROBIN, &bc_data_for_jac);
  
  d4est_laplacian_flux_data_t* flux_data_for_res = d4est_laplacian_flux_new(p4est, input_file,  BC_ROBIN, &bc_data_for_res);
  

  problem_ctx_t ctx;
  ctx.multi_puncture_params = multi_puncture_params;
  ctx.smooth_pred_params = &smooth_pred_params;
  ctx.flux_data_for_jac = flux_data_for_jac;
  ctx.flux_data_for_res = flux_data_for_res;
  
  d4est_elliptic_eqns_t prob_fcns;
  prob_fcns.build_residual = multi_puncture_build_residual;
  prob_fcns.apply_lhs = multi_puncture_apply_jac;
  prob_fcns.user = &ctx;
  
  

  d4est_elliptic_data_t prob_vecs;
  prob_vecs.Au = P4EST_ALLOC(double, initial_nodes);
  prob_vecs.u = P4EST_ALLOC(double, initial_nodes);
  prob_vecs.local_nodes = initial_nodes;

  double* error = P4EST_ALLOC(double, prob_vecs.local_nodes);
  double* u_prev = P4EST_ALLOC(double, prob_vecs.local_nodes);
  
  d4est_laplacian_flux_sipg_params_t* sipg_params = flux_data_for_jac->flux_data;
  
  d4est_estimator_bi_penalty_data_t penalty_data;
  penalty_data.u_penalty_fcn = houston_u_prefactor_maxp_minh;
  penalty_data.u_dirichlet_penalty_fcn = houston_u_dirichlet_prefactor_maxp_minh;
  penalty_data.gradu_penalty_fcn = houston_gradu_prefactor_maxp_minh;
  penalty_data.penalty_prefactor = sipg_params->sipg_penalty_prefactor;
  penalty_data.sipg_flux_h = sipg_params->sipg_flux_h;
  
  d4est_amr_smooth_pred_marker_t amr_marker;
  amr_marker.user = (void*)&ctx;
  amr_marker.mark_element_fcn = amr_mark_element;
  amr_marker.set_element_gamma_fcn = amr_set_element_gamma;

  d4est_amr_t* d4est_amr =
    d4est_amr_init
    (
     p4est,
     input_file,
     &amr_marker
    );


   


  d4est_amr_t* d4est_amr_use_puncture_finder = d4est_amr_custom_init(p4est,d4est_amr->max_degree,d4est_amr->num_of_amr_steps,
                                                                     multi_puncture_find_punctures_element_marker, multi_puncture_params);


  d4est_amr_t* d4est_amr_use_puncture_finder_and_prefine_outside_cube = d4est_amr_custom_init(p4est,d4est_amr->max_degree,d4est_amr->num_of_amr_steps,
                                                                                              multi_puncture_find_punctures_and_prefine_outside_cube_element_marker, multi_puncture_params);

  
  d4est_amr_t* d4est_amr_p_refine_only_in_center_cube = d4est_amr_custom_init(p4est,d4est_amr->max_degree,d4est_amr->num_of_amr_steps,
                                                                              multi_puncture_only_p_refine_in_center_cube, multi_puncture_params);

  d4est_amr_t* d4est_amr_p_refine_everywhere = d4est_amr_custom_init(p4est,d4est_amr->max_degree,d4est_amr->num_of_amr_steps,
                                                                     multi_puncture_p_refine_everywhere, multi_puncture_params);

  

  if (initial_extents->checkpoint_prefix == NULL){
    d4est_mesh_init_field
      (
       p4est,
       prob_vecs.u,
       multi_puncture_initial_guess,
       d4est_ops,
       d4est_geom,
       d4est_factors,
       INIT_FIELD_ON_LOBATTO,
       NULL
      );
  }
  else {
    d4est_h5_read_dataset(p4est->mpirank,initial_extents->checkpoint_prefix,"u",H5T_NATIVE_DOUBLE, prob_vecs.u);
  }
  
  d4est_util_copy_1st_to_2nd(prob_vecs.u, u_prev, prob_vecs.local_nodes);
  
  
  // Norm function contexts
  
  d4est_norms_fcn_L2_ctx_t L2_norm_ctx;
  L2_norm_ctx.p4est = p4est;
  L2_norm_ctx.d4est_ops = d4est_ops;
  L2_norm_ctx.d4est_geom = d4est_geom;
  L2_norm_ctx.d4est_quad = d4est_quad;
  
  d4est_norms_fcn_energy_ctx_t energy_norm_ctx;
  energy_norm_ctx.p4est = p4est;
  energy_norm_ctx.d4est_ops = d4est_ops;
  energy_norm_ctx.d4est_geom = d4est_geom;
  energy_norm_ctx.d4est_quad = d4est_quad;
  energy_norm_ctx.d4est_factors = d4est_factors;
  energy_norm_ctx.fit = NULL;
  // These are updated later
  energy_norm_ctx.ghost = *ghost;
  energy_norm_ctx.ghost_data = *ghost_data;
  energy_norm_ctx.energy_norm_data = NULL;
  energy_norm_ctx.energy_estimator_sq_local = -1.;

  if (p4est->mpirank == 0)
    d4est_norms_write_headers(
      (const char * []){"u", NULL},
      (const char * []){"L_2", "L_infty", "energy_norm", "energy_estimator", NULL}
    );

  
  for (int level = 0; level < d4est_amr->num_of_amr_steps + 1; ++level){

    double* estimator = d4est_estimator_bi_compute
      (
       p4est,
       &prob_vecs,
       &prob_fcns,
       penalty_data,
       zero_fcn,
       *ghost,
       *ghost_data,
       d4est_ops,
       d4est_geom,
       d4est_quad,
       d4est_factors,
       NO_DIAM_APPROX
      );
    
    d4est_estimator_stats_t* stats = P4EST_ALLOC(d4est_estimator_stats_t,1);
    d4est_estimator_stats_compute(p4est, estimator, stats);
    d4est_estimator_stats_print(stats);

    d4est_linalg_vec_axpyeqz(-1., prob_vecs.u, u_prev, error, prob_vecs.local_nodes);
    d4est_elliptic_eqns_build_residual
      (
       p4est,
       *ghost,
       *ghost_data,
       &prob_fcns,
       &prob_vecs,
       d4est_ops,
       d4est_geom,
       d4est_quad,
       d4est_factors
      );

   d4est_vtk_save
      (
       p4est,
       d4est_ops,
       input_file,
       "d4est_vtk",
       (const char * []){"u","u_prev","error", NULL},
       (double* []){prob_vecs.u, u_prev, error},
       (const char * []){NULL},
       (double* []){NULL},
       level
      );

    d4est_ip_energy_norm_data_t ip_norm_data;
    ip_norm_data.u_penalty_fcn = sipg_params->sipg_penalty_fcn;
    ip_norm_data.sipg_flux_h = sipg_params->sipg_flux_h;
    ip_norm_data.penalty_prefactor = sipg_params->sipg_penalty_prefactor;
    
    energy_norm_ctx.energy_norm_data = &ip_norm_data;
    energy_norm_ctx.energy_estimator_sq_local = stats->total;
    energy_norm_ctx.ghost = *ghost;
    energy_norm_ctx.ghost_data = *ghost_data;

    d4est_norms_save(
      p4est,
      d4est_factors,
      (const char * []){ "u", NULL },
      (double * []){ prob_vecs.u },
      (double * []){ u_prev },
      (d4est_xyz_fcn_t []){ NULL },
      (void * []){ NULL },
      (const char * []){"L_2", "L_infty", "energy_norm", "energy_estimator", NULL},
      (d4est_norm_fcn_t[]){ &d4est_norms_fcn_L2, &d4est_norms_fcn_Linfty, &d4est_norms_fcn_energy, &d4est_norms_fcn_energy_estimator },
      (void * []){ &L2_norm_ctx, NULL, &energy_norm_ctx, &energy_norm_ctx }
    );
    

    // if (p4est->mpirank == 0){
    //   printf("[D4EST_OUTPUT]: Norms in cubic region only\n");
    // }
    // d4est_norms_norms
    //   (
    //    p4est,
    //    d4est_ops,
    //    d4est_geom,
    //    d4est_quad,
    //    d4est_factors,
    //    *ghost,
    //    *ghost_data,
    //    NULL,
    //    stats->total,
    //    error,
    //    NULL,
    //    skip_curved_elements
    //   );

    
    if (level != d4est_amr->num_of_amr_steps){

      if (p4est->mpirank == 0)
        printf("[D4EST_INFO]: AMR REFINEMENT LEVEL %d\n", level+1);

      d4est_amr_t* d4est_amr_normal = NULL;
      d4est_amr_t* d4est_amr_p_refine = NULL;
      if (init_params.use_puncture_finder == 1){
        d4est_amr_normal = d4est_amr_use_puncture_finder;
        d4est_amr_p_refine = d4est_amr_p_refine_only_in_center_cube;
      }
      else if (init_params.use_puncture_finder == 2){
        d4est_amr_normal = d4est_amr_use_puncture_finder_and_prefine_outside_cube ;
        d4est_amr_p_refine = d4est_amr_p_refine_everywhere;
      }
      else if (init_params.use_puncture_finder == 3){
        d4est_amr_normal = d4est_amr_use_puncture_finder;
        d4est_amr_p_refine = d4est_amr_p_refine_everywhere;
      }
      else {
        d4est_amr_normal = d4est_amr;
        d4est_amr_p_refine = d4est_amr_p_refine_only_in_center_cube;
      }
      

      d4est_amr_step
        (
         p4est,
         ghost,
         ghost_data,
         d4est_ops,
         (level >= init_params.amr_level_for_uniform_p) ? d4est_amr_p_refine : d4est_amr_normal,
         &prob_vecs.u,
         estimator,
         stats
        );
      
    }

    P4EST_FREE(stats);
    

    prob_vecs.local_nodes = d4est_mesh_update
                  (
                   p4est,
                   *ghost,
                   *ghost_data,
                   d4est_ops,
                   d4est_geom,
                   d4est_quad,
                   d4est_factors,
                   INITIALIZE_QUADRATURE_DATA,
                   INITIALIZE_GEOMETRY_DATA,
                   INITIALIZE_GEOMETRY_ALIASES,
                   d4est_mesh_set_quadratures_after_amr,
                   initial_extents
                  );

    
    prob_vecs.Au = P4EST_REALLOC(prob_vecs.Au, double, prob_vecs.local_nodes);
    u_prev = P4EST_REALLOC(u_prev, double, prob_vecs.local_nodes);
    error = P4EST_REALLOC(error, double, prob_vecs.local_nodes);
    d4est_util_copy_1st_to_2nd(prob_vecs.u, u_prev, prob_vecs.local_nodes);



   int min_level, max_level;

    multigrid_get_level_range(p4est, &min_level, &max_level);
    if (p4est->mpirank){
      printf("[min_level, max_level] = [%d,%d]\n", min_level, max_level);
    }
    int num_of_levels = (max_level-min_level) + 1;

 
    multigrid_logger_t* logger = multigrid_logger_residual_init
                                 (
                                 );
    
    multigrid_element_data_updater_t* updater = multigrid_element_data_updater_init
                                                (
                                                 num_of_levels,
                                                 ghost,
                                                 ghost_data,
                                                 d4est_factors,
                                                 d4est_mesh_set_quadratures_after_amr,
                                                 initial_extents
                                                );
    
    multigrid_user_callbacks_t* user_callbacks = multigrid_matrix_operator_init(p4est, num_of_levels);
    
    d4est_solver_multigrid_t* mg_data = multigrid_data_init(p4est,
                                                    d4est_ops,
                                                    d4est_geom,
                                                    d4est_quad,
                                                    num_of_levels,
                                                    logger,
                                                    user_callbacks,
                                                    updater,
                                                    input_file
                                                   );

    d4est_krylov_pc_t* pc = d4est_krylov_pc_multigrid_create(mg_data, multi_punctured4est_krylov_pc_setup_fcn);
    ctx.use_matrix_operator = 1;
    ctx.mg_data = mg_data;

    
    if (!init_params.do_not_solve){

      d4est_solver_newton_petsc_params_t newton_params;
      d4est_solver_newton_petsc_input(p4est, input_file, "[NEWTON_PETSC]", &newton_params);

      d4est_solver_krylov_petsc_params_t krylov_params;
      d4est_solver_krylov_petsc_input(p4est, input_file, "d4est_solver_krylov_petsc", &krylov_params);
      
      d4est_solver_newton_petsc_solve
        (
         p4est,
         &prob_vecs,
         &prob_fcns,
         ghost,
         ghost_data,
         d4est_ops,
         d4est_geom,
         d4est_quad,
         d4est_factors,
         &krylov_params,
         &newton_params,
         pc
        );
    }

    d4est_checkpoint_save
      (
       level,
       "checkpoint",
       p4est,
       d4est_factors,
       (const char * []){"u", NULL},
       (double* []){prob_vecs.u}
      );


    d4est_krylov_pc_multigrid_destroy(pc);
    multigrid_logger_residual_destroy(logger);
    multigrid_element_data_updater_destroy(updater, num_of_levels);
    multigrid_data_destroy(mg_data);
    multigrid_matrix_operator_destroy(user_callbacks);

    P4EST_FREE(estimator);
    
    
  }

  printf("[D4EST_INFO]: Starting garbage collection...\n");
  
  d4est_amr_destroy(d4est_amr);
  d4est_amr_destroy(d4est_amr_use_puncture_finder);
  d4est_amr_destroy(d4est_amr_p_refine_only_in_center_cube);
  d4est_amr_destroy(d4est_amr_use_puncture_finder_and_prefine_outside_cube);
  d4est_amr_destroy(d4est_amr_p_refine_everywhere);
  d4est_laplacian_flux_destroy(flux_data_for_jac);
  d4est_laplacian_flux_destroy(flux_data_for_res);
  P4EST_FREE(error);
  P4EST_FREE(u_prev);
  P4EST_FREE(prob_vecs.u);
  P4EST_FREE(prob_vecs.Au);
}
