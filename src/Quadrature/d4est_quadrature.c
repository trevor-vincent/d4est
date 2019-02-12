#define _GNU_SOURCE
#include <pXest.h>
#include <d4est_quadrature.h>
#include <d4est_quadrature_legendre.h>
#include <d4est_quadrature_lobatto.h>
#include <d4est_quadrature_compactified.h>
#include <d4est_util.h>
#include <d4est_linalg.h>
#include <d4est_kron.h>
#include <ini.h>

typedef struct {

  const char* input_section;
  char* name;

} d4est_quadrature_input_t;


static
int d4est_quadrature_input_handler
(
 void* user,
 const char* section,
 const char* name,
 const char* value
)
{
  d4est_quadrature_input_t* pconfig = (d4est_quadrature_input_t*)user;

  if (d4est_util_match_couple(section,pconfig->input_section,name,"name")) {
    D4EST_ASSERT(pconfig->name == NULL);
    asprintf(&pconfig->name,"%s",value);
  }
  else {
    return 0;  /* unknown section/name, error */
  }
  return 1;
}

static
d4est_quadrature_input_t
d4est_quadrature_input
(
 int mpirank,
 const char* input_file,
 const char* input_section
)
{
  
  d4est_quadrature_input_t input;
  input.input_section = input_section;
  input.name = NULL;
  
  if (ini_parse(input_file, d4est_quadrature_input_handler, &input) < 0) {
    D4EST_ABORT("Can't load input file");
  }

  D4EST_CHECK_INPUT(input_section, input.name, NULL);
  if (mpirank == 0) {
    zlog_category_t *c_default = zlog_get_category("d4est_quadrature");
    zlog_info(c_default, "Loading %s quadrature", input.name);
  }
  return input;
}


d4est_quadrature_t*
d4est_quadrature_new
(
 p4est_t* p4est,
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 const char* input_file,
 const char* input_section
)
{
  
  d4est_quadrature_input_t input = d4est_quadrature_input(p4est->mpirank,input_file, input_section);
  d4est_quadrature_t* d4est_quad = P4EST_ALLOC(d4est_quadrature_t, 1);

  if (d4est_util_match(input.name,"legendre")) {
    d4est_quad->quad_type = QUAD_TYPE_GAUSS_LEGENDRE;
    d4est_quadrature_legendre_new(d4est_quad, d4est_geom, input_file);
  }
  else if (d4est_util_match(input.name,"lobatto")) {
    d4est_quad->quad_type = QUAD_TYPE_GAUSS_LEGENDRE_LOBATTO;
    d4est_quadrature_lobatto_new(d4est_quad, d4est_geom, input_file);
  }
  /* else if (d4est_util_match(input.name,"legendre_compactified_c1pc2t_neg4")){ */
    /* d4est_quad->quad_type = QUAD_TYPE_GAUSS_LEGENDRE_COMPACTIFIED_C1PC2T_NEG4; */
    /* d4est_quadrature_compactified_new(p4est, d4est_ops, d4est_geom, d4est_quad, input_file, input_section); */
  /* } */
  /* else if (d4est_util_match(input.name,"legendre_compactified_c1pc2t_neg3")){ */
    /* d4est_quad->quad_type = QUAD_TYPE_GAUSS_LEGENDRE_COMPACTIFIED_C1PC2T_NEG3; */
    /* d4est_quadrature_compactified_new(p4est, d4est_ops, d4est_geom, d4est_quad, input_file, input_section); */
  /* } */
  /* else if (d4est_util_match(input.name,"legendre_compactified_c1pc2t_neg2")){ */
    /* d4est_quad->quad_type = QUAD_TYPE_GAUSS_LEGENDRE_COMPACTIFIED_C1PC2T_NEG2; */
    /* d4est_quadrature_compactified_new(p4est, d4est_ops, d4est_geom, d4est_quad, input_file, input_section); */
  /* } */
  /* else if (d4est_util_match(input.name,"legendre_compactified_c1pc2t_neg1")){ */
    /* d4est_quad->quad_type = QUAD_TYPE_GAUSS_LEGENDRE_COMPACTIFIED_C1PC2T_NEG1; */
    /* d4est_quadrature_compactified_new(p4est, d4est_ops, d4est_geom, d4est_quad, input_file, input_section); */
  /* } */
  else if (d4est_util_match(input.name,"none")){
  }
  else {
    zlog_category_t *c_default = zlog_get_category("d4est_quadrature");
    zlog_error(c_default, "You tried to use %s quadrature", input.name);
    D4EST_ABORT("This quadrature is currently not supported");
  }

  free(input.name);

  return d4est_quad;
}

void
d4est_quadrature_reinit(p4est_t* p4est,
                        /* d4est_ghost_t* ghost, */
                        /* void* ghost_data, */
                        d4est_operators_t* d4est_ops,
                        d4est_geometry_t* d4est_geom,
                        d4est_quadrature_t* d4est_quad)
{
  if (d4est_quad->user_reinit != NULL)
    d4est_quad->user_reinit(p4est, d4est_ops, d4est_geom, d4est_quad);
}
  
void
d4est_quadrature_destroy(p4est_t* p4est,
                        d4est_operators_t* d4est_ops,
                        d4est_geometry_t* d4est_geom,
                        d4est_quadrature_t* d4est_quad){
  if (d4est_quad->user_destroy != NULL)
    d4est_quad->user_destroy(p4est, d4est_ops, d4est_geom, d4est_quad);
  P4EST_FREE(d4est_quad);
}


