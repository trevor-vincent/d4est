#ifndef MULTI_PUNCTURES_FCNS_WITH_OPT_H
#define MULTI_PUNCTURES_FCNS_WITH_OPT_H 

#include <d4est_util.h>
#include <d4est_element_data.h>
#include <d4est_hessian.h>
#include <d4est_amr_smooth_pred.h>
#include <d4est_laplacian_with_opt.h>
#include <d4est_laplacian_with_opt_flux.h>
#include <d4est_solver_multigrid.h>
#include <d4est_solver_multigrid_matrix_operator.h>

#define MAX_PUNCTURES 10

typedef struct {

  double XYZ [3];
  double P [3];
  double S [3];
  double M;
  
} black_hole_data_t;

typedef struct {
  
  black_hole_data_t* bh;
  int number_of_punctures;

} multi_punctures_params_t;

static
void
init_random_puncture_data
(
 p4est_t* p4est,
 int num_punctures
)
{

  D4EST_ASSERT(num_punctures < MAX_PUNCTURES);
  
  double rand_x [MAX_PUNCTURES];
  double rand_y [MAX_PUNCTURES];
  double rand_Px [MAX_PUNCTURES];
  double rand_Py [MAX_PUNCTURES];
  double rand_Sz [MAX_PUNCTURES];
  double rand_M [MAX_PUNCTURES];

  d4est_util_gen_rand_vec(&rand_x[0], num_punctures, 12413243, -3., 3.);
  d4est_util_gen_rand_vec(&rand_y[0], num_punctures, 113243, -3., 3.);
  d4est_util_gen_rand_vec(&rand_Px[0], num_punctures, 2413243, -.2, .2);
  d4est_util_gen_rand_vec(&rand_Py[0], num_punctures, 132413243, -.2, .2);
  d4est_util_gen_rand_vec(&rand_Sz[0], num_punctures, 1243, -.2, .2);
  d4est_util_gen_rand_vec(&rand_M[0], num_punctures, 14, 0., 1.);

  double total_M = 0.;
  for (int i = 0; i < num_punctures; i++){
    total_M += rand_M[i];
  }
  for (int i = 0; i < num_punctures; i++){
    rand_M[i] /= total_M;
  }

  if (p4est->mpirank == 0){
    for (int i = 0; i < num_punctures; i++){
      printf("puncture%d_M = %.15f\n",i, rand_M[i]);
      printf("puncture%d_X = %.15f\n",i, rand_x[i]);
      printf("puncture%d_Y = %.15f\n",i, rand_y[i]);
      printf("puncture%d_Z = %.15f\n",i, 0.);
      printf("puncture%d_PX = %.15f\n",i, rand_Px[i]);
      printf("puncture%d_PY = %.15f\n",i, rand_Py[i]);
      printf("puncture%d_PZ = %.15f\n",i, 0.);
      printf("puncture%d_SX = %.15f\n",i, 0.);
      printf("puncture%d_SY = %.15f\n",i, 0.);
      printf("puncture%d_SZ = %.15f\n",i, rand_Sz[i]);
      
    }
  }
  

}
 

static
int multi_punctures_input_parser1
(
 void* user,
 const char* section,
 const char* name,
 const char* value
)
{
  multi_punctures_params_t* pconfig = (multi_punctures_params_t*) user;
  if (d4est_util_match_couple(section,"problem",name,"number_of_punctures")) {
    D4EST_ASSERT(pconfig->number_of_punctures == -1);
    pconfig->number_of_punctures = atoi(value);
    return 1;
  }
  else {
    return 0;
  }
}

