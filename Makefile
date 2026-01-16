CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -O2 -Iinclude -Idep

MINGW = x86_64-w64-mingw32-gcc
MINGW_FLAGS = -Wall -Wextra -std=gnu99 -O2 -Iinclude -Idep -Wno-cast-function-type

COMMON_SRCS = src/utils.c src/des.c src/netntlmv1.c src/rainbow.c src/table.c src/opencl_host.c src/opencl_dyn.c
LOOKUP_SRCS = src/main.c $(COMMON_SRCS)
PRECOMPUTE_SRCS = src/precompute_main.c $(COMMON_SRCS)
CANDIDATE_LOOKUP_SRCS = src/candidate_lookup_main.c $(COMMON_SRCS)
CANDIDATE_CHECK_SRCS = src/candidate_check_main.c $(COMMON_SRCS)

all: gpu_lookup precompute candidate_lookup candidate_check

gpu_lookup: $(LOOKUP_SRCS)
	$(CC) $(CFLAGS) $(LOOKUP_SRCS) -o $@ -ldl

precompute: $(PRECOMPUTE_SRCS)
	$(CC) $(CFLAGS) $(PRECOMPUTE_SRCS) -o $@ -ldl

candidate_lookup: $(CANDIDATE_LOOKUP_SRCS)
	$(CC) $(CFLAGS) $(CANDIDATE_LOOKUP_SRCS) -o $@ -ldl

candidate_check: $(CANDIDATE_CHECK_SRCS)
	$(CC) $(CFLAGS) $(CANDIDATE_CHECK_SRCS) -o $@ -ldl

windows: gpu_lookup.exe precompute.exe candidate_lookup.exe candidate_check.exe

gpu_lookup.exe: $(LOOKUP_SRCS)
	$(MINGW) $(MINGW_FLAGS) $(LOOKUP_SRCS) -o $@

precompute.exe: $(PRECOMPUTE_SRCS)
	$(MINGW) $(MINGW_FLAGS) $(PRECOMPUTE_SRCS) -o $@

candidate_lookup.exe: $(CANDIDATE_LOOKUP_SRCS)
	$(MINGW) $(MINGW_FLAGS) $(CANDIDATE_LOOKUP_SRCS) -o $@

candidate_check.exe: $(CANDIDATE_CHECK_SRCS)
	$(MINGW) $(MINGW_FLAGS) $(CANDIDATE_CHECK_SRCS) -o $@

clean:
	rm -f gpu_lookup gpu_lookup.exe
	rm -f precompute precompute.exe
	rm -f candidate_lookup candidate_lookup.exe
	rm -f candidate_check candidate_check.exe

.PHONY: all windows clean