void d4est_quadrature_apply_galerkin_integral
(
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geometry,
 d4est_quadrature_t* d4est_quadrature,
 void* object,
 d4est_quadrature_object_type_t object_type,
 d4est_quadrature_integrand_type_t integrand_type,
 double* in_quad,
 int deg_lobatto,
 double* jac_quad,
 int deg_quad,
 double* out
){
  double* quad_weights [(P4EST_DIM)];
  double* interp_lobatto_to_quad_trans [(P4EST_DIM)];

  int dim = (object_type == QUAD_OBJECT_MORTAR) ? ((P4EST_DIM)-1) : (P4EST_DIM);
  int nodes_lobatto = deg_lobatto + 1;
  int nodes_quad = deg_quad + 1;
  int volume_nodes_quad = d4est_lgl_get_nodes(dim, deg_quad);
  double* w_j_in_quad = P4EST_ALLOC(double, volume_nodes_quad);
  
  for (int dir = 0; dir < dim; dir++){
     quad_weights[dir] = d4est_quadrature->get_weights
                         (
                          d4est_ops,
                          d4est_geometry,
                          d4est_quadrature,
                          object,
                          object_type,
                          integrand_type,
                          deg_quad,
                          dir
                         );
     
     interp_lobatto_to_quad_trans[dir] = d4est_quadrature->get_interp_trans
                                       (
                                        d4est_ops,
                                        d4est_geometry,
                                        d4est_quadrature,
                                        object,
                                        object_type,
                                        integrand_type,
                                        deg_lobatto,
                                        deg_quad,
                                        dir
                                       );
  }

  if (dim == 3){
    d4est_kron_vec1_o_vec2_o_vec3_dot_xy(quad_weights[2], quad_weights[1], quad_weights[0], jac_quad, in_quad, nodes_quad, nodes_quad, nodes_quad, w_j_in_quad);
    d4est_kron_A1A2A3x_nonsqr(out, interp_lobatto_to_quad_trans[2], interp_lobatto_to_quad_trans[1], interp_lobatto_to_quad_trans[0], w_j_in_quad,
                               nodes_lobatto, nodes_quad, nodes_lobatto, nodes_quad, nodes_lobatto, nodes_quad);
  }
  else if (dim == 2) {
    d4est_kron_vec1_o_vec2_dot_xy(quad_weights[1], quad_weights[0], jac_quad, in_quad, nodes_quad, nodes_quad, w_j_in_quad);
    d4est_kron_A1A2x_nonsqr(out, interp_lobatto_to_quad_trans[1], interp_lobatto_to_quad_trans[0], w_j_in_quad, nodes_lobatto, nodes_quad,
                             nodes_lobatto, nodes_quad);
  }
  else if (dim == 1){
    d4est_kron_vec_dot_xy(quad_weights[0], jac_quad, in_quad, nodes_quad, w_j_in_quad);
    d4est_linalg_matvec_plus_vec(1.0, interp_lobatto_to_quad_trans[0], w_j_in_quad, 0., out, nodes_lobatto, nodes_quad);
  }
  
  else {
    P4EST_FREE(w_j_in_quad);
    P4EST_FREE(in_quad);
    D4EST_ABORT("Apply mass matrix ref space, wrong dimension.");
  }
  P4EST_FREE(w_j_in_quad);
}


/* void d4est_quadrature_compute_mass_matrix */
/* ( */
/*  d4est_operators_t* d4est_ops, */
/*  d4est_quadrature_t* d4est_quadrature, */
/*  d4est_geometry_t* d4est_geometry, */
/*  p4est_qcoord_t q, */
/*  p4est_qcoord_t dq, */
/*  p4est_topidx_t tree, */
/*  int deg_lobatto, */
/*  int deg_quad, */
/*  int dim, */
/*  double* jac_quad, */
/*  double* M */
/* ) */
/* { */
/*   int volume_nodes_lobatto = d4est_lgl_get_nodes(dim,deg_lobatto); */
  
/*   double* u = P4EST_ALLOC_ZERO(double, volume_nodes_lobatto); */
/*   double* Mu = P4EST_ALLOC_ZERO(double, volume_nodes_lobatto); */

/*   for (int i = 0; i < volume_nodes_lobatto; i++){ */
/*     u[i] = 1.; */
/*     d4est_quad_apply_mass_matrix */
/*       ( */
/*        d4est_ops, */
/*        d4est_quadrature, */
/*        d4est_geometry, */
/*        q, */
/*        dq, */
/*        tree, */
/*        u, */
/*        deg_lobatto, */
/*        jac_quad, */
/*        deg_quad, */
/*        dim, */
/*        Mu */
/*       ); */
/*     d4est_linalg_set_column(M, Mu, i, volume_nodes_lobatto, volume_nodes_lobatto); */
/*     u[i] = 0.; */
/*   } */

/*   P4EST_FREE(Mu); */
/*   P4EST_FREE(u); */
/* } */