static
int multi_punctures_input_parser2
(
 void* user,
 const char* section,
 const char* name,
 const char* value
)
{
  multi_punctures_params_t* pconfig = (multi_punctures_params_t*) user;
  for (int i = 0; i < pconfig->number_of_punctures; i++){

    int hit = 0;
    char* bh_X;
    char* bh_Y;
    char* bh_Z;
    char* bh_PX;
    char* bh_PY;
    char* bh_PZ;
    char* bh_SX;
    char* bh_SY;
    char* bh_SZ;
    char* bh_M;
    asprintf(&bh_X,"puncture%d_X", i);
    asprintf(&bh_Y,"puncture%d_Y", i);
    asprintf(&bh_Z,"puncture%d_Z", i);
    asprintf(&bh_PX,"puncture%d_PX", i);
    asprintf(&bh_PY,"puncture%d_PY", i);
    asprintf(&bh_PZ,"puncture%d_PZ", i);
    asprintf(&bh_SX,"puncture%d_SX", i);
    asprintf(&bh_SY,"puncture%d_SY", i);
    asprintf(&bh_SZ,"puncture%d_SZ", i);
    asprintf(&bh_M,"puncture%d_M", i);

    if (d4est_util_match_couple(section,"problem",name,bh_M)) {
      D4EST_ASSERT(pconfig->bh[i].M == -1);
      pconfig->bh[i].M = atof(value);
      hit++;
    }
    else if (d4est_util_match_couple(section,"problem",name,bh_X)) {
      D4EST_ASSERT(pconfig->bh[i].XYZ[0] == -1);
      pconfig->bh[i].XYZ[0] = atof(value);
      hit++;
    }
    else if (d4est_util_match_couple(section,"problem",name,bh_Y)) {
      D4EST_ASSERT(pconfig->bh[i].XYZ[1] == -1);
      pconfig->bh[i].XYZ[1] = atof(value);
      hit++;
    }
    else if (d4est_util_match_couple(section,"problem",name,bh_Z)) {
      D4EST_ASSERT(pconfig->bh[i].XYZ[2] == -1);
      pconfig->bh[i].XYZ[2] = atof(value);
      hit++;
    }
    else if (d4est_util_match_couple(section,"problem",name,bh_PX)) {
      D4EST_ASSERT(pconfig->bh[i].P[0] == -1);
      pconfig->bh[i].P[0] = atof(value);
      hit++;
    }
    else if (d4est_util_match_couple(section,"problem",name,bh_PY)) {
      D4EST_ASSERT(pconfig->bh[i].P[1] == -1);
      pconfig->bh[i].P[1] = atof(value);
      hit++;
    }
    else if (d4est_util_match_couple(section,"problem",name,bh_PZ)) {
      D4EST_ASSERT(pconfig->bh[i].P[2] == -1);
      pconfig->bh[i].P[2] = atof(value);
      hit++;
    }
    else if (d4est_util_match_couple(section,"problem",name,bh_SX)) {
      D4EST_ASSERT(pconfig->bh[i].S[0] == -1);
      pconfig->bh[i].S[0] = atof(value);
      hit++;
    }
    else if (d4est_util_match_couple(section,"problem",name,bh_SY)) {
      D4EST_ASSERT(pconfig->bh[i].S[1] == -1);
      pconfig->bh[i].S[1] = atof(value);
      hit++;
    }
    else if (d4est_util_match_couple(section,"problem",name,bh_SZ)) {
      D4EST_ASSERT(pconfig->bh[i].S[2] == -1);
      pconfig->bh[i].S[2] = atof(value);
      hit++;
    }
      
    free(bh_X);
    free(bh_Y);
    free(bh_Z);
    free(bh_PX);
    free(bh_PY);
    free(bh_PZ);
    free(bh_SX);
    free(bh_SY);
    free(bh_SZ);
    free(bh_M);
      
    if (hit)
      return 1;
  }
  return 0;    
}



static
void
multi_punctures_params_destroy
(
 multi_punctures_params_t* params
){
  P4EST_FREE(params->bh);
  P4EST_FREE(params);
}

