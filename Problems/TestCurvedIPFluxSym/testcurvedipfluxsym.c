#include <sc_reduce.h>
#include <pXest.h>
#include <util.h>
#include <d4est_linalg.h>
#include <d4est_element_data.h>
#include <sipg_flux_vector_fcns.h>
#include <curved_gauss_sipg_flux_scalar_fcns.h>
#include <curved_gauss_sipg_flux_vector_fcns.h>
#include <curved_gauss_central_flux_vector_fcns.h>
#include <problem.h>
#include <problem_data.h>
#include <problem_weakeqn_ptrs.h>
#include <central_flux_params.h>
#include <curved_poisson_operator.h>
#include <krylov_petsc.h>
#include <matrix_sym_tester.h>
#include <dg_norm.h>
#include <d4est_geometry.h>
#include <d4est_geometry_sphere.h>
#include <d4est_geometry_disk.h>
#include <curved_poisson_debug_vecs.h>
#include <d4est_vtk.h>
#include <ini.h>
#include "time.h"
#include "util.h"

double global_Rinf;
double global_sigma;

static int
random_h_refine(p4est_t * p4est, p4est_topidx_t which_tree,
                 p4est_quadrant_t * quadrant)
{
  /* d4est_element_data_t* data = (d4est_element_data_t*) quadrant->p.user_data; */
  return rand()%2;
}

static
double analytic_fcn
(
 double x,
 double y
 #if (P4EST_DIM)==3
 ,
 double z
#endif
)
{
  /* int n = 3; */
  /* double arg = -(x*x + y*y + z*z)/(global_sigma*global_sigma); */
  /* double N = 1./sqrt(pow(2,n)*pow(global_sigma,2*n)*pow(M_PI,n)); */
  /* return N*exp(arg); */
  double r2 = x*x + y*y;
#if (P4EST_DIM)==3
 r2 += z*z;
#endif 
  return global_Rinf*global_Rinf - r2;
}

static
double boundary_fcn
(
 double x,
 double y
#if (P4EST_DIM)==3
 ,
 double z
#endif
)
{
  /* double r2 = x*x + y*y + z*z; */
  /* printf("boundary_fcn(x,y,z) = %.25f, r2 = %.25f, global_Rinf = %.25f\n ", analytic_fcn(x,y,z), r2, global_Rinf);  */
  /* return analytic_fcn(x,y,z); */
  return zero_fcn(x,y
#if (P4EST_DIM)==3
                  ,z
#endif
                  );
}

static
double f_fcn
(
 double x,
 double y
#if (P4EST_DIM)==3
 ,
 double z
#endif
)
{

#if (P4EST_DIM)==3
  return -6.;
#else
  return -4;
#endif
  /* return 2*analytic_fcn(x,y,z)*(2*x*x + 2*y*y + 2*z*z - 3*global_sigma*global_sigma)/(global_sigma*global_sigma*global_sigma*global_sigma); */
}

 
static int
refine_function
(
 p4est_t * p4est,
 p4est_topidx_t which_tree,
 p4est_quadrant_t *quadrant
)
{
  return 1;
}


void
problem_help()
{
}

p4est_connectivity_t*
problem_build_conn()
{
  /* mpi_assert((P4EST_DIM)==3); */
  p4est_connectivity_t* conn;

#if (P4EST_DIM)==3
  conn = p8est_connectivity_new_sphere();
#endif
#if (P4EST_DIM)==2
  conn = p4est_connectivity_new_disk();
#endif
  return conn;
}

