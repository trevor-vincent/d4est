#ifndef D4EST_ESTIMATOR_STATS_H
#define D4EST_ESTIMATOR_STATS_H 

#include <pXest.h>
#include <d4est_element_data.h>

typedef struct {

  int mpirank;
  int tree;

  int estimator_sample_size;
  double estimator_total;
  double estimator_mean;
  double estimator_max;
  double estimator_at_percentile;
  int percentile;
    
} d4est_estimator_stats_t;


/* This file was automatically generated.  Do not edit! */
void d4est_estimator_stats_print(d4est_estimator_stats_t *stats);
double d4est_estimator_stats_get_global_percentile_parallel(p4est_t *p4est,int percentile,double *estimator,int local_size_temp);
void d4est_estimator_stats_compute_per_region(p4est_t *p4est,double *estimator,d4est_estimator_stats_t **stats,int number_of_regions,int(*in_region)(int,d4est_element_data_t *),int compute_percentile,int compute_mean,int compute_max);
void d4est_estimator_stats_compute_aux(p4est_t *p4est,d4est_estimator_stats_t *stats,double *eta2,int local_size_temp,double total_eta2,int compute_percentile,int compute_mean,int compute_max);
void d4est_estimator_stats_compute(p4est_t *p4est,double *estimator,d4est_estimator_stats_t *stats,int compute_percentile,int compute_mean,int compute_max);

#endif
