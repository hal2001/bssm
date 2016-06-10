#include "ng_bsm.h"

// [[Rcpp::export]]
double ng_bsm_loglik(arma::vec& y, arma::mat& Z, arma::cube& T,
  arma::cube& R, arma::vec& a1, arma::mat& P1, arma::vec& phi, bool slope,
  bool seasonal, bool noise, arma::uvec fixed, arma::mat& xreg, arma::vec& beta,
  unsigned int distribution, arma::vec init_signal, unsigned int nsim_states,
  unsigned int seed) {

  ng_bsm model(y, Z, T, R, a1, P1, phi, slope, seasonal, noise, fixed, xreg, beta,
    distribution, seed);

  if (nsim_states == 0) {
    model.conv_tol = 1.0e-12;
    model.max_iter = 1000;
  }

  double ll = model.approx(init_signal, model.max_iter, model.conv_tol);
  double ll_w = 0;
  if (nsim_states > 1) {
    arma::cube alpha = model.sim_smoother(nsim_states);
    double ll_approx_u = 0.0;
    for (unsigned int t = 0; t < model.n; t++) {
      if (arma::is_finite(model.ng_y(t))) {
        ll_approx_u += model.ng_y(t) * init_signal(t) - model.phi(t) * exp(init_signal(t)) +
          0.5 * pow(model.y(t) - init_signal(t), 2) / model.HH(t);
      }
    }
    arma::vec weights = exp(model.importance_weights(alpha) - ll_approx_u);
    ll_w = log(sum(weights) / nsim_states);
  }
  return model.log_likelihood() + ll + ll_w;
}

// [[Rcpp::export]]
List ng_bsm_filter(arma::vec& y, arma::mat& Z, arma::cube& T,
  arma::cube& R, arma::vec& a1, arma::mat& P1, arma::vec &phi, bool slope,
  bool seasonal, bool noise, arma::uvec fixed, arma::mat& xreg, arma::vec& beta,
  unsigned int distribution, arma::vec init_signal) {

  ng_bsm model(y, Z, T, R, a1, P1, phi, slope, seasonal, noise, fixed, xreg, beta,
    distribution,1);

  double logLik = model.approx(init_signal, 1000, 1e-12);

  arma::mat at(a1.n_elem, y.n_elem + 1);
  arma::mat att(a1.n_elem, y.n_elem);
  arma::cube Pt(a1.n_elem, a1.n_elem, y.n_elem + 1);
  arma::cube Ptt(a1.n_elem, a1.n_elem, y.n_elem);

  logLik += model.filter(at, att, Pt, Ptt);

  arma::inplace_trans(at);
  arma::inplace_trans(att);

  return List::create(
    Named("at") = at,
    Named("att") = att,
    Named("Pt") = Pt,
    Named("Ptt") = Ptt,
    Named("logLik") = logLik);
}


// [[Rcpp::export]]
List ng_bsm_mcmc_full(arma::vec& y, arma::mat& Z, arma::cube& T,
  arma::cube& R, arma::vec& a1, arma::mat& P1, arma::vec& phi,
  unsigned int distribution,
  arma::vec& theta_lwr, arma::vec& theta_upr, unsigned int n_iter,
  unsigned int nsim_states, unsigned int n_burnin, unsigned int n_thin,
  double gamma, double target_acceptance, arma::mat& S, bool slope,
  bool seasonal, bool noise, arma::uvec fixed, arma::mat& xreg, arma::vec& beta,
  arma::vec& init_signal, unsigned int method, unsigned int seed, bool log_space) {


  ng_bsm model(y, Z, T, R, a1, P1, phi, slope, seasonal, noise, fixed, xreg, beta,
    distribution, seed, log_space);

  if (method == 1) {
    return model.mcmc_full(theta_lwr, theta_upr, n_iter,
      nsim_states, n_burnin, n_thin, gamma, target_acceptance, S, init_signal);
  } else {
    return model.mcmc_da(theta_lwr, theta_upr, n_iter,
      nsim_states, n_burnin, n_thin, gamma, target_acceptance, S, init_signal);
  }

}


// [[Rcpp::export]]
arma::mat ng_bsm_predict2(arma::vec& y, arma::mat& Z, arma::cube& T,
  arma::cube& R, arma::vec& a1, arma::mat& P1, arma::vec& phi,
  unsigned int distribution, arma::vec& theta_lwr,
  arma::vec& theta_upr, unsigned int n_iter, unsigned int nsim_states,
  unsigned int n_burnin, unsigned int n_thin, double gamma,
  double target_acceptance, arma::mat& S, unsigned int n_ahead,
  unsigned int interval, bool slope, bool seasonal, bool noise, arma::uvec fixed,
  arma::mat& xreg, arma::vec& beta, arma::vec& init_signal, unsigned int seed,
  bool log_space) {

  ng_bsm model(y, Z, T, R, a1, P1, phi, slope, seasonal, noise, fixed, xreg, beta,
    distribution, seed, log_space);

  return model.predict2(theta_lwr, theta_upr, n_iter, nsim_states, n_burnin,
    n_thin, gamma, target_acceptance, S, n_ahead, interval, init_signal);

}