typedef struct {

  /* int degree; */
  int num_unifrefs;
  int num_randrefs;
  /* int gauss_quad_deg; */
  int deg_R0;
  int deg_quad_R0;
  int deg_R1;
  int deg_quad_R1;
  int deg_R2;
  int deg_quad_R2;

  int use_ip_flux;
  double ip_flux_penalty;

  double R0;
  double R1;
  double R2;
  /* double Rinf; */
  /* double w; */
  KSPType krylov_type;
  
  int count;
  
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
  if (util_match_couple(section,"amr",name,"num_unifrefs")) {
    mpi_assert(pconfig->num_unifrefs == -1);
    pconfig->num_unifrefs = atoi(value);
    pconfig->count += 1;
  }
  else if (util_match_couple(section,"amr",name,"num_randrefs")) {
    mpi_assert(pconfig->num_randrefs == -1);
    pconfig->num_randrefs = atoi(value);
    pconfig->count += 1;
  }
  else if (util_match_couple(section,"flux",name,"use_ip_flux")) {
    mpi_assert(pconfig->use_ip_flux == -1);
    pconfig->use_ip_flux = atoi(value);
    pconfig->count += 1;
  }
  /* else if (util_match_couple(section,"amr",name, "initial_degree")) { */
    /* mpi_assert(pconfig->degree == -1); */
    /* pconfig->degree = atoi(value); */
    /* pconfig->count += 1; */
  /* } */
  else if (util_match_couple(section,"flux",name,"ip_flux_penalty")) {
    mpi_assert(pconfig->ip_flux_penalty == -1);
    pconfig->ip_flux_penalty = atof(value);
    pconfig->count += 1;
  } 
  /* else if (util_match_couple(section,"problem",name,"gauss_quad_deg")) { */
  /*   mpi_assert(pconfig->gauss_quad_deg == -1); */
  /*   pconfig->gauss_quad_deg = atoi(value); */
  /*   pconfig->count += 1; */
  /* } */
  else if (util_match_couple(section,"problem",name,"R0")) {
    mpi_assert(pconfig->R0 == -1);
    pconfig->R0 = atof(value);
    pconfig->count += 1;
  }
  else if (util_match_couple(section,"problem",name,"R1")) {
    mpi_assert(pconfig->R1 == -1);
    pconfig->R1 = atof(value);
    pconfig->count += 1;
  }
  else if (util_match_couple(section,"problem",name,"R2")) {
    mpi_assert(pconfig->R2 == -1);
    pconfig->R2 = atof(value);
    pconfig->count += 1;
  }
  /* else if (util_match_couple(section,"problem",name,"Rinf")) { */
    /* mpi_assert(pconfig->Rinf == -1); */
    /* pconfig->Rinf = atof(value); */
    /* pconfig->count += 1; */
  /* } */
  /* else if (util_match_couple(section,"problem",name,"w")) { */
    /* mpi_assert(pconfig->w == -1); */
    /* pconfig->w = atof(value); */
    /* pconfig->count += 1; */
  /* } */
  else if (util_match_couple(section,"problem",name,"deg_R0")) {
    mpi_assert(pconfig->deg_R0 == -1);
    pconfig->deg_R0 = atoi(value);
    pconfig->count += 1;
  }
  else if (util_match_couple(section,"problem",name,"deg_quad_R0")) {
    mpi_assert(pconfig->deg_quad_R0 == -1);
    pconfig->deg_quad_R0 = atoi(value);
    pconfig->count += 1;
  }
  else if (util_match_couple(section,"problem",name,"deg_R1")) {
    mpi_assert(pconfig->deg_R1 == -1);
    pconfig->deg_R1 = atoi(value);
    pconfig->count += 1;
  }
  else if (util_match_couple(section,"problem",name,"deg_quad_R1")) {
    mpi_assert(pconfig->deg_quad_R1 == -1);
    pconfig->deg_quad_R1 = atoi(value);
    pconfig->count += 1;
  }
  else if (util_match_couple(section,"problem",name,"deg_R2")) {
    mpi_assert(pconfig->deg_R2 == -1);
    pconfig->deg_R2 = atoi(value);
    pconfig->count += 1;
  }
  else if (util_match_couple(section,"problem",name,"deg_quad_R2")) {
    mpi_assert(pconfig->deg_quad_R2 == -1);
    pconfig->deg_quad_R2 = atoi(value);
    pconfig->count += 1;
  }  
  else if (util_match_couple(section,"solver",name,"krylov_type")) {
    mpi_assert(pconfig->krylov_type = "");
    if (strcmp("cg", value) == 0){
      pconfig->krylov_type = KSPCG;
    }
    else if (strcmp("gmres", value) == 0){
      pconfig->krylov_type = KSPGMRES;
    }
    else {
      mpi_abort("not a support KSP solver type");
    }
    pconfig->count += 1;
  }
  else {
    return 0;  /* unknown section/name, error */
  }
  return 1;
}

