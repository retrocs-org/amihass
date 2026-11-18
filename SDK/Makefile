VC ?= vc +aos68k
CFLAGS ?= -O
AmiHASSExample: example.c amihass_client.c quote.c
	$(VC) $(CFLAGS) -I. -o $@ example.c amihass_client.c quote.c -lamiga
