UNAME := $(shell uname -s 2>nul || echo Windows)

ifneq ($(findstring cl,$(CC)),cl)
    ifneq ($(CC),gcc)
        CC := $(shell where cl.exe 2>nul || echo gcc)
    endif
endif

ifeq ($(OS),Windows_NT)
    RM = del /q 2>nul || exit 0
    EXT = .exe
    LIB_EXT = .lib
    AR = lib.exe /nologo
    ARFLAGS = /out:
    OBJ_EXT = .obj
else
    RM = rm -f
    EXT =
    LIB_EXT = .a
    AR = ar rcs
    ARFLAGS =
    OBJ_EXT = .o
endif

CFLAGS = -std=c11 -Wall -Wextra

MYALLOC_OBJ = myalloc$(OBJ_EXT)
MYALLOC_DBG_OBJ = myalloc_dbg$(OBJ_EXT)
MYALLOC_LIB = myalloc$(LIB_EXT)
MYALLOC_DBG_LIB = myalloc_dbg$(LIB_EXT)
ALLOCATOR = allocator$(EXT)
EXAMPLE = example$(EXT)

.PHONY: all clean test

all: $(MYALLOC_LIB) $(MYALLOC_DBG_LIB) $(ALLOCATOR) $(EXAMPLE)

$(MYALLOC_OBJ): myalloc.c myalloc.h
	$(CC) $(CFLAGS) -c myalloc.c -o $@

$(MYALLOC_DBG_OBJ): myalloc.c myalloc.h
	$(CC) $(CFLAGS) -DMYALLOC_DEBUG -c myalloc.c -o $@

$(MYALLOC_LIB): $(MYALLOC_OBJ)
ifneq ($(findstring lib.exe,$(AR)),)
	$(AR) $(ARFLAGS)$@ $^
else
	$(AR) $@ $^
endif

$(MYALLOC_DBG_LIB): $(MYALLOC_DBG_OBJ)
ifneq ($(findstring lib.exe,$(AR)),)
	$(AR) $(ARFLAGS)$@ $^
else
	$(AR) $@ $^
endif

$(ALLOCATOR): main.c $(MYALLOC_LIB)
	$(CC) $(CFLAGS) -DMYALLOC_DEBUG main.c $(MYALLOC_DBG_LIB) -o $@

$(EXAMPLE): example_usage.c $(MYALLOC_DBG_LIB)
	$(CC) $(CFLAGS) -DMYALLOC_DEBUG example_usage.c $(MYALLOC_DBG_LIB) -o $@

test: $(ALLOCATOR)
	./$(ALLOCATOR)

clean:
	-$(RM) $(MYALLOC_OBJ) $(MYALLOC_DBG_OBJ)
	-$(RM) $(MYALLOC_LIB) $(MYALLOC_DBG_LIB)
	-$(RM) $(ALLOCATOR) $(EXAMPLE)