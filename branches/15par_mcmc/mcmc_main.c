// mcmc_main.c
// Main routine of spinning MCMC code



#include <mcmc.h>

// Main program:
int main(int argc, char * argv[])
{
  // Interferometers are managed via the `database'; the `network' is a vector of pointers to the database (see below).
  // The interferometers that are actually used need to be initialised via the `ifoinit()'-function in order to determine noise PSD, signal FT &c.
  
  clock_t time0 = clock();
  
  if(domcmc>=1) printf("\n");
  printf("\n   Starting MCMC code...\n");
  int ifonr=0;
  double snr=0.0;
  
  useoldmcmcoutputformat = 0; //Set to 1 if you want to ... exactly!
  
  
  
  //Initialise stuff for the run
  struct runpar run;
  setconstants(&run);    //Set the global constants (which are variable in C). This routine should eventually disappear.
  run.setranpar  = (int*)calloc(npar,sizeof(int));
  sprintf(run.infilename,"mcmc.input"); //Default input filename
  if(argc > 1) sprintf(run.infilename,argv[1]);
  
  readlocalfile();                //Read system-dependent data, e.g. path to data files
  readinputfile(&run);            //Read data for this run from input.mcmc
  //setmcmcseed(&run);              //Set mcmcseed if 0, otherwise keep the current value
  setseed(&run.mcmcseed);         //Set mcmcseed if 0, otherwise keep the current value; more general routine
  setrandomtrueparameters(&run);  //Randomise the injection parameters where wanted
  writeinputfile(&run);           //Write run data to nicely formatted input.mcmc.<mcmcseed>
  
  if(waveformversion==1) {
    printf("   Using Apostolatos, 1.5PN, 12-parameter waveform.\n");
  } else if(waveformversion==2) {
    printf("   Using LAL, 3.5PN, 15-parameter waveform.\n");
  } else {
    printf("   Unknown waveform: %d.\n",waveformversion);
    exit(1);
  }
  
  //Set up the data for the IFOs you may want to use (H1,L1 + VIRGO by default)
  struct interferometer database[3];
  set_ifo_data(run, database);
  
  //Define interferometer network with IFOs.  The first run.networksize are actually used
  struct interferometer *network[3] = {&database[0], &database[1], &database[2]}; //H1L1V
  //struct interferometer *network[3] = {&database[0], &database[2], &database[1]}; //H1VL1
  int networksize = run.networksize;
  
  //Initialise interferometers, read and prepare data, inject signal (takes some time)
  if(networksize == 1) {
    printf("   Initialising 1 IFO, reading noise and data...\n");
  } else {
    printf("   Initialising %d IFOs, reading noise and data files...\n",networksize);
  }
  ifoinit(network, networksize);
  if(inject) {
    if(run.targetsnr < 0.001) printf("   A signal with the 'true' parameter values was injected.\n");
  } else {
    printf("   No signal was injected.\n");
  }
  
  
  //Get a parameter set to calculate SNR or write the wavefrom to disc
  struct parset dummypar;
  gettrueparameters(&dummypar);
  dummypar.loctc    = (double*)calloc(networksize,sizeof(double));
  dummypar.localti  = (double*)calloc(networksize,sizeof(double));
  dummypar.locazi   = (double*)calloc(networksize,sizeof(double));
  dummypar.locpolar = (double*)calloc(networksize,sizeof(double));
  localpar(&dummypar, network, networksize);
  
  
  //printf("  %lf  %lf  %lf  %lf\n",*dummypar.loctc,*dummypar.localti,*dummypar.locazi,*dummypar.locpolar);
  
  //Calculate SNR
  run.netsnr = 0.0;
  if(dosnr==1) {
    for(ifonr=0; ifonr<networksize; ++ifonr) {
      //printmuch=1;
      snr = signaltonoiseratio(&dummypar, network, ifonr);
      //printmuch=0;
      network[ifonr]->snr = snr;
      run.netsnr += snr*snr;
    }
    run.netsnr = sqrt(run.netsnr);
  }
  
  //Get the desired SNR by scaling the distance
  if(run.targetsnr > 0.001 && inject==1) {
    truepar[3] *= (run.netsnr/run.targetsnr);  //Use total network SNR
    //truepar[3] *= (run.netsnr/(run.targetsnr*sqrt((double)networksize)));  //Use geometric average SNR
    printf("   Setting distance to %lf Mpc to get a network SNR of %lf.\n",truepar[3],run.targetsnr);
    gettrueparameters(&dummypar);
    dummypar.loctc    = (double*)calloc(networksize,sizeof(double));
    dummypar.localti  = (double*)calloc(networksize,sizeof(double));
    dummypar.locazi   = (double*)calloc(networksize,sizeof(double));
    dummypar.locpolar = (double*)calloc(networksize,sizeof(double));
    localpar(&dummypar, network, networksize);
    
    //Recalculate SNR
    run.netsnr = 0.0;
    if(dosnr==1) {
      for(ifonr=0; ifonr<networksize; ++ifonr) {
	snr = signaltonoiseratio(&dummypar, network, ifonr);
	network[ifonr]->snr = snr;
	run.netsnr += snr*snr;
      }
      run.netsnr = sqrt(run.netsnr);
    }
    
    for(ifonr=0; ifonr<networksize; ++ifonr) ifodispose(network[ifonr]);
    //Reinitialise interferometers, read and prepare data, inject signal (takes some time)
    if(networksize == 1) {
      printf("   Reinitialising 1 IFO, reading data...\n");
    } else {
      printf("   Reinitialising %d IFOs, reading datafiles...\n",networksize);
    }
    ifoinit(network, networksize);
    printf("   A signal with the 'true' parameter values was injected.\n");
  }
  
  
  
  //Calculate 'null-likelihood'
  struct parset nullpar;
  getnullparameters(&nullpar);
  nullpar.loctc    = (double*)calloc(networksize,sizeof(double));
  nullpar.localti  = (double*)calloc(networksize,sizeof(double));
  nullpar.locazi   = (double*)calloc(networksize,sizeof(double));
  nullpar.locpolar = (double*)calloc(networksize,sizeof(double));
  localpar(&nullpar, network, networksize);
  run.logL0 = net_loglikelihood(&nullpar, networksize, network);
  if(inject == 0) run.logL0 *= 1.01;  //If no signal is injected, presumably there is one present in the data; enlarge the range that log(L) can take by lowering Lo (since L>Lo is forced)
  
  
  
  //Write the signal and its FFT to disc
  if(writesignal) {
    for(ifonr=0; ifonr<networksize; ++ifonr) {
      //printmuch=1;
      writesignaltodisc(&dummypar, network, ifonr);
      //printmuch=0;
    }
  }
  writesignal=0;
  
  
  //Write some injection-signal parameters to screen (used to be in the MCMC routine)
  printf("\n\n");
  printf("   Global     :    Source position:  RA: %5.2lfh, dec: %6.2lfd;  J0 points to:  RA: %5.2lfh, dec: %6.2lfd;   inclination J0: %5.2lfd  \n",rightAscension(dummypar.longi,GMST(dummypar.tc))*r2h,asin(dummypar.sinlati)*r2d,rightAscension(dummypar.phiJ0,GMST(dummypar.tc))*r2h,asin(dummypar.sinthJ0)*r2d,(pi/2.0-acos(dummypar.NdJ))*r2d);
  for(ifonr=0;ifonr<networksize;ifonr++) printf("   %-11s:    theta: %5.1lfd,  phi: %5.1lfd;   azimuth: %5.1lfd,  altitude: %5.1lfd\n",network[ifonr]->name,dummypar.localti[ifonr]*r2d,dummypar.locazi[ifonr]*r2d,fmod(pi-(dummypar.locazi[ifonr]+network[ifonr]->rightarm)+mtpi,tpi)*r2d,(pi/2.0-dummypar.localti[ifonr])*r2d);
  
  printf("\n  %10s  %10s  %6s  %20s  %6s  ","niter","nburn","seed","null likelihood","ndet");
  for(ifonr=0;ifonr<networksize;ifonr++) printf("%16s%4s  ",network[ifonr]->name,"SNR");
  printf("%20s  ","Network SNR");
  printf("\n  %10d  %10d  %6d  %20.10lf  %6d  ",iter,nburn,run.mcmcseed,run.logL0,networksize);
  for(ifonr=0;ifonr<networksize;ifonr++) printf("%20.10lf  ",network[ifonr]->snr);
  printf("%20.10lf\n\n",run.netsnr);
  printf("    %8s  %8s  %17s  %8s  %8s  %8s  %8s  %8s  %8s  %8s  %8s  %8s\n", "Mc","eta","tc","logdL","spin","kappa","longi","sinlati","phase","sinthJ0","phiJ0","alpha");
  printf("    %8.5f  %8.5f  %17.6lf  %8.5f  %8.5f  %8.5f  %8.5f  %8.5f  %8.5f  %8.5f  %8.5f  %8.5f\n\n", dummypar.mc,dummypar.eta,dummypar.tc,dummypar.logdl,dummypar.spin,dummypar.kappa,dummypar.longi,dummypar.sinlati,dummypar.phase,dummypar.sinthJ0,dummypar.phiJ0,dummypar.alpha);
  printf("    %8s  %8s  %17s  %8s  %8s  %8s  %8s  %8s  %8s  %8s  %8s  %8s\n", "M1","M2","tc","d_L","spin","th_SL","RA","Dec","phase","th_J0","phi_J0","alpha");
  printf("    %8.5f  %8.5f  %17.6lf  %8.2f  %8.5f  %8.4f  %8.4f  %8.4f  %8.4f  %8.4f  %8.4f  %8.4f\n\n", dummypar.m1,dummypar.m2,dummypar.tc,exp(dummypar.logdl),dummypar.spin,acos(dummypar.kappa)*r2d,rightAscension(dummypar.longi,GMST(dummypar.tc))*r2h,asin(dummypar.sinlati)*r2d,dummypar.phase*r2d,asin(dummypar.sinthJ0)*r2d,dummypar.phiJ0*r2d,dummypar.alpha*r2d);
  
  
  
  
  //Do MCMC
  clock_t time1 = clock();
  if(domcmc==1) {
    //printmuch=1;
    mcmc(&run, network);
    //printmuch=0;
  }
  clock_t time2 = clock();
  
  
  
  
  
  //Calculate matches between two signals
  if(domatch==1) {
    if(1==2) {
      printf("\n\n");
      gettrueparameters(&dummypar);
      dummypar.loctc    = (double*)calloc(networksize,sizeof(double));
      dummypar.localti  = (double*)calloc(networksize,sizeof(double));
      dummypar.locazi   = (double*)calloc(networksize,sizeof(double));
      dummypar.locpolar = (double*)calloc(networksize,sizeof(double));
      
      FILE *fout;
      fout = fopen("tc.dat","w");
      double fac=0.0;
      double matchpar = dummypar.tc,matchres=0.0;
      for(fac=-0.002;fac<0.002;fac+=0.00005) {
	dummypar.tc = matchpar+fac;
	for(ifonr=0;ifonr<networksize;ifonr++) {
	  localpar(&dummypar, network, networksize);
	  matchres = match(&dummypar,network,ifonr,networksize);
	  printf("%10.6f  %10.6f\n",fac,matchres);
	  fprintf(fout,"%10.6f  %10.6f\n",fac,matchres);
	}
      }
      fclose(fout);
    }
    
    
    //Compute match between waveforms with parameter sets par1 and par2
    if(1==1) {
      printf("\n\n");
      struct parset par1;
      allocparset(&par1, networksize);
      struct parset par2;
      allocparset(&par2, networksize);
      
      double eta=0.0;
      //for(eta=0.01;eta<0.25001;eta+=0.001) {
      //for(eta=0.1;eta<0.12001;eta+=0.001) {
      for(eta=0.111;eta<0.1111;eta+=0.001) {
	getparameterset(&par1, 3.0,0.11,700009012.346140,3.0, 0.5,0.9,3.0,0.5, 1.0,0.1,2.0,3.0);
	
	getparameterset(&par2, 3.0,eta,700009012.346140,3.0, 0.5,0.9,3.0,0.5, 1.0,0.1,2.0,3.0);
	
	double matchres = parmatch(&par1,&par2,network, networksize);
	double overlap = paroverlap(&par1,&par2,network,0);
	
	printf("   Eta: %6.4f,  match: %10.5lf,  overlap: %g \n",eta,matchres,overlap);
      }
      printf("\n");
      
      freeparset(&par1);
      freeparset(&par2);
    }
  
    //Compute Fisher matrix for parameter set par
    if(1==1) {
      printf("\n\n  Computing Fisher matrix...\n\n");
      int i=0, j=0;
      struct parset par;
      double **matrix  = (double**)calloc(npar,sizeof(double*));
      for(i=0;i<npar;i++) matrix[i]  = (double*)calloc(npar,sizeof(double));
      allocparset(&par, networksize);
      getparameterset(&par, 3.0,0.11,700009012.346140,3.0, 0.5,0.9,3.0,0.5, 1.0,0.1,2.0,3.0);
      
      //computeFishermatrixIFO(par,npar,network,networksize,0,matrix);
      //computeFishermatrix(&par,npar,network,networksize,matrix);
      
      for(i=0;i<npar;i++) {
	for(j=0;j<npar;j++) {
	  printf("  %12.4g",matrix[i][j]/(double)networksize);
	}
	printf("\n\n");
      }
      printf("\n\n");
      
      
      
      freeparset(&par);
      for(i=0;i<npar;i++) {
	free(matrix[i]);
      }
      free(matrix);
    }
  } //if(domatch=1)

  
  
    
  
  //Get rid of allocated memory and quit
  for(ifonr=0; ifonr<networksize; ++ifonr) ifodispose(network[ifonr]);
  free(run.setranpar);
  freeparset(&nullpar);
  freeparset(&dummypar);
  
  clock_t time3 = clock();
  printf("   Timimg:\n");
  if(domcmc>=1) {
    printf("     initialisation:%10.2lfs\n", ((double)time1 - (double)time0)*1.e-6 );
    printf("     MCMC:          %10.2lfs\n", ((double)time2 - (double)time1)*1.e-6 );
  }
  printf("     total time:    %10.2lfs\n", (double)time3*1.e-6);
  
  
  printf("\n   MCMC code done.\n\n");
  if(domcmc>=1) printf("\n");
  return 0;
}

