#include "bssm.h"
#include "ngssm.h"
#include "ng_bsm.h"
#include "svm.h"

// [[Rcpp::plugins(openmp)]]
template <typename T>
void is_correction_summary(T mod, const arma::mat& theta, const arma::mat& y_store, const arma::mat& H_store,
  const arma::vec& ll_approx_u, const arma::uvec& counts, unsigned int nsim_states,
  unsigned int n_threads, arma::uvec seeds, 
  arma::vec& weights_store, arma::mat& alphahat, arma::cube& Vt, arma::mat& mean, arma::cube& Vmean, 
  bool const_nsim) {
  
  unsigned n_iter = theta.n_cols;
  
  arma::uvec cum_counts = arma::cumsum(counts);
  alphahat.zeros();
  Vt.zeros();
  mean.zeros();
  Vmean.zeros();
  arma::cube Valpha(mod.m, mod.m, mod.n, arma::fill::zeros);
  double cumsumw = 0;
#pragma omp parallel num_threads(n_threads) default(none)           \
  shared(ll_approx_u, n_iter, nsim_states, y_store, H_store, theta, \
    weights_store, seeds, counts, cum_counts, alphahat, Vt, Valpha, mean, Vmean, cumsumw, const_nsim) firstprivate(mod)
    {
      if (seeds.n_elem == 1) {
        mod.engine = std::mt19937(seeds(0));
      } else {
        mod.engine = std::mt19937(seeds(omp_get_thread_num()));
      }
      
#pragma omp for schedule(static)
      for (int i = 0; i < n_iter; i++) {
        
        mod.y = y_store.col(i);
        mod.H = H_store.col(i);
        mod.HH = arma::square(H_store.col(i));
        arma::vec theta_i = theta.col(i);
        mod.update_model(theta_i);
        
        unsigned int m = nsim_states;
        if (!const_nsim) {
          m *= counts(i);
        }
        arma::cube alpha = mod.sim_smoother(m, mod.distribution != 0);
        arma::vec weights = exp(mod.importance_weights(alpha) - ll_approx_u(i));
        
        weights_store(i) = arma::mean(weights);
        arma::mat alphahat_i(mod.m, mod.n);
        arma::cube Vt_i(mod.m, mod.m, mod.n);
       // weights *= counts(i);
        running_weighted_summary(alpha, alphahat_i, Vt_i, weights);
        double w = arma::mean(weights)*counts(i);
        double tmp = w + cumsumw;
        
        arma::mat diff = alphahat_i - alphahat;
        
        arma::mat alphahat_tmp = diff * w / tmp;
        arma::cube Valpha_tmp(diff.n_rows, diff.n_rows, diff.n_cols);
        for (unsigned int t = 0; t < diff.n_cols; t++) {
          Valpha_tmp.slice(t) =  w * diff.col(t) * (alphahat_i.col(t) - alphahat_tmp.col(t)).t();
        }
        arma::cube V_tmp = (Vt_i - Vt) * w / tmp;
#pragma omp critical
{
  Vt += V_tmp;
  alphahat += alphahat_tmp;
  Valpha += Valpha_tmp;
  cumsumw = tmp;
}

      }
      Vt = Vt + Valpha/cumsumw * sum(counts) / (sum(counts) - 1);
     
    }
}
template void is_correction_summary<ngssm>(ngssm mod, const arma::mat& theta, const arma::mat& y_store, const arma::mat& H_store,
  const arma::vec& ll_approx_u, const arma::uvec& counts, unsigned int nsim_states,
  unsigned int n_threads, arma::uvec seeds, arma::vec& weights_store, 
  arma::mat& alphahat, arma::cube& Vt, arma::mat& mean, arma::cube& Vmean, bool const_nsim);

template void is_correction_summary<ng_bsm>(ng_bsm mod, const arma::mat& theta, const arma::mat& y_store, const arma::mat& H_store,
  const arma::vec& ll_approx_u, const arma::uvec& counts, unsigned int nsim_states,
  unsigned int n_threads, arma::uvec seeds, arma::vec& weights_store,
  arma::mat& alphahat, arma::cube& Vt, arma::mat& mean, arma::cube& Vmean, bool const_nsim);

template void is_correction_summary<svm>(svm mod, const arma::mat& theta, const arma::mat& y_store, const arma::mat& H_store,
  const arma::vec& ll_approx_u, const arma::uvec& counts, unsigned int nsim_states,
  unsigned int n_threads, arma::uvec seeds, arma::vec& weights_store, 
  arma::mat& alphahat, arma::cube& Vt, arma::mat& mean, arma::cube& Vmean, bool const_nsim);
