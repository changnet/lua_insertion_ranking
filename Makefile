##### Build defaults #####
CMAKE = cmake
MAKE = make
CXX = g++

TARGET_SO =         lua_insertion_ranking.so
TARGET_A  =         liblua_insertion_ranking.a

# -std=c++03 warning: ISO C++ 1998 does not support ‘long long’ [-Wlong-long]
CFLAGS =            -g3 -Wall -fno-inline
#CFLAGS =           -O2 -Wall #-DNDEBUG

SHAREDDIR = .sharedlib
STATICDIR = .staticlib

AR= ar rcu
RANLIB= ranlib

OBJS = linsertion_ranking.o

SHAREDOBJS = $(addprefix $(SHAREDDIR)/,$(OBJS))
STATICOBJS = $(addprefix $(STATICDIR)/,$(OBJS))

DEPS := $(SHAREDOBJS + STATICOBJS:.o=.d)

#The dash at the start of '-include' tells Make to continue when the .d file doesn't exist (e.g. on first compilation)
-include $(DEPS)

.PHONY: all clean test staticlib sharedlib

$(SHAREDDIR)/%.o: %.cpp
	@[ ! -d $(SHAREDDIR) ] & mkdir -p $(SHAREDDIR)
	$(CXX) -c $(CFLAGS) -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -o $@ $<

$(STATICDIR)/%.o: %.cpp
	@[ ! -d $(STATICDIR) ] & mkdir -p $(STATICDIR)
	$(CXX) -c $(CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -o $@ $<

all: $(TARGET_SO) $(TARGET_A)

staticlib: $(TARGET_A)
sharedlib: $(TARGET_SO)

$(TARGET_SO): $(SHAREDOBJS)
	$(CXX) $(LDFLAGS) -shared -o $@ $(SHAREDOBJS)

$(TARGET_A): $(STATICOBJS)
	$(AR) $@ $(STATICOBJS)
	$(RANLIB) $@

test:
	lua test.lua

clean:
	rm -f -R $(SHAREDDIR) $(STATICDIR) $(TARGET_SO) $(TARGET_A)