static
multi_punctures_params_t*
multi_punctures_params_init
(
 p4est_t* p4est,
 const char* input_file
){

  multi_punctures_params_t* params = P4EST_ALLOC(multi_punctures_params_t,1);
  params->number_of_punctures = -1;
  
  if (ini_parse(input_file, multi_punctures_input_parser1, params) < 0){
    D4EST_ABORT("Can't load input file");
  }
  D4EST_CHECK_INPUT("problem", params->number_of_punctures, -1);
  
  params->bh = P4EST_ALLOC(black_hole_data_t, params->number_of_punctures);

  for (int i = 0; i < params->number_of_punctures; i++){
    params->bh[i].M = -1;
    params->bh[i].XYZ[0] = -1;
    params->bh[i].XYZ[1] = -1;
    params->bh[i].XYZ[2] = -1;
    params->bh[i].P[0] = -1;
    params->bh[i].P[1] = -1;
    params->bh[i].P[2] = -1;
    params->bh[i].S[0] = -1;
    params->bh[i].S[1] = -1;
    params->bh[i].S[2] = -1;
  }
  
  if (ini_parse(input_file, multi_punctures_input_parser2, params) < 0){
    D4EST_ABORT("Can't load input file");
  }  

  for (int i = 0; i < params->number_of_punctures; i++){
    D4EST_CHECK_INPUT("problem",params->bh[i].M,-1);
    D4EST_CHECK_INPUT("problem",params->bh[i].XYZ[0],-1);
    D4EST_CHECK_INPUT("problem",params->bh[i].XYZ[1],-1);
    D4EST_CHECK_INPUT("problem",params->bh[i].XYZ[2],-1);
    D4EST_CHECK_INPUT("problem",params->bh[i].P[0],-1);
    D4EST_CHECK_INPUT("problem",params->bh[i].P[1],-1);
    D4EST_CHECK_INPUT("problem",params->bh[i].P[2],-1);
    D4EST_CHECK_INPUT("problem",params->bh[i].S[0],-1);
    D4EST_CHECK_INPUT("problem",params->bh[i].S[1],-1);
    D4EST_CHECK_INPUT("problem",params->bh[i].S[2],-1);

    if (p4est->mpirank == 0){
      printf("[PUNCTURE_INFO]: Puncture %d\n", i);
      printf("[PUNCTURE_INFO]: x, y, z = %.15f, %.15f, %.15f\n",
             params->bh[i].XYZ[0], params->bh[i].XYZ[1], params->bh[i].XYZ[2]);
      printf("[PUNCTURE_INFO]: Px, Py, Pz = %.15f, %.15f, %.15f\n",
             params->bh[i].P[0], params->bh[i].P[1], params->bh[i].P[2]);
      printf("[PUNCTURE_INFO]: Sx, Sy, Sz = %.15f, %.15f, %.15f\n",
             params->bh[i].S[0], params->bh[i].S[1], params->bh[i].S[2]);
    }    
    
  }

  return params;
}

typedef struct {

  int use_matrix_operator;
  d4est_solver_multigrid_t* mg_data;
  multi_punctures_params_t* multi_punctures_params;
  d4est_laplacian_with_opt_flux_data_t* flux_data_for_jac;
  d4est_laplacian_with_opt_flux_data_t* flux_data_for_res;
  d4est_amr_smooth_pred_params_t* smooth_pred_params;
  
} problem_ctx_t;


static
double Aij_fcn
(
 int a,
 int b,
 double x,
 double y,
 double z,
 int i,
 void* user
)
{
  problem_ctx_t* ctx = user;
  multi_punctures_params_t* params = ctx->multi_punctures_params;
  
  double dxn = (x - params->bh[i].XYZ[0]);
  double dyn = (y - params->bh[i].XYZ[1]);
  double dzn = (z - params->bh[i].XYZ[2]);

  double rn = sqrt(dxn*dxn + dyn*dyn + dzn*dzn);

  double n [] = {(x - params->bh[i].XYZ[0])/rn,
                 (y - params->bh[i].XYZ[1])/rn,
                 (z - params->bh[i].XYZ[2])/rn};

  double P [] = {params->bh[i].P[0], params->bh[i].P[1], params->bh[i].P[2]};
  double S [] = {params->bh[i].S[0], params->bh[i].S[1], params->bh[i].S[2]};

  double ScrossN [] = {-n[2]*S[1] + n[1]*S[2], n[2]*S[0] - n[0]*S[2], -n[1]*S[0] + n[0]*S[1]};

  double gab = (a == b) ? 1. : 0.;
  double PdotN = P[0]*n[0] + P[1]*n[1] + P[2]*n[2];
  
  double term1 = (3./(2.*rn*rn))*(P[a]*n[b] + P[b]*n[a] - (gab - n[a]*n[b])*PdotN);
  double term2 = (3./(rn*rn*rn))*(ScrossN[a]*n[b] + ScrossN[b]*n[a]);

  return (term1 + term2);
}

