#ifdef _OPENMP
#include <omp.h>
#endif
#include <sitmo.h>
#include <ramcmc.h>
#include "nlg_amcmc.h"
#include "nlg_ssm.h"

#include "rep_mat.h"
#include "filter_smoother.h"

nlg_amcmc::nlg_amcmc(const arma::uvec& prior_distributions, 
  const arma::mat& prior_parameters, const unsigned int n_iter, 
  const unsigned int n_burnin, const unsigned int n_thin, const unsigned int n, 
  const unsigned int m, const double target_acceptance, const double gamma, 
  const arma::mat& S, const bool store_modes) :
  mcmc(prior_distributions, prior_parameters, n_iter, n_burnin, n_thin, n, m,
    target_acceptance, gamma, S, true),
    weight_storage(arma::vec(n_samples, arma::fill::zeros)),
    approx_loglik_storage(arma::vec(n_samples)),
    scales_storage(arma::vec(n_samples)),
    prior_storage(arma::vec(n_samples)),
    store_modes(store_modes),
    mode_storage(arma::cube(m, n, n_samples * store_modes)){
}

void nlg_amcmc::trim_storage() {
  theta_storage.resize(n_par, n_stored);
  posterior_storage.resize(n_stored);
  count_storage.resize(n_stored);
  alpha_storage.resize(alpha_storage.n_rows, alpha_storage.n_cols, n_stored);
  scales_storage.resize(n_stored);
  weight_storage.resize(n_stored);
  approx_loglik_storage.resize(n_stored);
  prior_storage.resize(n_stored);
  mode_storage.resize(mode_storage.n_rows, mode_storage.n_cols, n_stored);
}

void nlg_amcmc::expand() {
  
  //trim extras first just in case
  trim_storage();
  n_stored = arma::accu(count_storage);
  
  arma::mat expanded_theta = rep_mat(theta_storage, count_storage);
  theta_storage.set_size(n_par, n_stored);
  theta_storage = expanded_theta;
  
  arma::vec expanded_posterior = rep_vec(posterior_storage, count_storage);
  posterior_storage.set_size(n_stored);
  posterior_storage = expanded_posterior;
  
  arma::cube expanded_alpha = rep_cube(alpha_storage, count_storage);
  alpha_storage.set_size(alpha_storage.n_rows, alpha_storage.n_cols, n_stored);
  alpha_storage = expanded_alpha;
  
  arma::mat expanded_scales = rep_mat(scales_storage, count_storage);
  scales_storage.set_size(scales_storage.n_rows, n_stored);
  scales_storage = expanded_scales;
  
  arma::vec expanded_weight = rep_vec(weight_storage, count_storage);
  weight_storage.set_size(n_stored);
  weight_storage = expanded_weight;
  
  arma::vec expanded_approx_loglik = rep_vec(approx_loglik_storage, count_storage);
  approx_loglik_storage.set_size(n_stored);
  approx_loglik_storage = expanded_approx_loglik;
  
  arma::vec expanded_prior = rep_vec(prior_storage, count_storage);
  prior_storage.set_size(n_stored);
  prior_storage = expanded_prior;
  
  arma::cube expanded_mode = rep_cube(mode_storage, count_storage);
  mode_storage.set_size(mode_storage.n_rows, mode_storage.n_cols, n_stored);
  mode_storage = expanded_mode;
  count_storage.resize(n_stored);
  count_storage.ones();
  
}
// run approximate MCMC for
// non-linear Gaussian state space model

