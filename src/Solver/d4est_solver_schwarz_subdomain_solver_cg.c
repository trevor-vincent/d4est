#define _GNU_SOURCE
#include <pXest.h>
#include <d4est_solver_schwarz_metadata.h>
#include <d4est_solver_schwarz_geometric_data.h>
#include <d4est_solver_schwarz_subdomain_solver_cg.h>
#include <d4est_solver_schwarz_operators.h>
#include <d4est_solver_schwarz_apply_lhs.h>
#include <d4est_linalg.h>
#include <zlog.h>
#include <ini.h>
#include <time.h>
static
int d4est_solver_schwarz_subdomain_solver_cg_input_handler
(
 void* user,
 const char* section,
 const char* name,
 const char* value
)
{
  d4est_solver_schwarz_subdomain_solver_cg_t* pconfig = (d4est_solver_schwarz_subdomain_solver_cg_t*)user;
  const char* input_section = pconfig->input_section;
  
  if (d4est_util_match_couple(section,input_section,name,"subdomain_atol")) {
    D4EST_ASSERT(pconfig->subdomain_atol == -1);
    pconfig->subdomain_atol = atof(value);
  }
  else if (d4est_util_match_couple(section,input_section,name,"subdomain_rtol")) {
    D4EST_ASSERT(pconfig->subdomain_rtol == -1);
    pconfig->subdomain_rtol = atof(value);
  }
  else if (d4est_util_match_couple(section,input_section,name,"subdomain_iter")) {
    D4EST_ASSERT(pconfig->subdomain_iter == -1);
    pconfig->subdomain_iter = atoi(value);
  }
  else if (d4est_util_match_couple(section,input_section,name,"subdomain_monitor")) {
    D4EST_ASSERT(pconfig->subdomain_monitor == -1);
    pconfig->subdomain_monitor = atoi(value);
  }
  else {
    return 0;  /* unknown section/name, error */
  }
  return 1;
}

void
d4est_solver_schwarz_subdomain_solver_cg_destroy
(
 void* params
)
{

  d4est_solver_schwarz_subdomain_solver_cg_t* cg_params
    = params;
  P4EST_FREE(cg_params); 
}

d4est_solver_schwarz_subdomain_solver_cg_t*
d4est_solver_schwarz_subdomain_solver_cg_init
(
 p4est_t* p4est,
 const char* input_file,
 const char* input_section
)
{
  d4est_solver_schwarz_subdomain_solver_cg_t* solver_cg =
    P4EST_ALLOC(d4est_solver_schwarz_subdomain_solver_cg_t, 1);
  
  zlog_category_t* c_default = zlog_get_category("d4est_solver_schwarz_subdomain_solver_cg");
  solver_cg->subdomain_iter = -1;
  solver_cg->subdomain_rtol = -1;
  solver_cg->subdomain_atol = -1;
  solver_cg->subdomain_monitor = -1;
  solver_cg->input_section = input_section;
  solver_cg->num_subdomains = p4est->local_num_quadrants;
  if(
     ini_parse(input_file,
               d4est_solver_schwarz_subdomain_solver_cg_input_handler,
               solver_cg) < 0
  ){
    D4EST_ABORT("Can't load input file");
  }

  D4EST_CHECK_INPUT(input_section, solver_cg->subdomain_iter, -1);
  D4EST_CHECK_INPUT(input_section, solver_cg->subdomain_rtol, -1);
  D4EST_CHECK_INPUT(input_section, solver_cg->subdomain_atol, -1);
  D4EST_CHECK_INPUT(input_section, solver_cg->subdomain_monitor, -1);

  if (solver_cg->subdomain_iter <= 0 ||
      solver_cg->subdomain_rtol <= 0 ||
      solver_cg->subdomain_atol <= 0 
     ){
    D4EST_ABORT("Some subdomain solver options are <= 0");
  }

  return solver_cg;
}


