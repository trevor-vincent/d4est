#include <sc_reduce.h>
#include <pXest.h>
#include <util.h>
#include <d4est_linalg.h>
#include <d4est_element_data.h>
#include <ip_flux_params.h>
#include <ip_flux.h>
#include <problem.h>
#include <problem_data.h>
#include <d4est_elliptic_eqns.h>
#include <d4est_estimator_bi.h>
#include <d4est_hp_amr.h>
#include <d4est_hp_amr_smooth_pred.h>
#include <d4est_geometry.h>
#include <d4est_geometry_cubed_sphere.h>
#include <d4est_vtk.h>
#include <d4est_mesh.h>
#include <ini.h>
#include <d4est_poisson.h>
#include <curved_Gauss_primal_sipg_kronbichler_flux_fcns.h>
#include <util.h>
#include "./twopuncturesfcns_nomg.h"
#include <time.h>


double*
get_storage(d4est_element_data_t* ed){
  return &(ed->u_elem[0]);
}

/* soon to be in the input files */
static const double pi = 3.1415926535897932384626433832795;

typedef struct {

  double* u;
  int* eta2;
  double* jacobian;

} vtk_nodal_vecs_t;

double
get_diam
(
 d4est_element_data_t* ed
)
{
  return d4est_geometry_compute_diam(ed->xyz,ed->deg, NO_DIAM_APPROX); 
}

void
vtk_field_plotter
(
 d4est_vtk_context_t* vtk_ctx,
 void* user
)
{
  vtk_nodal_vecs_t* vecs = user;
  vtk_ctx = d4est_vtk_write_dg_point_dataf(vtk_ctx,
                                           2,
                                           0,
                                           "u",
                                           vecs->u,
                                           "jacobian",
                                           vecs->jacobian,
                                           vtk_ctx
                                          );


   vtk_ctx = d4est_vtk_write_dg_cell_dataf
                (
                 vtk_ctx,
                 1,
                 1,
                 1,
                 0,
                 1,
                 0,
                 0,
                 vtk_ctx
                );
  
   vtk_ctx = d4est_vtk_write_dg_cell_dataf
             (
              vtk_ctx,
              1,
              1,
              1,
              0,
              1,
              1,
              0,
              "eta",
              vecs->eta2,
              vtk_ctx
             );

}


static
int
amr_mark_element
(
 p4est_t* p4est,
 double eta2,
 estimator_stats_t** stats,
 d4est_element_data_t* elem_data,
 void* user
)
{
  int elem_bin;

  /* outer shell */
  if (elem_data->tree < 6){
    elem_bin = 0;
  }
  /* inner shell */
  else if(elem_data->tree < 12){
    elem_bin = 1;
  }
  /* center cube */
  else {
    elem_bin = 2;
  }
 
  /* if (elem_data->tree == 12){ */
  double eta2_percentile
    = estimator_stats_get_percentile(stats[elem_bin], 5);

  /* if (elem_data->tree == 12) */
    return (eta2 >= eta2_percentile);
  /* else */
    /* return 0; */
  /* } */
  /* else { */
    /* return 0; */
  /* } */
}

static
gamma_params_t
amr_set_element_gamma
(
 p4est_t* p4est,
 double eta2,
 estimator_stats_t** stats,
 d4est_element_data_t* elem_data,
 void* user
)
{
  gamma_params_t gamma_hpn;
  gamma_hpn.gamma_h = 0;
  gamma_hpn.gamma_p = 0;
  gamma_hpn.gamma_n = 0;

  return gamma_hpn;
}

typedef struct {

  int num_of_amr_levels;
  int deg;
  int deg_quad;
  
} problem_input_t;

static
int problem_input_handler
(
 void* user,
 const char* section,
 const char* name,
 const char* value
)
{
  problem_input_t* pconfig = (problem_input_t*)user;
  if (util_match_couple(section,"problem",name,"num_of_amr_levels")) {
    mpi_assert(pconfig->num_of_amr_levels == -1);
    pconfig->num_of_amr_levels = atoi(value);
  }
  else if (util_match_couple(section,"problem",name,"deg")) {
    mpi_assert(pconfig->deg == -1);
    pconfig->deg = atoi(value);
  }
  else if (util_match_couple(section,"problem",name,"deg_quad")) {
    mpi_assert(pconfig->deg_quad == -1);
    pconfig->deg_quad = atoi(value);
  }
  else {
    return 0;
  }
  return 1;
}


