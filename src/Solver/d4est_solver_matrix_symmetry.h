#ifndef D4EST_SOLVER_MATRIX_SYMMETRY_H
#define D4EST_SOLVER_MATRIX_SYMMETRY_H 

#include <pXest.h>
#include <d4est_elliptic_data.h>
#include <d4est_elliptic_eqns.h>


typedef enum {SYM_PRINT_UNEQUAL_PAIRS_AND_XYZ,SYM_PRINT_UNEQUAL_PAIRS, SYM_PRINT_MAT_AND_TRANSPOSE_AS_VECS, SYM_PRINT_MAT}  d4est_solver_test_symmetry_print_option_t;
/* This file was automatically generated.  Do not edit! */
int d4est_solver_matrix_symmetry(p4est_t *p4est,d4est_ghost_t *ghost,d4est_ghost_data_t *ghost_data,d4est_elliptic_eqns_t *fcns,d4est_elliptic_data_t *vecs,d4est_operators_t *d4est_ops,d4est_geometry_t *d4est_geom,d4est_quadrature_t *d4est_quad,d4est_mesh_data_t *d4est_factors,d4est_solver_test_symmetry_print_option_t print,double sym_eps);




#endif
