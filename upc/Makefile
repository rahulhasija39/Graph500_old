include ../make.inc
MPICC ?= upcc

BIN =  graph500_mpi_one_sided


all:	$(BIN)

GENLIB=../generator/libgraph_generator_seq.a

graph500_mpi_simple: main.c bfs_simple.c convert_to_csr.c find_roots.c utils.c validate.c $(GENLIB)
	$(MPICC) $(CFLAGS) $(LDFLAGS) -o graph500_mpi_simple bfs_simple.c convert_to_csr.c find_roots.c main.c utils.c validate.c $(GENLIB) -lm

graph500_mpi_one_sided: main.c bfs_one_sided.c convert_to_csr.c find_roots.c utils.c validate.c $(GENLIB)
	$(MPICC) $(CFLAGS) $(LDFLAGS) -o graph500_mpi_one_sided bfs_one_sided.c convert_to_csr.c find_roots.c main.c utils.c validate.c $(GENLIB) -lm 

graph500_mpi_custom: main.c bfs_custom.c convert_to_csr.c find_roots.c utils.c validate.c $(GENLIB)
	$(MPICC) $(CFLAGS) $(LDFLAGS) -o graph500_mpi_custom bfs_custom.c convert_to_csr.c find_roots.c main.c utils.c validate.c $(GENLIB) -lm

$(GENLIB):
	$(MAKE) -C ../generator -f ../generator/Makefile.seq

clean:
	-rm -f graph500_mpi_* *.o *.a
	-$(MAKE) -C ../generator -f ../generator/Makefile.seq clean