static
problem_input_t
problem_input
(
 const char* input_file
)
{
  problem_input_t input;
  input.num_of_amr_levels = -1;
  input.deg = -1;
  input.deg_quad = -1;
  
  if (ini_parse(input_file, problem_input_handler, &input) < 0) {
    mpi_abort("Can't load input file");
  }

  D4EST_CHECK_INPUT("problem", input.num_of_amr_levels, -1);
  D4EST_CHECK_INPUT("problem", input.deg, -1);
  D4EST_CHECK_INPUT("problem", input.deg_quad, -1);
  printf("[PROBLEM]: deg = %d\n",input.deg);
  printf("[PROBLEM]: deg_quad = %d\n",input.deg_quad);
  printf("[PROBLEM]: num_of_amr_levels = %d\n",input.num_of_amr_levels);
  return input;
}



p4est_t*
problem_build_p4est
(
 sc_MPI_Comm mpicomm,
 p4est_connectivity_t* conn,
 p4est_locidx_t min_quadrants,
 int min_level, 
 int fill_uniform
)
{
  return p4est_new_ext
    (
     mpicomm,
     conn,
     min_quadrants,
     min_level,
     fill_uniform,
     sizeof(d4est_element_data_t),
     NULL,
     NULL
    );
}

p4est_t*
problem_load_p4est_from_checkpoint
(
 const char* filename,
 sc_MPI_Comm mpicomm,
 p4est_connectivity_t** conn
){
  int autopartition = 1;
  int load_data = 1;
  int broadcasthead = 0;
  
  return p4est_load_ext (filename,
                mpicomm,
                sizeof(d4est_element_data_t),
                load_data,
                autopartition,
                broadcasthead,
                NULL,
                conn);
}


int
in_bin_fcn
(
 d4est_element_data_t* elem_data,
 int bin
)
{
  int elem_bin;

  /* outer shell */
  if (elem_data->tree < 6){
    elem_bin = 0;
  }
  /* inner shell */
  else if(elem_data->tree < 12){
    elem_bin = 1;
  }
  /* center cube */
  else {
    elem_bin = 2;
  }

  return (elem_bin == bin);
}



void
problem_set_degrees
(
 d4est_element_data_t* elem_data,
 void* user_ctx
)
{
  problem_input_t* input = user_ctx;
  elem_data->deg = input->deg;
  elem_data->deg_quad = input->deg_quad;
  printf("PROBLEM_SET_DEGREES BEGIN\n");
  printf("elem_data->id = %d\n", elem_data->id);
  printf("elem_data->deg = %d\n", elem_data->deg);
  printf("elem_data->deg_quad = %d\n", elem_data->deg_quad);
  printf("input->deg = %d\n", input->deg);
  printf("input->deg_quad = %d\n", input->deg_quad);
  printf("PROBLEM_SET_DEGREES END\n");
}