void nlg_amcmc::approx_mcmc(nlg_ssm model, const unsigned int max_iter, 
  const double conv_tol, const bool end_ram, const unsigned int iekf_iter) {
  
  unsigned int m = model.m;
  unsigned n = model.n;
  
  double logprior = model.log_prior_pdf.eval(model.theta);
  
  arma::mat mode_estimate(m, n);
  mgg_ssm approx_model0 = model.approximate(mode_estimate, max_iter, conv_tol, iekf_iter);
  double sum_scales = arma::accu(model.scaling_factors(approx_model0, mode_estimate));
  // compute the log-likelihood of the approximate model
  double loglik = approx_model0.log_likelihood() + sum_scales;
  
  double acceptance_prob = 0.0;
  std::normal_distribution<> normal(0.0, 1.0);
  std::uniform_real_distribution<> unif(0.0, 1.0);
  
  arma::vec theta = model.theta;
  bool new_value = true;
  unsigned int n_values = 0;
  
  for (unsigned int i = 1; i <= n_iter; i++) {
    if (i % 16 == 0) {
      Rcpp::checkUserInterrupt();
    }
    
    // sample from standard normal distribution
    arma::vec u(n_par);
    for(unsigned int j = 0; j < n_par; j++) {
      u(j) = normal(model.engine);
    }
    
    // propose new theta
    arma::vec theta_prop = theta + S * u;
    // compute prior
    double logprior_prop = model.log_prior_pdf.eval(theta_prop);
    if (arma::is_finite(logprior_prop)) {
      // update parameters
      model.theta = theta_prop;
      arma::mat mode_estimate_prop(m, n);
      mgg_ssm approx_model = model.approximate(mode_estimate_prop, max_iter, 
        conv_tol, iekf_iter);
      double sum_scales_prop = 
        arma::accu(model.scaling_factors(approx_model, mode_estimate_prop));
      // compute the log-likelihood of the approximate model
      double loglik_prop = approx_model.log_likelihood() + sum_scales_prop;
      
      if(arma::is_finite(loglik_prop)) {
        acceptance_prob = std::min(1.0, std::exp(loglik_prop - loglik +
          logprior_prop - logprior));
      } else {
        acceptance_prob = 0.0; 
      }
      
      if (unif(model.engine) < acceptance_prob) {
        if (i > n_burnin) {
          acceptance_rate++;
          n_values++;
        }
        loglik = loglik_prop;
        logprior = logprior_prop;
        theta = theta_prop;
        sum_scales = sum_scales_prop;
        mode_estimate = mode_estimate_prop;
        new_value = true;
      }
    } else acceptance_prob = 0.0;
    
    if (i > n_burnin && n_values % n_thin == 0) {
      //new block
      if (new_value) {
        approx_loglik_storage(n_stored) = loglik;
        prior_storage(n_stored) = logprior;
        theta_storage.col(n_stored) = theta;
        scales_storage(n_stored) = sum_scales;
        count_storage(n_stored) = 1;
        if(store_modes) {
          mode_storage.slice(n_stored) = mode_estimate;
        }
        n_stored++;
        new_value = false;
      } else {
        count_storage(n_stored - 1)++;
      }
    }
    
    if (!end_ram || i <= n_burnin) {
      ramcmc::adapt_S(S, u, acceptance_prob, target_acceptance, i, gamma);
    }
  }
  
  trim_storage();
  acceptance_rate /= (n_iter - n_burnin);
}

void nlg_amcmc::is_correction_bsf(nlg_ssm model, const unsigned int nsim_states, 
  const unsigned int is_type, const unsigned int n_threads) {
  
  if(n_threads > 1) {
#ifdef _OPENMP
#pragma omp parallel num_threads(n_threads) default(none) firstprivate(model)
{
  model.engine = sitmo::prng_engine(omp_get_thread_num() + 1);
  unsigned thread_size = std::floor(static_cast <double> (n_stored) / n_threads);
  unsigned int start = omp_get_thread_num() * thread_size;
  unsigned int end = (omp_get_thread_num() + 1) * thread_size - 1;
  if(omp_get_thread_num() == static_cast<int>(n_threads - 1)) {
    end = n_stored - 1;
  }
  
  arma::mat theta_piece = theta_storage(arma::span::all, arma::span(start, end));
  arma::cube alpha_piece(model.n, model.m, end - start + 1);
  arma::vec weights_piece(end - start + 1);
  arma::vec approx_loglik_piece = approx_loglik_storage.subvec(start, end);
  if (is_type != 1) {
    state_sampler_bsf_is2(model, nsim_states, approx_loglik_piece, theta_piece,
      alpha_piece, weights_piece);
  } else {
    arma::uvec count_piece = count_storage(arma::span(start, end));
    state_sampler_bsf_is1(model, nsim_states, approx_loglik_piece, theta_piece, 
      alpha_piece, weights_piece, count_piece);
  }
  alpha_storage.slices(start, end) = alpha_piece;
  weight_storage.subvec(start, end) = weights_piece;
}
#else
    if (is_type != 1) {
      state_sampler_bsf_is2(model, nsim_states, approx_loglik_storage, theta_storage, 
        alpha_storage, weight_storage);
    } else {
      state_sampler_bsf_is1(model, nsim_states, approx_loglik_storage, theta_storage, 
        alpha_storage, weight_storage, count_storage);
    }
#endif
  } else {
    if (is_type != 1) {
      state_sampler_bsf_is2(model, nsim_states, approx_loglik_storage, theta_storage, 
        alpha_storage, weight_storage);
    } else {
      state_sampler_bsf_is1(model, nsim_states, approx_loglik_storage, theta_storage, 
        alpha_storage, weight_storage, count_storage);
    }
  }
  posterior_storage = prior_storage + arma::log(weight_storage);
}

