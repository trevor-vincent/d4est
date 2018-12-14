#ifndef D4EST_SOLVER_SCHWARZ_HELPERS_H
#define D4EST_SOLVER_SCHWARZ_HELPERS_H 

#include <pXest.h>
#include <d4est_util.h>
#include <d4est_ghost.h>
#include <d4est_operators.h>
#include <d4est_mesh.h>
#include <d4est_solver_schwarz_metadata.h>
#include <d4est_solver_schwarz_operators.h>
#include <d4est_solver_schwarz_helpers.h>
#include <d4est_field.h>
#include <d4est_elliptic_data.h>
#include <d4est_elliptic_eqns.h>
#include <d4est_linalg.h>
#include <d4est_vtk.h>
#include <d4est_laplacian_flux.h>
#include <d4est_solver_schwarz_laplacian.h>


/* This file was automatically generated.  Do not edit! */
void d4est_solver_schwarz_compute_and_add_correction_version2(p4est_t *p4est,d4est_geometry_t *d4est_geom,d4est_quadrature_t *d4est_quad,d4est_mesh_data_t *d4est_factors,d4est_ghost_t *ghost,d4est_ghost_data_t *ghost_data,d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,d4est_laplacian_flux_data_t *flux_fcn_data,d4est_solver_schwarz_laplacian_mortar_data_t *laplacian_mortar_data,double *u,double *r);
double d4est_solver_schwarz_cg_solve_subdomain_single_core_version2(p4est_t *p4est,d4est_geometry_t *d4est_geom,d4est_quadrature_t *d4est_quad,d4est_mesh_data_t *d4est_factors,d4est_ghost_t *ghost,d4est_ghost_data_t *ghost_data,d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,d4est_laplacian_flux_data_t *flux_fcn_data,d4est_solver_schwarz_laplacian_mortar_data_t *laplacian_mortar_data,double *du_restricted_field_over_subdomain,double *rhs_restricted_field_over_subdomain,int iter,double atol,double rtol,int subdomain);
void d4est_solver_schwarz_compute_and_add_correction(p4est_t *p4est,d4est_geometry_t *d4est_geom,d4est_quadrature_t *d4est_quad,d4est_mesh_data_t *d4est_factors,d4est_ghost_t *ghost,d4est_ghost_data_t *ghost_data,d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,d4est_elliptic_eqns_t *elliptic_eqns,d4est_elliptic_data_t *elliptic_data,double *u,double *r,double iter,double atol,double rtol,d4est_vtk_helper_array_t *helper_array,int suffix_id);
double d4est_solver_schwarz_cg_solve_subdomain_single_core(p4est_t *p4est,d4est_geometry_t *d4est_geom,d4est_quadrature_t *d4est_quad,d4est_mesh_data_t *d4est_factors,d4est_ghost_t *ghost,d4est_ghost_data_t *ghost_data,d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,d4est_elliptic_data_t *elliptic_data,d4est_elliptic_eqns_t *elliptic_eqns,double *du_restricted_field_over_subdomain,double *rhs_restricted_field_over_subdomain,double *zeroed_u_field_over_mesh,double *zeroed_Au_field_over_mesh,int iter,double atol,double rtol,int subdomain);
double d4est_solver_schwarz_visualize_subdomain_helper_single_core(p4est_t *p4est,d4est_mesh_data_t *d4est_factors,d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,int subdomain,int *which_elements,double *subdomain_field_for_viz,double *weighted_subdomain_field_for_viz);
void d4est_solver_schwarz_apply_lhs_single_core(p4est_t *p4est,d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,d4est_ghost_t *ghost,d4est_ghost_data_t *ghost_data,d4est_geometry_t *d4est_geom,d4est_quadrature_t *d4est_quad,d4est_mesh_data_t *d4est_factors,d4est_elliptic_eqns_t *elliptic_eqns,d4est_elliptic_data_t *elliptic_data,double *u_restricted_field_over_subdomain,double *Au_restricted_field_over_subdomain,double *zeroed_u_field_over_mesh,double *zeroed_Au_field_over_mesh,int subdomain,d4est_vtk_helper_array_t *helper_array);
void d4est_solver_schwarz_compute_correction(d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,double *du_restricted_field_over_subdomains,double *correction_field_over_subdomains);
void d4est_solver_schwarz_zero_field_over_subdomain_single_core(p4est_t *p4est,d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,d4est_mesh_data_t *d4est_factors,d4est_solver_schwarz_subdomain_metadata_t sub_data,double *field);
void d4est_solver_schwarz_apply_weights_over_all_subdomains(d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,double *restricted_field_over_subdomains,double *weighted_restricted_field_over_subdomains);
void d4est_solver_schwarz_apply_weights_over_subdomain(d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,d4est_solver_schwarz_subdomain_metadata_t sub_data,double *restricted_field_over_subdomain,double *weighted_restricted_field_over_subdomain);
void d4est_solver_schwarz_convert_restricted_subdomain_field_to_global_nodal_field(p4est_t *p4est,d4est_mesh_data_t *d4est_factors,d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,double *field_on_subdomain,double *field,int mpirank,int sub_id,int local_nodes,d4est_field_init_type_t is_it_zeroed);
void d4est_solver_schwarz_apply_restrict_transpose_to_restricted_field_over_subdomains(d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,double *restricted_field_over_subdomains,double *transpose_restricted_field_over_subdomains);
void d4est_solver_schwarz_apply_restrict_transpose_to_restricted_field_over_subdomain(d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,double *restricted_field_over_subdomain,double *transpose_restricted_field_over_subdomain,int subdomain);
void d4est_solver_schwarz_convert_field_over_subdomains_to_restricted_field_over_subdomains(d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,double *field_over_subdomains,double *restricted_field_over_subdomains);
void d4est_solver_schwarz_convert_field_over_subdomain_to_restricted_field_over_subdomain(d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,double *field_over_subdomains,double *restricted_field_over_subdomains,int subdomain);
void d4est_solver_schwarz_convert_nodal_field_to_restricted_field_over_subdomains(p4est_t *p4est,d4est_operators_t *d4est_ops,d4est_mesh_data_t *d4est_factors,d4est_ghost_t *ghost,d4est_ghost_data_t *ghost_data,d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,double *field,int ghost_data_num_of_field,double *restricted_field_over_subdomains);
void d4est_solver_schwarz_convert_nodal_field_to_restricted_field_over_subdomain(p4est_t *p4est,d4est_operators_t *d4est_ops,d4est_mesh_data_t *d4est_factors,d4est_ghost_t *ghost,d4est_ghost_data_t *ghost_data,d4est_solver_schwarz_metadata_t *schwarz_data,d4est_solver_schwarz_operators_t *schwarz_ops,double *field,int ghost_data_num_of_field,double *restricted_field_over_subdomain,int subdomain);
int d4est_solver_schwarz_is_element_in_subdomain(d4est_solver_schwarz_subdomain_metadata_t *sub_data,int element_mpirank,int element_tree,int element_tree_quadid);

#endif