d4est_solver_schwarz_subdomain_solver_info_t
d4est_solver_schwarz_subdomain_solver_cg
(
 p4est_t* p4est,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 d4est_mesh_data_t* d4est_factors,
 d4est_ghost_t* ghost,
 d4est_solver_schwarz_operators_t* schwarz_ops,
 d4est_solver_schwarz_metadata_t* schwarz_metadata,
 d4est_solver_schwarz_geometric_data_t* schwarz_geometric_data,
 d4est_solver_schwarz_apply_lhs_t* apply_lhs,
 double* du_restricted_field_over_subdomain,
 double* rhs_restricted_field_over_subdomain,
 int subdomain,
 void* params
)
{
 
  clock_t begin = clock();
  zlog_category_t* c_default = zlog_get_category("d4est_schwarz_subdomain");

  d4est_solver_schwarz_subdomain_solver_cg_t* cg_params
    = params;

  d4est_solver_schwarz_subdomain_metadata_t* sub_data =
    &schwarz_metadata->subdomain_metadata[subdomain];

  int iter = cg_params->subdomain_iter;
  double atol = cg_params->subdomain_atol;
  double rtol = cg_params->subdomain_rtol;
  
  int nodes
    = schwarz_metadata->subdomain_metadata[subdomain].restricted_nodal_size;
  
  double delta_new, delta_old, temp_max, temp_min, d_dot_Ad;
  double alpha = -1.;
  double beta = -1.;
  double alpha_old = -1;
  double beta_old = -1;

  double* d = P4EST_ALLOC(double, nodes); 
  double* Ad = P4EST_ALLOC(double, nodes); 
  double* r = P4EST_ALLOC(double, nodes);
  
  apply_lhs->apply_lhs_fcn
    (
     p4est,
     schwarz_ops->d4est_ops,
     d4est_geom,
     d4est_quad,
     d4est_factors,
     ghost,
     schwarz_ops,
     schwarz_metadata,
     schwarz_geometric_data,
     subdomain,
     du_restricted_field_over_subdomain,
     Ad,
     apply_lhs->apply_lhs_ctx
    );


  d4est_util_copy_1st_to_2nd(Ad, r, nodes);
  d4est_linalg_vec_xpby(rhs_restricted_field_over_subdomain, -1., r, nodes);
  d4est_util_copy_1st_to_2nd(r, d, nodes);
  delta_new = d4est_linalg_vec_dot(r,r,nodes);
  double delta_0 = delta_new;  
  double tol_break = atol*atol + delta_0 * rtol*rtol;

  int i;
  for (i = 0;
       i < iter;
       i++){

    apply_lhs->apply_lhs_fcn
      (
       p4est,
       schwarz_ops->d4est_ops,
       d4est_geom,
       d4est_quad,
       d4est_factors,
       ghost,
       schwarz_ops,
       schwarz_metadata,
       schwarz_geometric_data,
       subdomain,
       d,
       Ad,
       apply_lhs->apply_lhs_ctx
      );
    
    d_dot_Ad = d4est_linalg_vec_dot(d, Ad, nodes);

    alpha_old = alpha;
    alpha = delta_new/d_dot_Ad;
        
    d4est_linalg_vec_axpy(alpha, d, du_restricted_field_over_subdomain, nodes);
    d4est_linalg_vec_axpy(-alpha, Ad, r, nodes);

    delta_old = delta_new;
    delta_new = d4est_linalg_vec_dot(r, r, nodes);
    beta_old = beta;
    beta = delta_new/delta_old;
    d4est_linalg_vec_xpby(r, beta, d, nodes);    

    if (cg_params->subdomain_monitor >= 1){
      zlog_info(c_default,
                "elems %d nodes %d rank %d sub %d sub_tree %d iter %d r %.15f ",
                p4est->global_num_quadrants,
                d4est_factors->global_nodes,
                p4est->mpirank,
                subdomain,
                sub_data->core_tree,
                i,
                sqrt(delta_new)
               );
    }

    
    if (delta_new < tol_break){
      break;
    }
    
  }

  /* if (cg_params->subdomain_monitor == 1){ */
  /*   clock_t end = clock(); */
  /*   double time_spent = (double)(end-begin)/CLOCKS_PER_SEC; */
  /*     zlog_info(c_default, */
  /*               "POST: rank %d subdomain %d core_tree %d       -       iters %d time %f r %.15f ", */
  /*               p4est->mpirank, */
  /*               subdomain, */
  /*               sub_data->core_tree, */
  /*               i, */
  /*               time_spent, */
  /*               sqrt(delta_new) */
  /*              ); */
  /* } */

  
  P4EST_FREE(Ad);
  P4EST_FREE(d);
  P4EST_FREE(r);

  return (d4est_solver_schwarz_subdomain_solver_info_t){.final_iter = i,
    .final_res = sqrt(delta_new)};
    
  /* retrun final_info; */
}
