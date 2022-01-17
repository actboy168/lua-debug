

#ifdef LUAJIT_VERSION
#include <lj_obj.h>
using Table= GCtab;
using Closure = GCfunc;
using UpVal = GCupval;
using Proto = GCproto;
using UDate = GCudata;
using TString = GCstr;
using StkId = TValue*;
#else
#include <lapi.h>
#include <lgc.h>
#include <lobject.h>
#include <lstate.h>
#include <ltable.h>
#endif