void nlg_amcmc::state_sampler_bsf_is2(nlg_ssm& model, const unsigned int nsim_states, 
  const arma::vec& approx_loglik_storage, const arma::mat& theta,
  arma::cube& alpha, arma::vec& weights) {
  
  for (unsigned int i = 0; i < theta.n_cols; i++) {
    
    model.theta = theta.col(i);
    
    arma::cube alpha_i(model.m, model.n, nsim_states);
    arma::mat weights_i(nsim_states, model.n);
    arma::umat indices(nsim_states, model.n - 1);
    double loglik = model.bsf_filter(nsim_states, alpha_i, weights_i, indices);
    if(arma::is_finite(loglik)) {
      weights(i) = std::exp(loglik - approx_loglik_storage(i));
      
      filter_smoother(alpha_i, indices);
      arma::vec w = weights_i.col(model.n - 1);
      std::discrete_distribution<unsigned int> sample(w.begin(), w.end());
      alpha.slice(i) = alpha_i.slice(sample(model.engine)).t();
    } else {
      weights(i) = 0.0;
      alpha.slice(i).zeros();
    }
  }
}


void nlg_amcmc::state_sampler_bsf_is1(nlg_ssm& model, const unsigned int nsim_states, 
  const arma::vec& approx_loglik_storage, const arma::mat& theta,
  arma::cube& alpha, arma::vec& weights, const arma::uvec& counts) {
  
  for (unsigned int i = 0; i < theta.n_cols; i++) {
    
    model.theta = theta.col(i);
    
    unsigned int m = nsim_states * counts(i);
    arma::cube alpha_i(model.m, model.n, m);
    arma::mat weights_i(m, model.n);
    arma::umat indices(m, model.n - 1);
    double loglik = model.bsf_filter(m, alpha_i, weights_i, indices);
    if(arma::is_finite(loglik)) {
      weights(i) = std::exp(loglik - approx_loglik_storage(i));
      filter_smoother(alpha_i, indices);
      arma::vec w = weights_i.col(model.n - 1);
      std::discrete_distribution<unsigned int> sample(w.begin(), w.end());
      alpha.slice(i) = alpha_i.slice(sample(model.engine)).t();
    } else {
      weights(i) = 0.0;
      alpha.slice(i).zeros();
    }
  }
}


