#include <pXest.h>
#include <d4est_quadrature.h>
#include <d4est_element_data.h>
#include <d4est_quadrature_legendre.h>
#include <d4est_geometry.h>
#include <d4est_mesh.h>
#include <d4est_geometry_cubed_sphere.h>
#include <petscsnes.h>
#include <d4est_linalg.h>
#include <d4est_mortars.h>
#include <d4est_amr.h>
#include <d4est_laplacian.h>
#include <d4est_hessian.h>
#include <d4est_laplacian_flux.h>
#include <d4est_solver_matrix_symmetry.h>
#include <d4est_util.h>
#include <d4est_norms.h>
#include <d4est_vtk.h>
#include <sc_reduce.h>
#include <limits.h>
#include <zlog.h>
#include <ini.h>
#include <d4est_solver_schwarz_metadata.h>
#include <d4est_solver_schwarz_transfer_ghost_data.h>
#include <d4est_solver_schwarz_helpers.h>
#include <d4est_solver_schwarz_geometric_data.h>
#include <d4est_solver_schwarz_laplacian_ext.h>
#include <d4est_solver_schwarz.h>

static void
d4est_test_schwarz_apply_lhs
(
 p4est_t* p4est,
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 d4est_mesh_data_t* d4est_factors,
 d4est_ghost_t* d4est_ghost,
 d4est_solver_schwarz_operators_t* schwarz_ops,
 d4est_solver_schwarz_metadata_t* schwarz_data,
 d4est_solver_schwarz_geometric_data_t* schwarz_geometric_data,
 int subdomain,
 double* u_restricted_field_over_subdomain,
 double* Au_restricted_field_over_subdomain,
 void* ctx
){

  d4est_solver_schwarz_laplacian_ext_apply_over_subdomain
    (
     p4est,
     d4est_ops,
     d4est_geom,
     d4est_quad,
     d4est_factors,
     d4est_ghost,
     schwarz_data,
     schwarz_ops,
     schwarz_geometric_data,
     ctx,
     u_restricted_field_over_subdomain,
     Au_restricted_field_over_subdomain,
     subdomain
    );
}


static void
d4est_test_build_residual
(
 p4est_t* p4est,
 d4est_ghost_t* ghost,
 d4est_ghost_data_t* ghost_data,
 d4est_elliptic_data_t* prob_vecs,
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 d4est_mesh_data_t* d4est_factors,
 void* user
)
{
  d4est_laplacian_flux_data_t* flux_data_for_apply_lhs = user;  
  d4est_laplacian_apply_aij(p4est, ghost, ghost_data, prob_vecs, flux_data_for_apply_lhs, d4est_ops, d4est_geom, d4est_quad, d4est_factors, 0);
  d4est_linalg_vec_xpby(prob_vecs->rhs, -1., prob_vecs->Au, prob_vecs->local_nodes);
}


static double
poisson_lorentzian_initial_guess
(
 double x,
 double y,
#if (P4EST_DIM)==3
 double z,
#endif
 void* user
)
{
  return 1.;
}


static void
d4est_test_apply_lhs
(
 p4est_t* p4est,
 d4est_ghost_t* ghost,
 d4est_ghost_data_t* ghost_data,
 d4est_elliptic_data_t* prob_vecs,
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 d4est_mesh_data_t* d4est_factors,
 void* user
)
{
  d4est_laplacian_flux_data_t* flux_data_for_apply_lhs = user;  
  d4est_laplacian_apply_aij(p4est, ghost, ghost_data, prob_vecs, flux_data_for_apply_lhs, d4est_ops, d4est_geom, d4est_quad, d4est_factors, 0);
}


typedef struct {

  double R_surface;

} lorentzian_params_t;

typedef struct {

  d4est_laplacian_flux_data_t* flux_data_for_apply_lhs;
  d4est_laplacian_flux_data_t* flux_data_for_build_rhs;

} problem_ctx_t;


static double
poisson_lorentzian_analytic_solution
(
 double x,
 double y,
#if (P4EST_DIM)==3
 double z,
#endif
 void* user
)
{
  double r = sqrt(x*x + y*y + z*z);
  return 1./sqrt(1.+r*r);
}


static double
poisson_lorentzian_rhs_fcn
(
 double x,
 double y,
#if (P4EST_DIM)==3
 double z,
#endif
 void* user
)
{
  return 3./pow((1. + x*x + y*y + z*z),2.5);
}