void
problem_init
(
 const char* input_file,
 p4est_t* p4est,
 d4est_geometry_t* d4est_geom,
 d4est_operators_t* d4est_ops,
 int proc_size,
 sc_MPI_Comm mpicomm
)
{
  problem_input_t input = problem_input(input_file);
  
  mpi_assert((P4EST_DIM) == 2 || (P4EST_DIM) == 3);
  int world_rank, world_size;
  sc_MPI_Comm_rank(sc_MPI_COMM_WORLD, &world_rank);
  sc_MPI_Comm_size(sc_MPI_COMM_WORLD, &world_size);

  p4est_ghost_t* ghost = p4est_ghost_new (p4est, P4EST_CONNECT_FACE);
  /* create space for storing the ghost data */
  d4est_element_data_t* ghost_data = P4EST_ALLOC (d4est_element_data_t,
                                                   ghost->ghosts.elem_count);

  p4est_partition(p4est, 0, NULL);
  p4est_balance (p4est, P4EST_CONNECT_FACE, NULL);

  twopunctures_params_t tp_params;
  init_twopunctures_data(&tp_params);


  d4est_elliptic_eqns_t prob_fcns;
  prob_fcns.build_residual = twopunctures_build_residual;
  prob_fcns.apply_lhs = twopunctures_apply_jac;

 
  d4est_mesh_geometry_storage_t* geometric_factors = d4est_mesh_geometry_storage_init(p4est);
  d4est_quadrature_t* d4est_quad = d4est_quadrature_new(p4est, d4est_ops, d4est_geom, input_file, "quadrature", "[QUADRATURE]");
  
  int local_nodes = d4est_mesh_update
                    (
                     p4est,
                     ghost,
                     ghost_data,
                     d4est_ops,
                     d4est_geom,
                     d4est_quad,
                     geometric_factors,
                     INITIALIZE_QUADRATURE_DATA,
                     INITIALIZE_GEOMETRY_DATA,
                     INITIALIZE_GEOMETRY_ALIASES,
                     problem_set_degrees,
                     (void*)&input
                    );
  
  /* double* Au = P4EST_ALLOC_ZERO(double, local_nodes); */
  /* double* rhs = P4EST_ALLOC_ZERO(double, local_nodes); */
  /* double* u = P4EST_ALLOC_ZERO(double, local_nodes); */
  /* double* u_prev = P4EST_ALLOC_ZERO(double, local_nodes); */
  
  
  /* d4est_elliptic_problem_data_t prob_vecs; */
  /* prob_vecs.rhs = rhs; */
  /* prob_vecs.Au = Au; */
  /* prob_vecs.u = u; */
  /* prob_vecs.local_nodes = local_nodes; */
  /* prob_vecs.user = &tp_params; */
  
  /* ip_flux_t* ip_flux = ip_flux_dirichlet_new(p4est, "[IP_FLUX]", input_file, zero_fcn); */
  /* prob_vecs.flux_fcn_data = ip_flux->mortar_fcn_ptrs; */
  
  /* smooth_pred_marker_t amr_marker; */
  /* amr_marker.user = (void*)&input; */
  /* amr_marker.mark_element_fcn = amr_mark_element; */
  /* amr_marker.set_element_gamma_fcn = amr_set_element_gamma; */
  /* amr_marker.name = "puncture_marker"; */

  /* d4est_hp_amr_scheme_t* scheme = */
  /*   d4est_hp_amr_smooth_pred_init */
  /*   ( */
  /*    p4est, */
  /*    8, */
  /*    amr_marker */
  /*   ); */

  /* d4est_estimator_bi_penalty_data_t penalty_data; */
  /* penalty_data.u_penalty_fcn = bi_u_prefactor_conforming_maxp_minh; */
  /* penalty_data.u_dirichlet_penalty_fcn = bi_u_prefactor_conforming_maxp_minh; */
  /* penalty_data.gradu_penalty_fcn = bi_gradu_prefactor_maxp_minh; */
  /* penalty_data.penalty_prefactor = ip_flux->ip_flux_params->ip_flux_penalty_prefactor; */
  /* penalty_data.ip_flux_h_calc = ip_flux->ip_flux_params->ip_flux_h_calc; */
  
  /* for (int level = 0; level < input.num_of_amr_levels; ++level){ */

  /*   if (world_rank == 0) */
  /*     printf("[D4EST_INFO]: AMR REFINEMENT LEVEL %d\n", level); */
    

  /*   d4est_estimator_bi_compute */
  /*     ( */
  /*      p4est, */
  /*      &prob_vecs, */
  /*      &prob_fcns, */
  /*      penalty_data, */
  /*      zero_fcn, */
  /*      ghost, */
  /*      ghost_data, */
  /*      d4est_ops, */
  /*      d4est_geom, */
  /*      d4est_quad, */
  /*      get_diam */
  /*     ); */

    
  /*   estimator_stats_t* stats [3]; */
  /*   for (int i = 0; i < 3; i++){ */
  /*     stats[i] = P4EST_ALLOC(estimator_stats_t, 1); */
  /*   } */
    
  /*   double local_eta2 = estimator_stats_compute_per_bin */
  /*                       ( */
  /*                        p4est, */
  /*                        &stats[0], */
  /*                        3, */
  /*                        in_bin_fcn */
  /*                       ); */

  /*   d4est_mesh_print_number_of_elements_per_tree(p4est); */
    
  /*   estimator_stats_compute_max_percentiles_across_proc */
  /*     ( */
  /*      stats, */
  /*      3 */
  /*     ); */

  /*   if (world_rank == 0){ */
  /*     for (int i = 0; i < 3; i++){ */
  /*       estimator_stats_print(stats[i]); */
  /*     } */
  /*   } */
    
  /*   double* jacobian_lgl = P4EST_ALLOC(double, local_nodes); */
  /*   int* deg_array = P4EST_ALLOC(int, p4est->local_num_quadrants); */
  /*   int* eta2_array = P4EST_ALLOC(int, p4est->local_num_quadrants); */
  /*   d4est_mesh_compute_jacobian_on_lgl_grid(p4est, d4est_ops, d4est_geom, jacobian_lgl); */
  /*   d4est_mesh_get_array_of_degrees(p4est, deg_array); */
  /*   d4est_mesh_get_array_of_estimators(p4est, eta2_array); */

  /*   vtk_nodal_vecs_t vtk_nodal_vecs; */
  /*   vtk_nodal_vecs.u = u; */
  /*   vtk_nodal_vecs.jacobian = jacobian_lgl; */
  /*   vtk_nodal_vecs.eta2 = eta2_array; */

    
  /*   char save_as [500]; */
  /*   sprintf(save_as, "%s_level_%d", "puncture_cubedsphere", level); */
    
  /*   /\* vtk output *\/ */
  /*   d4est_vtk_save_geometry_and_dg_fields */
  /*     ( */
  /*      save_as, */
  /*      p4est, */
  /*      d4est_ops, */
  /*      deg_array, */
  /*      input_file, */
  /*      "d4est_vtk_geometry", */
  /*      vtk_field_plotter, */
  /*      (void*)&vtk_nodal_vecs */
  /*     ); */

  /*   P4EST_FREE(jacobian_lgl); */
  /*   P4EST_FREE(deg_array); */
  /*   P4EST_FREE(eta2_array); */
    
  /*   d4est_hp_amr(p4est, */
  /*                d4est_ops, */
  /*                &u, */
  /*                &stats[0], */
  /*                scheme, */
  /*                get_storage */
  /*               );         */

  /*   for (int i = 0; i < 3; i++){ */
  /*     P4EST_FREE(stats[i]); */
  /*   } */
    
  /*   p4est_ghost_destroy(ghost); */
  /*   P4EST_FREE(ghost_data); */

  /*   ghost = p4est_ghost_new(p4est, P4EST_CONNECT_FACE); */
  /*   ghost_data = P4EST_ALLOC(d4est_element_data_t, ghost->ghosts.elem_count); */
    
  /*   local_nodes = d4est_mesh_update */
  /*                   ( */
  /*                    p4est, */
  /*                    ghost, */
  /*                    ghost_data, */
  /*                    d4est_ops, */
  /*                    d4est_geom, */
  /*                    d4est_quad, */
  /*                    geometric_factors, */
  /*                    INITIALIZE_QUADRATURE_DATA, */
  /*                    INITIALIZE_GEOMETRY_DATA, */
  /*                    INITIALIZE_GEOMETRY_ALIASES, */
  /*                    problem_set_degrees, */
  /*                    (void*)&input */
  /*                   ); */
    
  /*   u_prev = P4EST_REALLOC(u_prev, double, local_nodes); */
  /*   Au = P4EST_REALLOC(Au, double, local_nodes); */
  /*   prob_vecs.Au = Au; */
  /*   prob_vecs.u = u; */
  /*   prob_vecs.u0 = u; */
  /*   prob_vecs.local_nodes = local_nodes; */

  /*   d4est_linalg_copy_1st_to_2nd(u, u_prev, local_nodes); */


  /* } */
         

  d4est_mesh_geometry_storage_destroy(geometric_factors);
  ip_flux_dirichlet_destroy(ip_flux);
  
  if (ghost) {
    p4est_ghost_destroy (ghost);
    P4EST_FREE (ghost_data);
    ghost = NULL;
    ghost_data = NULL;
  }

  d4est_quadrature_destroy(p4est, d4est_ops, d4est_geom, d4est_quad);

  P4EST_FREE(Au);
  P4EST_FREE(rhs);
  P4EST_FREE(u_prev);
  /* P4EST_FREE(u_analytic); */
  P4EST_FREE(u);
}