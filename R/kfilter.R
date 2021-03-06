#' Kalman Filtering
#'
#' Function \code{kfilter} runs the Kalman filter for the given model, 
#' and returns the filtered estimates and one-step-ahead predictions of the 
#' states \eqn{\alpha_t} given the data up to time \eqn{t}.
#'
#' For non-Gaussian models, the Kalman filtering is based on the approximate Gaussian model.
#'
#' @param object Model object
#' @param ... Ignored.
#' @return List containing the log-likelihood (approximate in non-Gaussian case),
#' one-step-ahead predictions \code{at} and filtered
#' estimates \code{att} of states, and the corresponding variances \code{Pt} and
#'  \code{Ptt}.
#' @seealso \code{\link{bootstrap_filter}}
#' @export
#' @rdname kfilter
kfilter <- function(object, ...) {
  UseMethod("kfilter", object)
}

#' @method kfilter gssm
#' @export
kfilter.gssm <- function(object, ...) {
  
  out <- gaussian_kfilter(object, model_type = 1L)
  colnames(out$at) <- colnames(out$att) <- colnames(out$Pt) <-
    colnames(out$Ptt) <- rownames(out$Pt) <- rownames(out$Ptt) <- names(object$a1)
  out$at <- ts(out$at, start = start(object$y), frequency = frequency(object$y))
  out$att <- ts(out$att, start = start(object$y), frequency = frequency(object$y))
  out
}
#' @method kfilter mv_gssm
#' @export
kfilter.mv_gssm <- function(object, ...) {
  
  out <- gaussian_kfilter(object, model_type = -1L)
  colnames(out$at) <- colnames(out$att) <- colnames(out$Pt) <-
    colnames(out$Ptt) <- rownames(out$Pt) <- rownames(out$Ptt) <- names(object$a1)
  out$at <- ts(out$at, start = start(object$y), frequency = frequency(object$y))
  out$att <- ts(out$att, start = start(object$y), frequency = frequency(object$y))
  out
}
#' @method kfilter lgg_ssm
#' @export
kfilter.lgg_ssm <- function(object, ...) {
  
  out <- general_gaussian_kfilter(t(object$y), object$Z, object$H, object$T, 
    object$R, object$a1, object$P1, 
    object$theta, object$obs_intercept, object$state_intercept,
    object$log_prior_pdf, object$known_params, 
    object$known_tv_params, as.integer(object$time_varying), 
    object$n_states, object$n_etas)
  colnames(out$at) <- colnames(out$att) <- colnames(out$Pt) <-
    colnames(out$Ptt) <- rownames(out$Pt) <- rownames(out$Ptt) <- object$state_names
  out$at <- ts(out$at, start = start(object$y), frequency = frequency(object$y))
  out$att <- ts(out$att, start = start(object$y), frequency = frequency(object$y))
  out
}

#' @method kfilter bsm
#' @export
kfilter.bsm <- function(object, ...) {
  
  out <- gaussian_kfilter(object, model_type = 2L)
  colnames(out$at) <- colnames(out$att) <- colnames(out$Pt) <-
    colnames(out$Ptt) <- rownames(out$Pt) <- rownames(out$Ptt) <- names(object$a1)
  out$at <- ts(out$at, start = start(object$y), frequency = frequency(object$y))
  out$att <- ts(out$att, start = start(object$y), frequency = frequency(object$y))
  out
}

#' @method kfilter ngssm
#' @export
kfilter.ngssm <- function(object, ...) {
  kfilter(gaussian_approx(object))
}

#' @method kfilter ng_bsm
#' @export
kfilter.ng_bsm <- function(object, ...) {
  kfilter(gaussian_approx(object))
}

#' @method kfilter svm
#' @export
kfilter.svm <- function(object, ...) {
  kfilter(gaussian_approx(object))
}
#' @method kfilter ng_ar1
#' @export
kfilter.ng_ar1 <- function(object, ...) {
  kfilter(gaussian_approx(object))
}

#' (Iterated) Extended Kalman Filtering
#'
#' Function \code{ekf} runs the (iterated) extended Kalman filter for the given 
#' non-linear Gaussian model of class \code{nlg_ssm}, 
#' and returns the filtered estimates and one-step-ahead predictions of the 
#' states \eqn{\alpha_t} given the data up to time \eqn{t}.
#'
#' @param object Model object
#' @param iekf_iter If \code{iekf_iter > 0}, iterated extended Kalman filter 
#' is used with \code{iekf_iter} iterations.
#' @return List containing the log-likelihood,
#' one-step-ahead predictions \code{at} and filtered
#' estimates \code{att} of states, and the corresponding variances \code{Pt} and
#'  \code{Ptt}.
#' @export
#' @rdname ekf
#' @export
ekf <- function(object, iekf_iter = 0) {
  
  out <- ekf_nlg(t(object$y), object$Z, object$H, object$T, 
  object$R, object$Z_gn, object$T_gn, object$a1, object$P1, 
  object$theta, object$log_prior_pdf, object$known_params, 
  object$known_tv_params, object$n_states, object$n_etas, 
  as.integer(object$time_varying), iekf_iter)
  
  out$at <- ts(out$at, start = start(object$y), frequency = frequency(object$y))
  out$att <- ts(out$att, start = start(object$y), frequency = frequency(object$y))
  out
}
#' Unscented Kalman Filtering
#'
#' Function \code{ukf} runs the unscented Kalman filter for the given 
#' non-linear Gaussian model of class \code{nlg_ssm}, 
#' and returns the filtered estimates and one-step-ahead predictions of the 
#' states \eqn{\alpha_t} given the data up to time \eqn{t}.
#'
#' @param object Model object
#' @param alpha,beta,kappa Tuning parameters for the UKF.
#' @return List containing the log-likelihood,
#' one-step-ahead predictions \code{at} and filtered
#' estimates \code{att} of states, and the corresponding variances \code{Pt} and
#'  \code{Ptt}.
#' @export
#' @rdname ukf
#' @export
#' @export
ukf <- function(object, alpha = 1, beta = 0, kappa = 2) {
  
  out <- ukf_nlg(t(object$y), object$Z, object$H, object$T, 
    object$R, object$Z_gn, object$T_gn, object$a1, object$P1, 
    object$theta, object$log_prior_pdf, object$known_params, 
    object$known_tv_params, object$n_states, object$n_etas, 
    as.integer(object$time_varying),
    alpha, beta, kappa)
  
  out$at <- ts(out$at, start = start(object$y), frequency = frequency(object$y))
  out$att <- ts(out$att, start = start(object$y), frequency = frequency(object$y))
  out
}
