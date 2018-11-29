#include <sc_reduce.h>
#include <pXest.h>
#include <d4est_linalg.h>
#include <d4est_util.h>
#include <d4est_mesh.h>
#include <d4est_operators.h>
#include <d4est_solver_multigrid.h>
#include <d4est_solver_cg_eigs.h>
#include <d4est_amr.h>
#include <ini.h>
#include <d4est_util.h>
#include <d4est_solver_multigrid_callbacks.h>
#include <d4est_solver_multigrid_element_data_updater.h>
#include <d4est_solver_multigrid_smoother_cheby.h>
#include <d4est_solver_multigrid_smoother_schwarz.h>
#include <d4est_solver_multigrid_logger_residual.h>
#include <d4est_solver_multigrid_profiler_basic.h>
#include <d4est_solver_multigrid_mesh_analyzer.h>
#include <d4est_power_method.h>
#include <d4est_solver_multigrid_smoother_krylov_petsc.h>
#include <d4est_solver_multigrid_bottom_solver_cg.h>
#include <d4est_solver_multigrid_bottom_solver_cheby.h>
#include <d4est_solver_multigrid_bottom_solver_krylov_petsc.h>
#include <zlog.h>
#include <time.h>


int
d4est_solver_multigrid_get_p_coarsen_levels
(
 p4est_t* p4est
)
{
  int local_max_degree = d4est_mesh_get_local_max_degree(p4est);
  int global_max_degree = -1;
  
  if(p4est->mpisize > 1){
    sc_allreduce
      (
       &local_max_degree,
       &global_max_degree,
       1,
       sc_MPI_INT,
       sc_MPI_MAX,
       sc_MPI_COMM_WORLD
      );
  }
  else {
    global_max_degree = local_max_degree;
  }

  return global_max_degree - 1;
}


/* int */
/* d4est_solver_multigrid_get_h_coarsen_levels_post_initial */
/* ( */
/*  p4est_t* p4est, */
/*  int num_of_levels_last, */
/*  int has_there_been_h_refinements */
/* ) */
/* { */
/*   return num_of_levels_last + has_there_been_h_refinements; */
/* } */


/* int */
/* d4est_solver_multigrid_get_h_coarsen_levels_initial */
/* ( */
/*  p4est_t* p4est, */
/*  d4est_mesh_initial_extents_t* initial_extents */
/* ) */
/* { */
/*   zlog_category_t *c_default = zlog_get_category("d4est_solver_multigrid"); */
  
/*   int min_level = initial_extents->min_level; */
/*   int mpi_size = p4est->mpisize; */
/*   int num_trees = p4est->connectivity->num_trees; */
/*   int global_guess = num_trees*d4est_util_int_pow_int((P4EST_CHILDREN),min_level); */

/*   if (mpi_size == 1){ */
/*     return min_level + 1; */
/*   } */
  
/*   if (p4est->global_num_quadrants != global_guess){ */
/*     zlog_error(c_default, "p4est->global_num_quadrants != global_guess"); */
/*     zlog_error(c_default, "p4est->global_num_quadrants = %d", (int)p4est->global_num_quadrants); */
/*     zlog_error(c_default, "global_guess = %d", global_guess); */
/*     D4EST_ABORT(""); */
/*   } */
/*   if (mpi_size % num_trees != 0){ */
/*     zlog_error(c_default, "mpi_size = %d", mpi_size); */
/*     zlog_error(c_default, "num_trees = %d", num_trees); */
/*     zlog_error(c_default, "mpi_size mod num_trees != 0"); */
/*     D4EST_ABORT(""); */
/*   }   */
/*   if (((mpi_size/num_trees) % (P4EST_CHILDREN) != 0) && ((mpi_size/num_trees) != 1)){ */
/*     zlog_error(c_default, "(mpi_size/num_trees) = %d", (mpi_size/num_trees)); */
/*     zlog_error(c_default, "(P4EST_CHILDREN) = %d", (P4EST_CHILDREN)); */
/*     zlog_error(c_default, "(mpi_size/num_trees) mod (P4EST_CHILDREN) != 0"); */
/*   } */
/*   int power = d4est_util_is_power_of(mpi_size/num_trees, (P4EST_CHILDREN)); */

/*   if (power == -1){ */
/*     D4EST_ABORT("power == -1");     */
/*   } */

/*   if (p4est->mpirank == 0){ */
/*     zlog_info(c_default, "number of levels = %d\n", min_level - power + 1); */
/*   } */
/*   return min_level - power + 1; */
/* } */

/** 
 * Calculates the min level and max level of the d4est_solver_multigrid
 * hiearchy across cores. I would not trust the minimum except
 * for the case of one processor.
 * 
 * @param p4est 
 * @param min_level 
 * @param max_level 
 */
int
d4est_solver_multigrid_get_h_coarsen_levels
(
 p4est_t* p4est
)
{
  zlog_category_t* c_default = zlog_get_category("d4est_solver_multigrid");
  int level;
  int min = P4EST_MAXLEVEL;
  int max = -1;
  
  for (p4est_topidx_t tt = p4est->first_local_tree;
       tt <= p4est->last_local_tree;
       ++tt)
    {
      p4est_tree_t* tree = p4est_tree_array_index (p4est->trees, tt);
      sc_array_t* tquadrants = &tree->quadrants;
      int Q = (p4est_locidx_t) tquadrants->elem_count;
      for (int q = 0; q < Q; ++q) {
        p4est_quadrant_t* quad = p4est_quadrant_array_index (tquadrants, q);
        level = quad->level;
        min = (level < min) ? level : min;
        max = (level > max) ? level : max;
      }
    }

  int local_reduce [2];
  int global_reduce [2];
  local_reduce[0] = -1*min;
  local_reduce[1] = max;

  if(p4est->mpisize > 1){
    sc_allreduce
      (
       &local_reduce[0],
       &global_reduce[0],
       2,
       sc_MPI_INT,
       sc_MPI_MAX,
       sc_MPI_COMM_WORLD
      );
  }
  else {
    global_reduce[0] = local_reduce[0];
    global_reduce[1] = local_reduce[1];
  }

  
  int min_level = global_reduce[0]*-1;
  if(p4est->mpisize == 1){
    min_level = 0;
  }
  int max_level = global_reduce[1];

  /* return max_level + 1; */
  int num_trees = p4est->connectivity->num_trees;
  int mpi_size = p4est->mpisize;

  if (mpi_size > num_trees && mpi_size % num_trees != 0){
    if (p4est->mpirank == 0){
      zlog_error(c_default, "Please set starting conditinos such that mpi_size > num_trees && mpi_size mod num_trees == 0 OR mpi_size < num_trees && (mpi_size == 1 || mpi_size mod P4EST_CHILDREN == 0). This will give a proper multigrid hiearchy");
      zlog_error(c_default, "P4EST_CHILDREN = %d", (P4EST_CHILDREN));
      zlog_error(c_default, "mpi_size = %d", mpi_size);
      zlog_error(c_default, "num_trees = %d", num_trees);
    }
    D4EST_ABORT("");
  }

  int power = (mpi_size > num_trees) ? d4est_util_is_power_of(mpi_size/num_trees, (P4EST_CHILDREN)) : d4est_util_is_power_of(mpi_size, (P4EST_CHILDREN));
  
  if (p4est->mpirank == 0){
    zlog_info(c_default, "max_level on this amr step: %d", max_level);
    if (mpi_size != 1){
      zlog_info(c_default, "mpi_size/num_trees = %d = %d^%d\n", (mpi_size/num_trees), (P4EST_CHILDREN),power);
    }
    zlog_info(c_default, "guess for h levels = %d", max_level + 1 - power);
  }


  
  return max_level + 1 - power;
}