static
double AijAij_fcn
(
 double x,
 double y,
 double z,
 void *user
)
{
  problem_ctx_t* ctx = user;
  multi_punctures_params_t* params = ctx->multi_punctures_params;
  
  double AijAij = 0.;
  
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++){

      double Aij = 0.;
      for (int n = 0; n < params->number_of_punctures; n++){
        Aij += Aij_fcn(i,j,x,y,z,n,user);
      }
 
      AijAij += Aij*Aij;
    }
  
  return AijAij;
}


static
double psi_fcn
(
 double x,
 double y,
 double z,
 double u,
 void* user
)
{

  problem_ctx_t* ctx = user;
  multi_punctures_params_t* params = ctx->multi_punctures_params;
  double sumn_mn_o_2rn = 0.;

  for (int n = 0; n < params->number_of_punctures; n++){
    double dxn = (x - params->bh[n].XYZ[0]);
    double dyn = (y - params->bh[n].XYZ[1]);
    double dzn = (z - params->bh[n].XYZ[2]);
    double r = sqrt(dxn*dxn + dyn*dyn + dzn*dzn);
    sumn_mn_o_2rn += params->bh[n].M/(2.*r);
  }
  
  return 1. + u + sumn_mn_o_2rn;
}

static
double multi_punctures_neg_1o8_K2_psi_neg7
(
 double x,
 double y,
 double z,
 double u,
 void* user
)
{
  problem_ctx_t* ctx = user;
  multi_punctures_params_t* params = ctx->multi_punctures_params;

  double psi = psi_fcn(x,y,z,u,user);
  double AijAij = AijAij_fcn(x,y,z,user);

  return (-1./8.)*AijAij/(pow(psi*psi,3.5));
}


static
double multi_punctures_plus_7o8_K2_psi_neg8
(
 double x,
 double y,
 double z,
 double u,
 void* user
)
{
  problem_ctx_t* ctx = user;
  multi_punctures_params_t* params = ctx->multi_punctures_params;

  double psi = psi_fcn(x,y,z,u,user);
  double AijAij = AijAij_fcn(x,y,z,user);
  
  return (7./8.)*AijAij/(pow(psi*psi,4));
}


static
void multi_punctures_apply_jac_add_nonlinear_term_using_matrix
(
 p4est_t* p4est,
 d4est_ghost_t* ghost,
 d4est_ghost_data_t* ghost_data,
 d4est_elliptic_data_t* prob_vecs,
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 void* user
)
{
  double* M_plus_7o8_K2_psi_neg8_of_u0_u_vec = P4EST_ALLOC(double, prob_vecs->local_nodes);
  problem_ctx_t* ctx = user;
  d4est_solver_multigrid_t* mg_data = ctx->mg_data;
  d4est_solver_multigrid_matrix_op_t* matrix_op = mg_data->user_callbacks->user;

  int matrix_stride = 0;
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

        int volume_nodes = d4est_lgl_get_nodes((P4EST_DIM), ed->deg);
        d4est_linalg_matvec_plus_vec(1.,&(matrix_op->matrix[matrix_stride]), &prob_vecs->u[ed->nodal_stride], 0., &M_plus_7o8_K2_psi_neg8_of_u0_u_vec[ed->nodal_stride], volume_nodes, volume_nodes);
        
        matrix_stride += volume_nodes*volume_nodes;
        
      }
    }

  d4est_linalg_vec_axpy(1.0, M_plus_7o8_K2_psi_neg8_of_u0_u_vec, prob_vecs->Au, prob_vecs->local_nodes);
  P4EST_FREE(M_plus_7o8_K2_psi_neg8_of_u0_u_vec);
}