void nlg_amcmc::is_correction_psi(nlg_ssm model, const unsigned int nsim_states, 
  const unsigned int is_type, const unsigned int n_threads) {
  
  if(n_threads > 1) {
#ifdef _OPENMP
#pragma omp parallel num_threads(n_threads) default(none) firstprivate(model)
{
  model.engine = sitmo::prng_engine(omp_get_thread_num() + 1);
  unsigned thread_size = std::floor(static_cast <double> (n_stored) / n_threads);
  unsigned int start = omp_get_thread_num() * thread_size;
  unsigned int end = (omp_get_thread_num() + 1) * thread_size - 1;
  if(omp_get_thread_num() == static_cast<int>(n_threads - 1)) {
    end = n_stored - 1;
  }
  
  arma::mat theta_piece = theta_storage(arma::span::all, arma::span(start, end));
  arma::cube alpha_piece(model.n, model.m, thread_size);
  arma::vec weights_piece(thread_size);
  arma::cube mode_piece = 
    mode_storage(arma::span::all, arma::span::all, arma::span(start, end));
  if (is_type != 1) {
    state_sampler_psi_is2(model, nsim_states, theta_piece, mode_piece,
      alpha_piece, weights_piece);
  } else {
    arma::uvec count_piece = count_storage(arma::span(start, end));
    state_sampler_psi_is1(model, nsim_states, theta_piece, mode_piece,
      alpha_piece, weights_piece, count_piece);
  }
  alpha_storage.slices(start, end) = alpha_piece;
  weight_storage.subvec(start, end) = weights_piece;
}
#else
    if (is_type != 1) {
      state_sampler_psi_is2(model, nsim_states, theta_storage, mode_storage,
        alpha_storage, weight_storage);
    } else {
      state_sampler_psi_is1(model, nsim_states, theta_storage, mode_storage,
        alpha_storage, weight_storage, count_storage);
    }
#endif
  } else {
    if (is_type != 1) {
      state_sampler_psi_is2(model, nsim_states, theta_storage, mode_storage,
        alpha_storage, weight_storage);
    } else {
      state_sampler_psi_is1(model, nsim_states, theta_storage, mode_storage,
        alpha_storage, weight_storage, count_storage);
    }
  }
  posterior_storage = prior_storage + approx_loglik_storage - scales_storage + 
    arma::log(weight_storage);
}

void nlg_amcmc::state_sampler_psi_is2(nlg_ssm& model, const unsigned int nsim_states, 
  const arma::mat& theta, const arma::cube& mode, arma::cube& alpha, arma::vec& weights) {
  
  unsigned int p = model.p;
  unsigned int n = model.n;
  unsigned int m = model.m;
  unsigned int k = model.k;
  
  arma::vec a1(m);
  arma::mat P1(m, m);
  arma::cube Z(p, m, n);
  arma::cube H(p, p, (n - 1) * model.Htv + 1);
  arma::cube T(m, m, n);
  arma::cube R(m, k, (n - 1) * model.Rtv + 1);
  arma::mat D(p, n);
  arma::mat C(m, n);
  
  mgg_ssm approx_model(model.y, Z, H, T, R, a1, P1, arma::cube(0,0,0),
    arma::mat(0,0), D, C, model.seed);
  
  for (unsigned int i = 0; i < theta.n_cols; i++) {
    
    model.theta = theta.col(i);
    
    approx_model.a1 = model.a1_fn.eval(model.theta, model.known_params);
    approx_model.P1 = model.P1_fn.eval(model.theta, model.known_params);
    for (unsigned int t = 0; t < Z.n_slices; t++) {
      approx_model.Z.slice(t) = model.Z_gn.eval(t, mode.slice(i).col(t), model.theta, model.known_params, model.known_tv_params);
      approx_model.T.slice(t) = model.T_gn.eval(t, mode.slice(i).col(t), model.theta, model.known_params, model.known_tv_params);
      approx_model.D.col(t) = model.Z_fn.eval(t, mode.slice(i).col(t), model.theta, model.known_params, model.known_tv_params) -
        approx_model.Z.slice(t) * mode.slice(i).col(t);
      approx_model.C.col(t) =  model.T_fn.eval(t, mode.slice(i).col(t), model.theta, model.known_params, model.known_tv_params) -
        approx_model.T.slice(t) * mode.slice(i).col(t);
    }
    for (unsigned int t = 0; t < H.n_slices; t++) {
      approx_model.H.slice(t) = model.H_fn.eval(t, model.theta, model.known_params, model.known_tv_params);
    }
    for (unsigned int t = 0; t < R.n_slices; t++) {
      approx_model.R.slice(t) = model.R_fn.eval(t, model.theta, model.known_params, model.known_tv_params);
    }
    approx_model.compute_HH();
    approx_model.compute_RR();
    
    arma::cube alpha_i(model.m, model.n, nsim_states);
    arma::mat weights_i(nsim_states, model.n);
    arma::umat indices(nsim_states, model.n - 1);
    weights(i) = std::exp(model.psi_filter(approx_model, 0.0,nsim_states, alpha_i, weights_i, indices));
    
    filter_smoother(alpha_i, indices);
    arma::vec w = weights_i.col(model.n - 1);
    std::discrete_distribution<unsigned int> sample(w.begin(), w.end());
    alpha.slice(i) = alpha_i.slice(sample(model.engine)).t();
  }
}