static void
d4est_solver_multigrid_update_components
(
 p4est_t* p4est,
 int level,
 d4est_elliptic_data_t* data
)
{

  d4est_solver_multigrid_data_t* mg_data = p4est->user_pointer;
  
  if (mg_data->elem_data_updater != NULL){
    if (mg_data->elem_data_updater->update != NULL){
      mg_data->elem_data_updater->update(p4est, level, data);
    }
  }
  else {
    D4EST_ABORT("You should set elem_data_updater in d4est_solver_multigrid");
  }  
   
  if (mg_data->user_callbacks != NULL){
    if (mg_data->user_callbacks->update != NULL){
      mg_data->user_callbacks->update(p4est, level, data);
    }
  }

  if (mg_data->logger != NULL){
    if (mg_data->logger->update != NULL){
      mg_data->logger->update(p4est, level, data);
    }
  }

  if (mg_data->profiler != NULL){
    if (mg_data->profiler->update != NULL){
      mg_data->profiler->update(p4est, level, data);
    }
  }


  if (mg_data->analyzer != NULL){
    if (mg_data->analyzer->update != NULL){
      mg_data->analyzer->update(p4est, level, data);
    }
  }
  
  if (mg_data->bottom_solver != NULL){
    if (mg_data->bottom_solver->update != NULL){
      mg_data->bottom_solver->update(p4est, level, data);
    }
  }

  if (mg_data->smoother != NULL){
    if (mg_data->smoother->update != NULL){
      mg_data->smoother->update(p4est, level, data);
    }
  }

  
}

static
int d4est_solver_multigrid_input_handler
(
 void* user,
 const char* section,
 const char* name,
 const char* value
)
{
   d4est_solver_multigrid_data_t* mg_data = (( d4est_solver_multigrid_data_t*)user);
  
  if (d4est_util_match_couple(section,"multigrid",name,"vcycle_imax")) {
    D4EST_ASSERT(mg_data->vcycle_imax == -1);
    mg_data->vcycle_imax = atoi(value);
  }
  else if (d4est_util_match_couple(section,"multigrid",name,"use_p_coarsen")) {
    D4EST_ASSERT(mg_data->use_p_coarsen == 0);
    mg_data->use_p_coarsen = atoi(value);
    D4EST_ABORT("use_p_coarsen not working yet");
  }
  else if (d4est_util_match_couple(section,"multigrid",name,"use_power_method_debug")) {
    D4EST_ASSERT(mg_data->use_power_method_debug == 0);
    mg_data->use_power_method_debug = atoi(value);
  }
  else if (d4est_util_match_couple(section,"multigrid",name,"print_state_info")) {
    D4EST_ASSERT(mg_data->print_state_info == 0);
    mg_data->print_state_info = atoi(value);
  }
  else if (d4est_util_match_couple(section,"multigrid",name,"print_level_info")) {
    D4EST_ASSERT(mg_data->print_level_info == 0);
    mg_data->print_level_info = atoi(value);
  }
  else if (d4est_util_match_couple(section,"multigrid",name,"power_atol")) {
    D4EST_ASSERT(mg_data->power_atol == -1);
    mg_data->power_atol = atof(value);
  }
  else if (d4est_util_match_couple(section,"multigrid",name,"power_rtol")) {
    D4EST_ASSERT(mg_data->power_rtol == -1);
    mg_data->power_rtol = atof(value);
  }
  else if (d4est_util_match_couple(section,"multigrid",name,"power_imin")) {
    D4EST_ASSERT(mg_data->power_imin == -1);
    mg_data->power_imin = atoi(value);
  }
  else if (d4est_util_match_couple(section,"multigrid",name,"power_imax")) {
    D4EST_ASSERT(mg_data->power_imax == -1);
    mg_data->power_imax = atoi(value);
  } 
  else if (d4est_util_match_couple(section,"multigrid",name,"use_profiler")) {
    D4EST_ASSERT(mg_data->use_profiler == 0);
    mg_data->use_profiler = atoi(value);
  }
  else if (d4est_util_match_couple(section,"multigrid",name,"use_analyzer")) {
    D4EST_ASSERT(mg_data->use_analyzer == 0);
    mg_data->use_analyzer = atoi(value);
  }
  else if (d4est_util_match_couple(section,"multigrid",name,"vcycle_rtol")) {
    D4EST_ASSERT(mg_data->vcycle_rtol == -1);
    mg_data->vcycle_rtol = atof(value);
  }
  else if (d4est_util_match_couple(section,"multigrid",name,"vcycle_atol")) {
    D4EST_ASSERT(mg_data->vcycle_atol == -1);
    mg_data->vcycle_atol = atof(value);
  }
  else if (d4est_util_match_couple(section,"multigrid",name,"num_of_levels")) {
    D4EST_ASSERT(mg_data->num_of_levels == -1);
    mg_data->num_of_levels = atof(value);
  }
  else if (d4est_util_match_couple(section,"multigrid",name,"smoother_name")) {
    D4EST_ASSERT(mg_data->smoother_name[0] == '*');
    snprintf (mg_data->smoother_name, sizeof(mg_data->smoother_name), "%s", value);
  }
  else if (d4est_util_match_couple(section,"multigrid",name,"bottom_solver_name")) {
    D4EST_ASSERT(mg_data->bottom_solver_name[0] == '*');
    snprintf (mg_data->bottom_solver_name, sizeof(mg_data->bottom_solver_name), "%s", value);
  }
  else {
    return 0;  /* unknown section/name, error */
  }
  return 1;
}


void
d4est_solver_multigrid_set_smoother(p4est_t* p4est, const char* input_file,  d4est_solver_multigrid_data_t* mg_data){

  if(d4est_util_match(mg_data->smoother_name, "mg_smoother_krylov_petsc")){
    mg_data->smoother = d4est_solver_multigrid_smoother_krylov_petsc_init(p4est, input_file);
  }
  else if(d4est_util_match(mg_data->smoother_name, "mg_smoother_cheby")){
    mg_data->smoother = d4est_solver_multigrid_smoother_cheby_init
                        (
                         p4est,
                         mg_data->num_of_levels,
                         input_file
                        );
  }
  else if(d4est_util_match(mg_data->smoother_name, "mg_smoother_schwarz")){
    d4est_solver_multigrid_element_data_updater_t* updater = mg_data->elem_data_updater;
    mg_data->smoother = d4est_solver_multigrid_smoother_schwarz_init
                        (
                         p4est,
                         mg_data->num_of_levels,
                         mg_data->d4est_ops,
                         updater->current_d4est_ghost,
                         updater->current_d4est_factors,
                         input_file
                        );
  }
  else {
    zlog_category_t *c_default = zlog_get_category("d4est_solver_multigrid");
    zlog_error(c_default, "You chose the %s smoother.", mg_data->smoother_name);
    D4EST_ABORT("This smoother is not supported.");
  }
}