static
void multi_punctures_apply_jac_add_nonlinear_term
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
  double* M_plus_7o8_K2_psi_neg8_of_u0_u_vec = P4EST_ALLOC(double, prob_vecs->local_nodes);
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

        d4est_quadrature_volume_t mesh_object;
        mesh_object.dq = ed->dq;
        mesh_object.tree = ed->tree;
        mesh_object.element_id = ed->id;
        mesh_object.q[0] = ed->q[0];
        mesh_object.q[1] = ed->q[1];
        mesh_object.q[2] = ed->q[2];


        double* J_quad = d4est_mesh_get_jacobian_on_quadrature_points(d4est_factors,ed);

          d4est_mesh_data_on_element_t md_on_e = d4est_mesh_data_on_element
                                                 (
                                                  d4est_factors,
                                                  ed
                                                 );      
        
        d4est_quadrature_apply_fofufofvlilj
          (
           d4est_ops,
           d4est_geom,
           d4est_quad,
           &mesh_object,
           QUAD_OBJECT_VOLUME,
           QUAD_INTEGRAND_UNKNOWN,
           &prob_vecs->u[ed->nodal_stride],
           &prob_vecs->u0[ed->nodal_stride],
           NULL,
           ed->deg,
           md_on_e.xyz_quad,
           J_quad,
           ed->deg_quad,
           &M_plus_7o8_K2_psi_neg8_of_u0_u_vec[ed->nodal_stride],
           multi_punctures_plus_7o8_K2_psi_neg8,
           user,
           NULL,
           NULL,
           QUAD_APPLY_MATRIX,
           0, /* do not interpolate f */
           md_on_e.xyz
          );

      }
    }
  
  d4est_linalg_vec_axpy(1.0, M_plus_7o8_K2_psi_neg8_of_u0_u_vec, prob_vecs->Au, prob_vecs->local_nodes);
  P4EST_FREE(M_plus_7o8_K2_psi_neg8_of_u0_u_vec);
}


static
void multi_punctures_apply_jac
(
 p4est_t* p4est,
 d4est_ghost_t* d4est_ghost,
 d4est_ghost_data_t* d4est_ghost_data,
 d4est_elliptic_data_t* prob_vecs,
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 d4est_mesh_data_t* d4est_factors,
 void* user
)
{
  problem_ctx_t* ctx = user;
  multi_punctures_params_t* params = ctx->multi_punctures_params;
  d4est_laplacian_with_opt_flux_data_t* flux_data = ctx->flux_data_for_jac;
  D4EST_ASSERT(params->number_of_punctures > 0);
  d4est_laplacian_with_opt_apply_aij
    (
     p4est,
     d4est_ghost,
     d4est_ghost_data,
     prob_vecs,
     flux_data,
     d4est_ops,
     d4est_geom,
     d4est_quad,
     d4est_factors,
     0
    );

  if (ctx->use_matrix_operator == 0)
    multi_punctures_apply_jac_add_nonlinear_term
     (
     p4est,
     d4est_ghost,
     d4est_ghost_data,
     prob_vecs,
     d4est_ops,
     d4est_geom,
     d4est_quad,
     d4est_factors,
     user
    );
  else {
    
    d4est_solver_multigrid_t* mg_data = ctx->mg_data;
    d4est_solver_multigrid_matrix_op_t* matrix_op = mg_data->user_callbacks->user;

    if (matrix_op->matrix != matrix_op->matrix_at0){
    multi_punctures_apply_jac_add_nonlinear_term_using_matrix
    (
     p4est,
     d4est_ghost,
     d4est_ghost_data,
     prob_vecs,
     d4est_ops,
     d4est_geom,
     d4est_quad,
     user
    );
    }
    else {
    multi_punctures_apply_jac_add_nonlinear_term
    (
     p4est,
     d4est_ghost,
     d4est_ghost_data,
     prob_vecs,
     d4est_ops,
     d4est_geom,
     d4est_quad,
     d4est_factors,
     user
    );
    }
  }
}

