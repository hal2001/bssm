#' Gaussian approximation of non-Gaussian state space model
#'
#' Returns the approximating Gaussian model.
#' @param object model object.
#' @param max_iter Maximum number of iterations.
#' @param conv_tol Tolerance parameter.
#' @param ... Ignored.
#' @export
#' @rdname gaussian_approx
gaussian_approx <- function(object, max_iter, conv_tol, ...) {
  UseMethod("gaussian_approx", object)
}
#' @method gaussian_approx ngssm
#' @export
gaussian_approx.ngssm<- function(object, max_iter = 100, conv_tol = 1e-8, ...) {
  
  object$distribution <- 
    pmatch(object$distribution, c("poisson", "binomial", "negative binomial"))
  out <- 
    gaussian_approx_model(object, object$initial_mode, max_iter, conv_tol, model_type = 1L)
  out$y <- ts(out$y, start = start(object$y), end = end(object$y), frequency = frequency(object$y))
  model <- gssm(y = out$y, Z = object$Z, H = out$H, T = object$T, R = object$R, a1 = object$a1, P1 = object$P1,
    obs_intercept = object$obs_intercept, state_intercept = object$state_intercept, 
    state_names = names(object$a1))
  model$xreg <- object$xreg
  model$coefs <- object$coefs
  model
  }
#' @method gaussian_approx ng_bsm
#' @rdname gaussian_approx
#' @export
gaussian_approx.ng_bsm <- function(object, max_iter = 100, conv_tol = 1e-8, ...) {
  
  object$distribution <- pmatch(object$distribution, c("poisson", "binomial", "negative binomial"))
  out <- gaussian_approx_model(object, object$initial_mode, max_iter, conv_tol, model_type = 2L)
  out$y <- ts(out$y, start = start(object$y), end = end(object$y), frequency = frequency(object$y))
  model <- gssm(y = out$y, Z = object$Z, H = out$H, T = object$T, R = object$R, a1 = object$a1, P1 = object$P1,
    obs_intercept = object$obs_intercept, state_intercept = object$state_intercept, 
    state_names = names(object$a1))
  model$xreg <- object$xreg
  model$coefs <- object$coefs
  model
}
#' @method gaussian_approx svm
#' @export
gaussian_approx.svm <- function(object, max_iter = 100, conv_tol = 1e-8, ...) {
  
  out <- gaussian_approx_model(object, object$initial_mode, max_iter, conv_tol, model_type = 3L)
  out$y <- ts(out$y, start = start(object$y), end = end(object$y), frequency = frequency(object$y))
  model <- gssm(y = out$y, Z = object$Z, H = out$H, T = object$T, R = object$R, a1 = object$a1, P1 = object$P1,
    obs_intercept = object$obs_intercept, state_intercept = object$state_intercept, 
    state_names = names(object$a1))
  model$xreg <- object$xreg
  model$coefs <- object$coefs
  model
}
#' @method gaussian_approx ng_ar1
#' @export
gaussian_approx.ng_ar1 <- function(object, max_iter = 100, conv_tol = 1e-8, ...) {
  
  object$distribution <- pmatch(object$distribution, c("poisson", "binomial", "negative binomial"))
  out <- gaussian_approx_model(object, object$initial_mode, max_iter, conv_tol, model_type = 4L)
  out$y <- ts(out$y, start = start(object$y), end = end(object$y), frequency = frequency(object$y))
  model <- gssm(y = out$y, Z = object$Z, H = out$H, T = object$T, R = object$R, a1 = object$a1, P1 = object$P1,
    obs_intercept = object$obs_intercept, state_intercept = object$state_intercept, 
    state_names = names(object$a1))
  model$xreg <- object$xreg
  model$coefs <- object$coefs
  model
}

#' @method gaussian_approx nlg_ssm
#' @export
gaussian_approx.nlg_ssm <- function(object, max_iter = 100, 
  conv_tol = 1e-8, iekf_iter = 0, ...) {
  
  out <- gaussian_approx_model_nlg(t(object$y), object$Z, object$H, object$T, 
    object$R, object$Z_gn, object$T_gn, object$a1, object$P1, 
    object$theta, object$log_prior_pdf, object$known_params, 
    object$known_tv_params, object$n_states, object$n_etas,
    as.integer(object$time_varying),
    max_iter, conv_tol, iekf_iter)
  out$y <- ts(c(out$y), start = start(object$y), end = end(object$y), frequency = frequency(object$y))
  gssm(y = out$y, Z = matrix(out$Z, nrow=length(out$a1)), 
    H = c(out$H), T = out$T, R = out$R, a1 = c(out$a1), 
    P1 = out$P1, obs_intercept = out$D, state_intercept = out$C)
}