void d4est_quadrature_apply_stiffness_matrix
(
 d4est_operators_t* d4est_ops,
 d4est_quadrature_t* d4est_quadrature,
 d4est_geometry_t* d4est_geometry,
 void* object,
 d4est_quadrature_object_type_t object_type,
 d4est_quadrature_integrand_type_t integrand_type,
 double* in,
 int deg_lobatto,
 double* jac_quad,
 double* rst_xyz [(P4EST_DIM)][(P4EST_DIM)], /* must be padded with NULL if you want lower dim */
 int deg_quad,
 double* out
){
  D4EST_ASSERT (object_type == QUAD_OBJECT_VOLUME);
  /* for now assert this, can get rid of by p-prolonging then p-restricting */


  double* quad_weights [(P4EST_DIM)];
  double* interp_lobatto_to_quad_trans [(P4EST_DIM)];
  double* interp_lobatto_to_quad [(P4EST_DIM)]; 

  int dim = (P4EST_DIM);
  int nodes_lobatto = deg_lobatto + 1;
  int nodes_quad = deg_quad + 1;

  int volume_nodes_quad = d4est_lgl_get_nodes(dim, deg_quad);
  int volume_nodes_lobatto = d4est_lgl_get_nodes(dim, deg_lobatto);
  
  for (int dir = 0; dir < dim; dir++){
    interp_lobatto_to_quad[dir] = d4est_quadrature->get_interp
                                (
                                 d4est_ops,
                                 d4est_geometry,
                                 d4est_quadrature,
                                 object,
                                 object_type,
                                 integrand_type,
                                 deg_lobatto,
                                 deg_quad,
                                 dir
                                );

   interp_lobatto_to_quad_trans[dir] = d4est_quadrature->get_interp_trans
                                       (
                                        d4est_ops,
                                        d4est_geometry,
                                        d4est_quadrature,
                                        object,
                                        object_type,
                                        integrand_type,
                                        deg_lobatto,
                                        deg_quad,
                                        dir
                                       );


   quad_weights[dir] = d4est_quadrature->get_weights
                                       (
                                        d4est_ops,
                                        d4est_geometry,
                                        d4est_quadrature,
                                        object,
                                        object_type,
                                        integrand_type,
                                        deg_quad,
                                        dir
                                       );
  }
  
  double* Dl_in = P4EST_ALLOC(double, volume_nodes_lobatto);
  double* V_Dl_in = P4EST_ALLOC(double, volume_nodes_quad);
  double* W_V_Dl_in = P4EST_ALLOC(double, volume_nodes_quad);
  double* VT_W_V_Dl_in = P4EST_ALLOC(double, volume_nodes_lobatto);
  double* DTlp_VT_W_V_Dl_in = P4EST_ALLOC(double, volume_nodes_lobatto);
  
  d4est_util_fill_array(out, 0., volume_nodes_lobatto);
  
  for (int k = 0; k < dim; k++){
    for (int lp = 0; lp < dim; lp++){
      for (int l = 0; l < dim; l++){
        /* Apply Dbar in lth direction */
        d4est_operators_apply_dij(d4est_ops, in, dim, deg_lobatto, l, Dl_in);

        if ((P4EST_DIM) == 3) {

              d4est_kron_A1A2A3x_nonsqr(V_Dl_in, interp_lobatto_to_quad[2], interp_lobatto_to_quad[1], interp_lobatto_to_quad[0], Dl_in,
                               nodes_quad, nodes_lobatto, nodes_quad, nodes_lobatto, nodes_quad, nodes_lobatto);
              
              d4est_kron_vec1_o_vec2_o_vec3_dot_wxyz(quad_weights[2],quad_weights[1],quad_weights[0], jac_quad, rst_xyz[l][k], rst_xyz[lp][k], V_Dl_in, nodes_quad, nodes_quad, nodes_quad, W_V_Dl_in);
          d4est_kron_A1A2A3x_nonsqr(VT_W_V_Dl_in, interp_lobatto_to_quad_trans[2], interp_lobatto_to_quad_trans[1], interp_lobatto_to_quad_trans[0], W_V_Dl_in,
                                     nodes_lobatto, nodes_quad, nodes_lobatto, nodes_quad, nodes_lobatto, nodes_quad);
        }
        else if ((P4EST_DIM) == 2) {
          d4est_kron_A1A2x_nonsqr(V_Dl_in, interp_lobatto_to_quad[1], interp_lobatto_to_quad[0], Dl_in, nodes_quad, nodes_lobatto,
                            nodes_quad, nodes_lobatto);
          
          d4est_kron_vec1_o_vec2_dot_wxyz(quad_weights[1], quad_weights[0], jac_quad, rst_xyz[l][k], rst_xyz[lp][k], V_Dl_in, nodes_quad, nodes_quad, W_V_Dl_in);
          d4est_kron_A1A2x_nonsqr(VT_W_V_Dl_in, interp_lobatto_to_quad_trans[1], interp_lobatto_to_quad_trans[0], W_V_Dl_in, nodes_lobatto, nodes_quad,
                                   nodes_lobatto, nodes_quad);
        }
        else {
          P4EST_FREE(DTlp_VT_W_V_Dl_in);
          P4EST_FREE(VT_W_V_Dl_in);
          P4EST_FREE(W_V_Dl_in);
          P4EST_FREE(V_Dl_in);
          P4EST_FREE(Dl_in);
          D4EST_ABORT("ERROR: Apply mass matrix ref space, wrong dimension.");
        }

        d4est_operators_apply_dij_transpose(d4est_ops, VT_W_V_Dl_in, dim, deg_lobatto, lp, DTlp_VT_W_V_Dl_in);
        d4est_linalg_vec_axpy(1., DTlp_VT_W_V_Dl_in, out, volume_nodes_lobatto);
      }
    }
  }
  
  P4EST_FREE(DTlp_VT_W_V_Dl_in);
  P4EST_FREE(VT_W_V_Dl_in);
  P4EST_FREE(W_V_Dl_in);
  P4EST_FREE(V_Dl_in);
  P4EST_FREE(Dl_in);
}


void d4est_quadrature_apply_mass_matrix
(
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geometry,
 d4est_quadrature_t* d4est_quadrature,
 void* object,
 d4est_quadrature_object_type_t object_type,
 d4est_quadrature_integrand_type_t integrand_type,
 double* in,
 int deg_lobatto,
 double* jac_quad,
 int deg_quad,
 double* out
){

  D4EST_ASSERT(object_type == QUAD_OBJECT_VOLUME);

  double* quad_weights [(P4EST_DIM)];
  double* interp_lobatto_to_quad_trans [(P4EST_DIM)];
  double* interp_lobatto_to_quad [(P4EST_DIM)]; 

  int dim = (object_type == QUAD_OBJECT_MORTAR) ? ((P4EST_DIM)-1) : (P4EST_DIM);
  int nodes_lobatto = deg_lobatto+1;
  int nodes_quad = deg_quad+1;
  int volume_nodes_quad = d4est_lgl_get_nodes(dim, deg_quad);
  
  double* in_quad = P4EST_ALLOC(double, volume_nodes_quad);
  double* w_j_in_quad = P4EST_ALLOC(double, volume_nodes_quad);

  
  for (int dir = 0; dir < dim; dir++){
    interp_lobatto_to_quad[dir] = d4est_quadrature->get_interp
                                  (
                                   d4est_ops,
                                   d4est_geometry,
                                   d4est_quadrature,
                                   object,
                                   object_type,
                                   integrand_type,
                                   deg_lobatto,
                                   deg_quad,
                                   dir
                                  );

   interp_lobatto_to_quad_trans[dir] = d4est_quadrature->get_interp_trans
                                       (
                                        d4est_ops,
                                        d4est_geometry,
                                        d4est_quadrature,
                                        object,
                                        object_type,
                                        integrand_type,
                                        deg_lobatto,
                                        deg_quad,
                                        dir
                                       );


   quad_weights[dir] = d4est_quadrature->get_weights
                                       (
                                        d4est_ops,
                                        d4est_geometry,
                                        d4est_quadrature,
                                        object,
                                        object_type,
                                        integrand_type,
                                        deg_quad,
                                        dir
                                       );
  }
  
  if (dim == 2) {
    d4est_kron_A1A2x_nonsqr(in_quad, interp_lobatto_to_quad[1], interp_lobatto_to_quad[0], in, nodes_quad, nodes_lobatto,
                             nodes_quad, nodes_lobatto);
    d4est_kron_vec1_o_vec2_dot_xy(quad_weights[1], quad_weights[0], jac_quad, in_quad, nodes_quad, nodes_quad, w_j_in_quad);
    d4est_kron_A1A2x_nonsqr(out, interp_lobatto_to_quad_trans[1], interp_lobatto_to_quad_trans[0], w_j_in_quad, nodes_lobatto, nodes_quad,
                             nodes_lobatto, nodes_quad);
  }
  else if (dim == 3){
    d4est_kron_A1A2A3x_nonsqr(in_quad, interp_lobatto_to_quad[2], interp_lobatto_to_quad[1], interp_lobatto_to_quad[0], in,
                               nodes_quad, nodes_lobatto, nodes_quad, nodes_lobatto, nodes_quad, nodes_lobatto);
    d4est_kron_vec1_o_vec2_o_vec3_dot_xy(quad_weights[2], quad_weights[1], quad_weights[0], jac_quad, in_quad, nodes_quad, nodes_quad, nodes_quad, w_j_in_quad);
    d4est_kron_A1A2A3x_nonsqr(out, interp_lobatto_to_quad_trans[2], interp_lobatto_to_quad_trans[1], interp_lobatto_to_quad_trans[0], w_j_in_quad,
                               nodes_lobatto, nodes_quad, nodes_lobatto, nodes_quad, nodes_lobatto, nodes_quad);
  }
  else {
    P4EST_FREE(w_j_in_quad);
    P4EST_FREE(in_quad);
    D4EST_ABORT("ERROR: Apply mass matrix ref space, wrong dimension.");
  }
  P4EST_FREE(w_j_in_quad);
  P4EST_FREE(in_quad);
}