static double
poisson_lorentzian_boundary_fcn
(
 double x,
 double y,
#if (P4EST_DIM)==3
 double z,
#endif
 void* user
)
{
  lorentzian_params_t* params = user;
  double R = params->R_surface;
  double one_p_R2 = (1. + R*R);
  double den = sqrt(one_p_R2);
  /* printf("R = %.15f\n", R); */
  /* printf("3./pow((1. + R*R),2.5) = %.15f\n", 3./den); */
  return 1./den;
}



int main(int argc, char *argv[])
{

#ifndef D4EST_TEST
  D4EST_ABORT("D4EST_TEST not defined");
#endif
  
  sc_MPI_Comm mpicomm;
  PetscInitialize(&argc,&argv,(char*)0,NULL);
  mpicomm = PETSC_COMM_WORLD;
  
  int proc_size;
  int proc_rank;
  MPI_Comm_size(mpicomm, &proc_size);
  MPI_Comm_rank(mpicomm, &proc_rank);
  
#ifndef NDEBUG
  if(proc_rank == 0)
    printf("[D4EST_INFO]: DEBUG MODE ON\n");
  p4est_init(NULL, SC_LP_ERROR);
  /* p4est_init(NULL, SC_LP_ALWAYS); */
#else
  if(proc_rank == 0)
    printf("[D4EST_INFO]: DEBUG MODE OFF\n");
  p4est_init(NULL, SC_LP_ERROR);
#endif
  
#if (P4EST_DIM)==3
  if(proc_rank == 0)
    printf("[D4EST_INFO]: DIM = 3\n");
#else
  if(proc_rank == 0)
    printf("[D4EST_INFO]: DIM = 2\n");
#endif

  char* input_file = P4EST_ALLOC(char, 100);
  sprintf(input_file, "%s", (argc == 2) ? argv[1] : "d4est_test_schwarz_lorentzian.input");
  
  if (proc_rank == 0)
    printf("[D4EST_INFO]: options file = %s\n", input_file);
    
  zlog_category_t *c_geom = zlog_get_category("d4est_geometry");
  d4est_geometry_t* d4est_geom = d4est_geometry_new(proc_rank,
                                                    input_file,
                                                    "geometry",
                                                    c_geom);

  d4est_mesh_initial_extents_t* initial_grid_input = d4est_mesh_initial_extents_parse(input_file, d4est_geom);

  p4est_t* p4est;
  p4est = p4est_new_ext
          (
           mpicomm,
           d4est_geom->p4est_conn,
           initial_grid_input->min_quadrants,
           initial_grid_input->min_level,
           initial_grid_input->fill_uniform,
           sizeof(d4est_element_data_t),
           NULL,
           NULL
          );


  p4est_partition(p4est, 1, NULL);
  p4est_balance (p4est, P4EST_CONNECT_FULL, NULL);
       
  if (proc_rank == 0){
    printf("[D4EST_INFO]: mpisize = %d\n", proc_size);
  }
  if (proc_rank == 0 && initial_grid_input->load_from_checkpoint == 0){
    printf("[D4EST_INFO]: min_quadrants = %d\n", initial_grid_input->min_quadrants);
    printf("[D4EST_INFO]: min_level = %d\n", initial_grid_input->min_level);
    printf("[D4EST_INFO]: fill_uniform = %d\n", initial_grid_input->fill_uniform);
  }
  
  sc_MPI_Barrier(mpicomm);
  printf("[D4EST_INFO]: elements on proc %d = %d\n", proc_rank, p4est->local_num_quadrants);
  sc_MPI_Barrier(mpicomm);
  
  /* start just-in-time dg-math */
  d4est_operators_t* d4est_ops = d4est_ops_init(20);
  d4est_mesh_data_t* d4est_factors = d4est_mesh_data_init(p4est);
  d4est_quadrature_t* d4est_quad = d4est_quadrature_new(p4est, d4est_ops, d4est_geom, input_file, "quadrature");
  
  d4est_ghost_t* d4est_ghost = NULL;

  d4est_mesh_local_sizes_t local_sizes = d4est_mesh_update
                                         (
                                          p4est,
                                          &d4est_ghost,
                                          d4est_ops,
                                          d4est_geom,
                                          d4est_quad,
                                          d4est_factors,
                                          initial_grid_input,
                                          INITIALIZE_GHOST,
                                          INITIALIZE_QUADRATURE_DATA,
                                          INITIALIZE_GEOMETRY_DATA,
                                          INITIALIZE_GEOMETRY_ALIASES,
                                          d4est_mesh_set_initial_extents,
                                          (void*)initial_grid_input
                                         );

  
  dirichlet_bndry_eval_method_t eval_method = EVAL_BNDRY_FCN_ON_LOBATTO;


  lorentzian_params_t lorentzian_params;
  lorentzian_params.R_surface = ((d4est_geometry_cubed_sphere_attr_t*)d4est_geom->user)->R2;
  // Setup boundary conditions
  
  d4est_laplacian_dirichlet_bc_t bc_data_dirichlet_for_lhs;
  bc_data_dirichlet_for_lhs.dirichlet_fcn = zero_fcn;
  bc_data_dirichlet_for_lhs.eval_method = eval_method;
  bc_data_dirichlet_for_lhs.user = &lorentzian_params;
  
  d4est_laplacian_dirichlet_bc_t bc_data_dirichlet_for_rhs;
  bc_data_dirichlet_for_rhs.dirichlet_fcn = poisson_lorentzian_boundary_fcn;
  bc_data_dirichlet_for_rhs.eval_method = eval_method;
  bc_data_dirichlet_for_rhs.user = &lorentzian_params;
  
  d4est_laplacian_flux_data_t* flux_data_for_lhs = NULL; //d4est_laplacian_flux_new(p4est, input_file, BC_DIRICHLET, 
  d4est_laplacian_flux_data_t* flux_data_for_rhs = NULL; //d4est_laplacian_flux_new(p4est, input_file, BC_DIRICHLET, &bc_data_dirichlet_for_rhs);

  flux_data_for_lhs
    = d4est_laplacian_flux_new(p4est, input_file, BC_DIRICHLET, &bc_data_dirichlet_for_lhs);
  
  flux_data_for_rhs
    = d4est_laplacian_flux_new(p4est, input_file,  BC_DIRICHLET, &bc_data_dirichlet_for_rhs);
  
  /* problem_ctx_t ctx; */
  /* ctx.flux_data_for_apply_lhs = flux_data_for_lhs; */
  /* ctx.flux_data_for_build_rhs = flux_data_for_rhs; */
                           
  d4est_elliptic_eqns_t prob_fcns;
  prob_fcns.build_residual = d4est_test_build_residual;
  prob_fcns.apply_lhs = d4est_test_apply_lhs;
  prob_fcns.user = flux_data_for_lhs;

    
  double* u = P4EST_ALLOC_ZERO(double, local_sizes.local_nodes);
  double* rhs = P4EST_ALLOC(double, local_sizes.local_nodes);
  double* r = P4EST_ALLOC(double, local_sizes.local_nodes);
  double* sol = P4EST_ALLOC(double, local_sizes.local_nodes);
  double* error = P4EST_ALLOC(double, local_sizes.local_nodes);
  d4est_field_type_t field_type = NODAL;
  d4est_elliptic_data_t elliptic_data;
  elliptic_data.u = u;
  elliptic_data.Au = r;
  elliptic_data.rhs = rhs;
  elliptic_data.local_nodes = local_sizes.local_nodes;
  elliptic_data.field_types = &field_type;
  elliptic_data.num_of_fields = 1;


    
  d4est_mesh_init_field
    (
     p4est,
     sol,
     poisson_lorentzian_initial_guess,
     d4est_ops, // unnecessary?
     d4est_geom, // unnecessary?
     d4est_factors,
     INIT_FIELD_ON_LOBATTO,
     NULL
    );
  

  d4est_ghost_data_t* d4est_ghost_data = d4est_ghost_data_init(p4est,
                                                               d4est_ghost,
                                                               &field_type,
                                                               1);

    
  d4est_solver_schwarz_apply_lhs_t*
    apply_lhs = d4est_solver_schwarz_apply_lhs_init
    (
     d4est_test_schwarz_apply_lhs,
     flux_data_for_lhs
    );

  
  d4est_laplacian_build_rhs_with_strong_bc
    (
     p4est,
     d4est_ghost,
     d4est_ghost_data,
     d4est_ops,
     d4est_geom,
     d4est_quad,
     d4est_factors,
     &elliptic_data,
     flux_data_for_rhs,
     rhs,
     poisson_lorentzian_rhs_fcn,
     INIT_FIELD_ON_LOBATTO,
     NULL,
     0
    );

d4est_solver_schwarz_t* schwarz =
  d4est_solver_schwarz_init
    (
     p4est,
     d4est_ops,
     d4est_geom,
     d4est_quad,
     d4est_ghost,
     d4est_factors,
     NULL,
     apply_lhs,
     input_file,
     "d4est_solver_schwarz"
    );

 int iter = 10;
  for (int i = 0; i < iter; i++){


    /* if (d4est_ghost_data != NULL){ */
    /* d4est_ghost_data_destroy(d4est_ghost_data); */
    /* d4est_ghost_data = NULL; */
    /* }  */
    /* d4est_ghost_data = d4est_ghost_data_init */
    /*                    (p4est, */
    /*                     d4est_ghost, */
    /*                     &field_type, */
    /*                     1); */
    
    elliptic_data.u = u;
    elliptic_data.Au = r;
    elliptic_data.rhs = rhs;


    
    /* if (i == 0){ */
    d4est_elliptic_eqns_build_residual
      (
       p4est,
       d4est_ghost,
       d4est_ghost_data,
       &prob_fcns,
       &elliptic_data,
       d4est_ops,
       d4est_geom,
       d4est_quad,
       d4est_factors
    );

    /* DEBUG_PRINT_ARR_DBL_SUM(u, elliptic_data.local_nodes); */
    /* DEBUG_PRINT_ARR_DBL_SUM(r, elliptic_data.local_nodes); */
    /* DEBUG_PRINT_ARR_DBL_SUM(rhs, elliptic_data.local_nodes); */


    /* d4est_ghost_data_exchange(p4est,d4est_ghost,d4est_ghost_data, r); */
     

    double r2 = d4est_linalg_vec_dot(r, r, local_sizes.local_nodes);
    
    d4est_solver_schwarz_iterate
      (
       p4est,
       d4est_geom,
       d4est_quad,
       d4est_factors,
       d4est_ghost,
       schwarz,
       u,
       r
      );

    d4est_util_compute_error_array(sol, u, error, local_sizes.local_nodes);
    double l2 = d4est_mesh_compute_l2_norm_sqr
                (
                 p4est,
                 d4est_ops,
                 d4est_geom,
                 d4est_quad,
                 d4est_factors,
                 error,
                 local_sizes.local_nodes,
                 NULL,
                 NULL);


    double globals [2];
    double locals [] = {r2, l2};

  sc_reduce(
            &locals,
            &globals,
            2,
            sc_MPI_DOUBLE,
            sc_MPI_SUM,
            0,
            sc_MPI_COMM_WORLD
  );    

  if(p4est->mpirank == 0){
    printf("pre r2 norm, post l2 norm = %.15f, %.15f\n",globals[0], globals[1]);
  }

    
  }

   if (d4est_ghost_data != NULL){
    d4est_ghost_data_destroy(d4est_ghost_data);
    d4est_ghost_data = NULL;
  } 

  

  d4est_solver_schwarz_destroy
    (
     schwarz
    );

  d4est_solver_schwarz_apply_lhs_destroy
    (
     apply_lhs
    );
  
  /*  */

  d4est_laplacian_flux_destroy(flux_data_for_lhs);
  d4est_laplacian_flux_destroy(flux_data_for_rhs);

  
  P4EST_FREE(u);
  P4EST_FREE(rhs);
  P4EST_FREE(r);  
  P4EST_FREE(sol);  
  P4EST_FREE(error);  

  d4est_mesh_initial_extents_destroy(initial_grid_input);
  d4est_mesh_data_destroy(d4est_factors);
  d4est_quadrature_destroy(p4est, d4est_ops, d4est_geom, d4est_quad);
  
  if (d4est_ghost) {
    d4est_ghost_destroy(d4est_ghost);
  }

  
  d4est_ops_destroy(d4est_ops);
  p4est_destroy(p4est);
  d4est_geometry_destroy(d4est_geom);

  P4EST_FREE(input_file);
  
  PetscFinalize();
  return 0;
}