void
d4est_solver_multigrid_destroy_smoother( d4est_solver_multigrid_data_t* mg_data){

  if(d4est_util_match(mg_data->smoother_name, "mg_smoother_krylov_petsc")){
    d4est_solver_multigrid_smoother_krylov_petsc_destroy(mg_data->smoother);
  }
  else if(d4est_util_match(mg_data->smoother_name, "mg_smoother_cheby")){
    d4est_solver_multigrid_smoother_cheby_destroy(mg_data->smoother);
  }
  else if(d4est_util_match(mg_data->smoother_name, "mg_smoother_schwarz")){
    d4est_solver_multigrid_smoother_schwarz_destroy(mg_data->smoother);
  }
  else {
    zlog_category_t *c_default = zlog_get_category("d4est_solver_multigrid");
    zlog_error(c_default, "You chose the %s smoother.", mg_data->smoother_name);
    D4EST_ABORT("This smoother is not supported.");
  }
}

void
d4est_solver_multigrid_set_bottom_solver(p4est_t* p4est, const char* input_file,  d4est_solver_multigrid_data_t* mg_data){

  if(d4est_util_match(mg_data->bottom_solver_name, "mg_bottom_solver_krylov_petsc")){
    mg_data->bottom_solver = d4est_solver_multigrid_bottom_solver_krylov_petsc_init
                                               (
                                                p4est,
                                                input_file
                                               );
  }
  else if(d4est_util_match(mg_data->bottom_solver_name, "mg_bottom_solver_cheby")){
    mg_data->bottom_solver = d4est_solver_multigrid_bottom_solver_cheby_init
                                               (
                                                p4est,
                                                mg_data->num_of_levels,
                                                input_file
                                               );
  }
  else if(d4est_util_match(mg_data->bottom_solver_name, "mg_bottom_solver_cg")){
    mg_data->bottom_solver = d4est_solver_multigrid_bottom_solver_cg_init
                                               (
                                                p4est,
                                                input_file
                                               );
  }
  else {
    zlog_category_t *c_default = zlog_get_category("d4est_solver_multigrid");
    zlog_error(c_default, "You chose the %s bottom_solver.", mg_data->bottom_solver_name);
    D4EST_ABORT("This bottom_solver is not supported.");
  }
}


void
d4est_solver_multigrid_destroy_bottom_solver( d4est_solver_multigrid_data_t* mg_data){

  if(d4est_util_match(mg_data->bottom_solver_name, "mg_bottom_solver_krylov_petsc")){
    d4est_solver_multigrid_bottom_solver_krylov_petsc_destroy
                                               (
                                                mg_data->bottom_solver
                                               );
  }
  else if(d4est_util_match(mg_data->bottom_solver_name, "mg_bottom_solver_cheby")){
    d4est_solver_multigrid_bottom_solver_cheby_destroy
                                               (
                                                mg_data->bottom_solver
                                               );
  }
  else if(d4est_util_match(mg_data->bottom_solver_name, "mg_bottom_solver_cg")){
    d4est_solver_multigrid_bottom_solver_cg_destroy
      (
       mg_data->bottom_solver
      );
  }
  else {
    zlog_category_t *c_default = zlog_get_category("d4est_solver_multigrid");
    zlog_error(c_default, "You chose the %s bottom_solver.", mg_data->bottom_solver_name);
    D4EST_ABORT("This bottom_solver is not supported.");
  }
}


 d4est_solver_multigrid_data_t*
d4est_solver_multigrid_data_init
(
 p4est_t* p4est,
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 d4est_ghost_t** d4est_ghost,
 d4est_ghost_data_t** d4est_ghost_data,
 d4est_mesh_data_t* d4est_factors,
 d4est_mesh_initial_extents_t* initial_extents,
 const char* input_file
)
{
  zlog_category_t* c_default = zlog_get_category("d4est_solver_multigrid");
  d4est_solver_multigrid_data_t* mg_data = P4EST_ALLOC( d4est_solver_multigrid_data_t, 1);

  int d4est_solver_multigrid_min_level, d4est_solver_multigrid_max_level;
  int num_of_h_coarsen_levels = d4est_solver_multigrid_get_h_coarsen_levels(p4est);  

  D4EST_ASSERT(num_of_h_coarsen_levels >= 1);
  
  mg_data->d4est_ops = d4est_ops;
  mg_data->d4est_geom = d4est_geom;
  mg_data->d4est_quad = d4est_quad;
  mg_data->num_of_levels = num_of_h_coarsen_levels;
  mg_data->vcycle_atol = -1;
  mg_data->vcycle_rtol = -1;
  mg_data->vcycle_imax = -1;
  mg_data->use_profiler = 0;
  mg_data->use_analyzer = 0;
  mg_data->power_atol = -1;
  mg_data->print_state_info = 0;
  mg_data->print_level_info = 0;
  mg_data->power_rtol = -1;
  mg_data->power_imax = -1;
  mg_data->power_imin = -1;
  mg_data->linear_operator_updates = 0;
  mg_data->use_power_method_debug = 0;
  mg_data->num_of_p_coarsen_levels = 0;
  mg_data->use_p_coarsen = 0;
  mg_data->bottom_solver_name[0] = '*';
  mg_data->smoother_name[0] = '*';
  mg_data->Ae_at0 = NULL;
  mg_data->err_at0 = NULL;
  mg_data->rres_at0 = NULL;
  mg_data->res_at0 = NULL;
  mg_data->elements_on_level_of_multigrid = NULL;
  mg_data->elements_on_level_of_surrogate_multigrid = NULL;
  mg_data->nodes_on_level_of_multigrid = NULL;
  mg_data->nodes_on_level_of_surrogate_multigrid = NULL;
  
  if (ini_parse(input_file, d4est_solver_multigrid_input_handler, mg_data) < 0) {
    D4EST_ABORT("Can't load input file");
  }
 
  if (mg_data->use_p_coarsen == 1){
    mg_data->num_of_p_coarsen_levels = d4est_solver_multigrid_get_p_coarsen_levels(p4est);
    mg_data->num_of_levels += mg_data->num_of_p_coarsen_levels;
  }
  
  D4EST_CHECK_INPUT("multigrid", mg_data->vcycle_atol, -1);
  D4EST_CHECK_INPUT("multigrid", mg_data->vcycle_rtol, -1);
  D4EST_CHECK_INPUT("multigrid", mg_data->vcycle_imax, -1);
  D4EST_CHECK_INPUT("multigrid", mg_data->smoother_name[0], '*');
  D4EST_CHECK_INPUT("multigrid", mg_data->bottom_solver_name[0], '*');

  mg_data->coarse_grid_refinement = P4EST_ALLOC
    (
     d4est_solver_multigrid_refine_data_t,
     (mg_data->num_of_levels - 1) * p4est->local_num_quadrants
    );

  mg_data->elem_data_updater = d4est_solver_multigrid_element_data_updater_init
                               (
                                mg_data->num_of_levels,
                                *d4est_ghost,
                                *d4est_ghost_data,
                                d4est_factors,
                                d4est_mesh_set_quadratures_after_amr,
                                initial_extents
                               ); 
  
  d4est_solver_multigrid_set_smoother(p4est, input_file, mg_data);
  d4est_solver_multigrid_set_bottom_solver(p4est, input_file, mg_data);
  
  if(p4est->mpirank == 0){
    zlog_category_t *c_default = zlog_get_category("d4est_solver_multigrid");
    zlog_debug(c_default, "D4est_Solver_Multigrid h_levels p_levels total_levels = [%d,%d,%d]", mg_data->num_of_levels - mg_data->num_of_p_coarsen_levels, mg_data->num_of_p_coarsen_levels, mg_data->num_of_levels);
    zlog_debug(c_default, "d4est_solver_multigrid parameters");
    zlog_debug(c_default, "vcycle imax = %d", mg_data->vcycle_imax);
    zlog_debug(c_default, "vcycle rtol = %.25f", mg_data->vcycle_rtol);
    zlog_debug(c_default, "vcycle atol = %.25f", mg_data->vcycle_atol);
    zlog_debug(c_default, "smoother = %s", mg_data->smoother_name);
    zlog_debug(c_default, "bottom solver = %s", mg_data->bottom_solver_name);
    zlog_debug(c_default, "use_profiler = %d", mg_data->use_profiler);
    zlog_debug(c_default, "use_analyzer = %d", mg_data->use_analyzer);
    zlog_debug(c_default, "use_power_method_debug = %d", mg_data->use_power_method_debug);
  }

  mg_data->logger = d4est_solver_multigrid_logger_residual_init();
  if (mg_data->use_profiler == 1){
    mg_data->profiler = d4est_solver_multigrid_profiler_basic_init();
  }
  else {
    mg_data->profiler = NULL;
  }

  if (mg_data->use_analyzer == 1){
    mg_data->analyzer = d4est_solver_multigrid_mesh_analyzer_init();
  }
  else {
    mg_data->analyzer = NULL;
  }
  
  mg_data->input_file = malloc(strlen(input_file) + 1);
  strcpy(mg_data->input_file, input_file);
  
 
  mg_data->user_callbacks = NULL;

  return mg_data;
}