void nlg_amcmc::state_sampler_psi_is1(nlg_ssm& model, const unsigned int nsim_states, 
  const arma::mat& theta, const arma::cube& mode,
  arma::cube& alpha, arma::vec& weights, const arma::uvec& counts) {
  
  unsigned int p = model.p;
  unsigned int n = model.n;
  unsigned int m = model.m;
  unsigned int k = model.k;
  
  arma::vec a1(m);
  arma::mat P1(m, m);
  arma::cube Z(p, m, n);
  arma::cube H(p, p, (n - 1) * model.Htv + 1);
  arma::cube T(m, m, n);
  arma::cube R(m, k, (n - 1) * model.Rtv + 1);
  arma::mat D(p, n);
  arma::mat C(m, n);
  
  mgg_ssm approx_model(model.y, Z, H, T, R, a1, P1, arma::cube(0,0,0),
    arma::mat(0,0), D, C, model.seed);
  
  for (unsigned int i = 0; i < theta.n_cols; i++) {
    
    model.theta = theta.col(i);
    
    approx_model.a1 = model.a1_fn.eval(model.theta, model.known_params);
    approx_model.P1 = model.P1_fn.eval(model.theta, model.known_params);
    for (unsigned int t = 0; t < Z.n_slices; t++) {
      approx_model.Z.slice(t) = model.Z_gn.eval(t, mode.slice(i).col(t), model.theta, model.known_params, model.known_tv_params);
      approx_model.T.slice(t) = model.T_gn.eval(t, mode.slice(i).col(t), model.theta, model.known_params, model.known_tv_params);
      approx_model.D.col(t) = model.Z_fn.eval(t, mode.slice(i).col(t), model.theta, model.known_params, model.known_tv_params) -
        approx_model.Z.slice(t) * mode.slice(i).col(t);
      approx_model.C.col(t) =  model.T_fn.eval(t, mode.slice(i).col(t), model.theta, model.known_params, model.known_tv_params) -
        approx_model.T.slice(t) * mode.slice(i).col(t);
    }
    for (unsigned int t = 0; t < H.n_slices; t++) {
      approx_model.H.slice(t) = model.H_fn.eval(t, model.theta, model.known_params, model.known_tv_params);
    }
    for (unsigned int t = 0; t < R.n_slices; t++) {
      approx_model.R.slice(t) = model.R_fn.eval(t, model.theta, model.known_params, model.known_tv_params);
    }
    unsigned int m_sim = nsim_states * counts(i);
    arma::cube alpha_i(model.m, model.n, m_sim);
    arma::mat weights_i(m_sim, model.n);
    arma::umat indices(m_sim, model.n - 1);
    weights(i) = std::exp(model.psi_filter(approx_model, 0.0,m_sim, alpha_i, weights_i, indices));
    
    filter_smoother(alpha_i, indices);
    arma::vec w = weights_i.col(model.n - 1);
    std::discrete_distribution<unsigned int> sample(w.begin(), w.end());
    alpha.slice(i) = alpha_i.slice(sample(model.engine)).t();
  }
}


