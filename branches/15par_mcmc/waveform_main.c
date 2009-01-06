// waveform_main.c:
// Main routine to generate and save waveforms



#include <mcmc.h>

// Main program:
int main(int argc, char * argv[])
{
  // Interferometers are managed via the `database'; the `network' is a vector of pointers to the database (see below).
  // The interferometers that are actually used need to be initialised via the `ifoinit()'-function in order to determine noise PSD, signal FT &c.
  
  printf("\n\n   Starting MCMC code...\n");
  int i;
  double snr;
  
  waveformversion = 2;  // 1: Apostolatos, 1.5PN, 12 par.  2: LAL 3.5PN, 15 par
  useoldmcmcoutputformat = 0; //Set to 1 if you want to ... exactly!
  
  //Initialise stuff for the run
  struct runpar run;
  setconstants();    //Set the global constants (which are variable in C). This routine should eventually disappear.
  run.setranpar  = (int*)calloc(npar,sizeof(int));
  sprintf(run.infilename,"mcmc.input"); //Default input filename
  if(argc > 1) sprintf(run.infilename,argv[1]);

  readlocalfile();       //Read system-dependent data, e.g. path to data files
  readinputfile(&run);   //Read data for this run from input.mcmc
  //setmcmcseed(&run);     //Set mcmcseed (if 0), or use the current value
  setseed(&run.mcmcseed);         //Set mcmcseed if 0, otherwise keep the current value; more general routine
  setrandomtrueparameters(&run);  //Randomise the injection parameters where wanted
  writeinputfile(&run);  //Write run data to nicely formatted input.mcmc.<mcmcseed>
  
  dosnr = 1;
  domcmc = 0;
  domatch = 0;
  writesignal = 0;
  
  //Set up the data for the IFOs you may want to use (H1,L1 + VIRGO by default)
  struct interferometer database[3];
  set_ifo_data(run, database);
  
  //Define interferometer network which IFOs.  The first run.networksize are actually used
  struct interferometer *network[3] = {&database[0], &database[1], &database[2]};
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
  
  /*
  //Calculate 'null-likelihood'
  struct parset nullpar;
  getnullparameters(&nullpar);
  nullpar.loctc    = (double*)calloc(networksize,sizeof(double));
  nullpar.localti  = (double*)calloc(networksize,sizeof(double));
  nullpar.locazi   = (double*)calloc(networksize,sizeof(double));
  nullpar.locpolar = (double*)calloc(networksize,sizeof(double));
  localpar(&nullpar, network, networksize);
  run.logL0 = net_loglikelihood(&nullpar, networksize, network);
  if(inject == 0) run.logL0 *= 1.01;  //If no signal is injected, presumably there is one present in the data; enlarge the range that log(L) can take by owering Lo (since L>Lo is forced)
  */
  
  //Get a parameter set to calculate SNR or write the wavefrom to disc
  struct parset dummypar;
  gettrueparameters(&dummypar);
  dummypar.loctc    = (double*)calloc(networksize,sizeof(double));
  dummypar.localti  = (double*)calloc(networksize,sizeof(double));
  dummypar.locazi   = (double*)calloc(networksize,sizeof(double));
  dummypar.locpolar = (double*)calloc(networksize,sizeof(double));
  localpar(&dummypar, network, networksize);
  
  
  //Calculate SNR
  if(dosnr==1) {
    for (i=0; i<networksize; ++i) {
      snr = signaltonoiseratio(&dummypar, network, i);
      network[i]->snr = snr;
    }
  }
  
  //Write the signal and its FFT to disc
  writesignal = 1;
  if(writesignal) {
    for (i=0; i<networksize; ++i) {
      //printmuch=1;
      writesignaltodisc(&dummypar, network, i);
      //printmuch=0;
    }
  }
  writesignal=0;
  
  /*
  //Do MCMC
  if(domcmc==1) {
    //printmuch=1;
    mcmc(&run, network);
    //printmuch=0;
  } else {
  */
  printf("%10s  %10s  %6s  %20s  %6s  ","niter","nburn","seed","null likelihood","ndet");
  for(i=0;i<networksize;i++) {
    printf("%16s%4s  ",network[i]->name,"SNR");
  }
  printf("\n%10d  %10d  %6d  %20.10lf  %6d  ",iter,nburn,run.mcmcseed,run.logL0,networksize);
  for(i=0;i<networksize;i++) {
    printf("%20.10lf  ",network[i]->snr);
  }
  printf("\n\n%8s  %8s  %17s  %8s  %8s  %8s  %8s  %8s  %8s  %8s  %8s  %8s\n",
	 "Mc","eta","tc","logdL","sinlati","longi","phase","spin","kappa","sinthJ0","phiJ0","alpha");
  printf("%8.5f  %8.5f  %17.6lf  %8.5f  %8.5f  %8.5f  %8.5f  %8.5f  %8.5f  %8.5f  %8.5f  %8.5f\n\n",
	 dummypar.mc,dummypar.eta,dummypar.tc,dummypar.logdl,dummypar.sinlati,dummypar.longi,dummypar.phase,dummypar.spin,dummypar.kappa,dummypar.sinthJ0,dummypar.phiJ0,dummypar.alpha);
  //}
  
  //Calculate matches between two signals
  if(domatch==1) {
    printf("\n");
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
      for(i=0;i<networksize;i++) {
        localpar(&dummypar, network, networksize);
        matchres = match(&dummypar,network,i,networksize);
        printf("%10.6f  %10.6f\n",fac,matchres);
        fprintf(fout,"%10.6f  %10.6f\n",fac,matchres);
      }
    }
    fclose(fout);
  }
  
  
  //Get rid of allocated memory and quit  
  for (i=0; i<networksize; ++i)
    ifodispose(network[i]);
  
  /*
  free(nullpar.loctc);
  free(nullpar.localti);
  free(nullpar.locazi);
  free(nullpar.locpolar);
  */
  
  free(dummypar.loctc);
  free(dummypar.localti);
  free(dummypar.locazi);
  free(dummypar.locpolar);
  
  
  printf("\n   Waveform code done.\n\n\n");
  return 0;
}