/* void d4est_quadrature_form_fofufofvlilj_matrix */
/* ( */
/*  d4est_operators_t* d4est_ops, */
/*  d4est_quadrature_t* d4est_quadrature, */
/*  d4est_geometry_t* d4est_geometry, */
/*  p4est_qcoord_t q, */
/*  p4est_qcoord_t dq, */
/*  p4est_topidx_t tree, */
/*  double* u, */
/*  double* v, */
/*  int deg_lobatto, */
/*  double* xyz_quad [(P4EST_DIM)], */
/*  double* jac_quad, */
/*  int deg_quad, */
/*  int dim, */
/*  double* out, */
/*  d4est_xyz_fcn_ext_t fofu_fcn, */
/*  void* fofu_ctx, */
/*  d4est_xyz_fcn_ext_t fofv_fcn, */
/*  void* fofv_ctx */
/* ) */
/* { */
/*   if (fofu_fcn == NULL) */
/*     fofu_fcn = identity_fcn; */
/*   if (fofv_fcn == NULL) */
/*     fofv_fcn = identity_fcn; */
  
/*   double* u_quad = NULL; */
/*   double* v_quad = NULL; */

/*   int volume_nodes_quad = d4est_lgl_get_nodes(dim, deg_quad); */
  
/*   if (u != NULL){ */
/*     u_quad = P4EST_ALLOC(double, volume_nodes_quad); */
/*     d4est_quadrature_interpolate( */
/*                   d4est_ops, */
/*                   d4est_quadrature, */
/*                   d4est_geometry, */
/*                   q, */
/*                   dq, */
/*                   tree, */
/*                   u, */
/*                   deg_lobatto, */
/*                   u_quad, */
/*                   deg_quad, */
/*                   dim */
/*                  ); */
/*   } */
/*   if (v != NULL){ */
/*     v_quad = P4EST_ALLOC(double, volume_nodes_quad); */
/*     d4est_quadrature_interpolate( */
/*                   d4est_ops, */
/*                   d4est_quadrature, */
/*                   d4est_geometry, */
/*                   q, */
/*                   dq, */
/*                   tree, */
/*                   v, */
/*                   deg_lobatto, */
/*                   v_quad, */
/*                   deg_quad, */
/*                   dim */
/*                  ); */
/*   } */
  
/*   double* fofu_fofv_jac = P4EST_ALLOC(double, volume_nodes_quad); */
/*   for (int i = 0; i < volume_nodes_quad; i++){ */
/*     fofu_fofv_jac[i] = jac_quad[i]; */
/*     if (u != NULL || fofu_fcn != identity_fcn){ */
/*       fofu_fofv_jac[i] *= fofu_fcn(xyz_quad[0][i], */
/*                                    xyz_quad[1][i], */
/* #if (P4EST_DIM)==3 */
/*                                    xyz_quad[2][i], */
/* #endif */
/*                                    (u != NULL) ? u_quad[i] : 0, */
/*                                    fofu_ctx); */
/*     } */
/*     if (v != NULL || fofv_fcn != identity_fcn){ */
/*       fofu_fofv_jac[i] *= fofv_fcn(xyz_quad[0][i], */
/*                                    xyz_quad[1][i], */
/* #if (P4EST_DIM)==3 */
/*                                    xyz_quad[2][i], */
/* #endif */
/*                                    (v != NULL) ? v_quad[i] : 0, */
/*                                    fofv_ctx); */
/*     } */
/*   } */
  
/*   d4est_quadrature_compute_mass_matrix */
/*     ( */
/*      d4est_ops, */
/*      d4est_quadrature, */
/*      d4est_geometry, */
/*      q, */
/*      dq, */
/*      tree, */
/*      deg_lobatto, */
/*      deg_quad, */
/*      dim, */
/*      fofu_fofv_jac, */
/*      out */
/*     ); */
 
  
/*   if (u != NULL){ */
/*     P4EST_FREE(u_quad); */
/*   } */
/*   if (v != NULL){ */
/*     P4EST_FREE(v_quad); */
/*   } */
  
/* } */


