// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.
#pragma ident	"@(#)NameToken.h	1.4	00/07/17 SMI"

#ifndef NameToken_INCLUDED
#define NameToken_INCLUDED 1

#include "Location.h"
#include "StringC.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

struct NameToken {
  StringC name;
  StringC origName;
  Location loc;
};

#ifdef SP_NAMESPACE
}
#endif

#endif /* not NameToken_INCLUDED */
