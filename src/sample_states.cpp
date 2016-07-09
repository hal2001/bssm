#include "bssm.h"
#include "gssm.h"
#include "bsm.h"

// [[Rcpp::plugins(openmp)]]
template <typename T>
arma::cube sample_states(T mod, const arma::mat& theta, const arma::uvec& counts,
  unsigned int nsim_states, unsigned int n_threads, arma::uvec seeds) {

  unsigned n_iter = theta.n_cols;
  arma::cube alpha_store(mod.m, mod.n, nsim_states * arma::accu(counts));

  arma::uvec cum_counts = arma::cumsum(counts);

#pragma omp parallel num_threads(n_threads) default(none) shared(n_iter, \
  nsim_states, theta, counts, cum_counts, alpha_store, seeds) firstprivate(mod)
  {
    if (seeds.n_elem == 1) {
      mod.engine = std::mt19937(seeds(0));
    } else {
      mod.engine = std::mt19937(seeds(omp_get_thread_num()));
    }
#pragma omp for schedule(static)
    for (int i = 0; i < n_iter; i++) {

      arma::vec theta_i = theta.col(i);
      mod.update_model(theta_i);

      alpha_store.slices(nsim_states * (cum_counts(i)-counts(i)), nsim_states * cum_counts(i) - 1) =
        mod.sim_smoother(nsim_states * counts(i), true);

    }
  }
  return alpha_store;
}

template arma::cube sample_states<gssm>(gssm mod, const arma::mat& theta,
  const arma::uvec& counts, unsigned int nsim_states, unsigned int n_threads,
  arma::uvec seeds);
template arma::cube sample_states<bsm>(bsm mod, const arma::mat& theta,
  const arma::uvec& counts, unsigned int nsim_states, unsigned int n_threads,
  arma::uvec seeds);