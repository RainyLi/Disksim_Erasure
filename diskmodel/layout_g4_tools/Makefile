

include .paths


CFLAGS = -g $(DISKSIM_CFLAGS) $(DISKMODEL_CFLAGS) $(LIBPARAM_CFLAGS) $(LIBDDBG_CFLAGS) -Wall -Wno-unused -m32

LDFLAGS = $(DISKSIM_LDFLAGS)  $(MEMSMODEL_LDFLAGS) -L.. -ldiskmodel  $(LIBPARAM_LDFLAGS) $(LIBDDBG_LDFLAGS) -lm -m32

TARGETS = g4_skews
all: $(TARGETS)

%: %.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

%: %.c


clean:
	rm -f *.o $(TARGETS)

distclean: clean
	rm -f *~
