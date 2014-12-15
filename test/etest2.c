/* Copyright (C) The Scalable Software Infrastructure Project. All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the project nor the names of its contributors 
      may be used to endorse or promote products derived from this software 
      without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE SCALABLE SOFTWARE INFRASTRUCTURE PROJECT
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
   TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE SCALABLE SOFTWARE INFRASTRUCTURE
   PROJECT BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
   OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
        #include "lis_config.h"
#else
#ifdef HAVE_CONFIG_WIN_H
        #include "lis_config_win.h"
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "lis.h"

#undef __FUNC__
#define __FUNC__ "main"
LIS_INT main(LIS_INT argc, char* argv[])
{
    LIS_INT err,i,j,m,n,nnz,is,ie,nn,ii,jj,ctr;
    LIS_INT nprocs,mtype,my_rank;
    int int_nprocs,int_my_rank;
    LIS_INT nesol;
    LIS_MATRIX A,A0;
    LIS_VECTOR x;
    LIS_REAL evalue0;
    LIS_ESOLVER esolver;
    LIS_REAL residual;
    LIS_INT iter;
    double time;
    double itime,ptime,p_c_time,p_i_time;
    LIS_INT *ptr,*index;
    LIS_SCALAR *value;
    char esolvername[128];
    
    LIS_DEBUG_FUNC_IN;
    
    lis_initialize(&argc, &argv);

#ifdef USE_MPI
    MPI_Comm_size(MPI_COMM_WORLD,&int_nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD,&int_my_rank);
    nprocs = int_nprocs;
    my_rank = int_my_rank;
#else
    nprocs  = 1;
    my_rank = 0;
#endif
    
    if( argc < 6 )
      {
	if( my_rank==0 ) 
	  {
	      printf("Usage: %s m n matrix_type evector_filename rhistory_filename [options]\n", argv[0]);
	  }
	CHKERR(1);
      }

    if( my_rank==0 )
      {
	printf("\n");
#ifdef _LONGLONG
	printf("number of processes = %lld\n",nprocs);
#else
	printf("number of processes = %d\n",nprocs);
#endif
      }

#ifdef _OPENMP
    if( my_rank==0 )
      {
#ifdef _LONGLONG
	printf("max number of threads = %lld\n",omp_get_num_procs());
	printf("number of threads = %lld\n",omp_get_max_threads());
#else
	printf("max number of threads = %d\n",omp_get_num_procs());
	printf("number of threads = %d\n",omp_get_max_threads());
#endif
      }
#endif
		
    /* generate matrix */
    m  = atoi(argv[1]);
    n  = atoi(argv[2]);
    mtype  = atoi(argv[3]);
    if( m<=0 || n<=0 )
      {
#ifdef _LONGLONG
	if( my_rank==0 ) printf("m=%lld <=0 or n=%lld <=0\n",m,n);
#else
	if( my_rank==0 ) printf("m=%d <=0 or n=%d <=0\n",m,n);
#endif
	CHKERR(1);
      }
    nn = m*n;
    err = lis_matrix_create(LIS_COMM_WORLD,&A);
    err = lis_matrix_set_size(A,0,nn);
    CHKERR(err);

    ptr   = (LIS_INT *)malloc((A->n+1)*sizeof(LIS_INT));
    if( ptr==NULL ) CHKERR(1);
    index = (LIS_INT *)malloc(5*A->n*sizeof(LIS_INT));
    if( index==NULL ) CHKERR(1);
    value = (LIS_SCALAR *)malloc(5*A->n*sizeof(LIS_SCALAR));
    if( value==NULL ) CHKERR(1);

    lis_matrix_get_range(A,&is,&ie);
    ctr = 0;
    for(ii=is;ii<ie;ii++)
      {
	i = ii/m;
	j = ii - i*m;
	if( i>0 )   { jj = ii - m; index[ctr] = jj; value[ctr++] = -1.0;}
	if( i<n-1 ) { jj = ii + m; index[ctr] = jj; value[ctr++] = -1.0;}
	if( j>0 )   { jj = ii - 1; index[ctr] = jj; value[ctr++] = -1.0;}
	if( j<m-1 ) { jj = ii + 1; index[ctr] = jj; value[ctr++] = -1.0;}
	index[ctr] = ii; value[ctr++] = 4.0;
	ptr[ii-is+1] = ctr;
      }
    ptr[0] = 0;
    err = lis_matrix_set_csr(ptr[ie-is],ptr,index,value,A);
    CHKERR(err);
    err = lis_matrix_assemble(A);
    CHKERR(err);

    nnz = A->nnz;
#ifdef USE_MPI
    MPI_Allreduce(&nnz,&i,1,LIS_MPI_INT,MPI_SUM,A->comm);
    nnz   = i;
#endif
#ifdef _LONGLONG
    if( my_rank==0 ) printf("matrix size = %lld x %lld (%lld nonzero entries)\n\n",nn,nn,nnz);
#else
    if( my_rank==0 ) printf("matrix size = %d x %d (%d nonzero entries)\n\n",nn,nn,nnz);
#endif

    err = lis_matrix_duplicate(A,&A0);
    CHKERR(err);
    lis_matrix_set_type(A0,mtype);
    err = lis_matrix_convert(A,A0);
    CHKERR(err);
    lis_matrix_destroy(A);
    A = A0;
    lis_vector_duplicate(A,&x);

    err = lis_esolver_create(&esolver);
    CHKERR(err);

    lis_esolver_set_option("-eprint mem",esolver);
    lis_esolver_set_optionC(esolver);
    lis_esolve(A, x, &evalue0, esolver);
    lis_esolver_get_esolver(esolver,&nesol);
    lis_esolver_get_esolvername(nesol,esolvername);
    lis_esolver_get_residualnorm(esolver, &residual);
    lis_esolver_get_iter(esolver, &iter);
    lis_esolver_get_timeex(esolver,&time,&itime,&ptime,&p_c_time,&p_i_time);
    if( my_rank==0 ) {
      printf("%s: mode number          = %d\n", esolvername, 0);
#ifdef _LONG__DOUBLE
      printf("%s: eigenvalue           = %Le\n", esolvername, evalue0);
#else
      printf("%s: eigenvalue           = %e\n", esolvername, evalue0);
#endif
#ifdef _LONGLONG
      printf("%s: number of iterations = %lld\n",esolvername, iter);
#else
      printf("%s: number of iterations = %d\n",esolvername, iter);
#endif
      printf("%s: elapsed time         = %e sec.\n", esolvername, time);
      printf("%s:   preconditioner     = %e sec.\n", esolvername, ptime);
      printf("%s:     matrix creation  = %e sec.\n", esolvername, p_c_time);
      printf("%s:   linear solver      = %e sec.\n", esolvername, itime);
#ifdef _LONG__DOUBLE
      printf("%s: relative residual    = %Le\n\n",esolvername, residual);
#else
      printf("%s: relative residual    = %e\n\n",esolvername, residual);
#endif
  }

    /* write eigenvector */
    lis_output_vector(x,LIS_FMT_MM,argv[4]);

    /* write residual history */
    lis_esolver_output_rhistory(esolver, argv[5]);

    CHKERR(err);

    lis_esolver_destroy(esolver);
    lis_matrix_destroy(A);
    lis_matrix_destroy(A0);
    lis_vector_destroy(x);

    lis_finalize();

    LIS_DEBUG_FUNC_OUT;

    return 0;
}

 