static
void problem_build_rhs
(
 p4est_t* p4est,
 problem_data_t* prob_vecs,
 curved_weakeqn_ptrs_t* prob_fcns,
 p4est_ghost_t* ghost,
 d4est_element_data_t* ghost_data,
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 void* user
)
{
  ip_flux_params_t* ip_flux_params = user;
  double* f = P4EST_ALLOC(double, prob_vecs->local_nodes);
  
  d4est_mesh_init_field
    (
     p4est,
     f,
     f_fcn,
     d4est_ops,
     d4est_geom->p4est_geom
    );
  
    /* prob_vecs->curved_vector_flux_fcn_data = curved_gauss_sipg_flux_vector_dirichlet_fetch_fcns */
    /*                                         ( */
    /*                                          boundary_fcn, */
    /*                                          ip_flux_params */
    /*                                         ); */
    /* prob_vecs->curved_scalar_flux_fcn_data = curved_gauss_sipg_flux_scalar_dirichlet_fetch_fcns */
    /*                                         (boundary_fcn); */


  for (p4est_topidx_t tt = p4est->first_local_tree;
       tt <= p4est->last_local_tree;
       ++tt)
    {
      p4est_tree_t* tree = p4est_tree_array_index (p4est->trees, tt);
      sc_array_t* tquadrants = &tree->quadrants;
      int Q = (p4est_locidx_t) tquadrants->elem_count;
      for (int q = 0; q < Q; ++q) {
        p4est_quadrant_t* quad = p4est_quadrant_array_index (tquadrants, q);
        d4est_element_data_t* ed = quad->p.user_data;
        d4est_operators_apply_curvedgaussMass(d4est_ops,
                                     &f[ed->nodal_stride],
                                     ed->deg,
                                     ed->J_quad,
                                     ed->deg_quad,
                                     (P4EST_DIM),
                                     &prob_vecs->rhs[ed->nodal_stride]
                                    );
      }
    }    
  
  d4est_linalg_vec_scale(-1., prob_vecs->rhs, prob_vecs->local_nodes);

  int local_nodes = prob_vecs->local_nodes;
  double* u_eq_0 = P4EST_ALLOC_ZERO(double, local_nodes);
  double* tmp = prob_vecs->u;
  
  prob_vecs->u = u_eq_0; 
  curved_gauss_poisson_apply_aij(p4est, ghost, ghost_data, prob_vecs, d4est_ops, d4est_geom);
  d4est_linalg_vec_axpy(-1., prob_vecs->Au, prob_vecs->rhs, local_nodes);

  prob_vecs->u = tmp;
  P4EST_FREE(u_eq_0);


  /* prob_vecs->curved_vector_flux_fcn_data = curved_gauss_sipg_flux_vector_dirichlet_fetch_fcns */
  /*                                         ( */
  /*                                          zero_fcn, */
  /*                                          ip_flux_params */
  /*                                         ); */
  /* prob_vecs->curved_scalar_flux_fcn_data = curved_gauss_sipg_flux_scalar_dirichlet_fetch_fcns */
  /*                                         (zero_fcn); */

  P4EST_FREE(f);
}


static
problem_input_t
problem_input
(
 const char* input_file
)
{
  int num_of_options = 13;
  
  problem_input_t input;
  /* input.degree = -1; */
  input.num_unifrefs = -1;
  input.ip_flux_penalty = -1;
  input.R2 = -1;
  input.R1 = -1;
  input.R0 = -1;
  input.use_ip_flux = -1;
  /* input.w = -1; */
  /* input.Rinf = -1; */
  input.num_randrefs = -1;
  input.deg_R0 = -1;
  input.deg_quad_R0 = -1;
  input.deg_R1 = -1;
  input.deg_quad_R1 = -1;
  input.deg_R2 = -1;
  input.deg_quad_R2 = -1;
  /* input.gauss_quad_deg = -1; */
  input.krylov_type = "";
  
  input.count = 0;
  
  if (ini_parse(input_file, problem_input_handler, &input) < 0) {
    mpi_abort("Can't load input file");
  }

  mpi_assert(input.count == num_of_options);
  return input;
}