void d4est_quadrature_apply_fofufofvlilj
(
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 void* object,
 d4est_quadrature_object_type_t object_type,
 d4est_quadrature_integrand_type_t integrand_type, 
 double* vec,
 double* u,
 double* v,
 int deg_lobatto,
 double* xyz_quad [(P4EST_DIM)],
 double* jac_quad,
 int deg_quad,
 double* out,
 d4est_xyzu_fcn_t fofu_fcn,
 void* fofu_ctx,
 d4est_xyzu_fcn_t fofv_fcn,
 void* fofv_ctx,
 d4est_quadrature_apply_or_compute_matrix_t apply_or_compute_matrix,
 int interpolate_f,
 double* xyz_lobatto [(P4EST_DIM)]
)
{
  if (fofu_fcn == NULL)
    fofu_fcn = identity_fcn;
  if (fofv_fcn == NULL)
    fofv_fcn = identity_fcn;
  
  double* u_quad = NULL;
  double* v_quad = NULL;
  double* fofu_fofv_jac = NULL;
  int dim = (object_type == QUAD_OBJECT_MORTAR) ? ((P4EST_DIM)-1) : (P4EST_DIM);
  int volume_nodes_quad = d4est_lgl_get_nodes(dim, deg_quad);
  
 if (!interpolate_f){
  if (u != NULL){
    u_quad = P4EST_ALLOC(double, volume_nodes_quad);
    d4est_quadrature_interpolate(
                  d4est_ops,
                  d4est_quad,
                  d4est_geom,
                  object,
                  object_type,
                  integrand_type,
                  u,
                  deg_lobatto,
                  u_quad,
                  deg_quad
                 );
  }
  if (v != NULL){
    v_quad = P4EST_ALLOC(double, volume_nodes_quad);
    d4est_quadrature_interpolate(
                  d4est_ops,
                  d4est_quad,
                  d4est_geom,
                  object,
                  object_type,
                  integrand_type,
                  v,
                  deg_lobatto,
                  v_quad,
                  deg_quad
                 );
  }
  
  fofu_fofv_jac = P4EST_ALLOC(double, volume_nodes_quad);
  for (int i = 0; i < volume_nodes_quad; i++){
    fofu_fofv_jac[i] = jac_quad[i];
    if (u != NULL || fofu_fcn != identity_fcn){
      fofu_fofv_jac[i] *= fofu_fcn(xyz_quad[0][i],
                                   xyz_quad[1][i],
#if (P4EST_DIM)==3
                                   xyz_quad[2][i],
#endif
                                   (u != NULL) ? u_quad[i] : 0,
                                   fofu_ctx);
    }
    if (v != NULL || fofv_fcn != identity_fcn){
      fofu_fofv_jac[i] *= fofv_fcn(xyz_quad[0][i],
                                   xyz_quad[1][i],
#if (P4EST_DIM)==3
                                   xyz_quad[2][i],
#endif
                                   (v != NULL) ? v_quad[i] : 0,
                                   fofv_ctx);
    }
  }
  }
  else {
    int volume_nodes_lobatto = d4est_lgl_get_nodes(dim, deg_lobatto);
    double* fofu_fofv_lobatto = P4EST_ALLOC(double, volume_nodes_lobatto);
    fofu_fofv_jac = P4EST_ALLOC(double, volume_nodes_quad);
    
    for (int i = 0; i < volume_nodes_lobatto; i++){
      fofu_fofv_lobatto[i] = 1.0;
      if (u != NULL || fofu_fcn != identity_fcn){
        fofu_fofv_lobatto[i] *= fofu_fcn(xyz_lobatto[0][i],
                                 xyz_lobatto[1][i],
#if (P4EST_DIM)==3
                                 xyz_lobatto[2][i],
#endif
                                 (u != NULL) ? u[i] : 0,
                                 fofu_ctx);
      }
      if (v != NULL || fofv_fcn != identity_fcn){
        fofu_fofv_lobatto[i] *= fofv_fcn(xyz_lobatto[0][i],
                                 xyz_lobatto[1][i],
#if (P4EST_DIM)==3
                                 xyz_lobatto[2][i],
#endif
                                 (v != NULL) ? v[i] : 0,
                                 fofv_ctx);
      }
    }

    d4est_quadrature_interpolate(
                                 d4est_ops,
                                 d4est_quad,
                                 d4est_geom,
                                 object,
                                 object_type,
                                 integrand_type,
                                 fofu_fofv_lobatto,
                                 deg_lobatto,
                                 fofu_fofv_jac,
                                 deg_quad
    );

    for (int i = 0; i < volume_nodes_quad; i++){
      fofu_fofv_jac[i] *= jac_quad[i];
    }
    
    P4EST_FREE(fofu_fofv_lobatto);
  }
 
  if (apply_or_compute_matrix == QUAD_APPLY_MATRIX){
    D4EST_ASSERT(vec != NULL);
    d4est_quadrature_apply_mass_matrix
      (
       d4est_ops,
       d4est_geom,
       d4est_quad,
       object,
       object_type,
       integrand_type,
       vec,
       deg_lobatto,
       fofu_fofv_jac,
       deg_quad,
       out
      );
  }
  else if (apply_or_compute_matrix == QUAD_COMPUTE_MATRIX)
    d4est_quadrature_compute_mass_matrix
    (
       d4est_ops,
       d4est_geom,
       d4est_quad,
       object,
       object_type,
       integrand_type,
       deg_lobatto,
       fofu_fofv_jac,
       deg_quad,
       out
    );
 else
   D4EST_ABORT("Not a supported option");
  
  if (u != NULL){
    P4EST_FREE(u_quad);
  }
  if (v != NULL){
    P4EST_FREE(v_quad);
  }

  P4EST_FREE(fofu_fofv_jac);
  
}

void d4est_quadrature_apply_fofufofvlj
(
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 void* object,
 d4est_quadrature_object_type_t object_type,
 d4est_quadrature_integrand_type_t integrand_type, 
 double* u,
 double* v,
 int deg_lobatto,
 double* jac_quad,
 double* xyz_quad [(P4EST_DIM)],
 int deg_quad,
 double* out,
 d4est_xyzu_fcn_t fofu_fcn,
 void* fofu_ctx,
 d4est_xyzu_fcn_t fofv_fcn,
 void* fofv_ctx,
 int interpolate_f,
 double* xyz_lobatto [(P4EST_DIM)]
)
{
  if(interpolate_f){
    if(xyz_lobatto == NULL){
      D4EST_ABORT("interpolate_f == 1, but xyz_lobatto == NULL");
    }
  }
  
  if (fofu_fcn == NULL){
    fofu_fcn = identity_fcn;
  }
  if (fofv_fcn == NULL){
    fofv_fcn = identity_fcn;
  }
  
  double* u_quad = NULL;
  double* v_quad = NULL;
  double* fofu_fofv  = NULL;
  int dim = (object_type == QUAD_OBJECT_MORTAR) ? ((P4EST_DIM)-1) : (P4EST_DIM);
  int volume_nodes_quad = d4est_lgl_get_nodes(dim, deg_quad);


  if (!interpolate_f){
  if (u != NULL){
    u_quad = P4EST_ALLOC(double, volume_nodes_quad);
    d4est_quadrature_interpolate(
                  d4est_ops,
                  d4est_quad,
                  d4est_geom,
                  object,
                  object_type,
                  integrand_type,
                  u,
                  deg_lobatto,
                  u_quad,
                  deg_quad
                 );
  }
  if (v != NULL){
    v_quad = P4EST_ALLOC(double, volume_nodes_quad);
    d4est_quadrature_interpolate(
                  d4est_ops,
                  d4est_quad,
                  d4est_geom,
                  object,
                  object_type,
                  integrand_type,
                  v,
                  deg_lobatto,
                  v_quad,
                  deg_quad
                 );
  }
  
  fofu_fofv = P4EST_ALLOC(double, volume_nodes_quad);
  for (int i = 0; i < volume_nodes_quad; i++){
    fofu_fofv[i] = 1.0;
    if (u != NULL || fofu_fcn != identity_fcn){
      fofu_fofv[i] *= fofu_fcn(xyz_quad[0][i],
                                   xyz_quad[1][i],
#if (P4EST_DIM)==3
                                   xyz_quad[2][i],
#endif
                                   (u != NULL) ? u_quad[i] : 0,
                                   fofu_ctx);
    }
    if (v != NULL || fofv_fcn != identity_fcn){
      fofu_fofv[i] *= fofv_fcn(xyz_quad[0][i],
                                   xyz_quad[1][i],
#if (P4EST_DIM)==3
                                   xyz_quad[2][i],
#endif
                                   (v != NULL) ? v_quad[i] : 0,
                                   fofv_ctx);
    }
  }
  }
  else {
    int volume_nodes_lobatto = d4est_lgl_get_nodes(dim, deg_lobatto);
    double* fofu_fofv_lobatto = P4EST_ALLOC(double, volume_nodes_lobatto);
    fofu_fofv = P4EST_ALLOC(double, volume_nodes_quad);
    
    for (int i = 0; i < volume_nodes_lobatto; i++){
      fofu_fofv_lobatto[i] = 1.0;
      if (u != NULL || fofu_fcn != identity_fcn){
        fofu_fofv_lobatto[i] *= fofu_fcn(xyz_lobatto[0][i],
                                 xyz_lobatto[1][i],
#if (P4EST_DIM)==3
                                 xyz_lobatto[2][i],
#endif
                                 (u != NULL) ? u[i] : 0,
                                 fofu_ctx);
      }
      if (v != NULL || fofv_fcn != identity_fcn){
        fofu_fofv_lobatto[i] *= fofv_fcn(xyz_lobatto[0][i],
                                 xyz_lobatto[1][i],
#if (P4EST_DIM)==3
                                 xyz_lobatto[2][i],
#endif
                                 (v != NULL) ? v[i] : 0,
                                 fofv_ctx);
      }
    }

    d4est_quadrature_interpolate
      (
       d4est_ops,
       d4est_quad,
       d4est_geom,
       object,
       object_type,
       integrand_type,
       fofu_fofv_lobatto,
       deg_lobatto,
       fofu_fofv,
       deg_quad
      );
    
    P4EST_FREE(fofu_fofv_lobatto);
  }
  d4est_quadrature_apply_galerkin_integral
    (
     d4est_ops,
     d4est_geom,
     d4est_quad,
     object,
     object_type,
     integrand_type,
     fofu_fofv,
     deg_lobatto,
     jac_quad,
     deg_quad,
     out
    );

  P4EST_FREE(u_quad);
  P4EST_FREE(v_quad);
  P4EST_FREE(fofu_fofv);
  
}