void
d4est_solver_multigrid_set_user_callbacks
(
  d4est_solver_multigrid_data_t* mg_data,
 d4est_solver_multigrid_user_callbacks_t* user_callbacks
)
{
  mg_data->user_callbacks = user_callbacks;
}


void
d4est_solver_multigrid_data_destroy( d4est_solver_multigrid_data_t* mg_data)
{
  if (mg_data->profiler != NULL){
    d4est_solver_multigrid_profiler_basic_destroy(mg_data->profiler);
  }
  if (mg_data->analyzer != NULL){
    d4est_solver_multigrid_mesh_analyzer_destroy(mg_data->analyzer);
  }
  
  d4est_solver_multigrid_logger_residual_destroy(mg_data->logger);
  d4est_solver_multigrid_element_data_updater_destroy(mg_data->elem_data_updater, mg_data->num_of_levels);
  d4est_solver_multigrid_destroy_smoother(mg_data);
  d4est_solver_multigrid_destroy_bottom_solver(mg_data);
  free(mg_data->input_file);
  P4EST_FREE(mg_data->coarse_grid_refinement);
  P4EST_FREE(mg_data);
}

/* static void */
/* d4est_solver_multigrid_vcycle_test_levels */
/* ( */
/*  p4est_t* p4est, */
/*  multigrid_data_t* mg_data */
/* ) */
/* { */

/* } */


