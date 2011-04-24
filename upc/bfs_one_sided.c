/* Copyright (C) 2010 The Trustees of Indiana University.                  */
/*                                                                         */
/* Use, modification and distribution is subject to the Boost Software     */
/* License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at */
/* http://www.boost.org/LICENSE_1_0.txt)                                   */
/*                                                                         */
/*  Authors: Jeremiah Willcock                                             */
/*           Andrew Lumsdaine                                              */

#include "common.h"
//#include <mpi.h>
#include <upc.h>
#include <upc_collective.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

#pragma upc strict

/* This BFS represents its queues as bitmaps and uses some data representation
 * tricks to fit with the use of MPI one-sided operations.  It is not much
 * faster than the standard version on the machines I have tested it on, but
 * systems that have good RDMA hardware and good MPI one-sided implementations
 * might get better performance from it.  This code might also be good to
 * translate to UPC, Co-array Fortran, SHMEM, or GASNet since those systems are
 * more designed for one-sided remote memory operations. */
//void run_mpi_bfs(const csr_graph* const g, int64_t root, int64_t* pred, int64_t* nvisited) {
void run_upc_bfs(const csr_graph* const g, int64_t root, shared int64_t* pred, int64_t* nvisited) {

	upc_flag_t sync_mode = UPC_IN_MYSYNC | UPC_OUT_MYSYNC;
  const size_t nlocalverts = g->nlocalverts;//why verticies that are local to this task?
  const int64_t nglobalverts = g->nglobalverts; //total global verticies?  Is this including the verticies which are considered local to this this thread?
 
	shared int64_t *nvisited_local = (shared int64_t*) xUPC_Alloc_mem( sizeof( int64_t));
	nvisited_local = 0;

  /* Set up a second predecessor map so we can read from one and modify the
   * other. */
  shared int64_t* orig_pred = pred;
  //int64_t* pred2 = (int64_t*)xMPI_Alloc_mem(nlocalverts * sizeof(int64_t));//why does this point to empty memroy?
  shared int64_t* pred2 = (shared int64_t*)xUPC_Alloc_mem(nlocalverts * sizeof(int64_t));//why does this point to empty memroy?

  /* The queues (old and new) are represented as bitmaps.  Each bit in the
   * queue bitmap says to check elts_per_queue_bit elements in the predecessor
   * map for vertices that need to be visited.  In other words, the queue
   * bitmap is an overapproximation of the actual queue; because MPI_Accumulate
   * does not get any information on the result of the update, sometimes
   * elements are also added to the bitmap when they were actually already
   * black.  Because of this, the predecessor map needs to be checked to be
   * sure a given vertex actually needs to be processed. */
  const int elts_per_queue_bit = 4; //What the hell are elts? elements maybe? whats a queue_bit?
  const int ulong_bits = sizeof(unsigned long) * CHAR_BIT; //ulong_bits, u is a what??
  int64_t queue_nbits = (nlocalverts + elts_per_queue_bit - 1) / elts_per_queue_bit; //why is all I can really say?
  int64_t queue_nwords = (queue_nbits + ulong_bits - 1) / ulong_bits; //why again, why not just subtract by 1?
  shared unsigned long* queue_bitmap1 = (shared unsigned long*)xUPC_Alloc_mem(queue_nwords * sizeof(unsigned long)); 
  shared unsigned long* queue_bitmap2 = (shared unsigned long*)xUPC_Alloc_mem(queue_nwords * sizeof(unsigned long));
  upc_memset(queue_bitmap1, 0, queue_nwords * sizeof(unsigned long));//why again only bitmap1?  

  /* List of local vertices (used as sources in MPI_Accumulate). *///Using equivelient upc_gather();
  shared int64_t* local_vertices = (shared int64_t*)xUPC_Alloc_mem(nlocalverts * sizeof(int64_t));  //Why does it store a pointer to this int64_t*
  { size_t i; for (i = 0; i < nlocalverts; ++i) local_vertices[i] = VERTEX_TO_GLOBAL(i);}//why the brackets?

  /* List of all bit masks for an unsigned long (used as sources in
   * MPI_Accumulate). */
//  unsigned long masks[ulong_bits];
 // {int i; for (i = 0; i < ulong_bits; ++i) masks[i] = (1UL << i);}//why the brackets?

  /* Coding of predecessor map: */
  /* - White (not visited): INT64_MAX */
  /* - Grey (in queue): 0 .. nglobalverts-1 */
  /* - Black (done): -nglobalverts .. -1 */

  /* Set initial predecessor map. */
  {size_t i; for (i = 0; i < nlocalverts; ++i) pred[i] = INT64_MAX;}//Again Why?

  /* Mark root as grey and add it to the queue. */
  if (VERTEX_OWNER(root) == rank) {
    pred[VERTEX_LOCAL(root)] = root;
    queue_bitmap1[VERTEX_LOCAL(root) / elts_per_queue_bit / ulong_bits] |= (1UL << ((VERTEX_LOCAL(root) / elts_per_queue_bit) % ulong_bits));
  }

  /* Create MPI windows on the two predecessor arrays and the two queues. */
/*
  MPI_Win pred_win, pred2_win, queue1_win, queue2_win;
  MPI_Win_create(pred, nlocalverts * sizeof(int64_t), sizeof(int64_t), MPI_INFO_NULL, MPI_COMM_WORLD, &pred_win);
  MPI_Win_create(pred2, nlocalverts * sizeof(int64_t), sizeof(int64_t), MPI_INFO_NULL, MPI_COMM_WORLD, &pred2_win);
  MPI_Win_create(queue_bitmap1, queue_nwords * sizeof(unsigned long), sizeof(unsigned long), MPI_INFO_NULL, MPI_COMM_WORLD, &queue1_win);
  MPI_Win_create(queue_bitmap2, queue_nwords * sizeof(unsigned long), sizeof(unsigned long), MPI_INFO_NULL, MPI_COMM_WORLD, &queue2_win);
*/
  while (1) {
    int64_t i;
    /* Clear the next-level queue. */
    upc_memset(queue_bitmap2, 0, queue_nwords * sizeof(unsigned long));

    /* The pred2 array is pred with all grey vertices changed to black. */
    upc_memcpy(pred2, pred, nlocalverts * sizeof(int64_t));
    for (i = 0; i < (int64_t)nlocalverts; ++i) {
      if (pred2[i] >= 0 && pred2[i] < nglobalverts) pred2[i] -= nglobalverts;
    }

    /* Start one-sided operations for this level. */
  //  MPI_Win_fence(MPI_MODE_NOPRECEDE, pred2_win);
  //  MPI_Win_fence(MPI_MODE_NOPRECEDE, queue2_win);
	upc_fence;

    /* Step through the words of the queue bitmap. */
    for (i = 0; i < queue_nwords; ++i) {
      unsigned long val = queue_bitmap1[i];
      int bitnum;
      /* Skip any that are all zero. */
      if (!val) continue;
      /* Scan the bits in the word. */
      for (bitnum = 0; bitnum < ulong_bits; ++bitnum) {
        size_t first_v_local = (size_t)((i * ulong_bits + bitnum) * elts_per_queue_bit);
        if (first_v_local >= nlocalverts) break;
        int bit = (int)((val >> bitnum) & 1);
        /* Skip any that are zero. */
        if (!bit) continue;
        /* Scan the queue elements corresponding to this bit. */
        int qelem_idx;
        for (qelem_idx = 0; qelem_idx < elts_per_queue_bit; ++qelem_idx) {
          size_t v_local = first_v_local + qelem_idx;
          if (v_local >= nlocalverts) continue;
          /* Since the queue is an overapproximation, check the predecessor map
           * to be sure this vertex is grey. */
          if (pred[v_local] >= 0 && pred[v_local] < nglobalverts) {
            ++nvisited_local;
            size_t ei, ei_end = g->rowstarts[v_local + 1];
            /* Walk the incident edges. */
            for (ei = g->rowstarts[v_local]; ei < ei_end; ++ei) {
              int64_t w = g->column[ei];
              if (w == VERTEX_TO_GLOBAL(v_local)) continue; /* Self-loop */
              /* Set the predecessor of the other edge endpoint (note use of
               * MPI_MIN and the coding of the predecessor map). */
            //  MPI_Accumulate(&local_vertices[v_local], 1, INT64_T_MPI_TYPE, VERTEX_OWNER(w), VERTEX_LOCAL(w), 1, INT64_T_MPI_TYPE, MPI_MIN, pred2_win);
							upc_all_reduceI( pred2,  (local_vertices + v_local), (upc_op_t) UPC_MIN, nlocalverts*THREADS,nlocalverts, NULL, sync_mode);
              /* Mark the endpoint in the remote queue (note that the min may
               * not do an update, so the queue is an overapproximation in this
               * way as well). */
             // MPI_Accumulate(&masks[((VERTEX_LOCAL(w) / elts_per_queue_bit) % ulong_bits)], 1, MPI_UNSIGNED_LONG, VERTEX_OWNER(w), VERTEX_LOCAL(w) / elts_per_queue_bit / ulong_bits, 1, MPI_UNSIGNED_LONG, MPI_BOR, queue2_win);
            }
          }
        }
      }
    }
    /* End one-sided operations. */
    //MPI_Win_fence(MPI_MODE_NOSUCCEED, queue2_win);
   // MPI_Win_fence(MPI_MODE_NOSUCCEED, pred2_win);
		upc_fence;
    /* Test if there are any elements in the next-level queue (globally); stop
     * if none. */
    shared int* any_set = (shared int*)xUPC_Alloc_mem( sizeof(int));
    for (i = 0; i < queue_nwords; ++i) {
      if (queue_bitmap2[i] != 0) 
				{/*any_set = 1;*/ break;
}
    }
    //MPI_Allreduce(MPI_IN_PLACE, &any_set, 1, MPI_INT, MPI_LOR, MPI_COMM_WORLD);
		upc_all_reduceI( (shared int*) any_set,(shared int*) any_set, UPC_LOGOR, THREADS, 1, NULL, sync_mode);
    if (!any_set) break;
upc_free(any_set);
    /* Swap queues and predecessor maps. */
   // {MPI_Win temp = queue1_win; queue1_win = queue2_win; queue2_win = temp;}
    {shared unsigned long* temp = queue_bitmap1; queue_bitmap1 = queue_bitmap2; queue_bitmap2 = temp;}
   // {MPI_Win temp = pred_win; pred_win = pred2_win; pred2_win = temp;}
    {shared int64_t* temp = pred; pred = pred2; pred2 = temp;}
  }
  //MPI_Win_free(&pred_win);
  //MPI_Win_free(&pred2_win);
 // MPI_Win_free(&queue1_win);
  //MPI_Win_free(&queue2_win);
  //MPI_Free_mem(local_vertices);
 // MPI_Free_mem(queue_bitmap1);
  //MPI_Free_mem(queue_bitmap2);
	upc_free( local_vertices);
	upc_free( queue_bitmap1);
	upc_free( queue_bitmap2);
  /* Clean up the predecessor map swapping since the surrounding code does not
   * allow the BFS to change the predecessor map pointer. */
  if (pred2 != orig_pred) {
    upc_memcpy(orig_pred, pred2, nlocalverts * sizeof(int64_t));
    //MPI_Free_mem(pred2);
		upc_free( pred2);
  } else {
    //MPI_Free_mem(pred);
		upc_free( pred);
  }

  /* Change from special coding of predecessor map to the one the benchmark
   * requires. */
  size_t i;
  for (i = 0; i < nlocalverts; ++i) {
    if (orig_pred[i] < 0) {
      orig_pred[i] += nglobalverts;
    } else if (orig_pred[i] == INT64_MAX) {
      orig_pred[i] = -1;
    }
  }

  /* Count visited vertices. */
  //MPI_Allreduce(MPI_IN_PLACE, &nvisited_local, 1, INT64_T_MPI_TYPE, MPI_SUM, MPI_COMM_WORLD);
	upc_all_reduceI( (shared int64_t*) nvisited_local, (shared int64_t*) nvisited_local, (upc_op_t) UPC_ADD, nlocalverts * THREADS, nlocalverts, NULL, sync_mode); 
  *nvisited = nvisited_local;
	free(nvisited_local);
}
