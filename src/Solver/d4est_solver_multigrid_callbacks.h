#ifndef D4EST_SOLVER_MULTIGRID_CALLBACKS_H
#define D4EST_SOLVER_MULTIGRID_CALLBACKS_H 

static void
d4est_solver_multigrid_p_coarsen
(
 p4est_iter_volume_info_t * info,
 void *user_data
){
  d4est_solver_multigrid_t* mg_data = (d4est_solver_multigrid_t*) info->p4est->user_pointer;
  d4est_solver_multigrid_refine_data_t* coarse_grid_refinement = (d4est_solver_multigrid_refine_data_t*) mg_data->coarse_grid_refinement;
  p4est_quadrant_t *q = info->quad;
  d4est_element_data_t* ed = (d4est_element_data_t *) q->p.user_data;
  int* stride = &mg_data->stride;
  coarse_grid_refinement[*stride].hrefine = 0;
  coarse_grid_refinement[*stride].degh[0] = ed->deg;
  coarse_grid_refinement[*stride].degH = d4est_util_max_int(ed->deg - 1,1);
  ed->deg = coarse_grid_refinement[*stride].degH;
  (*stride)++;
}

static int
d4est_solver_multigrid_coarsen (
                   p4est_t * p4est,
                   p4est_topidx_t which_tree,
                   p4est_quadrant_t * children[]
                  )
{
  d4est_solver_multigrid_t* mg_data = (d4est_solver_multigrid_t*) p4est->user_pointer;
  d4est_solver_multigrid_refine_data_t* coarse_grid_refinement = (d4est_solver_multigrid_refine_data_t*) mg_data->coarse_grid_refinement;
  int* stride = &mg_data->stride;

  if (children[1] != NULL){
    coarse_grid_refinement[*stride].hrefine = 1;
    int i;

    for (i = 0; i < (P4EST_CHILDREN); i++){
      coarse_grid_refinement[*stride].degh[i] = ((d4est_element_data_t*)children[i]->p.user_data)->deg;
    }
  }
  else{
    coarse_grid_refinement[*stride].hrefine = 0;
    coarse_grid_refinement[*stride].degh[0] = ((d4est_element_data_t*)children[0]->p.user_data)->deg;
    coarse_grid_refinement[*stride].degH = ((d4est_element_data_t*)children[0]->p.user_data)->deg;
  }
  (*stride)++;
  return 1;
}

static void
d4est_solver_multigrid_coarsen_init
(
 p4est_t *p4est,
 p4est_topidx_t which_tree,
 p4est_quadrant_t *quadrant
)
{

  d4est_solver_multigrid_t* mg_data = (d4est_solver_multigrid_t*) p4est->user_pointer;
  d4est_solver_multigrid_refine_data_t* coarse_grid_refinement = mg_data->coarse_grid_refinement;
  
  int* stride = &mg_data->stride;
  int children = (coarse_grid_refinement[*stride - 1].hrefine == 1) ? (P4EST_CHILDREN) : 1;
  int min_deg = coarse_grid_refinement[*stride - 1].degh[0];

  int i;
  if (children == (P4EST_CHILDREN))
    for(i = 0; i < children; i++){
      if (coarse_grid_refinement[*stride - 1].degh[i] < min_deg)
        min_deg = coarse_grid_refinement[*stride - 1].degh[i];
    }
 
  ((d4est_element_data_t*)quadrant->p.user_data)->deg = min_deg;
  coarse_grid_refinement[*stride - 1].degH = min_deg;
}

static void
d4est_solver_multigrid_store_balance_changes
(
 p4est_t * p4est,
 p4est_topidx_t which_tree,
 int num_outgoing,
 p4est_quadrant_t * outgoing[],
 int num_incoming,
 p4est_quadrant_t * incoming[]
)
{
  if (num_outgoing != 1)
    {
      printf("num_outgoing != 1 in store_balance_changes");
      exit(1);
    }
 
  d4est_solver_multigrid_t* mg_data = (d4est_solver_multigrid_t*) p4est->user_pointer;
  d4est_solver_multigrid_refine_data_t* coarse_grid_refinement = mg_data->coarse_grid_refinement;

  int parent_id = ((d4est_element_data_t*) outgoing[0]->p.user_data)->id;
  
  int stride = mg_data->stride;
  coarse_grid_refinement[stride + parent_id].hrefine = 2;

  int i;
  for (i = 0; i < (P4EST_CHILDREN); i++){
      d4est_element_data_t* child_data = (d4est_element_data_t*) incoming[i]->p.user_data;
      child_data->deg = coarse_grid_refinement[stride + parent_id].degh[i];
  }
}