static void
d4est_solver_multigrid_vcycle
(
 p4est_t* p4est,
 d4est_elliptic_data_t* vecs,
 d4est_elliptic_eqns_t* fcns
)
{
  zlog_category_t *c_default = zlog_get_category("d4est_solver_multigrid");

   d4est_solver_multigrid_data_t* mg_data = p4est->user_pointer;
  int level;

  /* start and end levels of d4est_solver_multigrid */
  int toplevel = mg_data->num_of_levels - 1;
  int bottomlevel = 0;

  /* double* max_eigs = mg_data->max_eigs;/\* P4EST_ALLOC(double, mg_data->num_of_levels); *\/ */
  
  int* elements_on_level_of_multigrid = P4EST_ALLOC_ZERO(int, mg_data->num_of_levels);
  int* nodes_on_level_of_multigrid = P4EST_ALLOC_ZERO(int, mg_data->num_of_levels);
  int* elements_on_level_of_surrogate_multigrid = P4EST_ALLOC_ZERO(int, mg_data->num_of_levels);
  int* nodes_on_level_of_surrogate_multigrid = P4EST_ALLOC_ZERO(int, mg_data->num_of_levels);

  int total_elements_on_surrogate_multigrid = 0;
  int total_nodes_on_surrogate_multigrid = 0;
  int total_elements_on_multigrid = 0;
  int total_nodes_on_multigrid = 0;

  elements_on_level_of_multigrid[toplevel] = p4est->local_num_quadrants;
  nodes_on_level_of_multigrid[toplevel] = vecs->local_nodes;
  
  total_elements_on_multigrid += elements_on_level_of_multigrid[toplevel];
  total_nodes_on_multigrid += nodes_on_level_of_multigrid[toplevel];
  
  elements_on_level_of_surrogate_multigrid[toplevel] = p4est->local_num_quadrants;
  nodes_on_level_of_surrogate_multigrid[toplevel] = nodes_on_level_of_multigrid[toplevel];
  
  total_elements_on_surrogate_multigrid += elements_on_level_of_surrogate_multigrid[toplevel];
  total_nodes_on_surrogate_multigrid += nodes_on_level_of_surrogate_multigrid[toplevel];
 
  
  /* initialize */
  mg_data->fine_nodes = total_nodes_on_multigrid;
  mg_data->coarse_nodes = total_nodes_on_multigrid;

  double* Ae_at0 = P4EST_ALLOC(double, total_nodes_on_multigrid);
  double* err_at0 = P4EST_ALLOC_ZERO(double, total_nodes_on_multigrid);
  double* res_at0 = P4EST_ALLOC(double, total_nodes_on_multigrid);
  double* rres_at0 = P4EST_ALLOC(double, total_nodes_on_multigrid);

  /* for logging */
  mg_data->Ae_at0 = &(Ae_at0)[0];
  mg_data->err_at0 = &(err_at0)[0];
  mg_data->res_at0 = &(res_at0)[0];
  mg_data->rres_at0 = &(rres_at0)[0];
  mg_data->elements_on_level_of_multigrid = elements_on_level_of_multigrid;
  mg_data->nodes_on_level_of_multigrid = nodes_on_level_of_multigrid;
  mg_data->elements_on_level_of_surrogate_multigrid = elements_on_level_of_surrogate_multigrid;
  mg_data->nodes_on_level_of_surrogate_multigrid = nodes_on_level_of_surrogate_multigrid;


  
  double* u = vecs->u;
  
  d4est_elliptic_data_t vecs_for_smooth;
  d4est_elliptic_data_copy_ptrs(vecs, &vecs_for_smooth);
  d4est_elliptic_data_t vecs_for_bottom_solve;
  d4est_elliptic_data_copy_ptrs(vecs, &vecs_for_bottom_solve);
  
  /* initialize error to zero */
  /* d4est_util_fill_array(mg_data->err, 0., mg_data->fine_nodes); */

  /* initialize stride */
  mg_data->stride = 0;


  /**********************************************************/
  /**********************************************************/
  /******************* BEGIN GOING DOWN V *******************/
  /**********************************************************/
  /**********************************************************/
  /* DEBUG_PRINT_ARR_DBL(vecs->u, vecs->local_nodes); */

  if (mg_data->print_state_info && p4est->mpirank == 0){
  zlog_debug(c_default, "Level = %d", toplevel);
  zlog_debug(c_default, "State = %s", "PRE_V");
  zlog_debug(c_default, "elements_on_level = %d", elements_on_level_of_multigrid[toplevel]);
  zlog_debug(c_default, "elements_on_surrogate_level = %d", elements_on_level_of_surrogate_multigrid[toplevel]);
  zlog_debug(c_default, "nodes_on_level = %d", nodes_on_level_of_multigrid[toplevel]);
  zlog_debug(c_default, "nodes_on_surrogate_level = %d", nodes_on_level_of_surrogate_multigrid[toplevel]);
  }
  mg_data->mg_state = PRE_V; d4est_solver_multigrid_update_components(p4est, toplevel, NULL);

  
  int stride_to_fine_data = 0;
  for (level = toplevel; level > bottomlevel; --level){

    int fine_level = level;
    int coarse_level = level-1;

    /**********************************************************/
    /**********************************************************/
    /******************* PRE SMOOTH ***************************/
    /**********************************************************/
    /**********************************************************/
    
    /* set initial guess for error */
    d4est_util_fill_array(&err_at0[stride_to_fine_data], 0., nodes_on_level_of_multigrid[level]);//mg_data->fine_nodes);

    if (level == toplevel){
      vecs_for_smooth.Au = vecs->Au;
      vecs_for_smooth.u = vecs->u;
      vecs_for_smooth.rhs = vecs->rhs;
      vecs_for_smooth.local_nodes = vecs->local_nodes;
    }
    else{
      vecs_for_smooth.Au = &Ae_at0[stride_to_fine_data];//mg_data->Ae;//[mg_data->fine_nodes];
      vecs_for_smooth.u = &err_at0[stride_to_fine_data];//mg_data->err;//[mg_data->fine_nodes];
      vecs_for_smooth.rhs = &res_at0[stride_to_fine_data];//mg_data->res;//)[mg_data->fine_nodes];
      vecs_for_smooth.local_nodes = nodes_on_level_of_multigrid[level];
    }


      /* printf("******* LEVEL %d INFO *******\n", level); */
      /* DEBUG_PRINT_ARR_DBL_SUM(vecs_for_smooth.Au, vecs_for_smooth.local_nodes); */
      /* DEBUG_PRINT_ARR_DBL_SUM(vecs_for_smooth.u, vecs_for_smooth.local_nodes); */
      /* DEBUG_PRINT_ARR_DBL_SUM(vecs_for_smooth.rhs, vecs_for_smooth.local_nodes); */
      /* printf("******* DONE LEVEL %d INFO *******\n", level); */
    
    /**********************************************************/
    /**********************************************************/
    /******************* BEGIN SMOOTH *************************/
    /**********************************************************/
    /**********************************************************/

    if (mg_data->print_state_info && p4est->mpirank == 0){
      zlog_debug(c_default, "Level = %d", level);
      zlog_debug(c_default, "State = %s", "DOWNV_PRE_SMOOTH");
      zlog_debug(c_default, "elements_on_level = %d", elements_on_level_of_multigrid[level]);
      zlog_debug(c_default, "elements_on_surrogate_level = %d", elements_on_level_of_surrogate_multigrid[level]);
      zlog_debug(c_default, "nodes_on_level = %d", nodes_on_level_of_multigrid[level]);
      zlog_debug(c_default, "nodes_on_surrogate_level = %d", nodes_on_level_of_surrogate_multigrid[level]);
    }
    mg_data->mg_state = DOWNV_PRE_SMOOTH; d4est_solver_multigrid_update_components(p4est, level, &vecs_for_smooth);
    
    mg_data->smoother->smooth
      (
       p4est,
       &vecs_for_smooth,
       fcns,
       &rres_at0[stride_to_fine_data],
       fine_level
      );
    
    if (mg_data->print_state_info && p4est->mpirank == 0){
      zlog_debug(c_default, "Level = %d", level);
      zlog_debug(c_default, "State = %s", "DOWNV_POST_SMOOTH");
      zlog_debug(c_default, "elements_on_level = %d", elements_on_level_of_multigrid[level]);
      zlog_debug(c_default, "elements_on_surrogate_level = %d", elements_on_level_of_surrogate_multigrid[level]);
      zlog_debug(c_default, "nodes_on_level = %d", nodes_on_level_of_multigrid[level]);
      zlog_debug(c_default, "nodes_on_surrogate_level = %d", nodes_on_level_of_surrogate_multigrid[level]);
    }
    mg_data->mg_state = DOWNV_POST_SMOOTH; d4est_solver_multigrid_update_components(p4est, level, &vecs_for_smooth);


    if (mg_data->use_power_method_debug){

      d4est_solver_multigrid_element_data_updater_t* updater = mg_data->elem_data_updater;
      d4est_ghost_t* d4est_ghost = updater->current_d4est_ghost;
      d4est_ghost_data_t* d4est_ghost_data = updater->current_d4est_ghost_data;
      
      d4est_power_method
        (
         p4est,
         &vecs_for_smooth,
         fcns,
         d4est_ghost,
         d4est_ghost_data,
         mg_data->d4est_ops,
         mg_data->d4est_geom,
         mg_data->d4est_quad,
         updater->current_d4est_factors,
         mg_data->power_atol,
         mg_data->power_rtol,
         mg_data->power_imax,
         mg_data->power_imin
        );
    }
    
    /**********************************************************/
    /**********************************************************/
    /********************* END SMOOTH *************************/
    /**********************************************************/
    /**********************************************************/


    /**********************************************************/
    /**********************************************************/
    /******************* BEGIN COARSEN ************************/
    /**********************************************************/
    /**********************************************************/
  if (mg_data->print_state_info && p4est->mpirank == 0){
    zlog_debug(c_default, "Level = %d", level);
    zlog_debug(c_default, "State = %s", "DOWNV_PRE_COARSEN");
    zlog_debug(c_default, "elements_on_level = %d", elements_on_level_of_multigrid[level]);
    zlog_debug(c_default, "elements_on_surrogate_level = %d", elements_on_level_of_surrogate_multigrid[level]);
    zlog_debug(c_default, "nodes_on_level = %d", nodes_on_level_of_multigrid[level]);
    zlog_debug(c_default, "nodes_on_surrogate_level = %d", nodes_on_level_of_surrogate_multigrid[level]);
  }
    mg_data->mg_state = DOWNV_PRE_COARSEN; d4est_solver_multigrid_update_components(p4est, level, NULL);

    /* increments the stride */
    if(mg_data->use_p_coarsen == 1 && (level > toplevel - mg_data->num_of_p_coarsen_levels)){
      p4est_iterate(p4est,
                    NULL,
                    NULL,
                    d4est_solver_multigrid_p_coarsen,
                    NULL,
#if P4EST_DIM==3
                    NULL,
#endif
                    NULL);
    }
    else{  
      p4est_coarsen_ext (p4est,
                         0,
                         1,
                         d4est_solver_multigrid_coarsen,
                         d4est_solver_multigrid_coarsen_init,
                         NULL
                        );
    }
  if (mg_data->print_state_info && p4est->mpirank == 0){
    zlog_debug(c_default, "Level = %d", level);
    zlog_debug(c_default, "State = %s", "DOWNV_POST_COARSEN");
    zlog_debug(c_default, "elements_on_level = %d", elements_on_level_of_multigrid[level]);
    zlog_debug(c_default, "elements_on_surrogate_level = %d", elements_on_level_of_surrogate_multigrid[level]);
    zlog_debug(c_default, "nodes_on_level = %d", nodes_on_level_of_multigrid[level]);
    zlog_debug(c_default, "nodes_on_surrogate_level = %d", nodes_on_level_of_surrogate_multigrid[level]);
  }
    
    mg_data->mg_state = DOWNV_POST_COARSEN; d4est_solver_multigrid_update_components(p4est, level, NULL);

    /**********************************************************/
    /**********************************************************/
    /******************* END COARSEN **************************/
    /**********************************************************/
    /**********************************************************/

    /* update surrogate info */
    nodes_on_level_of_surrogate_multigrid[level-1] = d4est_mesh_get_local_nodes(p4est);
    elements_on_level_of_surrogate_multigrid[level-1] = p4est->local_num_quadrants;

    total_elements_on_surrogate_multigrid += elements_on_level_of_surrogate_multigrid[level-1];
    total_nodes_on_surrogate_multigrid += nodes_on_level_of_surrogate_multigrid[level-1];
        
    /* decrements the stride */
    mg_data->stride -= elements_on_level_of_surrogate_multigrid[level-1];

    /**********************************************************/
    /**********************************************************/
    /******************* BEGIN BALANCE ************************/
    /**********************************************************/
    /**********************************************************/
    mg_data->mg_state = DOWNV_PRE_BALANCE; d4est_solver_multigrid_update_components(p4est, level, &vecs_for_smooth);
    
    /* does not change the stride */
    p4est_balance_ext (p4est, P4EST_CONNECT_FACE, NULL, d4est_solver_multigrid_store_balance_changes);

    mg_data->mg_state = DOWNV_POST_BALANCE; d4est_solver_multigrid_update_components(p4est, level, &vecs_for_smooth);
    /**********************************************************/
    /**********************************************************/
    /******************* END BALANCE **************************/
    /**********************************************************/
    /**********************************************************/
  
    /* update coarse grid info */
    elements_on_level_of_multigrid[level-1] = p4est->local_num_quadrants;
    total_elements_on_multigrid += elements_on_level_of_multigrid[level-1];
    nodes_on_level_of_multigrid[level-1] = d4est_mesh_get_local_nodes(p4est);
    total_nodes_on_multigrid += nodes_on_level_of_multigrid[level-1];

    mg_data->fine_nodes = nodes_on_level_of_multigrid[level];
    mg_data->coarse_nodes = nodes_on_level_of_multigrid[level - 1];

    Ae_at0 = P4EST_REALLOC(Ae_at0, double, total_nodes_on_multigrid);
    err_at0 = P4EST_REALLOC(err_at0, double, total_nodes_on_multigrid);
    res_at0 = P4EST_REALLOC(res_at0, double, total_nodes_on_multigrid);
    rres_at0 = P4EST_REALLOC(rres_at0, double, total_nodes_on_multigrid);

    /* always zero these before restriction or prolongation */
    mg_data->coarse_stride = 0;
    mg_data->fine_stride = 0;
    mg_data->temp_stride = 0;
    mg_data->intergrid_ptr = &rres_at0[stride_to_fine_data];//&(mg_data->rres)[0];

  if (mg_data->print_state_info && p4est->mpirank == 0){
    zlog_debug(c_default, "Level = %d", level);
    zlog_debug(c_default, "State = %s", "DOWNV_PRE_RESTRICTION");
    zlog_debug(c_default, "elements_on_level = %d", elements_on_level_of_multigrid[level]);
    zlog_debug(c_default, "elements_on_surrogate_level = %d", elements_on_level_of_surrogate_multigrid[level]);
    zlog_debug(c_default, "nodes_on_level = %d", nodes_on_level_of_multigrid[level]);
    zlog_debug(c_default, "nodes_on_surrogate_level = %d", nodes_on_level_of_surrogate_multigrid[level]);
  }
    
    mg_data->mg_state = DOWNV_PRE_RESTRICTION; d4est_solver_multigrid_update_components(p4est, level, &vecs_for_smooth);
    
    p4est_iterate(
                  p4est,
                  NULL,
                  NULL,
                  d4est_solver_multigrid_apply_restriction,
                  NULL,
#if (P4EST_DIM)==3
                  NULL,
#endif
                  NULL
    );

  if (mg_data->print_state_info && p4est->mpirank == 0){
    zlog_debug(c_default, "Level = %d", level);
    zlog_debug(c_default, "State = %s", "DOWNV_POST_RESTRICTION");
    zlog_debug(c_default, "elements_on_level = %d", elements_on_level_of_multigrid[level]);
    zlog_debug(c_default, "elements_on_surrogate_level = %d", elements_on_level_of_surrogate_multigrid[level]);
    zlog_debug(c_default, "nodes_on_level = %d", nodes_on_level_of_multigrid[level]);
    zlog_debug(c_default, "nodes_on_surrogate_level = %d", nodes_on_level_of_surrogate_multigrid[level]);
  }
    
    mg_data->mg_state = DOWNV_POST_RESTRICTION; d4est_solver_multigrid_update_components(p4est, level, &vecs_for_smooth);

    d4est_util_copy_1st_to_2nd
      (
       &(rres_at0)[stride_to_fine_data + mg_data->fine_nodes],
       &(res_at0)[stride_to_fine_data + mg_data->fine_nodes],
       nodes_on_level_of_multigrid[coarse_level]//mg_data->coarse_nodes
      );

    stride_to_fine_data += mg_data->fine_nodes;
  }

  /**********************************************************/
  /**********************************************************/
  /******************* END GOING DOWN V *********************/
  /**********************************************************/
  /**********************************************************/

  

  /**********************************************************/
  /**********************************************************/
  /******************* BEGIN COARSE SOLVE *******************/
  /**********************************************************/
  /**********************************************************/

  /* set initial guess for error */
  d4est_util_fill_array(&err_at0[stride_to_fine_data], 0., nodes_on_level_of_multigrid[bottomlevel]);
  
  /* vecs_for_bottom_solve.Au = mg_data->Ae;//[mg_data->fine_nodes]; */
  vecs_for_bottom_solve.Au = &Ae_at0[stride_to_fine_data];//[mg_data->fine_nodes];
  vecs_for_bottom_solve.u = &err_at0[stride_to_fine_data];//mg_data->err;//[mg_data->fine_nodes];
  vecs_for_bottom_solve.rhs = &res_at0[stride_to_fine_data];//mg_data->res;//)[mg_data->fine_nodes];
  vecs_for_bottom_solve.local_nodes = nodes_on_level_of_multigrid[bottomlevel];


  mg_data->mg_state = COARSE_PRE_SOLVE; d4est_solver_multigrid_update_components(p4est, level, &vecs_for_bottom_solve);
  
  mg_data->bottom_solver->solve
    (
     p4est,
     &vecs_for_bottom_solve,
     fcns,
     /* &(mg_data->rres[mg_data->fine_nodes]), */
     &rres_at0[stride_to_fine_data]//[mg_data->fine_nodes]),
    );

  mg_data->mg_state = COARSE_POST_SOLVE; d4est_solver_multigrid_update_components(p4est, level, &vecs_for_bottom_solve);

  /* d4est_util_print_matrix(&rres_at0[stride_to_fine_data],mg_data->coarse_nodes,1,"rres coarse solve= ", 0); */
  
  /**********************************************************/
  /**********************************************************/
  /******************* END COARSE SOLVE *********************/
  /**********************************************************/
  /**********************************************************/
    
  mg_data->stride = total_elements_on_surrogate_multigrid;
  mg_data->stride -= elements_on_level_of_surrogate_multigrid[toplevel]; /* subtract fine grid */
  
  /**********************************************************/
  /**********************************************************/
  /******************* BEGIN GOING UP THE V *****************/
  /**********************************************************/
  /**********************************************************/
  for (level = bottomlevel; level < toplevel; ++level){

    int fine_level = level + 1;
    int coarse_level = level;

    mg_data->fine_nodes = nodes_on_level_of_multigrid[level+1];
    mg_data->coarse_nodes = nodes_on_level_of_multigrid[level];
    stride_to_fine_data -= mg_data->fine_nodes;
    mg_data->stride -= elements_on_level_of_surrogate_multigrid[level];
    mg_data->fine_stride = 0;
    mg_data->coarse_stride = 0;
    mg_data->temp_stride = 0;

    
    d4est_util_copy_1st_to_2nd(&err_at0[stride_to_fine_data + mg_data->fine_nodes],//&(mg_data->err)[mg_data->fine_nodes],
                           &rres_at0[stride_to_fine_data + mg_data->fine_nodes],//&(mg_data->rres)[mg_data->fine_nodes],
                           nodes_on_level_of_multigrid[coarse_level]);

    mg_data->intergrid_ptr = &rres_at0[stride_to_fine_data];/* mg_data->rres */;


    /**********************************************************/
    /**********************************************************/
    /******************* BEGIN REFINE/PROLONGATION ************/
    /**********************************************************/
    /**********************************************************/

    
    mg_data->mg_state = UPV_PRE_REFINE; d4est_solver_multigrid_update_components(p4est, level, &vecs_for_smooth);
    
    /* increments the stride */
    p4est_refine_ext(p4est,
                     0,
                     P4EST_QMAXLEVEL,
                     d4est_solver_multigrid_refine_and_apply_prolongation,
                     NULL,
                     d4est_solver_multigrid_refine_and_apply_prolongation_replace
                    );


    mg_data->mg_state = UPV_POST_REFINE; d4est_solver_multigrid_update_components(p4est, level, &vecs_for_smooth);


    /**********************************************************/
    /**********************************************************/
    /******************* END REFINE/PROLONGATION **************/
    /**********************************************************/
    /**********************************************************/


    
    /* P4EST_FREE(*ghost_data); */
    /* p4est_ghost_destroy (*ghost); */
    /* *ghost = p4est_ghost_new (p4est, P4EST_CONNECT_FACE); */
    /* *ghost_data = P4EST_ALLOC (element_data_t, */
    /*                            (*ghost)->ghosts.elem_count); */

    /* element_data_init(p4est, -1); */
    
    /* decrements the stride */
    mg_data->stride -= elements_on_level_of_surrogate_multigrid[level];

    /* If the grid is not the finiest */
    if(fine_level != toplevel){
      vecs_for_smooth.Au = &Ae_at0[stride_to_fine_data];//mg_data->Ae;
      vecs_for_smooth.u = &err_at0[stride_to_fine_data];//mg_data->err;
      vecs_for_smooth.rhs = &res_at0[stride_to_fine_data];//mg_data->res;
      vecs_for_smooth.local_nodes = mg_data->fine_nodes;
    }
    else {
      vecs_for_smooth.Au = vecs->Au;//&Ae_at0[stride_to_fine_data];//mg_data->Ae;
      vecs_for_smooth.u = vecs->u;//&err_at0[stride_to_fine_data];//mg_data->err;
      vecs_for_smooth.rhs = vecs->rhs;//&res_at0[stride_to_fine_data];//mg_data->res;
      vecs_for_smooth.local_nodes = mg_data->fine_nodes;
    }
     
    d4est_linalg_vec_axpy(1.0, &rres_at0[stride_to_fine_data], vecs_for_smooth.u, mg_data->fine_nodes);



    /**********************************************************/
    /**********************************************************/
    /******************* BEGIN SMOOTH *************************/
    /**********************************************************/
    /**********************************************************/
    

    mg_data->mg_state = UPV_PRE_SMOOTH; d4est_solver_multigrid_update_components(p4est, level, &vecs_for_smooth);
    
    mg_data->smoother->smooth
      (
       p4est,
       &vecs_for_smooth,
       fcns,
       &rres_at0[stride_to_fine_data],//mg_data->rres,
       fine_level
      );

    mg_data->mg_state = UPV_POST_SMOOTH; d4est_solver_multigrid_update_components(p4est, level, &vecs_for_smooth);



    if (mg_data->use_power_method_debug){

      d4est_solver_multigrid_element_data_updater_t* updater = mg_data->elem_data_updater;
      d4est_ghost_t* d4est_ghost = updater->current_d4est_ghost;
      d4est_ghost_data_t* d4est_ghost_data = updater->current_d4est_ghost_data;
      
      d4est_power_method
        (
         p4est,
         &vecs_for_smooth,
         fcns,
         d4est_ghost,
         d4est_ghost_data,
         mg_data->d4est_ops,
         mg_data->d4est_geom,
         mg_data->d4est_quad,
         updater->current_d4est_factors,
         mg_data->power_atol,
         mg_data->power_rtol,
         mg_data->power_imax,
         mg_data->power_imin
        );
    }
    

    
    /**********************************************************/
    /**********************************************************/
    /******************* END SMOOTH ***************************/
    /**********************************************************/
    /**********************************************************/
 
    
  }

  mg_data->mg_state = POST_V; d4est_solver_multigrid_update_components(p4est, toplevel, NULL);

  if (p4est->mpirank == 0 || mg_data->print_level_info){
    for (level = toplevel; level >= bottomlevel; --level){
      zlog_debug(c_default, "For each processor, we have Level %d, Number of elements %d, Number of nodes %d",
             level,
             elements_on_level_of_multigrid[level],
             nodes_on_level_of_multigrid[level]
            );
    }
  }

  
  mg_data->vcycle_r2_local_current = d4est_linalg_vec_dot(&rres_at0[stride_to_fine_data],
                                           &rres_at0[stride_to_fine_data],
                                           mg_data->fine_nodes);
  
  /**********************************************************/
  /**********************************************************/
  /******************* END GOING UP THE V *******************/
  /**********************************************************/
  /**********************************************************/
  
  P4EST_FREE(elements_on_level_of_multigrid);
  P4EST_FREE(nodes_on_level_of_multigrid);
  P4EST_FREE(elements_on_level_of_surrogate_multigrid);
  P4EST_FREE(nodes_on_level_of_surrogate_multigrid);
  P4EST_FREE(Ae_at0);
  P4EST_FREE(err_at0);
  P4EST_FREE(res_at0);
  P4EST_FREE(rres_at0);
}