d4est_rst_t
d4est_quadrature_get_rst_points
(
 d4est_operators_t* d4est_ops,
 d4est_quadrature_t* d4est_quadrature,
 d4est_geometry_t* d4est_geometry,
 void* object,
 d4est_quadrature_object_type_t object_type,
 d4est_quadrature_integrand_type_t integrand_type,
 int degree
)
{
  int dim = (object_type == QUAD_OBJECT_MORTAR) ? ((P4EST_DIM)-1) : (P4EST_DIM);
  d4est_rst_t rst_points;
  rst_points.r = d4est_quadrature->get_rst(d4est_ops, d4est_geometry, d4est_quadrature,object,object_type, integrand_type, degree, 0);
  if (dim >= 2)
    rst_points.s = d4est_quadrature->get_rst(d4est_ops, d4est_geometry, d4est_quadrature,  object, object_type, integrand_type, degree, 1);
  else
    rst_points.s = NULL;
  if (dim == 3)
    rst_points.t = d4est_quadrature->get_rst(d4est_ops, d4est_geometry, d4est_quadrature,  object, object_type, integrand_type, degree, 2);
  else
    rst_points.t = NULL;
  return rst_points;
}


void
d4est_quadrature_interpolate
(
 d4est_operators_t* d4est_ops,
 d4est_quadrature_t* d4est_quadrature,
 d4est_geometry_t* d4est_geometry,
 void* object,
 d4est_quadrature_object_type_t object_type,
 d4est_quadrature_integrand_type_t integrand_type,
 double* u_lobatto_in,
 int deg_lobatto,
 double* u_quad_out,
 int deg_quad
)
{
  /* D4EST_ASSERT(object_type == QUAD_OBJECT_MORTAR || object_type == QUAD_OBJECT_VOLUME); */
  double* interp_lobatto_to_quad [(P4EST_DIM)]; 
  int dim = (object_type == QUAD_OBJECT_MORTAR) ? ((P4EST_DIM)-1) : (P4EST_DIM);
  int nodes_lobatto = deg_lobatto + 1;
  int nodes_quad = deg_quad + 1;

  for (int dir = 0; dir < dim; dir++){
    interp_lobatto_to_quad[dir] = d4est_quadrature->get_interp
                                (
                                 d4est_ops,
                                 d4est_geometry,
                                 d4est_quadrature,
                                 object,
                                 object_type,
                                 integrand_type, 
                                 deg_lobatto,
                                 deg_quad,
                                 dir
                                );
  }
  
  if (dim == 3){
    d4est_kron_A1A2A3x_nonsqr(u_quad_out, interp_lobatto_to_quad[2], interp_lobatto_to_quad[1], interp_lobatto_to_quad[0], u_lobatto_in,
                               nodes_quad, nodes_lobatto, nodes_quad, nodes_lobatto, nodes_quad, nodes_lobatto);
  }
  else if (dim == 2){
    d4est_kron_A1A2x_nonsqr(u_quad_out, interp_lobatto_to_quad[1], interp_lobatto_to_quad[0], u_lobatto_in, nodes_quad, nodes_lobatto,
                             nodes_quad, nodes_lobatto);
  }
  else if (dim == 1){
    d4est_linalg_matvec_plus_vec(1.0,interp_lobatto_to_quad[0], u_lobatto_in, 0., u_quad_out, nodes_quad, nodes_lobatto);
  }
  else {
    D4EST_ABORT("[D4EST_ERROR]: dim = 1, 2 or 3\n");
  }  
}

