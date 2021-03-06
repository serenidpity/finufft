#include "../src/finufft.h"
#include "../src/dirft.h"
#include <math.h>
#include <vector>
#include <stdio.h>
#include <iostream>
#include <iomanip>

// how big a problem to check direct DFT for in 1D...
#define BIGPROB 1e8

// for omp rand filling
#define CHUNK 1000000

int main(int argc, char* argv[])
/* Test executable for finufft in 1d, all 3 types.

   Usage: finufft1d_test [Nmodes [Nsrc [tol [debug [spread_sort]]]]]

   debug = 0: rel errors and overall timing, 1: timing breakdowns
           2: also spreading output

   Example: finufft1d_test 1000000 1000000 1e-12

   Barnett 1/22/17 - 2/9/17
*/
{
  INT M = 1e6, N = 1e6;      // defaults: M = # srcs, N = # modes out
  double w, tol = 1e-6;      // default
  nufft_opts opts;
  opts.debug = 0;            // 1 to see sub-timings
  opts.spread_sort = 1;      // default
  int isign = +1;            // choose which exponential sign to test
  if (argc>1) { sscanf(argv[1],"%lf",&w); N = (INT)w; }
  if (argc>2) { sscanf(argv[2],"%lf",&w); M = (INT)w; }
  if (argc>3) {
    sscanf(argv[3],"%lf",&tol);
    if (tol<=0.0) { printf("tol must be positive!\n"); return 1; }
  }
  if (argc>4) sscanf(argv[4],"%d",&opts.debug);
  opts.spread_debug = (opts.debug>1) ? 1 : 0;  // see output from spreader
  if (argc>5) sscanf(argv[5],"%d",&opts.spread_sort);  // 0 or 1
  if (argc==1 || argc>6) {
    fprintf(stderr,"Usage: finufft1d_test [Nmodes [Nsrc [tol [debug [spread_sort]]]]]\n");
    return 1;
  }
  cout << scientific << setprecision(15);

  FLT *x = (FLT *)malloc(sizeof(FLT)*M);        // NU pts
  CPX* c = (CPX*)malloc(sizeof(CPX)*M);   // strengths 
  CPX* F = (CPX*)malloc(sizeof(CPX)*N);   // mode ampls
#pragma omp parallel
  {
    unsigned int se=MY_OMP_GET_THREAD_NUM();  // needed for parallel random #s
#pragma omp for schedule(dynamic,CHUNK)
    for (INT j=0; j<M; ++j) {
      x[j] = PI*randm11r(&se);   // fills [-pi,pi)
      c[j] = crandm11r(&se);
    }
  }
  //for (INT j=0; j<M; ++j) x[j] = 0.999 * PI*randm11();  // avoid ends
  //for (INT j=0; j<M; ++j) x[j] = PI*(2*j/(FLT)M-1);  // test a grid

  printf("test 1d type-1:\n"); // -------------- type 1
  CNTime timer; timer.start();
  int ier = finufft1d1(M,x,c,isign,tol,N,F,opts);
  //for (int j=0;j<N;++j) cout<<F[j]<<endl;
  double t=timer.elapsedsec();
  if (ier!=0) {
    printf("error (ier=%d)!\n",ier);
    exit(ier);
  } else
    printf("\t%ld NU pts to %ld modes in %.3g s \t%.3g NU pts/s\n",(INT64)M,(INT64)N,t,M/t);

  INT nt = (INT)(0.37*N);   // check arb choice of mode near the top (N/2)
  CPX Ft = {0,0};
  //#pragma omp declare reduction (cmplxadd:CPX:omp_out=omp_out+omp_in) initializer(omp_priv={0.0,0.0})  // only for openmp v 4.0!
  //#pragma omp parallel for schedule(dynamic,CHUNK) reduction(cmplxadd:Ft)
  for (INT j=0; j<M; ++j)
    Ft += c[j] * exp(ima*((FLT)(isign*nt))*x[j]);
  printf("one mode: rel err in F[%ld] is %.3g\n",(INT64)nt,abs(Ft-F[N/2+nt])/infnorm(N,F));
  if ((INT64)M*N<=BIGPROB) {                  // also full direct eval
    CPX* Ft = (CPX*)malloc(sizeof(CPX)*N);
    dirft1d1(M,x,c,isign,N,Ft);
    printf("dirft1d: rel l2-err of result F is %.3g\n",relerrtwonorm(N,Ft,F));
    free(Ft);
  }

  printf("test 1d type-2:\n"); // -------------- type 2
 #pragma omp parallel
  {
    unsigned int se=MY_OMP_GET_THREAD_NUM();  // needed for parallel random #s
#pragma omp for schedule(dynamic,CHUNK)
    for (INT m=0; m<N; ++m) F[m] = crandm11r(&se);
  }
  timer.restart();
  ier = finufft1d2(M,x,c,isign,tol,N,F,opts);
  //cout<<"c:\n"; for (int j=0;j<M;++j) cout<<c[j]<<endl;
  t=timer.elapsedsec();
  if (ier!=0) {
    printf("error (ier=%d)!\n",ier);
    exit(ier);
  } else
    printf("\t%ld modes to %ld NU pts in %.3g s \t%.3g NU pts/s\n",(INT64)N,(INT64)M,t,M/t);

  INT jt = M/2;          // check arbitrary choice of one targ pt
  CPX ct = {0,0};
  INT m=0, k0 = N/2;          // index shift in fk's = mag of most neg freq
  //#pragma omp parallel for schedule(dynamic,CHUNK) reduction(cmplxadd:ct)
  for (INT m1=-k0; m1<=(N-1)/2; ++m1)
    ct += F[m++] * exp(ima*((FLT)(isign*m1))*x[jt]);   // crude direct
  printf("one targ: rel err in c[%ld] is %.3g\n",(INT64)jt,abs(ct-c[jt])/infnorm(M,c));
  if ((INT64)M*N<=BIGPROB) {                  // also full direct eval
    CPX* ct = (CPX*)malloc(sizeof(CPX)*M);
    dirft1d2(M,x,ct,isign,N,F);
    printf("dirft1d: rel l2-err of result c is %.3g\n",relerrtwonorm(M,ct,c));
    //cout<<"c/ct:\n"; for (int j=0;j<M;++j) cout<<c[j]/ct[j]<<endl;
    free(ct);
  }

  printf("test 1d type-3:\n"); // -------------- type 3
  // reuse the strengths c, interpret N as number of targs:
#pragma omp parallel
  {
    unsigned int se=MY_OMP_GET_THREAD_NUM();
#pragma omp for schedule(dynamic,CHUNK)
    for (INT j=0; j<M; ++j) x[j] = 2.0 + PI*randm11r(&se);  // new x_j srcs
  }
  FLT* s = (FLT*)malloc(sizeof(FLT)*N);    // targ freqs
  FLT S = (FLT)N/2;                   // choose freq range sim to type 1
#pragma omp parallel
  {
    unsigned int se=MY_OMP_GET_THREAD_NUM();
#pragma omp for schedule(dynamic,CHUNK)
    for (INT k=0; k<N; ++k) s[k] = S*(1.7 + randm11r(&se)); //S*(1.7 + k/(FLT)N); // offset
  }
  timer.restart();
  ier = finufft1d3(M,x,c,isign,tol,N,s,F,opts);
  t=timer.elapsedsec();
  if (ier!=0) {
    printf("error (ier=%d)!\n",ier);
    exit(ier);
  } else
    printf("\t%ld NU to %ld NU in %.3g s   %.3g srcs/s, %.3g targs/s\n",(INT64)M,(INT64)N,t,M/t,N/t);

  INT kt = N/2;          // check arbitrary choice of one targ pt
  Ft = {0,0};
  //#pragma omp parallel for schedule(dynamic,CHUNK) reduction(cmplxadd:Ft)
  for (INT j=0;j<M;++j)
    Ft += c[j] * exp(ima*(FLT)isign*s[kt]*x[j]);
  printf("one targ: rel err in F[%ld] is %.3g\n",(INT64)kt,abs(Ft-F[kt])/infnorm(N,F));
  if ((INT64)M*N<=BIGPROB) {                  // also full direct eval
    CPX* Ft = (CPX*)malloc(sizeof(CPX)*N);
    dirft1d3(M,x,c,isign,N,s,Ft);       // writes to F
    printf("dirft1d: rel l2-err of result F is %.3g\n",relerrtwonorm(N,Ft,F));
    //cout<<"s, F, Ft:\n"; for (int k=0;k<N;++k) cout<<s[k]<<" "<<F[k]<<"\t"<<Ft[k]<<"\t"<<F[k]/Ft[k]<<endl;
    free(Ft);
  }

  free(x); free(c); free(F);
  return 0;
}