p4est_geometry_t*
problem_build_geom
(
 p4est_connectivity_t* conn
)
{
  /* mpi_assert((P4EST_DIM)==3); */
  p4est_geometry_t* geom;
  problem_input_t input = problem_input("options.input");

#if (P4EST_DIM)==3
  geom = d4est_geometry_new_sphere(conn, input.R2, input.R1, input.R0);
  
#endif
#if (P4EST_DIM)==2
  geom = d4est_geometry_new_disk(conn, input.R1, input.R2);
#endif
  
  /* printf("input.R2 = %.25f\n", input.R2); */

  
  return geom;  
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

void
problem_set_degrees
(
 d4est_element_data_t* elem_data,
 void* user_ctx
)
{
  problem_input_t* input = user_ctx;
  /* outer shell */
  if (elem_data->tree < 6){
    elem_data->deg = input->deg_R2;
    elem_data->deg_quad = input->deg_quad_R2;
  }
  /* inner shell */
  else if(elem_data->tree < 12){
    elem_data->deg = input->deg_R1;
    elem_data->deg_quad = input->deg_quad_R1;
  }
  /* center cube */
  else {
    elem_data->deg = input->deg_R0;
    elem_data->deg_quad = input->deg_quad_R0;
  } 
}



void
problem_init
(
 int argc,
 char* argv [],
 p4est_t* p4est,
 p4est_geometry_t* p4est_geom,
 d4est_operators_t* d4est_ops,
 int proc_size,
 sc_MPI_Comm mpicomm,
 int load_from_checkpoint
)
{

  problem_input_t input = problem_input("options.input");
  
  int level;
  /* int degree = input.degree;          */
  global_Rinf = input.R2;
  global_sigma = input.R0;
  
  mpi_assert((P4EST_DIM) == 2 || (P4EST_DIM) == 3);
  int world_rank, world_size;
  sc_MPI_Comm_rank(sc_MPI_COMM_WORLD, &world_rank);
  sc_MPI_Comm_size(sc_MPI_COMM_WORLD, &world_size);

  double* Au = P4EST_ALLOC_ZERO(double, 1);
  double* rhs = P4EST_ALLOC_ZERO(double, 1);
  double* u = P4EST_ALLOC_ZERO(double, 1);
  double* u_analytic = P4EST_ALLOC_ZERO(double, 1);
  int local_nodes = 1;

  /* ip_flux_params_t ip_flux_params; */
  /* ip_flux_params.ip_flux_penalty_prefactor = atof(argv[6]); */
  /* ip_flux_params.ip_flux_penalty_calculate_fcn = sipg_flux_vector_calc_penalty_maxp2_over_minh; */
  
  ip_flux_params_t ip_flux_params;
  ip_flux_params.ip_flux_penalty_prefactor = input.ip_flux_penalty;
  ip_flux_params.ip_flux_penalty_calculate_fcn = sipg_flux_vector_calc_penalty_maxp2_over_minh;

  central_flux_params_t central_flux_params;
  central_flux_params.central_flux_penalty_prefactor = input.ip_flux_penalty;
  
  p4est_ghost_t* ghost = p4est_ghost_new (p4est, P4EST_CONNECT_FACE);
  /* create space for storing the ghost data */
  d4est_element_data_t* ghost_data = P4EST_ALLOC (d4est_element_data_t,
                                                   ghost->ghosts.elem_count);

  p4est_partition(p4est, 0, NULL);
  p4est_balance (p4est, P4EST_CONNECT_FACE, NULL);
  /* d4est_mesh_geometry_storage_t* geometric_factors = geometric_factors_init(p4est); */


  /* grid_fcn_t boundary_flux_fcn = zero_fcn; */
  
  problem_data_t prob_vecs;
  prob_vecs.rhs = rhs;
  prob_vecs.Au = Au;
  prob_vecs.u = u;
  prob_vecs.local_nodes = local_nodes;

  for (level = 0; level < input.num_unifrefs; ++level){

    if (level != 0){
      p4est_refine_ext(p4est,
                       0,
                       -1,
                       refine_function,
                       NULL,
                       NULL
                      );

      p4est_partition(p4est, 0, NULL);
      p4est_balance_ext
        (
         p4est,
         P4EST_CONNECT_FULL,
         NULL,
         NULL
        );

      p4est_ghost_destroy(ghost);
      P4EST_FREE(ghost_data);

      ghost = p4est_ghost_new(p4est, P4EST_CONNECT_FACE);
      ghost_data = P4EST_ALLOC(d4est_element_data_t, ghost->ghosts.elem_count);      
    }

  }
  
  curved_weakeqn_ptrs_t prob_fcns;
  prob_fcns.apply_lhs = curved_gauss_poisson_apply_aij;

     
    d4est_mesh_geometry_storage_t* geometric_factors = geometric_factors_init(p4est);


    d4est_geometry_t dgeom;
    dgeom.p4est_geom = p4est_geom;
    dgeom.interp_to_gauss = 1;
    dgeom.dxdr_method = INTERP_X_ON_LOBATTO;    
    /* d4est_element_data_init(p4est, geometric_factors, d4est_ops, &dgeom, degree, input.gauss_quad_deg); */
    d4est_element_data_init_new(p4est,
                             geometric_factors,
                             d4est_ops,
                             &dgeom,
                             problem_set_degrees,
                                 (void*)&input);



    
  /* srand(102230213); */
  /* int num_randrefs = input.num_randrefs; */

  
  /* for (int randrefs = 0; randrefs < num_randrefs; ++randrefs){ */
  /*     p4est_refine_ext(p4est, */
  /*                      0, */
  /*                      -1, */
  /*                      random_h_refine, */
  /*                      NULL, */
  /*                      NULL */
  /*                     ); */

  /*     p4est_balance_ext */
  /*       ( */
  /*        p4est, */
  /*        P4EST_CONNECT_FULL, */
  /*        NULL, */
  /*        NULL */
  /*       ); */

  /*     p4est_ghost_destroy(ghost); */
  /*     P4EST_FREE(ghost_data); */

  /*     ghost = p4est_ghost_new(p4est, P4EST_CONNECT_FACE); */
  /*     ghost_data = P4EST_ALLOC(d4est_element_data_t, ghost->ghosts.elem_count); */

  /*   d4est_element_data_init(p4est, geometric_factors, d4est_ops, &dgeom, degree, input.gauss_quad_deg); */

      
  /* } */

    /* d4est_element_data_init(p4est, */
    /*                          geometric_factors, */
    /*                          d4est_ops, */
    /*                          &dgeom, degree, */
    /*                          degree_gauss_diff[0], */
    /*                          GAUSS_INTEG); */



    
    local_nodes = d4est_element_data_get_local_nodes(p4est);

    Au = P4EST_REALLOC(Au, double, local_nodes);
    u = P4EST_REALLOC(u, double, local_nodes);
    rhs = P4EST_REALLOC(rhs, double, local_nodes);
    u_analytic = P4EST_REALLOC(u_analytic, double, local_nodes);

    prob_vecs.Au = Au;
    prob_vecs.u = u;
    prob_vecs.rhs = rhs;
    prob_vecs.local_nodes = local_nodes;


    if (input.use_ip_flux){
      printf("Using ip flux\n");
      prob_vecs.curved_vector_flux_fcn_data = curved_gauss_sipg_flux_vector_dirichlet_fetch_fcns
                                              (
                                               zero_fcn,
                                               &ip_flux_params
                                              );
    }
    else{
      printf("Using central flux\n");
      prob_vecs.curved_vector_flux_fcn_data = curved_gauss_central_flux_vector_dirichlet_fetch_fcns
                                              (
                                               zero_fcn,
                                               &central_flux_params
                                              );
    }

    
    prob_vecs.curved_scalar_flux_fcn_data = curved_gauss_sipg_flux_scalar_dirichlet_fetch_fcns
                                            (zero_fcn);

    
    /* d4est_linalg_fill_vec(u, 0., local_nodes); */
    /* d4est_mesh_init_field(p4est,f,f_fcn,d4est_ops); */

    /* double total_volume = 0.; */
    /* for (p4est_topidx_t tt = p4est->first_local_tree; */
    /*      tt <= p4est->last_local_tree; */
    /*      ++tt) */
    /*   { */
    /*     p4est_tree_t* tree = p4est_tree_array_index (p4est->trees, tt); */
    /*     sc_array_t* tquadrants = &tree->quadrants; */
    /*     int Q = (p4est_locidx_t) tquadrants->elem_count; */
    /*     for (int q = 0; q < Q; ++q) { */
    /*       p4est_quadrant_t* quad = p4est_quadrant_array_index (tquadrants, q); */
    /*       d4est_element_data_t* ed = quad->p.user_data; */
    /*       printf("Tree, Element, Volume = %d, %d, %.25f\n", ed->tree, ed->id, ed->volume); */
    /*       total_volume += ed->volume; */
    /*     } */
    /*   } */

    /* printf("Total volume = %.25f\n", total_volume); */
    /* printf("Theoretical volume = %.25f\n", (4./3.)*M_PI*pow((input.Rinf),3)); */
    
    /* serial_matrix_sym_tester */
    /*   ( */
    /*    p4est, */
    /*    &prob_vecs, /\* only needed for # of nodes *\/ */
    /*    (void*)&prob_fcns, */
    /*    .0000000001, */
    /*    d4est_ops, */
    /*    1, /\* is it curved *\/ */
    /*    3, /\* should we print *\/ */
    /*    &dgeom */
    /*   ); */

     /* p4est_vtk_write_all */
     /*   (p4est, p4est_geom,     /\* we do not need to transform from the vertex space into physical space, so we do not need a p4est_geometry_t * pointer *\/ */
     /*    0.99,    /\* draw each quadrant at almost full scale *\/ */
     /*    1,       /\* do not write the tree id's of each quadrant (there is only one tree in this example) *\/ */
     /*    1,       /\* do write the refinement level of each quadrant *\/ */
     /*    1,       /\* do write the mpi process id of each quadrant *\/ */
     /*    0,       /\* do not wrap the mpi rank (if this were > 0, the modulus of the rank relative to this number would be written instead of the rank) *\/ */
     /*    0,       /\* write one scalar field: the solution value *\/ */
     /*    0,       /\* write no vector fields *\/ */
     /*    "non-constant-jacobian" */
     /*   ); */


    /* d4est_mesh_init_field */
  /*   ( */
  /*    p4est, */
  /*    u, */
  /*    analytic_fcn, */
  /*    d4est_ops */
  /*   ); */

    d4est_linalg_fill_vec(u, 0., local_nodes);
    
     /* p4est_vtk_write_file */
     /*   (p4est, */
     /*    p4est_geom, */
     /*    "compact-sphere" */
     /*   ); */

     problem_build_rhs
       (
        p4est,
        &prob_vecs,
        &prob_fcns,
        ghost,
        ghost_data,
        d4est_ops,
        &dgeom,
        &ip_flux_params
       );



     /* DEBUG_PRINT_ARR_DBL(prob_vecs.rhs, prob_vecs.local_nodes); */
     

  /* for (int i = 0; i < local_nodes; i++){ */
  /*   u[i] = util_uniform_rand(14234232, 0., 1.); */
  /* } */
  
  /* /\* d4est_linalg_fill_vec(u, 1., local_nodes); *\/ */
  
  /* prob_fcns.apply_lhs(p4est, ghost, ghost_data, &prob_vecs, d4est_ops, &dgeom); */
  /* DEBUG_PRINT_3ARR_DBL(prob_vecs.u, prob_vecs.rhs, prob_vecs.Au, prob_vecs.local_nodes); */


     
     /* krylov_petsc_params_t params = {input.krylov_type, 1, 0, NULL, NULL, NULL}; */
     
     krylov_petsc_info_t info =
       krylov_petsc_solve
       (
        p4est,
        &prob_vecs,
        (void*)&prob_fcns,
        &ghost,
        (void**)&ghost_data,
        d4est_ops,
        &dgeom,
        NULL,
        NULL,
        NULL
        /* &params */
       );

     /* double* vertex_u = P4EST_ALLOC(double, (P4EST_CHILDREN)*p4est->local_num_quadrants); */
     /* double* vertex_u_analytic = P4EST_ALLOC(double, (P4EST_CHILDREN)*p4est->local_num_quadrants); */
     /* d4est_mesh_init_field(p4est, u_analytic, analytic_fcn, d4est_ops); */
  
     /* d4est_element_data_store_nodal_vec_in_vertex_array */
     /*   ( */
     /*    p4est, */
     /*    u, */
     /*    vertex_u */
     /*   ); */

     /* d4est_element_data_store_nodal_vec_in_vertex_array */
     /*   ( */
     /*    p4est, */
     /*    u_analytic, */
     /*    vertex_u_analytic */
     /*   ); */


     /* sc_array_t* vertex_u_sc = sc_array_new_data((void*)vertex_u, sizeof(double), (P4EST_CHILDREN)*p4est->local_num_quadrants); */
     /* sc_array_t* vertex_u_analytic_sc = sc_array_new_data((void*)vertex_u_analytic, sizeof(double), (P4EST_CHILDREN)*p4est->local_num_quadrants); */
      
     /* d4est_vtk_context_t* vtk_ctx = d4est_vtk_context_new(p4est, "compact-sphere"); */
     /* d4est_vtk_context_set_geom(vtk_ctx, p4est_geom); */
     /* d4est_vtk_context_set_scale(vtk_ctx, .95); */

     /* vtk_ctx = d4est_vtk_write_header(vtk_ctx); */

     /* vtk_ctx = d4est_vtk_write_point_dataf(vtk_ctx, 2, 0, "vertex_u",vertex_u_sc, "vertex_u_analytic",vertex_u_analytic_sc,vtk_ctx); */

     /* d4est_vtk_write_footer(vtk_ctx); */
     /* /\* d4est_vtk_context_destroy(vtk_ctx); *\/ */


     /* sc_array_destroy(vertex_u_sc); */
     /* sc_array_destroy(vertex_u_analytic_sc);      */

     /* P4EST_FREE(vertex_u); */
     /* P4EST_FREE(vertex_u_analytic); */


     int* deg_array = P4EST_ALLOC(int, p4est->local_num_quadrants);
     int vtk_nodes = 0;
     
     int stride = 0;
     for (p4est_topidx_t tt = p4est->first_local_tree;
          tt <= p4est->last_local_tree;
          ++tt)
       {
         p4est_tree_t* tree = p4est_tree_array_index (p4est->trees, tt);
         sc_array_t* tquadrants = &tree->quadrants;
         int Q = (p4est_locidx_t) tquadrants->elem_count;
         for (int q = 0; q < Q; ++q) {
           /* k++; */
           p4est_quadrant_t* quad = p4est_quadrant_array_index (tquadrants, q);
           d4est_element_data_t* ed = quad->p.user_data;
           deg_array[stride] = ed->deg;
           vtk_nodes = util_int_pow_int(deg_array[stride], (P4EST_DIM))*(P4EST_CHILDREN);
           stride++;
         }
       }


  /* for (int i = 0; i < p4est->local_num_quadrants; i++){ */
  /*   printf("util_int_pow_int(deg_array[i], (P4EST_DIM)) = %d\n",util_int_pow_int(deg_array[i], (P4EST_DIM))); */
  /*   printf("deg_array[i] = %d\n",deg_array[i]); */
  /*   printf("(P4EST_DIM) = %d\n",(P4EST_DIM)); */
  /* } */
     
     /* d4est_element_data_print_element_data_debug */
     /*   ( */
     /*    p4est */
     /*   ); */

     /* sc_array_t* u_sc = sc_array_new_data((void*)u, sizeof(double), (P4EST_CHILDREN)*p4est->local_num_quadrants); */
     /* sc_array_t* u_analytic_sc = sc_array_new_data((void*)u_analytic, sizeof(double), (P4EST_CHILDREN)*p4est->local_num_quadrants); */

     /* double* error = P4EST_ALLOC(double, local_nodes); */

     d4est_mesh_init_field(p4est, u_analytic, analytic_fcn, d4est_ops, p4est_geom);

     /* for (int i = 0; i < local_nodes; i++){ */
     /*   error[i] = fabs(error[i]); */
     /* } */
  
          
     /* d4est_linalg_vec_axpyeqz(-1., u, u_analytic, error, local_nodes); */

     /* printf("Number of elements = %d\n", p4est->local_num_quadrants); */
     /* printf("Number of nodes = %d\n", prob_vecs.local_nodes); */
     

    /*  double* AU_1 = P4EST_ALLOC(double, prob_vecs.local_nodes); */
    /*  for (int i = 0; i < prob_vecs.local_nodes; i++) */
    /*    prob_vecs.u[i] = (i == 2) ? 1. : 0.; */
     
    /*  prob_vecs.Au = AU_1; */
    /*  curved_poisson_debug_vecs_t* debug_vecs_1 = curved_poisson_apply_aij_debug(p4est, ghost, ghost_data, &prob_vecs, d4est_ops, &dgeom, 0); */

    /*  /\* for (int i = 0; i < prob_vecs.local_nodes; i++) *\/ */
    /*  /\*   prob_vecs.u[i] = (i == 0) ? 1. : 0.; *\/ */
    /*  /\* curved_poisson_debug_vecs_t* debug_vecs_2 = curved_poisson_apply_aij_debug(p4est, ghost, ghost_data, &prob_vecs, d4est_ops, &dgeom, 0); *\/ */
    /* curved_poisson_debug_vecs_t* debug_vecs_2 = curved_poisson_apply_aij_debug(p4est, ghost, ghost_data, &prob_vecs, d4est_ops, &dgeom, 1); */
     

    /*  DEBUG_PRINT_ARR_DBL(AU_1, prob_vecs.local_nodes); */
     
    /*  curved_poisson_debug_vecs_print(debug_vecs_1); */
    /*  curved_poisson_debug_vecs_print(debug_vecs_2); */
    /*  curved_poisson_debug_vecs_destroy(debug_vecs_1); */
    /*  curved_poisson_debug_vecs_destroy(debug_vecs_2); */
     
    /*  P4EST_FREE(AU_1); */



     
     /* P4EST_FREE(u1); */
    /* curved_gauss_poisson_apply_aij(p4est, ghost, ghost_data, &prob_vecs, d4est_ops, &dgeom); */
    /* DEBUG_PRINT_2ARR_DBL(prob_vecs.u, prob_vecs.Au, prob_vecs.local_nodes); */

     
  /*    p4est_geometry_t* geom_vtk = d4est_geometry_new_sphere(p4est->connectivity, input.R2, input.R1, input.R0); */
     

     d4est_vtk_context_t* vtk_ctx = d4est_vtk_dg_context_new(p4est, d4est_ops, "testcurvedipflux");
     d4est_vtk_context_set_geom(vtk_ctx, p4est_geom);
     d4est_vtk_context_set_scale(vtk_ctx, .99);
     d4est_vtk_context_set_deg_array(vtk_ctx, deg_array);
     vtk_ctx = d4est_vtk_write_dg_header(vtk_ctx, d4est_ops);
     /* vtk_ctx = d4est_vtk_write_dg_point_dataf(vtk_ctx, 3, 0, "u",u, "u_analytic",u_analytic,"error", error, vtk_ctx); */
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


  
  d4est_vtk_write_footer(vtk_ctx);
  P4EST_FREE(deg_array);
  /* P4EST_FREE(error); */
  
  /* d4est_linalg_vec_axpy(-1., u, u_analytic, local_nodes); */
  /* d4est_linalg_copy_1st_to_2nd(u, error, local_nodes); */
  d4est_linalg_vec_axpy(-1., u, u_analytic, local_nodes);
  
  /* serial_matrix_sym_tester */
  /*   ( */
  /*    p4est, */
  /*    &prob_vecs, /\* only needed for # of nodes *\/ */
  /*    (void*)&prob_fcns, */
  /*    .0000000001, */
  /*    d4est_ops, */
  /*    1, /\* is it curved *\/ */
  /*    0, /\* should we print *\/ */
  /*    &dgeom */
  /*   ); */

  
  
  double local_l2_norm_sqr =  d4est_element_data_compute_l2_norm_sqr
                              (
                               p4est,
                               u_analytic,
                               local_nodes,
                               d4est_ops,
                               DO_NOT_STORE_LOCALLY
                              );

  double local_nodes_dbl = (double)local_nodes;
  double local_reduce [2];
  local_reduce[0] = local_nodes_dbl;
  local_reduce[1] = local_l2_norm_sqr;

  double global_reduce [2];

  sc_reduce
    (
     &local_reduce[0],
     &global_reduce[0],
     2,
     sc_MPI_DOUBLE,
     sc_MPI_SUM,
     0,
     sc_MPI_COMM_WORLD
    );

  double global_nodes_dbl = global_reduce[0];
  double global_l2_norm_sqr = global_reduce[1];
    
  if (world_rank == 0){
    /* end = clock(); */
    /* double time_spent = (double)(end - begin) / CLOCKS_PER_SEC; */
    printf
      (
       "\n\n[HP_AMR]: %d %d %.25f %d %.25f %f \n\n",
       /* degree, */
       (int)p4est->global_num_quadrants,
       (int)global_nodes_dbl,
       sqrt(global_l2_norm_sqr),
       -1.,
       /* info.iterations, */
       /* info.residual_norm, */
       -1.,
       0.
      );
  }

     
    geometric_factors_destroy(geometric_factors);

    if (ghost) {
      p4est_ghost_destroy (ghost);
      P4EST_FREE (ghost_data);
      ghost = NULL;
      ghost_data = NULL;
    }

  P4EST_FREE(Au);
  P4EST_FREE(rhs);
  P4EST_FREE(u_analytic);
  P4EST_FREE(u);
}
