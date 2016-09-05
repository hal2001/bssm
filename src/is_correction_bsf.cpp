#include "bssm.h"
#include "ngssm.h"
#include "ng_bsm.h"
#include "svm.h"

// [[Rcpp::plugins(openmp)]]
template <typename T>
void is_correction_bsf(T mod, const arma::mat& theta, const arma::vec& ll_store, const arma::uvec& counts, unsigned int nsim_states,
  unsigned int n_threads, arma::uvec seeds, arma::vec& weights_store, arma::cube& alpha_store) {
  
  unsigned n_iter = theta.n_cols;
  arma::uvec cum_counts = arma::cumsum(counts);
#pragma omp parallel num_threads(n_threads) default(none) \
  shared(n_iter, nsim_states, theta, ll_store,            \
    weights_store, alpha_store, seeds, counts, cum_counts) firstprivate(mod)
    {
#ifdef _OPENMP
      if (seeds.n_elem == 1) {
        mod.engine = std::mt19937(seeds(0));
      } else {
        mod.engine = std::mt19937(seeds(omp_get_thread_num()));
      }
#endif
#pragma omp for schedule(static)
      for (unsigned int i = 0; i < n_iter; i++) {
        
        arma::vec theta_i = theta.col(i);
        mod.update_model(theta_i);
        
        arma::cube alpha(mod.m, mod.n, nsim_states * counts(i));
        arma::mat V(nsim_states * counts(i), mod.n);
        arma::umat ind(nsim_states * counts(i), mod.n - 1);
        double logU = mod.particle_filter(nsim_states * counts(i), alpha, V, ind);
        weights_store(i) = exp(logU - ll_store(i)); 
        alpha_store.slice(i) = mod.backward_simulate(alpha, V, ind);
        
      }
    }
  
}

template void is_correction_bsf<ngssm>(ngssm mod, const arma::mat& theta, const arma::vec& ll_store, const arma::uvec& counts,
  unsigned int nsim_states, unsigned int n_threads, arma::uvec seeds, arma::vec& weights_store,
  arma::cube& alpha_store);
template void is_correction_bsf<ng_bsm>(ng_bsm mod, const arma::mat& theta, const arma::vec& ll_store, const arma::uvec& counts,
  unsigned int nsim_states, unsigned int n_threads, arma::uvec seeds, arma::vec& weights_store,
  arma::cube& alpha_store);
template void is_correction_bsf<svm>(svm mod, const arma::mat& theta, const arma::vec& ll_store, const arma::uvec& counts,
  unsigned int nsim_states, unsigned int n_threads, arma::uvec seeds, arma::vec& weights_store,
  arma::cube& alpha_store);
