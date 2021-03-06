#include <pXest.h>
#include <d4est_quadrature.h>
#include <d4est_element_data.h>
#include <d4est_quadrature_legendre.h>
#include <d4est_geometry.h>
#include <d4est_mesh.h>
#include <d4est_geometry_brick.h>
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

double
poly_vec_fcn
(
 double x,
 double y,
#if (P4EST_DIM)==3
 double z,
#endif
 void* user
){
#if (P4EST_DIM)==3
  return (x-1.)*x*(y-1.)*y*(z-1)*z;
#else
  return (x-1.)*x*(y-1.)*y;
#endif
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
#if (P4EST_DIM)==3
  sprintf(input_file, "%s", (argc == 2) ? argv[1] : "d4est_test_schwarz_parallelAu.input");
  #else
    sprintf(input_file, "%s", (argc == 2) ? argv[1] : "d4est_test_schwarz_parallelAu_2d.input");
#endif
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

  d4est_solver_schwarz_metadata_t* schwarz_data
    = d4est_solver_schwarz_metadata_init(
                                p4est,
                                d4est_ghost,
                                input_file,
                                "d4est_solver_schwarz"
    );

  d4est_solver_schwarz_operators_t* schwarz_ops
    = d4est_solver_schwarz_operators_init
    (d4est_ops);

    double* poly_vec = P4EST_ALLOC(double, d4est_factors->local_sizes.local_nodes);
  /* double* poly_vec_final = P4EST_ALLOC(double, d4est_factors->local_sizes.local_nodes); */
  d4est_mesh_init_field
    (
     p4est,
     poly_vec,
     poly_vec_fcn,
     d4est_ops, // unnecessary?
     d4est_geom, // unnecessary?
     d4est_factors,
     INIT_FIELD_ON_LOBATTO,
     NULL
    );

  dirichlet_bndry_eval_method_t eval_method = EVAL_BNDRY_FCN_ON_LOBATTO;

  /* / Setup boundary conditions */
  d4est_laplacian_dirichlet_bc_t bc_data_for_lhs;
  bc_data_for_lhs.dirichlet_fcn = poly_vec_fcn;
  bc_data_for_lhs.eval_method = eval_method;  

    
  d4est_laplacian_flux_data_t* flux_data_for_apply_lhs = d4est_laplacian_flux_new(p4est, input_file, BC_DIRICHLET, &bc_data_for_lhs);
  
  double* poly_vec_over_subdomains = P4EST_ALLOC(double,
                                                 schwarz_data->restricted_nodal_size);

  double* Apoly_vec_over_subdomains = P4EST_ALLOC(double,
                                                    schwarz_data->restricted_nodal_size);


  d4est_field_type_t field_type = NODAL;
  d4est_ghost_data_t* d4est_ghost_data = d4est_ghost_data_init(p4est,
                                                               d4est_ghost,
                                                               &field_type,
                                                               1);

    
  d4est_ghost_data_exchange(p4est,d4est_ghost,d4est_ghost_data, poly_vec);
  
  d4est_solver_schwarz_convert_nodal_field_to_restricted_field_over_subdomains
    (
     p4est,
     d4est_ops,
     d4est_factors,
     d4est_ghost,
     d4est_ghost_data,
     schwarz_data,
     schwarz_ops,
     poly_vec,
     0,
     poly_vec_over_subdomains
    );

  /* DEBUG_PRINT_MPI_ARR_DBL(p4est->mpirank,poly_vec_over_subdomains, schwarz_data->restricted_nodal_size); */
  
  d4est_solver_schwarz_geometric_data_t* schwarz_geometric_data = 
    d4est_solver_schwarz_geometric_data_init
    (
     p4est,
     d4est_ops,
     d4est_geom,
     d4est_quad,
     d4est_ghost,
     d4est_factors,
     schwarz_data,
     input_file,
     "d4est_solver_schwarz"
    );

  
  for (int i = 0; i < schwarz_data->num_subdomains; i++){

    d4est_solver_schwarz_subdomain_metadata_t* sub_data = &schwarz_data->subdomain_metadata[i];

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
       flux_data_for_apply_lhs,
       &poly_vec_over_subdomains[sub_data->restricted_nodal_stride],
       &Apoly_vec_over_subdomains[sub_data->restricted_nodal_stride],
       i
      );
  }

  /* DEBUG_PRINT_2ARR_DBL(poly_vec_over_subdomains, */
  /*                      Apoly_vec_over_subdomains, */
  /*                      schwarz_data->restricted_nodal_size); */

  DEBUG_PRINT_MPI_ARR_DBL_SUM(p4est->mpirank, Apoly_vec_over_subdomains,
                          schwarz_data->restricted_nodal_size);


  double local_sum = d4est_util_sum_array_dbl
                  (Apoly_vec_over_subdomains,
                   schwarz_data->restricted_nodal_size);

  double global_sum;
  sc_reduce(
            &local_sum,
            &global_sum,
            1,
            sc_MPI_DOUBLE,
            sc_MPI_SUM,
            0,
            sc_MPI_COMM_WORLD
  );

  if (p4est->mpirank == 0)
    printf("total sum = %.15f\n", global_sum);

  
  
  if (d4est_ghost_data != NULL){
    d4est_ghost_data_destroy(d4est_ghost_data);
    d4est_ghost_data = NULL;
  } 
  d4est_solver_schwarz_geometric_data_destroy(schwarz_geometric_data);

  P4EST_FREE(poly_vec);
  P4EST_FREE(poly_vec_over_subdomains);
  P4EST_FREE(Apoly_vec_over_subdomains);
  d4est_laplacian_flux_destroy(flux_data_for_apply_lhs);
  
  d4est_solver_schwarz_metadata_destroy
    (
     schwarz_data
    );

  d4est_solver_schwarz_operators_destroy
    (
     schwarz_ops
    );


  d4est_mesh_initial_extents_destroy(initial_grid_input);
  d4est_mesh_data_destroy(d4est_factors);
  d4est_quadrature_destroy(p4est, d4est_ops, d4est_geom, d4est_quad);
  
  if (d4est_ghost) {
    d4est_ghost_destroy(d4est_ghost);
  }


  if (d4est_ghost_data != NULL){
    d4est_ghost_data_destroy(d4est_ghost_data);
    d4est_ghost_data = NULL;
  }

  
  d4est_ops_destroy(d4est_ops);
  p4est_destroy(p4est);
  d4est_geometry_destroy(d4est_geom);

  P4EST_FREE(input_file);
  
  PetscFinalize();
  return 0;
}