/* Computes \sum w_i u_i v_i */
double
d4est_quadrature_innerproduct
(
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 void* object,
 d4est_quadrature_object_type_t object_type,
 d4est_quadrature_integrand_type_t integrand_type, 
 double* u,
 double* v,
 double* jac_quad,
 int deg_quad
)
{

  double* quad_weights [(P4EST_DIM)];

  int dim = (object_type == QUAD_OBJECT_MORTAR) ? ((P4EST_DIM)-1) : (P4EST_DIM);
  int nodes_quad = deg_quad+1;  
  
  for (int dir = 0; dir < dim; dir++)
    {
      quad_weights[dir] = d4est_quad->get_weights
                          (
                           d4est_ops,
                           d4est_geom,
                           d4est_quad,
                           object,
                           object_type,
                           integrand_type, 
                           deg_quad,
                           dir
                          );
    }
  
  D4EST_ASSERT(u != NULL);
  int volume_nodes_quad = d4est_lgl_get_nodes(dim, deg_quad);
  double* wuv = P4EST_ALLOC(double, volume_nodes_quad);
  double wdotuv = 0.;

  if (dim == 3){
    if (v != NULL){
      d4est_kron_vec1_o_vec2_o_vec3_dot_xy(quad_weights[2], quad_weights[1], quad_weights[0], u, v, nodes_quad, nodes_quad, nodes_quad, wuv);
    }
    else {
      d4est_kron_vec1_o_vec2_o_vec3_dot_x(quad_weights[2], quad_weights[1], quad_weights[0], u,nodes_quad,nodes_quad,nodes_quad,wuv);
    }
  }
  else if (dim == 2){
    if (v != NULL){
      d4est_kron_vec1_o_vec2_dot_xy(quad_weights[1], quad_weights[0],  u, v, nodes_quad, nodes_quad, wuv);
    }
    else {
      d4est_kron_vec1_o_vec2_dot_x(quad_weights[1], quad_weights[0], u, nodes_quad, nodes_quad, wuv);
    }
  }
  else if (dim == 1){
    if (v != NULL){
      d4est_kron_vec_dot_xy(quad_weights[0], u, v, deg_quad + 1, wuv);
    }
    else {
      d4est_kron_vec_dot_x(quad_weights[0], u, deg_quad + 1, wuv);
    }
  }
  else {
    D4EST_ABORT("dim must be 1,2,3");
  }

  if (jac_quad == NULL){
    for (int i = 0; i < volume_nodes_quad; i++){
      wdotuv += wuv[i];
    }
  }
  else {
    for (int i = 0; i < volume_nodes_quad; i++){
      wdotuv += jac_quad[i]*wuv[i];
    }
  }
  P4EST_FREE(wuv);

  return wdotuv;
}


double
d4est_quadrature_lebesgue_measure
(
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 void* object,
 d4est_quadrature_object_type_t object_type,
 d4est_quadrature_integrand_type_t integrand_type, 
 double* jac_object,
 int deg_object
)
{
  int volume_nodes = d4est_lgl_get_nodes((P4EST_DIM), deg_object);
  double* ones = P4EST_ALLOC(double, volume_nodes);

  for (int i = 0; i < volume_nodes; i++)
    ones[i] = 1.;
  
  double measure = d4est_quadrature_innerproduct
    (
     d4est_ops,
     d4est_geom,
     d4est_quad,
     object,
     object_type,
     integrand_type, 
     ones,
     NULL,
     jac_object,
     deg_object
    );
  
  P4EST_FREE(ones);
  return measure;
}



void d4est_quadrature_compute_mass_matrix
(
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geometry,
 d4est_quadrature_t* d4est_quadrature,
 void* object,
 d4est_quadrature_object_type_t object_type,
 d4est_quadrature_integrand_type_t integrand_type,
 int deg_lobatto,
 double* jac_quad,
 int deg_quad,
 double* out
)
{
  int dim = (object_type == QUAD_OBJECT_MORTAR) ? ((P4EST_DIM)-1) : (P4EST_DIM);
  int volume_nodes_lobatto = d4est_lgl_get_nodes(dim,deg_lobatto);
  
  double* u = P4EST_ALLOC_ZERO(double, volume_nodes_lobatto);
  double* Mu = P4EST_ALLOC_ZERO(double, volume_nodes_lobatto);

  for (int i = 0; i < volume_nodes_lobatto; i++){
    u[i] = 1.;
    d4est_quadrature_apply_mass_matrix
      (
       d4est_ops,
       d4est_geometry,
       d4est_quadrature,
       object,
       object_type,
       integrand_type,
       u,
       deg_lobatto,
       jac_quad,
       deg_quad,
       Mu
      );
    
    d4est_linalg_set_column(out, Mu, i, volume_nodes_lobatto, volume_nodes_lobatto);
    u[i] = 0.;
  }

  P4EST_FREE(Mu);
  P4EST_FREE(u);
}



/* void d4est_quadrature_compute_inverse_mass_matrix */
/* ( */
/*  d4est_operators_t* d4est_ops, */
/*  d4est_geometry_t* d4est_geometry, */
/*  d4est_quadrature_t* d4est_quadrature, */
/*  void* object, */
/*  d4est_quadrature_object_type_t object_type, */
/*  d4est_quadrature_integrand_type_t integrand_type, */
/*  int deg_lobatto, */
/*  double* jac_quad, */
/*  int deg_quad, */
/*  double* out */
/* ) */
/* { */

/*   int volume_nodes_lobatto = d4est_lgl_get_nodes(dim,deg_lobatto); */
/*   d4est_quadrature_compute_mass_matrix */
/*     ( */
/*      d4est_ops, */
/*      d4est_geometry, */
/*      d4est_quadrature, */
/*      object, */
/*      object_type, */
/*      integrand_type, */
/*      deg_lobatto, */
/*      jac_quad, */
/*      deg_quad, */
/*      out */
/*     ); */
/*     d4est_linalg_invert(out, volume_nodes_lobatto); */
/* } */