static double
d4est_solver_multigrid_compute_residual
(
 p4est_t* p4est,
 d4est_elliptic_data_t* vecs,
 d4est_elliptic_eqns_t* fcns
){
  
   d4est_solver_multigrid_data_t* mg_data = p4est->user_pointer;
  d4est_solver_multigrid_element_data_updater_t* updater = mg_data->elem_data_updater;
  d4est_ghost_t* d4est_ghost = updater->current_d4est_ghost;
  d4est_ghost_data_t* d4est_ghost_data = updater->current_d4est_ghost_data;
  /* d4est_geometry_t* d4est_geom = mg_data->d4est_geom; */
  
  if (mg_data->mg_state == START){
    double* Au;
    double* rhs;
    int local_nodes = vecs->local_nodes;
    Au = vecs->Au;
    rhs = vecs->rhs;
    double* r = P4EST_ALLOC(double, vecs->local_nodes);

    d4est_elliptic_eqns_apply_lhs
      (
       p4est,
       d4est_ghost,
       d4est_ghost_data,
       fcns,
       vecs,
       mg_data->d4est_ops,
       mg_data->d4est_geom,
       mg_data->d4est_quad,
       updater->current_d4est_factors
      );

    
    d4est_linalg_vec_axpyeqz(-1., Au, rhs, r, local_nodes);
    double r2_0_local = d4est_linalg_vec_dot(r,r,local_nodes);
    P4EST_FREE(r);
  
    double r2_0_global = -1;
    sc_allreduce
      (
       &r2_0_local,
       &r2_0_global,
       1,
       sc_MPI_DOUBLE,
       sc_MPI_SUM,
       sc_MPI_COMM_WORLD
      );
  
    return r2_0_global;
  }
  else {
    double r2_i_local = mg_data->vcycle_r2_local_current;
    double r2_i_global;
    sc_allreduce
      (
       &r2_i_local,
       &r2_i_global,
       1,
       sc_MPI_DOUBLE,
       sc_MPI_SUM,
       sc_MPI_COMM_WORLD
      );
    return r2_i_global;
  }
}

