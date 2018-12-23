#include <d4est_solver_multigrid_logger_residual.h>

void
d4est_solver_multigrid_logger_residual_destroy
(
 d4est_solver_multigrid_logger_t* logger
)
{
  P4EST_FREE(logger);
}

static
void
d4est_solver_multigrid_logger_residual_update
(
 p4est_t* p4est,
 int level,
 d4est_elliptic_data_t* vecs
)
{
  d4est_solver_multigrid_t* mg_data = p4est->user_pointer;
  double r2_i_global = mg_data->vcycle_r2_global_current;
  double old_r2_i_global = mg_data->vcycle_r2_global_last;
  int v = mg_data->vcycle_num_finished;
  
  if((mg_data->mg_state == START || mg_data->mg_state == POST_RESIDUAL_UPDATE) && (p4est->mpirank == 0)){
    zlog_category_t *c_default = zlog_get_category("d4est_solver_multigrid");
    zlog_info(c_default, "%d %.30f %f", v, sqrt(r2_i_global), sqrt(r2_i_global/old_r2_i_global));
  }
  else {
    return;
  }
}

d4est_solver_multigrid_logger_t*
d4est_solver_multigrid_logger_residual_init
(
)
{
  d4est_solver_multigrid_logger_t* logger = P4EST_ALLOC(d4est_solver_multigrid_logger_t, 1);
  logger->update = d4est_solver_multigrid_logger_residual_update;
  logger->user = NULL;
  return logger;
}