static void
d4est_solver_multigrid_apply_restriction
(
 p4est_iter_volume_info_t* info,
 void* user_data
)
{
  d4est_solver_multigrid_t* mg_data = (d4est_solver_multigrid_t*) info->p4est->user_pointer;
  d4est_solver_multigrid_refine_data_t* coarse_grid_refinement = mg_data->coarse_grid_refinement;
  d4est_operators_t* d4est_ops = mg_data->d4est_ops;

  
  double* x = mg_data->intergrid_ptr;
  int* stride = &mg_data->stride;
  int* fine_stride = &mg_data->fine_stride;
  int* coarse_stride = &mg_data->coarse_stride;
  int* temp_stride = &mg_data->temp_stride;
  int fine_nodes = mg_data->fine_nodes;

  double* x_children = &x[*fine_stride];
  double* x_parent = &x[fine_nodes + *coarse_stride];

  if (coarse_grid_refinement[(*stride)].hrefine == 0){
    int degh = coarse_grid_refinement[(*stride)].degh[0];
    int degH = coarse_grid_refinement[(*stride)].degH;

    if(mg_data->user_callbacks != NULL && mg_data->user_callbacks->mg_restrict_user_callback != NULL){
      mg_data->user_callbacks->mg_restrict_user_callback(info, user_data, &degh, degH, 1);
    }
    
    d4est_operators_apply_p_prolong_transpose
      (
       d4est_ops,
       x_children,
       degh,
       (P4EST_DIM),
       degH,
       x_parent
      );

    (*stride)++;
    (*fine_stride)+=d4est_lgl_get_nodes((P4EST_DIM), degh);
    (*coarse_stride)+=d4est_lgl_get_nodes((P4EST_DIM), degH);
  }
  
  else if (coarse_grid_refinement[(*stride)].hrefine == 1){
    int* degh = &(coarse_grid_refinement[(*stride)].degh[0]);
    int degH = coarse_grid_refinement[(*stride)].degH;

    if(mg_data->user_callbacks != NULL && mg_data->user_callbacks->mg_restrict_user_callback != NULL){
      mg_data->user_callbacks->mg_restrict_user_callback(info, user_data, degh, degH, (P4EST_CHILDREN));
    }
    
    d4est_operators_apply_hp_prolong_transpose
      (
       d4est_ops,
       x_children,
       degh,
       (P4EST_DIM),
       degH,
       x_parent
      );

    (*stride)++;
    for (int i = 0; i < (P4EST_CHILDREN); i++){
      (*fine_stride) += d4est_lgl_get_nodes( (P4EST_DIM) , degh[i] );
    }
    (*coarse_stride) += d4est_lgl_get_nodes( (P4EST_DIM) , degH);
  }
  else {
    int deg = coarse_grid_refinement[(*stride)].degh[*temp_stride];

    if(mg_data->user_callbacks != NULL && mg_data->user_callbacks->mg_restrict_user_callback != NULL){
      mg_data->user_callbacks->mg_restrict_user_callback(info, user_data, &deg, deg, 1);
    }
    
    int nodes = d4est_lgl_get_nodes( (P4EST_DIM) , deg );
    d4est_util_copy_1st_to_2nd(x_children,
                           x_parent,
                           nodes
                          );
    if (*temp_stride == (P4EST_CHILDREN)-1){
      (*fine_stride) += nodes;
      (*coarse_stride) += nodes;
      (*stride)++;
      *temp_stride = 0;
    }
    else{
      (*fine_stride) += nodes;
      (*coarse_stride) += nodes;
      *temp_stride = *temp_stride + 1;
    }
  }
}