static void
multi_punctures_build_residual_add_nonlinear_term
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
  double* M_neg_1o8_K2_psi_neg7_vec= P4EST_ALLOC(double, prob_vecs->local_nodes);
 
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


        d4est_quadrature_volume_t mesh_object;
        mesh_object.dq = ed->dq;
        mesh_object.tree = ed->tree;
        mesh_object.element_id = ed->id;
        mesh_object.q[0] = ed->q[0];
        mesh_object.q[1] = ed->q[1];
        mesh_object.q[2] = ed->q[2];

        double* J_quad = d4est_mesh_get_jacobian_on_quadrature_points(d4est_factors,ed);

        d4est_mesh_data_on_element_t md_on_e = d4est_mesh_data_on_element
                                               (
                                                d4est_factors,
                                                ed
                                               );

        
        d4est_quadrature_apply_fofufofvlj
          (
           d4est_ops,
           d4est_geom,
           d4est_quad,
           &mesh_object,
           QUAD_OBJECT_VOLUME,
           QUAD_INTEGRAND_UNKNOWN,
           &prob_vecs->u[ed->nodal_stride],
           NULL,
           ed->deg,
           J_quad,
           md_on_e.xyz_quad,
           ed->deg_quad,
           &M_neg_1o8_K2_psi_neg7_vec[ed->nodal_stride],
           multi_punctures_neg_1o8_K2_psi_neg7,
           user,
           NULL,
           NULL,
           0,
           md_on_e.xyz
          );
      }
    }

  d4est_linalg_vec_axpy(1.0,
                  M_neg_1o8_K2_psi_neg7_vec,
                  prob_vecs->Au,
                  prob_vecs->local_nodes);

  P4EST_FREE(M_neg_1o8_K2_psi_neg7_vec);

}


static void
multi_punctures_build_residual
(
 p4est_t* p4est,
 d4est_ghost_t* d4est_ghost,
 d4est_ghost_data_t* d4est_ghost_data,
 d4est_elliptic_data_t* prob_vecs,
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 d4est_mesh_data_t* d4est_factors,
 void* user
)
{
  problem_ctx_t* ctx = user;
  multi_punctures_params_t* params = ctx->multi_punctures_params;
  d4est_laplacian_with_opt_flux_data_t* flux_data = ctx->flux_data_for_res;
  
  d4est_laplacian_with_opt_apply_aij
    (
     p4est,
     d4est_ghost,
     d4est_ghost_data,
     prob_vecs,
     flux_data,
     d4est_ops,
     d4est_geom,
     d4est_quad,
     d4est_factors,
     0
    );

  multi_punctures_build_residual_add_nonlinear_term
    (
     p4est,
     d4est_ghost,
     d4est_ghost_data,
     prob_vecs,
     d4est_ops,
     d4est_geom,
     d4est_quad,
     d4est_factors,
     user
    );
}

double
multi_punctures_robin_coeff_brick_fcn
(
 double x,
 double y,
#if (P4EST_DIM)==3
 double z,
#endif
 void* user,
 d4est_laplacian_with_opt_flux_boundary_data_t* boundary_data,
 int mortar_node
)
{
  
  double r2 = x*x + y*y + z*z;
  
  if (fabs(boundary_data->n_on_f_m_quad[0][mortar_node]) > 0.)
    return boundary_data->n_on_f_m_quad[0][mortar_node]*x/r2;
  else if (fabs(boundary_data->n_on_f_m_quad[1][mortar_node]) > 0.)
    return boundary_data->n_on_f_m_quad[1][mortar_node]*y/r2;
  else if (fabs(boundary_data->n_on_f_m_quad[2][mortar_node]) > 0.)
    return boundary_data->n_on_f_m_quad[2][mortar_node]*z/r2;
  else{
    D4EST_ABORT("Should not occur");
    return NAN;
  }
}