void d4est_quadrature_apply_inverse_mass_matrix
(
 d4est_operators_t* d4est_ops,
 double* in,
 int deg_Lobatto,
 double* jac_Gauss,
 int deg_Gauss,
 int dim,
 double* out
){
  D4EST_ASSERT(dim == 1 || dim == 2 || dim == 3);
  D4EST_ASSERT(deg_Lobatto == deg_Gauss);
  int volume_nodes_Gauss = d4est_lgl_get_nodes(dim, deg_Gauss);

  double* in_Gauss = P4EST_ALLOC(double, volume_nodes_Gauss);
  double* one_over_w_j_in_Gauss = P4EST_ALLOC(double, volume_nodes_Gauss);
  
  double* Gauss_weights = d4est_operators_fetch_gauss_weights_1d(d4est_ops, deg_Gauss);
  double* GLL_to_GL_interp_trans_inverse = d4est_operators_fetch_lobatto_to_gauss_interp_trans_inverse_1d(d4est_ops, deg_Lobatto);
  double* GLL_to_GL_interp_inverse = d4est_operators_fetch_lobatto_to_gauss_interp_inverse_1d(d4est_ops, deg_Lobatto);
  
  int nodes_Lobatto = deg_Lobatto + 1;
  int nodes_Gauss = deg_Gauss + 1;
  
  if (dim == 1){
    d4est_linalg_matvec_plus_vec(1.0, GLL_to_GL_interp_trans_inverse, in, 0., in_Gauss, nodes_Gauss, nodes_Lobatto);
    d4est_kron_oneover_vec_dot_oneover_x_dot_y(Gauss_weights, jac_Gauss, in_Gauss, nodes_Gauss, one_over_w_j_in_Gauss);
    d4est_linalg_matvec_plus_vec(1.0, GLL_to_GL_interp_inverse, one_over_w_j_in_Gauss, 0., out, nodes_Lobatto, nodes_Gauss);
  }
  
  else if (dim == 2){
    d4est_kron_A1A2x_nonsqr
      (
       in_Gauss,
       GLL_to_GL_interp_trans_inverse,
       GLL_to_GL_interp_trans_inverse,
       in,
       nodes_Gauss,
       nodes_Lobatto,
       nodes_Gauss,
       nodes_Lobatto
      );
    
    d4est_kron_oneover_vec_o_vec_dot_oneover_x_dot_y
      (
       Gauss_weights,
       jac_Gauss,
       in_Gauss,
       nodes_Gauss,
       one_over_w_j_in_Gauss
      );
    
    d4est_kron_A1A2x_nonsqr
      (
       out,
       GLL_to_GL_interp_inverse,
       GLL_to_GL_interp_inverse,
       one_over_w_j_in_Gauss,
       nodes_Lobatto,
       nodes_Gauss,
       nodes_Lobatto,
       nodes_Gauss
      );
  }
  
  else if (dim == 3) {
    d4est_kron_A1A2A3x_nonsqr(
                               in_Gauss,
                               GLL_to_GL_interp_trans_inverse,
                               GLL_to_GL_interp_trans_inverse,
                               GLL_to_GL_interp_trans_inverse,
                               in,
                               nodes_Gauss,
                               nodes_Lobatto,
                               nodes_Gauss,
                               nodes_Lobatto,
                               nodes_Gauss,
                               nodes_Lobatto
                              );
    
    d4est_kron_oneover_vec_o_vec_o_vec_dot_oneover_x_dot_y
      (
       Gauss_weights,
       jac_Gauss,
       in_Gauss,
       nodes_Gauss,
       one_over_w_j_in_Gauss
      );
    
    d4est_kron_A1A2A3x_nonsqr(
                               out,
                               GLL_to_GL_interp_inverse,
                               GLL_to_GL_interp_inverse,
                               GLL_to_GL_interp_inverse,
                               one_over_w_j_in_Gauss,
                               nodes_Lobatto,
                               nodes_Gauss,
                               nodes_Lobatto,
                               nodes_Gauss,
                               nodes_Lobatto,
                               nodes_Gauss
                              );
  } else {
    P4EST_FREE(one_over_w_j_in_Gauss);
    P4EST_FREE(in_Gauss);
    D4EST_ABORT("ERROR: Apply mass matrix ref space, wrong dimension.");
  }  
  P4EST_FREE(one_over_w_j_in_Gauss);
  P4EST_FREE(in_Gauss);
}





/* void d4est_quadrature_form_fofufofvlilj_matrix */
/* ( */
/*  d4est_operators_t* d4est_ops, */
/*  d4est_quadrature_t* d4est_quadrature, */
/*  d4est_geometry_t* d4est_geometry, */
/*  p4est_qcoord_t q, */
/*  p4est_qcoord_t dq, */
/*  p4est_topidx_t tree, */
/*  double* u, */
/*  double* v, */
/*  int deg_lobatto, */
/*  double* xyz_quad [(P4EST_DIM)], */
/*  double* jac_quad, */
/*  int deg_quad, */
/*  int dim, */
/*  double* out, */
/*  d4est_xyz_fcn_ext_t fofu_fcn, */
/*  void* fofu_ctx, */
/*  d4est_xyz_fcn_ext_t fofv_fcn, */
/*  void* fofv_ctx */
/* ) */
/* { */
/*   if (fofu_fcn == NULL) */
/*     fofu_fcn = identity_fcn; */
/*   if (fofv_fcn == NULL) */
/*     fofv_fcn = identity_fcn; */
  
/*   double* u_quad = NULL; */
/*   double* v_quad = NULL; */

/*   int volume_nodes_quad = d4est_lgl_get_nodes(dim, deg_quad); */
  
/*   if (u != NULL){ */
/*     u_quad = P4EST_ALLOC(double, volume_nodes_quad); */
/*     d4est_quadrature_interpolate( */
/*                   d4est_ops, */
/*                   d4est_quadrature, */
/*                   d4est_geometry, */
/*                   q, */
/*                   dq, */
/*                   tree, */
/*                   u, */
/*                   deg_lobatto, */
/*                   u_quad, */
/*                   deg_quad, */
/*                   dim */
/*                  ); */
/*   } */
/*   if (v != NULL){ */
/*     v_quad = P4EST_ALLOC(double, volume_nodes_quad); */
/*     d4est_quadrature_interpolate( */
/*                   d4est_ops, */
/*                   d4est_quadrature, */
/*                   d4est_geometry, */
/*                   q, */
/*                   dq, */
/*                   tree, */
/*                   v, */
/*                   deg_lobatto, */
/*                   v_quad, */
/*                   deg_quad, */
/*                   dim */
/*                  ); */
/*   } */
  
/*   double* fofu_fofv_jac = P4EST_ALLOC(double, volume_nodes_quad); */
/*   for (int i = 0; i < volume_nodes_quad; i++){ */
/*     fofu_fofv_jac[i] = jac_quad[i]; */
/*     if (u != NULL || fofu_fcn != identity_fcn){ */
/*       fofu_fofv_jac[i] *= fofu_fcn(xyz_quad[0][i], */
/*                                    xyz_quad[1][i], */
/* #if (P4EST_DIM)==3 */
/*                                    xyz_quad[2][i], */
/* #endif */
/*                                    (u != NULL) ? u_quad[i] : 0, */
/*                                    fofu_ctx); */
/*     } */
/*     if (v != NULL || fofv_fcn != identity_fcn){ */
/*       fofu_fofv_jac[i] *= fofv_fcn(xyz_quad[0][i], */
/*                                    xyz_quad[1][i], */
/* #if (P4EST_DIM)==3 */
/*                                    xyz_quad[2][i], */
/* #endif */
/*                                    (v != NULL) ? v_quad[i] : 0, */
/*                                    fofv_ctx); */
/*     } */
/*   } */
  
/*   d4est_quadrature_compute_mass_matrix */
/*     ( */
/*      d4est_ops, */
/*      d4est_quadrature, */
/*      d4est_geometry, */
/*      q, */
/*      dq, */
/*      tree, */
/*      deg_lobatto, */
/*      deg_quad, */
/*      dim, */
/*      fofu_fofv_jac, */
/*      out */
/*     ); */
 
  
/*   if (u != NULL){ */
/*     P4EST_FREE(u_quad); */
/*   } */
/*   if (v != NULL){ */
/*     P4EST_FREE(v_quad); */
/*   } */
  
/* } */
/* This file was automatically generated.  Do not edit! */
/* This file was automatically generated.  Do not edit! */
/* This file was automatically generated.  Do not edit! */
/* This file was automatically generated.  Do not edit! */
/* This file was automatically generated.  Do not edit! */
/* This file was automatically generated.  Do not edit! */
/* This file was automatically generated.  Do not edit! */
