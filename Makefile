#
# OMNeT++/OMNEST Makefile for ErgodicityTest
#
# This file was generated with the command:
#  opp_makemake -f --deep -KINET4_PROJ=../../inet4 -KNEDDEMO_PROJ=../../neddemo -KQUEUEINGLIB_PROJ=../../queueinglib -KQUEUEINGLIBEXT_PROJ=../../queueinglibext -KRESULTFILES_PROJ=../../resultfiles -KROUTING_PROJ=../../routing -KSOCKETS_PROJ=../../sockets -DINET_IMPORT -DQUEUEING_IMPORT -I440055(INET4_PROJ)/src -I440055(QUEUEINGLIB_PROJ) -L440055(INET4_PROJ)/src -L440055(QUEUEINGLIB_PROJ) -L440055(QUEUEINGLIBEXT_PROJ) -lINET440055(D) -lqueueinglib440055(D) -lqueueinglibext440055(D)
#

# Name of target to be created (-o option)
TARGET_DIR = .
TARGET_NAME = ErgodicityTest$(D)
TARGET = $(TARGET_NAME)$(EXE_SUFFIX)
TARGET_IMPLIB = $(TARGET_NAME)$(IMPLIB_SUFFIX)
TARGET_IMPDEF = $(TARGET_NAME)$(IMPDEF_SUFFIX)
TARGET_FILES = $(TARGET_DIR)/$(TARGET)

# User interface (uncomment one) (-u option)
USERIF_LIBS = $(ALL_ENV_LIBS) # that is, $(TKENV_LIBS) $(QTENV_LIBS) $(CMDENV_LIBS)
#USERIF_LIBS = $(CMDENV_LIBS)
#USERIF_LIBS = $(TKENV_LIBS)
#USERIF_LIBS = $(QTENV_LIBS)

# C++ include paths (with -I)
INCLUDE_PATH = -I440055(INET4_PROJ)/src -I440055(QUEUEINGLIB_PROJ)

# Additional object and library files to link with
EXTRA_OBJS =

# Additional libraries (-L, -l options)
LIBS = $(LDFLAG_LIBPATH)440055(INET4_PROJ)/src $(LDFLAG_LIBPATH)440055(QUEUEINGLIB_PROJ) $(LDFLAG_LIBPATH)440055(QUEUEINGLIBEXT_PROJ)  -lINET440055(D) -lqueueinglib440055(D) -lqueueinglibext440055(D)

# Output directory
PROJECT_OUTPUT_DIR = out
PROJECTRELATIVE_PATH =
O = $(PROJECT_OUTPUT_DIR)/$(CONFIGNAME)/$(PROJECTRELATIVE_PATH)

# Object files for local .cc, .msg and .sm files
OBJS = $O/src/ClientTcpPoisson.o $O/src/RRScheduler.o $O/src/tryQ.o $O/src/tryQStrategy.o $O/src/tryServer.o

# Message files
MSGFILES =

# SM files
SMFILES =

# Other makefile variables (-K)
INET4_PROJ=../../inet4
NEDDEMO_PROJ=../../neddemo
QUEUEINGLIB_PROJ=../../queueinglib
QUEUEINGLIBEXT_PROJ=../../queueinglibext
RESULTFILES_PROJ=../../resultfiles
ROUTING_PROJ=../../routing
SOCKETS_PROJ=../../sockets

#------------------------------------------------------------------------------

# Pull in OMNeT++ configuration (Makefile.inc)

ifneq ("$(OMNETPP_CONFIGFILE)","")
CONFIGFILE = $(OMNETPP_CONFIGFILE)
else
CONFIGFILE = $(shell opp_configfilepath)
endif

ifeq ("$(wildcard $(CONFIGFILE))","")
$(error Config file '$(CONFIGFILE)' does not exist -- add the OMNeT++ bin directory to the path so that opp_configfilepath can be found, or set the OMNETPP_CONFIGFILE variable to point to Makefile.inc)
endif