double
multi_punctures_robin_coeff_sphere_fcn
(
 double x,
 double y,
#if (P4EST_DIM)==3
 double z,
#endif
 void* user,
 d4est_laplacian_with_opt_flux_boundary_data_t* boundary_data,
 int mortar_node
)
{
  double r2 = x*x + y*y + z*z;
  return 1/sqrt(r2);
}



double
multi_punctures_robin_bc_rhs_fcn
(
 double x,
 double y,
#if (P4EST_DIM)==3
 double z,
#endif
 void* user,
 d4est_laplacian_with_opt_flux_boundary_data_t* boundary_data,
 int mortar_node
)
{
  return 0.;
}

static double
multi_punctures_initial_guess
(
 double x,
 double y,
#if (P4EST_DIM)==3
 double z,
#endif
 void* user
)
{
  /* double r2 = x*x + y*y + z*z; */
  /* if (r2 > 10.) */
  /*   return 1/(r2); */
  /* else */
  return 0.;
}


static
void multi_punctures_d4est_krylov_pc_setup_fcn
(
 d4est_krylov_pc_t* d4est_krylov_pc
)
{
  zlog_category_t *c_def = zlog_get_category("d4est_solver_multigrid_matrix_operator");
  d4est_krylov_pc_multigrid_data_t* d4est_krylov_pcmgdata = d4est_krylov_pc->pc_data;
  d4est_solver_multigrid_t* mg_data = d4est_krylov_pcmgdata->mg_data;
  krylov_ctx_t* ctx = d4est_krylov_pc->pc_ctx;

  if (ctx->p4est->mpirank == 0){
    zlog_info(c_def, "Running two_punctures_pc_setup_fcn");
  }
  
    d4est_solver_multigrid_matrix_setup_fofufofvlilj_operator
      (
       ctx->p4est,
       ctx->d4est_ops,
       ctx->d4est_geom,
       ctx->d4est_quad,
       ctx->d4est_factors,
       ctx->vecs->u0,
       NULL,
       multi_punctures_plus_7o8_K2_psi_neg8,
       ctx->fcns->user,
       NULL,
       NULL,
       mg_data->user_callbacks->user
      ); 
}


