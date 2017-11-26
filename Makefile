CC := g++
CFLAGS := -g -Wall -std=c++11
LIBS := -lsoundio -lsndfile

ifdef DEBUG
  CFLAGS += -O0
else
  CFLAGS += -O3
endif

MAIN_SRCS = src/wavplayer.cc

HEADERS = $(wildcard src/*.h)

OBJECTS_SRC = $(wildcard src/*.cc)
OBJECTS_SRC_FILTERED = $(filter-out $(MAIN_SRCS), $(OBJECTS_SRC))

OBJDIR := obj

OBJECTS   := $(addprefix $(OBJDIR)/,$(OBJECTS))
TARGETOBJ := $(OBJDIR)/src

TARGETDIR := bin
TRG_wavplayer = $(TARGETDIR)/wavplayer

.PHONY: default all clean
.PRECIOUS: $(OBJECTS)

.PHONY: wavplayer
wavplayer: $(TRG_wavplayer)

default: all
all: wavplayer

%.o: %.cc $(HEADERS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.cc $(HEADERS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(TRG_wavplayer): $(OBJECTS) $(TARGETOBJ)/wavplayer.o
	@mkdir -p $(@D)
	$(CC) $(OBJECTS) $(TARGETOBJ)/wavplayer.o $(LIBS) -o $@

clean:
	rm -rf $(OBJDIR)
	rm -rf $(TRG_wavplayer)

