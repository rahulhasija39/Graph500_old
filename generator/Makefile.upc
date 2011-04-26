include ../make.inc
AR = ar
RANLIB = ranlib
CC = cc
CFLAGS =   -link-with=mpicc -network=udp -uses-mpi -print-mpicc -g -Drestrict=__restrict__ -DGRAPH_GENERATOR_MPI -DGRAPHGEN_DISTRIBUTED_MEMORY # -g -pg
# CFLAGS = -g -Wall -Drestrict= -DGRAPH_GENERATOR_MPI -DGRAPHGEN_DISTRIBUTED_MEMORY # -g -pg
LDFLAGS = -g # -g -pg
UPCCC ?= upcc
MPICC ?= mpicc
all: libgraph_generator_upc.a generator_test_upc
# all: generator_test_xmt

generator_test_upc: generator_test_upc.c libgraph_generator_upc.a
	$(UPCCC) $(CFLAGS) $(LDFLAGS) -o generator_test_upc generator_test_upc.c -L. -lgraph_generator_upc -lm

libgraph_generator_upc.a: btrd_binomial_distribution.o splittable_mrg.o mrg_transitions.o graph_generator.o permutation_gen.o apply_permutation_mpi.o make_graph.o utils.o scramble_edges.o
	$(AR) cruv libgraph_generator_upc.a btrd_binomial_distribution.o splittable_mrg.o mrg_transitions.o graph_generator.o permutation_gen.o apply_permutation_mpi.o make_graph.o utils.o scramble_edges.o
	$(RANLIB) libgraph_generator_upc.a

btrd_binomial_distribution.o: btrd_binomial_distribution.c btrd_binomial_distribution.h splittable_mrg.h mod_arith.h
	$(CC) $(CFLAGS) -c btrd_binomial_distribution.c

splittable_mrg.o: splittable_mrg.c splittable_mrg.h mod_arith.h
	$(CC) $(CFLAGS) -c splittable_mrg.c

mrg_transitions.o: mrg_transitions.c splittable_mrg.h
	$(CC) $(CFLAGS) -c mrg_transitions.c

graph_generator.o: graph_generator.c splittable_mrg.h mod_arith.h btrd_binomial_distribution.h graph_generator.h utils.h
	$(CC) $(CFLAGS) -c graph_generator.c

permutation_gen.o: permutation_gen.c splittable_mrg.h mod_arith.h btrd_binomial_distribution.h graph_generator.h permutation_gen.h utils.h
	$(UPCCC) $(CFLAGS) -c permutation_gen.c

make_graph.o: make_graph.c splittable_mrg.h mod_arith.h btrd_binomial_distribution.h graph_generator.h make_graph.h permutation_gen.h utils.h scramble_edges.h apply_permutation_mpi.h
	$(UPCCC) $(CFLAGS) -c make_graph.c

apply_permutation_mpi.o: apply_permutation_mpi.c splittable_mrg.h mod_arith.h btrd_binomial_distribution.h graph_generator.h make_graph.h permutation_gen.h utils.h apply_permutation_mpi.h
	$(UPCCC) $(CFLAGS) -c apply_permutation_mpi.c

utils.o: utils.c splittable_mrg.h mod_arith.h btrd_binomial_distribution.h graph_generator.h make_graph.h permutation_gen.h utils.h
	$(UPCCC) $(CFLAGS) -c utils.c

scramble_edges.o: scramble_edges.c splittable_mrg.h mod_arith.h btrd_binomial_distribution.h graph_generator.h make_graph.h permutation_gen.h utils.h scramble_edges.h
	$(UPCCC) $(CFLAGS) -c scramble_edges.c

clean:
	-rm -f generator_test_upc *.o *.a