static void
d4est_solver_multigrid_refine_and_apply_prolongation_replace
(
 p4est_t *p4est,
 p4est_topidx_t which_tree,
 int num_outgoing,
 p4est_quadrant_t *outgoing[],
 int num_incoming,
 p4est_quadrant_t *incoming[]
)
{
  /* printf("d4est_solver_multigrid refine and apply prolongation replace function\n"); */
  if (num_outgoing != 1)
    {
      printf("num_outgoing != 1 in store_balance_changes");
      exit(1);
    }
  
  d4est_solver_multigrid_t* mg_data = (d4est_solver_multigrid_t*) p4est->user_pointer;
  d4est_solver_multigrid_refine_data_t* coarse_grid_refinement = mg_data->coarse_grid_refinement;

  /* the refine function increments the stride, so the stride of
     the parent element we want is -1 */
  int stride = mg_data->stride - 1;
  
  int i;
  for (i = 0; i < (P4EST_CHILDREN); i++){
    d4est_element_data_t* child_data = (d4est_element_data_t*) incoming[i]->p.user_data;
    child_data->deg = coarse_grid_refinement[stride].degh[i];
  }
}

static int
d4est_solver_multigrid_refine_and_apply_prolongation
(
 p4est_t * p4est,
 p4est_topidx_t which_tree,
 p4est_quadrant_t * quadrant
)
{  
  d4est_solver_multigrid_t* mg_data = (d4est_solver_multigrid_t*) p4est->user_pointer;
  d4est_solver_multigrid_refine_data_t* coarse_grid_refinement = mg_data->coarse_grid_refinement;
  d4est_operators_t* d4est_ops = mg_data->d4est_ops;

  if(mg_data->user_callbacks != NULL && mg_data->user_callbacks->mg_prolong_user_callback != NULL){
    mg_data->user_callbacks->mg_prolong_user_callback(p4est,which_tree,quadrant);
  }
    
  double* x = mg_data->intergrid_ptr;

  int* stride = &mg_data->stride;
  int* fine_stride = &mg_data->fine_stride;
  int* coarse_stride = &mg_data->coarse_stride;
  int* temp_stride = &mg_data->temp_stride;
  int fine_nodes = mg_data->fine_nodes;

  double* x_children = &x[*fine_stride];
  double* x_parent = &x[fine_nodes + *coarse_stride];
  
  if (coarse_grid_refinement[(*stride)].hrefine == 0){
    int degh = coarse_grid_refinement[(*stride)].degh[0];
    int degH = coarse_grid_refinement[(*stride)].degH;
    d4est_operators_apply_p_prolong
      (
       d4est_ops,
       x_parent,
       degH,
       (P4EST_DIM),
       degh,
       x_children
      );
    
    (*stride)++;
    (*fine_stride)+=d4est_lgl_get_nodes((P4EST_DIM), degh);
    (*coarse_stride)+=d4est_lgl_get_nodes((P4EST_DIM), degH);
    
    return 0;
  }
  else if (coarse_grid_refinement[(*stride)].hrefine == 1){
 
   int* degh = &(coarse_grid_refinement[(*stride)].degh[0]);
   int degH = coarse_grid_refinement[(*stride)].degH;

    d4est_operators_apply_hp_prolong
      (
       d4est_ops,
       x_parent,
       degH,
       (P4EST_DIM),
       degh,
       x_children
      );   
    
    (*stride)++;
    int i;
    for (i = 0; i < (P4EST_CHILDREN); i++)
      (*fine_stride) += d4est_lgl_get_nodes( (P4EST_DIM) , degh[i] );
    (*coarse_stride) += d4est_lgl_get_nodes( (P4EST_DIM) , degH);

    return 1;
  }
  else {
    int deg = coarse_grid_refinement[(*stride)].degh[*temp_stride];
    int nodes = d4est_lgl_get_nodes( (P4EST_DIM) , deg );
    
    d4est_util_copy_1st_to_2nd(x_parent,
                           x_children,
                           nodes
                          );    
    if (*temp_stride == (P4EST_CHILDREN)-1){
      (*fine_stride) += nodes;
      (*coarse_stride) += nodes;
      (*stride)++;
      *temp_stride = 0;
    }
    else{
      (*fine_stride) += nodes;
      (*coarse_stride) += nodes;
      *temp_stride = *temp_stride + 1;
    }

    return 0;
  }
}

#endif
