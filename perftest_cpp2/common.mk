#
# A makefile shared by all UNIX-like architectures.
#

DEBUG_FLAG   += -Wall

cxx_cc_flags += $(DEBUG_FLAG)
cxx_ld_flags += $(DEBUG_FLAG)

syslibs      += -ldl -lm -lpthread

DEFINES_ARCH_SPECIFIC += -DRTI_UNIX

DEFINES       = $(DEFINES_ARCH_SPECIFIC)

ifneq ($(findstring -g, $(DEBUG_FLAG)),)
  # debug
  RELEASEDIR := Debug
else
  # not debug
  RELEASEDIR := Release
endif

OBJDIRROOT   := obj/$(ARCH)
OBJDIR        = $(OBJDIRROOT)/$(RELEASEDIR)
BINDIRROOT   := ../bin/$(ARCH)
BINDIR        = $(BINDIRROOT)/$(RELEASEDIR)
TYPEDIR      := ../DDSTypes2
IDLFILE      := ../idl/test.idl

INCLUDES      = -I. \
                -I$(TYPEDIR) \
                -I$(NDDSHOME)/include \
                -I$(NDDSHOME)/include/ndds \
                -I$(NDDSHOME)/include/hpp \
                -I$(NDDSHOME)/include/hpp/rtiboost \
                -I$(NDDSHOME)/include/ndds/hpp


LIBS          = -L$(NDDSHOME)/lib/$(ARCH)
ifneq ($(findstring -g, $(DEBUG_FLAG)),)
  # debug
  LIBS       += -lnddscpp2zd -lnddscppzd -lnddsczd -lnddscorezd
else
  # not debug
  LIBS       += -lnddscpp2z -lnddscppz  -lnddscz  -lnddscorez
endif
LIBS         += $(syslibs)

EXEC         := perftest_cpp2
TYPESOURCES   = $(TYPEDIR)/test.cxx        \
                $(TYPEDIR)/testImplPlugin.cxx  \
                $(TYPEDIR)/testImpl.cxx
APPSOURCES   := RTIDDSImpl.cxx \
                perftest_cpp.cxx


TYPEOBJS      = $(TYPESOURCES:$(TYPEDIR)/%.cxx=$(OBJDIR)/%.o)
APPOBJS       = $(APPSOURCES:%.cxx=$(OBJDIR)/%.o)
OBJS          = $(TYPEOBJS) $(APPOBJS)

DIRECTORIES   = $(OBJDIR).dir $(BINDIR).dir

all : $(ARCH)

# We stick the objects in a sub directory to keep your directory clean.
$(ARCH) : $(DIRECTORIES) $(COMMONOBJS) $(EXEC:%=$(BINDIR)/%)

$(BINDIR)/$(EXEC) : $(OBJS)
	$(cxx_ld) -o $(BINDIR)/$(EXEC) $(OBJS) $(LIBS) $(cxx_ld_flags)

$(APPOBJS)  : $(OBJDIR)/%.o : %.cxx               \
                              $(TYPEDIR)/test.hpp \
                              MessagingIF.h       \
                              RTIDDSImpl.h        \
                              perftest_cpp.h    
	$(cxx_cc) $(cxx_cc_flags)  -o $@ $(DEFINES) $(INCLUDES) -c $<

ifeq '$(OS_ARCH)' 'i86Win32'
RTIDDSGENSCRIPT = rtiddsgen.bat
else
RTIDDSGENSCRIPT = rtiddsgen
endif

generate: $(TYPESOURCES)

$(TYPESOURCES) : $(IDLFILE)
	$(NDDSHOME)/bin/$(RTIDDSGENSCRIPT) \
	    -replace      \
	    -language C++03 \
	    -d $(TYPEDIR) $(RTIDDSGEN_PREPROCESSOR) \
	    $(IDLFILE)

$(TYPEOBJS) : $(OBJDIR)/%.o : $(TYPEDIR)/%.cxx  \
                              $(TYPEDIR)/test.hpp
	$(cxx_cc) $(cxx_cc_flags)  -o $@ $(DEFINES) $(INCLUDES) -c $<

clean :
	-rm -r $(OBJDIRROOT)/* $(BINDIRROOT)/*/$(EXEC)

# Here is how we create those subdirectories automatically.
%.dir :
	@if [ ! -d $* ]; then \
		mkdir -p $* ; \
	fi;