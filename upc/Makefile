include ../make.inc
UPCCC ?= upcc 

BIN = graph500_upc_one_sided

all:	$(BIN)

GENLIB=../generator/libgraph_generator_upc.a


graph500_upc: main.c bfs_one_sided.c convert_to_csr.c find_roots.c utils.c validate.c $(GENLIB)
	$(UPCCC) $(CFLAGS) $(LDFLAGS) -o graph500_upc_one_sided bfs_one_sided.c convert_to_csr.c find_roots.c main.c utils.c validate.c $(GENLIB) -lm

$(GENLIB):
	$(MAKE) -C ../generator -f ../generator/Makefile.upc

clean:
	-rm -f graph500_upc_* *.o *.a
	-$(MAKE) -C ../generator -f ../generator/Makefile.upc clean