static void
multi_punctures_pointwise_residual
(
 p4est_t* p4est,
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 d4est_mesh_data_t* d4est_factors,
 double* u,
 double* residual_pointwise_quad,
 int interpolation_option,
 void* user
){
  //interpolation option = 0 -> interpolate u to quad and compute
  //f(u) on quad

  //interpolation option = 1 -> interpolate f(u) to quad  
  d4est_hessian_compute_hessian_trace_of_field_on_quadrature_points
    (
     p4est,
     d4est_ops,
     d4est_geom,
     d4est_quad,
     d4est_factors,
     HESSIAN_ANALYTICAL,
     u,
     residual_pointwise_quad
    );

  if (interpolation_option == 0){
    double* u_quad = P4EST_ALLOC(double, d4est_factors->local_sizes.local_nodes_quad);
    for (p4est_topidx_t tt = p4est->first_local_tree;
         tt <= p4est->last_local_tree;
         ++tt){
      p4est_tree_t* tree = p4est_tree_array_index (p4est->trees, tt);
      sc_array_t* tquadrants = &tree->quadrants;
      int QQ = (p4est_locidx_t) tquadrants->elem_count;

      for (int qq = 0; qq < QQ; ++qq) {
        p4est_quadrant_t* quad = p4est_quadrant_array_index (tquadrants, qq);
        d4est_element_data_t* ed = (d4est_element_data_t*)(quad->p.user_data);
        
        d4est_quadrature_volume_t mesh_object;
        mesh_object.dq =  ed->dq;
        mesh_object.tree = ed->tree;
        mesh_object.element_id = ed->id;
        
        mesh_object.q[0] = ed->q[0];
        mesh_object.q[1] = ed->q[1];
#if (P4EST_DIM)==3
        mesh_object.q[2] = ed->q[2];
#endif

        d4est_mesh_data_on_element_t md_on_e
          = d4est_mesh_data_on_element
          (
           d4est_factors,
           ed
          );



        d4est_quadrature_interpolate
          (
           d4est_ops,
           d4est_quad,
           d4est_geom,
           &mesh_object,
           QUAD_OBJECT_VOLUME,
           QUAD_INTEGRAND_UNKNOWN,
           &u[ed->nodal_stride],
           ed->deg,
           &u_quad[ed->quad_stride],
           ed->deg_quad
          );

        int volume_nodes_quad = d4est_lgl_get_nodes((P4EST_DIM), ed->deg_quad);


        for (int i = 0; i < volume_nodes_quad; i++){
          double x = md_on_e.xyz_quad[0][i];
          double y = md_on_e.xyz_quad[1][i];
          double z = md_on_e.xyz_quad[2][i];
          double u = u_quad[ed->quad_stride + i];

          residual_pointwise_quad[ed->quad_stride +  i] =
            multi_punctures_neg_1o8_K2_psi_neg7(x,y,z,u,user)
            - residual_pointwise_quad[ed->quad_stride +  i];
        }
      }
    }
    P4EST_FREE(u_quad);
  }
  else {
    double* f_quad = P4EST_ALLOC(double, d4est_factors->local_sizes.local_nodes_quad);
    double* f_lob = P4EST_ALLOC(double, d4est_factors->local_sizes.local_nodes);
    for (p4est_topidx_t tt = p4est->first_local_tree;
         tt <= p4est->last_local_tree;
         ++tt){
      p4est_tree_t* tree = p4est_tree_array_index (p4est->trees, tt);
      sc_array_t* tquadrants = &tree->quadrants;
      int QQ = (p4est_locidx_t) tquadrants->elem_count;

      for (int qq = 0; qq < QQ; ++qq) {
        p4est_quadrant_t* quad = p4est_quadrant_array_index (tquadrants, qq);
        d4est_element_data_t* ed = (d4est_element_data_t*)(quad->p.user_data);
        
        d4est_quadrature_volume_t mesh_object;
        mesh_object.dq =  ed->dq;
        mesh_object.tree = ed->tree;
        mesh_object.element_id = ed->id;
        
        mesh_object.q[0] = ed->q[0];
        mesh_object.q[1] = ed->q[1];
#if (P4EST_DIM)==3
        mesh_object.q[2] = ed->q[2];
#endif

        d4est_mesh_data_on_element_t md_on_e
          = d4est_mesh_data_on_element
          (
           d4est_factors,
           ed
          );
        
        int volume_nodes = d4est_lgl_get_nodes((P4EST_DIM), ed->deg);
        int volume_nodes_quad = d4est_lgl_get_nodes((P4EST_DIM), ed->deg_quad);

        for (int i = 0; i < volume_nodes; i++){
          double x = md_on_e.xyz[0][i];
          double y = md_on_e.xyz[1][i];
          double z = md_on_e.xyz[2][i];
          f_lob[ed->nodal_stride + i] =
            multi_punctures_neg_1o8_K2_psi_neg7(x,y,z,u[ed->nodal_stride + i],user);
        }


        d4est_quadrature_interpolate
          (
           d4est_ops,
           d4est_quad,
           d4est_geom,
           &mesh_object,
           QUAD_OBJECT_VOLUME,
           QUAD_INTEGRAND_UNKNOWN,
           &f_lob[ed->nodal_stride],
           ed->deg,
           &f_quad[ed->quad_stride],
           ed->deg_quad
          );
        
        for (int i = 0; i < volume_nodes_quad; i++){
          residual_pointwise_quad[ed->quad_stride +  i] =
            f_quad[ed->quad_stride + i]
            - residual_pointwise_quad[ed->quad_stride +  i];
        }
    
      }
    }
    P4EST_FREE(f_quad);
    P4EST_FREE(f_lob);
  }   
}


#endif
