#include <TMB.hpp>
template<class Type> Type objective_function<Type>::operator()(){
   // Data:
   DATA_VECTOR(ux);             // Vector of unique size values.
   DATA_IVECTOR(x_imm);         // Immature size measurements (n_imm).
   DATA_VECTOR(f_imm);          // Immature frequency observations (n_imm).
   DATA_IVECTOR(year_imm);      // Immature survey year (n_imm).
   DATA_IVECTOR(x_mat);         // Mature size measurements (n_mat).
   DATA_VECTOR(f_mat);          // Mature frequency observations (n_mat).
   DATA_IVECTOR(year_mat);      // Mature survey year (n_mat).
   DATA_SCALAR(delta_x);        // Size-measurement bin width.   
   
   // Instar growth parameters:                       
   PARAMETER(mu0);                             // First instar mean size.
   PARAMETER(log_sigma0);                      // Log-scale standard error for first instar.
   PARAMETER_VECTOR(log_hiatt_slope);          // Log-scale Hiatt slope parameters.
   PARAMETER_VECTOR(log_hiatt_intercept);      // Log-scale Hiatt intercept parameters.
   PARAMETER_VECTOR(log_growth_error);         // Log-scale Growth increment error inflation parameters.
   PARAMETER_VECTOR(mu_year_instar);           // Log-scale instar mean size year interaction (n_instar x n_year).
   PARAMETER(log_sigma_mu_year_instar);        // Log-scale instar mean year interaction error term.
   PARAMETER(delta_mat);                       // Maturity growth scaling factor.
   
   // Recruitment parameters:
   PARAMETER_VECTOR(log_n_imm_instar_0);       // First instar recruitment for all years (n_year).
   PARAMETER(log_sigma_n_imm_instar_0);        // Log-scale first instar annual recruitment error parameter.
   
   // Abundance parameters:
   PARAMETER_VECTOR(log_n_imm_year_0);         // First year immature instar abundances (n_instar-1).
   PARAMETER_VECTOR(log_n_skp_instar_0);       // First year skip abundances (n_instar - 5).
   PARAMETER_VECTOR(log_n_mat_instar_0);       // Mature abundances for first year ((n_instar-5) x 6).  
   
   // Selectivity parameters:
   PARAMETER(selectivity_x50);                 // Size-at-50% trawl selectivity.
   PARAMETER(log_selectivity_slope);           // Log-scale trawl selectivity slope.
   PARAMETER_VECTOR(log_year_effect);          // Log-scale year effect on abundance (n_year-1).
   PARAMETER(log_sigma_year_effect);           // Log-scale year effect error parameter.
 
   // Fishing selectivity parameters:
   PARAMETER(selectivity_x50_fishing);         // Size-at-50% selectivity for fishing.
   PARAMETER(log_selectivity_slope_fishing);   // Log-scale trawl selectivity slope for fishing.
   PARAMETER(logit_fishing_effect_rec);        // Logit-scale fishing effect for mature recruits.
   PARAMETER(logit_fishing_effect_res);        // Logit-scale fishing effect for mature residuals.

   PARAMETER_VECTOR(logit_year_fishing_effect_rec); // Logit-scale year x fishing effect for mature recruits.
   PARAMETER_VECTOR(logit_year_fishing_effect_res); // Logit-scale year x fishing effect for mature residuals.
   PARAMETER(log_sigma_year_fishing_effect);      // Log-scale year x fishing effect error parameter (n_year-1).
     
   // Moulting probability parameters:
   PARAMETER_VECTOR(logit_p_skp);              // Logit-scale skip-moulting probabilities (n_instar-1).
   PARAMETER_VECTOR(logit_p_mat);              // Logit-scale moult-to-maturity probabilities (n_instar-2).
   PARAMETER_VECTOR(logit_p_mat_year);         // Logit-scale mout-to-maturity instar x year interaction (n_instar-2 x n_year-1).
   PARAMETER(log_sigma_p_mat_year);            // Moult-to-maturity instar x year interaction error term.
   
   // Mortality parameters:
   PARAMETER(logit_M_imm);                     // Logit-scale immature mortality.
   PARAMETER_VECTOR(logit_M_mat);              // Logit-scale mature mortality.     
   
   // Initialize log-likelihood:
   Type v = 0;
   
   // Vector sizes:      
   int ni = x_imm.size();                           // Number of immature observations.
   int nm = x_mat.size();                           // Number of mature observations.
   int n_instar = log_n_imm_year_0.size() + 1;      // Number instars.
   int n_year   = log_n_imm_instar_0.size();        // Number of years.
   int n_size   = ux.size();                        // Number of unique size values.
   
   // Instar global mean and error:
   vector<Type> hiatt_intercept = exp(log_hiatt_intercept);
   vector<Type> hiatt_slope     = exp(log_hiatt_slope);
   vector<Type> growth_error    = exp(log_growth_error);
   vector<Type> mu(n_instar);
   vector<Type> log_sigma(n_instar);
   mu[0] = mu0;
   log_sigma[0] = log_sigma0; 
   for (int k = 1; k < 5; k++){
      mu[k] = hiatt_intercept[0] + mu[k-1] + hiatt_slope[0] * mu[k-1];
      log_sigma[k] = log(1 + hiatt_slope[0] + growth_error[0]) + log_sigma[k-1];
   }
   for (int k = 5; k < n_instar; k++){
      mu[k] = hiatt_intercept[1] + mu[k-1] + hiatt_slope[1] * mu[k-1];
      log_sigma[k] = log(1 + hiatt_slope[1] + growth_error[1]) + log_sigma[k-1];
   }
   vector<Type> sigma = exp(log_sigma);
   
   // Annual instar sizes for immatures:
   matrix<Type> mu_imm(n_instar,n_year);
   for (int k = 0; k < n_instar; k++){
      for (int y = 0; y < n_year; y++){
         v -= dnorm(mu_year_instar[y * n_instar + k], Type(0), exp(log_sigma_mu_year_instar), true); // Random effect.
         mu_imm(k,y) = mu[k] + mu_year_instar[y * n_instar + k];
      }
   }
   
   // Define mature instar sizes for recruitment and first year:
   array<Type> mu_mat(n_instar,n_year,6);
   for (int k = 0; k < n_instar; k++){
      for (int m = 0; m < 6; m++){
         mu_mat(k,0,m) = mu_imm(k,0) + delta_mat; // Instar sizes for first year matures are all identical.
      }
      for (int y = 1; y < n_year; y++){
         mu_mat(k,y,0) = mu_imm(k,y) + delta_mat; // Mature recruitment sizes for subsequent years.
      }
   }  
   
   // Define mature residual sizes using previous years' values:  
   for (int k = 0; k < n_instar; k++){
      for (int y = 1; y < n_year; y++){   
         for (int m = 1; m < 6; m++){ 
            mu_mat(k,y,m) = mu_mat(k,y-1,m-1);
         }
      }
   }
   
   // Population abundance variables:
   matrix<Type> n_imm(n_instar,n_year);   // Immatures.
   matrix<Type> n_skp(n_instar,n_year);   // Immature skip-moulters.
   array<Type>  n_mat(n_instar,n_year,6,n_size); // Matures.
   
   // Immature abundances for first year and first instar: 
   v -= sum(dnorm(log_n_imm_instar_0, 0, exp(log_sigma_n_imm_instar_0), true)); 
   for (int k = 1; k < n_instar; k++){ 
      n_imm(k,0) = exp(log_n_imm_year_0[k-1]);   // First-year immatures.
   }
   for (int y = 0; y < n_year; y++){ 
      n_imm(0,y) = exp(log_n_imm_instar_0[y]);  // First instar recruits.
   } 
   
   // Skip abundances for first year and first instar: 
   for (int k = 0; k < 5; k++){
      n_skp(k,0) = 0; 
   } 
   for (int k = 5; k < n_instar; k++){
      n_skp(k,0) = exp(log_n_skp_instar_0[k-5]);  
   }
   for (int y = 1; y < n_year; y++){ 
      n_skp(0,y) = 0;  
   }
   
   // Set all matures abundances smaller than reference instar to zero: 
   for (int k = 0; k < 5; k++){ 
      for (int y = 0; y < n_year; y++){ 
         for (int m = 0; m < 6; m++){ 
            for (int l = 0; l < n_size; l++){
               n_mat(k,y,m,l) = 0; // First year small mature recruits set to zero.
            }
         }
      }
   }
   
   // Initialize mature abundances for first year:
   for (int k = 5; k < n_instar; k++){
      for (int m = 0; m < 6; m++){ 
         for (int l = 0; l < n_size; l++){
            n_mat(k,0,m,l) = exp(log_n_mat_instar_0[(k-5) * 6 + m]) * 
                            (pnorm(ux[l] + delta_x / 2, mu_mat(k,0,m), sigma[k]) - 
                             pnorm(ux[l] - delta_x / 2, mu_mat(k,0,m), sigma[k]));   
         }
      }
   }
   
   // Moulting probabilities:
   vector<Type> p_skp = Type(1) / (Type(1) + exp(-logit_p_skp)); // Skip-moulting probabilities.
   v -= sum(dnorm(logit_p_mat_year, 0, exp(log_sigma_p_mat_year), true)); 
   matrix<Type> p_mat(n_instar-1, n_year-1);
   for (int y = 0; y < (n_year-1); y++){
      for (int k = 0; k < (n_instar-2); k++){
         p_mat(k,y) = Type(1) / (Type(1) + exp(-logit_p_mat[k] - logit_p_mat_year[k * (n_year-1) + y]));
      }
      p_mat(n_instar-2,y) = Type(1); // Second-to-last instar moult to maturity.
   }
   
   // Mortality probabilities:
   Type M_imm = Type(1) / (Type(1) + exp(-logit_M_imm));         // Immature annual mortality.
   vector<Type> M_mat = Type(1) / (Type(1) + exp(-logit_M_mat)); // Mature annual mortality.
 
   // Fishing selectivity parameter transforms:
   Type selectivity_slope = exp(log_selectivity_slope);
   Type selectivity_slope_fishing = exp(log_selectivity_slope_fishing);
   v -= sum(dnorm(logit_year_fishing_effect_rec, Type(0), exp(log_sigma_year_fishing_effect), true));
   v -= sum(dnorm(logit_year_fishing_effect_res, Type(0), exp(log_sigma_year_fishing_effect), true));
   vector<Type> fishing_effect_rec = Type(1) / (Type(1) + exp(-logit_fishing_effect_rec - logit_year_fishing_effect_rec));
   vector<Type> fishing_effect_res = Type(1) / (Type(1) + exp(-logit_fishing_effect_res - logit_year_fishing_effect_res));   
   
   //Type fishing_effect_rec = Type(1) / (Type(1) + exp(-logit_fishing_effect_rec));
   //Type fishing_effect_res = Type(1) / (Type(1) + exp(-logit_fishing_effect_res));
   
   // Population dynamics equations:
   for (int k = 1; k < n_instar; k++){
      for (int y = 1; y < n_year; y++){
          // Immatures:
          n_imm(k,y) = (Type(1)-p_mat(k-1,y-1)) * (Type(1)-p_skp[k-1]) * (Type(1)-M_imm) * n_imm(k-1,y-1); 
            
          // Skip-moulters:
          n_skp(k,y) = (Type(1)-p_mat(k-1,y-1)) * p_skp[k-1] * (Type(1)-M_imm) * n_imm(k,y-1);   
          
          // Matures:
          for (int l = 0; l < n_size; l++){
             // Define fishing size-selectivity curve for recruitment and residuals:
             Type selectivity_rec = fishing_effect_rec[y-1] / (Type(1) + exp(-selectivity_slope_fishing * (ux[l] - selectivity_x50_fishing)));
             Type selectivity_res = fishing_effect_res[y-1] / (Type(1) + exp(-selectivity_slope_fishing * (ux[l] - selectivity_x50_fishing)));
                
             // Mature recruitment:
             n_mat(k,y,0,l) = (Type(1)-selectivity_rec) * 
                              (Type(1)-M_mat[0]) * 
                              ((Type(1)-p_skp[k-1]) * p_mat(k-1,y-1) * n_imm(k-1,y-1) + n_skp(k-1,y-1)) *
                              (pnorm(ux[l] + delta_x / 2, mu_mat(k,y,0), sigma[k]) - 
                               pnorm(ux[l] - delta_x / 2, mu_mat(k,y,0), sigma[k]));
         
             // Mature residual groups:
             for (int m = 1; m < 6; m++){
                n_mat(k,y,m,l) = (Type(1)-selectivity_res) * (Type(1)-M_mat[1]) * n_mat(k,y-1,m-1,l); 
             }
         }
      }
   } 
   
   // Year effects:
   v -= sum(dnorm(log_year_effect, Type(0), exp(log_sigma_year_effect), true));
   vector<Type> year_effect = exp(log_year_effect);
   
   // Likelihood evaluation for immatures:
   vector<Type> eta_imm(ni);
   eta_imm.fill(0);
   for (int i = 0; i < ni; i++){ 
      // Define trawl size-selectivity curve:
      Type selectivity = Type(1) / (Type(1) + exp(-selectivity_slope * (ux[x_imm[i]] - selectivity_x50)));
      
      for (int k = 0; k < n_instar; k++){
         eta_imm[i] += year_effect[year_imm[i]] * 
                       selectivity *
                      (n_imm(k,year_imm[i]) + n_skp(k,year_imm[i])) * 
                      (pnorm(ux[x_imm[i]] + delta_x / 2, mu_imm(k,year_imm[i]), sigma[k]) - 
                       pnorm(ux[x_imm[i]] - delta_x / 2, mu_imm(k,year_imm[i]), sigma[k]));
      }
      if (eta_imm[i] > 0){ 
         v -= dpois(f_imm[i], eta_imm[i], true);
      }
   }
   
   // Likelihood evaluation for matures:
   vector<Type> eta_rec(nm);
   vector<Type> eta_res(nm);
   vector<Type> eta_mat(nm);
   eta_rec.fill(0);
   eta_res.fill(0);
   eta_mat.fill(0);

   for (int i = 0; i < nm; i++){ 
      // Define trawl size-selectivity curve:
      Type selectivity = Type(1) / (Type(1) + exp(-selectivity_slope * (ux[x_mat[i]] - selectivity_x50))); 

      // Loop over instars:
      for (int k = 0; k < n_instar; k++){
         // Recruitment
         eta_rec[i] += n_mat(k, year_mat[i], 0, x_mat[i]);
      
         // Cumulate recruitment and residual classes:
         for (int m = 1; m < 6; m++){
            eta_res[i] += n_mat(k, year_mat[i], m, x_mat[i]);
         }
         
         // Add year effects and selectivity adjustments:
         eta_rec[i] += year_effect[year_mat[i]] * selectivity * eta_rec[i];
         eta_res[i] += year_effect[year_mat[i]] * selectivity * eta_res[i];
         
         // Calculate total matures:
         eta_mat[i] += eta_rec[i] + eta_res[i];
      }
      
      // Poisson likelihood:
      if (eta_mat[i] > 0){ 
         v -= dpois(f_mat[i], eta_mat[i], true);
      }
   }   

   // Export results:
   REPORT(mu);
   REPORT(sigma);
   REPORT(mu_imm);
   REPORT(mu_mat);
   REPORT(p_skp);
   REPORT(p_mat);
   REPORT(n_imm);
   REPORT(n_skp);
   REPORT(n_mat);
   REPORT(eta_imm);
   REPORT(eta_rec);
   REPORT(eta_res);
   REPORT(eta_mat);
   REPORT(M_imm);
   REPORT(M_mat);
   REPORT(year_effect);
   REPORT(selectivity_slope);
   REPORT(fishing_effect_rec);
   REPORT(fishing_effect_res);
   REPORT(selectivity_slope_fishing);

   return v;
}