void nlg_amcmc::state_ekf_sample(nlg_ssm model, const unsigned int n_threads) {
  
  if(n_threads > 1) {
#ifdef _OPENMP
#pragma omp parallel num_threads(n_threads) default(none) firstprivate(model)
{
  model.engine = sitmo::prng_engine(omp_get_thread_num() + 1);
  unsigned thread_size = std::floor(static_cast <double> (n_stored) / n_threads);
  unsigned int start = omp_get_thread_num() * thread_size;
  unsigned int end = (omp_get_thread_num() + 1) * thread_size - 1;
  if(omp_get_thread_num() == static_cast<int>(n_threads - 1)) {
    end = n_stored - 1;
  }
  
  arma::mat theta_piece = theta_storage(arma::span::all, arma::span(start, end));
  arma::cube alpha_piece(model.n, model.m, thread_size);
  arma::cube mode_piece = 
    mode_storage(arma::span::all, arma::span::all, arma::span(start, end));
  ekf_sampler(model, theta_piece, mode_piece, alpha_piece);
  alpha_storage.slices(start, end) = alpha_piece;
}
#else
    ekf_sampler(model, theta_storage, mode_storage,
      alpha_storage);
#endif
  } else {
    ekf_sampler(model, theta_storage, mode_storage,
      alpha_storage);
  }
  posterior_storage = prior_storage + approx_loglik_storage;
}

void nlg_amcmc::ekf_sampler(nlg_ssm& model,
  const arma::mat& theta, const arma::cube& mode, arma::cube& alpha) {
  
  unsigned int p = model.p;
  unsigned int n = model.n;
  unsigned int m = model.m;
  unsigned int k = model.k;
  
  arma::vec a1(m);
  arma::mat P1(m, m);
  arma::cube Z(p, m, n);
  arma::cube H(p, p, (n - 1) * model.Htv + 1);
  arma::cube T(m, m, n);
  arma::cube R(m, k, (n - 1) * model.Rtv + 1);
  arma::mat D(p, n);
  arma::mat C(m, n);
  
  mgg_ssm approx_model(model.y, Z, H, T, R, a1, P1, arma::cube(0,0,0),
    arma::mat(0,0), D, C, model.seed);
  
  for (unsigned int i = 0; i < theta.n_cols; i++) {
    
    model.theta = theta.col(i);
    
    approx_model.a1 = model.a1_fn.eval(model.theta, model.known_params);
    approx_model.P1 = model.P1_fn.eval(model.theta, model.known_params);
    for (unsigned int t = 0; t < Z.n_slices; t++) {
      approx_model.Z.slice(t) = model.Z_gn.eval(t, mode.slice(i).col(t), model.theta, model.known_params, model.known_tv_params);
      approx_model.T.slice(t) = model.T_gn.eval(t, mode.slice(i).col(t), model.theta, model.known_params, model.known_tv_params);
      approx_model.D.col(t) = model.Z_fn.eval(t, mode.slice(i).col(t), model.theta, model.known_params, model.known_tv_params) -
        approx_model.Z.slice(t) * mode.slice(i).col(t);
      approx_model.C.col(t) =  model.T_fn.eval(t, mode.slice(i).col(t), model.theta, model.known_params, model.known_tv_params) -
        approx_model.T.slice(t) * mode.slice(i).col(t);
    }
    for (unsigned int t = 0; t < H.n_slices; t++) {
      approx_model.H.slice(t) = model.H_fn.eval(t, model.theta, model.known_params, model.known_tv_params);
    }
    for (unsigned int t = 0; t < R.n_slices; t++) {
      approx_model.R.slice(t) = model.R_fn.eval(t, model.theta, model.known_params, model.known_tv_params);
    }
    approx_model.compute_HH();
    approx_model.compute_RR();
    alpha.slice(i) = approx_model.simulate_states().slice(0).t();
  }
}