include $(CONFIGFILE)

# Simulation kernel and user interface libraries
OMNETPP_LIBS = $(OPPMAIN_LIB) $(USERIF_LIBS) $(KERNEL_LIBS) $(SYS_LIBS)
ifneq ($(PLATFORM),win32.x86_64)
LIBS += -Wl,-rpath,$(abspath 440055(INET4_PROJ)/src) -Wl,-rpath,$(abspath 440055(QUEUEINGLIB_PROJ)) -Wl,-rpath,$(abspath 440055(QUEUEINGLIBEXT_PROJ))
endif

COPTS = $(CFLAGS) $(IMPORT_DEFINES) -DINET_IMPORT -DQUEUEING_IMPORT $(INCLUDE_PATH) -I$(OMNETPP_INCL_DIR)
MSGCOPTS = $(INCLUDE_PATH)
SMCOPTS =

# we want to recompile everything if COPTS changes,
# so we store COPTS into $COPTS_FILE (if COPTS has changed since last build)
# and make the object files depend on it
COPTS_FILE = $O/.last-copts
ifneq ("$(COPTS)","$(shell cat $(COPTS_FILE) 2>/dev/null || echo '')")
  $(shell $(MKPATH) "$O")
  $(file >$(COPTS_FILE),$(COPTS))
endif

#------------------------------------------------------------------------------
# User-supplied makefile fragment(s)
#------------------------------------------------------------------------------

# Main target
all: $(TARGET_FILES)

$(TARGET_DIR)/% :: $O/%
	@mkdir -p $(TARGET_DIR)
	$(Q)$(LN) $< $@
ifeq ($(TOOLCHAIN_NAME),clang-msabi)
	-$(Q)-$(LN) $(<:%.dll=%.lib) $(@:%.dll=%.lib) 2>/dev/null
endif

$O/$(TARGET): $(OBJS)  $(wildcard $(EXTRA_OBJS)) Makefile $(CONFIGFILE)
	@$(MKPATH) $O
	@echo Creating executable: $@
	$(Q)$(CXX) $(LDFLAGS) -o $O/$(TARGET) $(OBJS) $(EXTRA_OBJS) $(AS_NEEDED_OFF) $(WHOLE_ARCHIVE_ON) $(LIBS) $(WHOLE_ARCHIVE_OFF) $(OMNETPP_LIBS)

.PHONY: all clean cleanall depend msgheaders smheaders

.SUFFIXES: .cc

$O/%.o: %.cc $(COPTS_FILE) | msgheaders smheaders
	@$(MKPATH) $(dir $@)
	$(qecho) "$<"
	$(Q)$(CXX) -c $(CXXFLAGS) $(COPTS) -o $@ $<

%_m.cc %_m.h: %.msg
	$(qecho) MSGC: $<
	$(Q)$(MSGC) -s _m.cc -MD -MP -MF $O/$(basename $<)_m.h.d $(MSGCOPTS) $?

%_sm.cc %_sm.h: %.sm
	$(qecho) SMC: $<
	$(Q)$(SMC) -c++ -suffix cc $(SMCOPTS) $?

msgheaders: $(MSGFILES:.msg=_m.h)

smheaders: $(SMFILES:.sm=_sm.h)

clean:
	$(qecho) Cleaning $(TARGET)
	$(Q)-rm -rf $O
	$(Q)-rm -f $(TARGET_FILES)
	$(Q)-rm -f $(call opp_rwildcard, . , *_m.cc *_m.h *_sm.cc *_sm.h)

cleanall:
	$(Q)$(CLEANALL_COMMAND)
	$(Q)-rm -rf $(PROJECT_OUTPUT_DIR)

help:
	@echo "$$HELP_SYNOPSYS"
	@echo "$$HELP_TARGETS"
	@echo "$$HELP_VARIABLES"
	@echo "$$HELP_EXAMPLES"

# include all dependencies
-include $(OBJS:%=%.d) $(MSGFILES:%.msg=$O/%_m.h.d)