void
d4est_solver_multigrid_solve
(
 p4est_t* p4est,
 d4est_elliptic_data_t* vecs,
 d4est_elliptic_eqns_t* fcns,
  d4est_solver_multigrid_data_t* mg_data
)
{
  zlog_category_t *c_default = zlog_get_category("d4est_solver_multigrid");
  clock_t start;
  if (p4est->mpirank == 0) {
    start = clock();
    zlog_info(c_default, "Performing d4est_solver_multigrid solve...");
  }
  if (mg_data->num_of_levels < 2){
    zlog_error(c_default, "The code sees less than two multigrid levels, cannot run multigrid, try increasing min_level in initial_mesh\n");
    D4EST_ABORT("");
  }
  
  void* tmp_ptr = p4est->user_pointer;
  p4est->user_pointer = mg_data;
  /*
   * Calculate the initial residual
   * for termination condition
   */
  
  mg_data->mg_state = START;
  mg_data->vcycle_r2_global_current = d4est_solver_multigrid_compute_residual
                                               (
                                                p4est,
                                                vecs,
                                                fcns
                                               );

  mg_data->vcycle_r2_global_last = mg_data->vcycle_r2_global_current;
  
  mg_data->vcycle_num_finished = 0;
  mg_data->vcycle_r2_global_stoptol = mg_data->vcycle_rtol*mg_data->vcycle_rtol*mg_data->vcycle_r2_global_current
                                      + mg_data->vcycle_atol*mg_data->vcycle_atol;


  d4est_solver_multigrid_update_components(p4est, -1, NULL);
  /**
   * START VCYCLING
   *
   */
  while
    (
     mg_data->vcycle_num_finished < mg_data->vcycle_imax
     &&
     mg_data->vcycle_r2_global_current > mg_data->vcycle_r2_global_stoptol
    )
    {

      d4est_solver_multigrid_vcycle(p4est, vecs, fcns);

      mg_data->vcycle_r2_global_current = d4est_solver_multigrid_compute_residual
                                          (
                                           p4est,
                                           vecs,
                                           fcns
                                          );

      mg_data->vcycle_num_finished++;
      mg_data->mg_state = POST_RESIDUAL_UPDATE;
      d4est_solver_multigrid_update_components(p4est, -1, NULL);
  
      if (sqrt(mg_data->vcycle_r2_global_current
               /mg_data->vcycle_r2_global_last) >= .99){
        break;
      }
      
      mg_data->vcycle_r2_global_last = mg_data->vcycle_r2_global_current;
    }


  mg_data->mg_state = END;
  d4est_solver_multigrid_update_components(p4est, -1, NULL);
  
  p4est->user_pointer = tmp_ptr;
  
  if (p4est->mpirank == 0) {
    double duration_seconds = ((double)(clock() - start)) / CLOCKS_PER_SEC;
    zlog_info(c_default, "solve completed in %.10f seconds.", duration_seconds);
  }
}