void nlg_amcmc::state_ekf_summary(nlg_ssm& model,
 arma::mat& alphahat, arma::cube& Vt) {
  
  unsigned int p = model.p;
  unsigned int n = model.n;
  unsigned int m = model.m;
  unsigned int k = model.k;
  
  arma::vec a1(m);
  arma::mat P1(m, m);
  arma::cube Z(p, m, n);
  arma::cube H(p, p, (n - 1) * model.Htv + 1);
  arma::cube T(m, m, n);
  arma::cube R(m, k, (n - 1) * model.Rtv + 1);
  arma::mat D(p, n);
  arma::mat C(m, n);
  
  mgg_ssm approx_model(model.y, Z, H, T, R, a1, P1, arma::cube(0,0,0),
    arma::mat(0,0), D, C, model.seed);
  
// first iteration
  model.theta = theta_storage.col(0);
  
  approx_model.a1 = model.a1_fn.eval(model.theta, model.known_params);
  approx_model.P1 = model.P1_fn.eval(model.theta, model.known_params);
  for (unsigned int t = 0; t < Z.n_slices; t++) {
    approx_model.Z.slice(t) = model.Z_gn.eval(t, mode_storage.slice(0).col(t), model.theta, model.known_params, model.known_tv_params);
    approx_model.T.slice(t) = model.T_gn.eval(t, mode_storage.slice(0).col(t), model.theta, model.known_params, model.known_tv_params);
    approx_model.D.col(t) = model.Z_fn.eval(t, mode_storage.slice(0).col(t), model.theta, model.known_params, model.known_tv_params) -
      approx_model.Z.slice(t) * mode_storage.slice(0).col(t);
    approx_model.C.col(t) =  model.T_fn.eval(t, mode_storage.slice(0).col(t), model.theta, model.known_params, model.known_tv_params) -
      approx_model.T.slice(t) * mode_storage.slice(0).col(t);
  }
  for (unsigned int t = 0; t < H.n_slices; t++) {
    approx_model.H.slice(t) = model.H_fn.eval(t, model.theta, model.known_params, model.known_tv_params);
  }
  for (unsigned int t = 0; t < R.n_slices; t++) {
    approx_model.R.slice(t) = model.R_fn.eval(t, model.theta, model.known_params, model.known_tv_params);
  }
  approx_model.compute_HH();
  approx_model.compute_RR();
  approx_model.smoother(alphahat, Vt);
  
  double sum_w = count_storage(0);
  arma::mat alphahat_i = alphahat;
  arma::cube Vt_i = Vt;
  
  arma::cube Valpha(m, m, n, arma::fill::zeros);
  
  for (unsigned int i = 1; i < theta_storage.n_cols; i++) {
    
    model.theta = theta_storage.col(i);
    
    approx_model.a1 = model.a1_fn.eval(model.theta, model.known_params);
    approx_model.P1 = model.P1_fn.eval(model.theta, model.known_params);
    for (unsigned int t = 0; t < Z.n_slices; t++) {
      approx_model.Z.slice(t) = model.Z_gn.eval(t, mode_storage.slice(i).col(t), model.theta, model.known_params, model.known_tv_params);
      approx_model.T.slice(t) = model.T_gn.eval(t, mode_storage.slice(i).col(t), model.theta, model.known_params, model.known_tv_params);
      approx_model.D.col(t) = model.Z_fn.eval(t, mode_storage.slice(i).col(t), model.theta, model.known_params, model.known_tv_params) -
        approx_model.Z.slice(t) * mode_storage.slice(i).col(t);
      approx_model.C.col(t) =  model.T_fn.eval(t, mode_storage.slice(i).col(t), model.theta, model.known_params, model.known_tv_params) -
        approx_model.T.slice(t) * mode_storage.slice(i).col(t);
    }
    for (unsigned int t = 0; t < H.n_slices; t++) {
      approx_model.H.slice(t) = model.H_fn.eval(t, model.theta, model.known_params, model.known_tv_params);
    }
    for (unsigned int t = 0; t < R.n_slices; t++) {
      approx_model.R.slice(t) = model.R_fn.eval(t, model.theta, model.known_params, model.known_tv_params);
    }
    approx_model.compute_HH();
    approx_model.compute_RR();
    approx_model.smoother(alphahat_i, Vt_i);
    
    arma::mat diff = alphahat_i - alphahat;
    double tmp = count_storage(i) + sum_w;
    alphahat = (alphahat * sum_w + alphahat_i * count_storage(i)) / tmp;
    
    for (unsigned int t = 0; t < model.n; t++) {
      Valpha.slice(t) += diff.col(t) * (alphahat_i.col(t) - alphahat.col(t)).t();
    }
    Vt = (Vt * sum_w + Vt_i * count_storage(i)) / tmp;
    sum_w = tmp;
  }
  Vt += Valpha / theta_storage.n_cols; // Var[E(alpha)] + E[Var(alpha)]
}


