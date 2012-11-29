///////////////////////////////////////////////////////////////////////////////
///
/// Copyright (c) 2007 - Oliver Schneider (assarbad.net)
///
/// Defines for the version information in the resource file
///
/// (File was in the PUBLIC DOMAIN  - Author: ddkwizard.assarbad.net)
///////////////////////////////////////////////////////////////////////////////

// $Id$

#ifndef __EXEVERSION_H_VERSION__
#define __EXEVERSION_H_VERSION__ 100

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "buildnumber.h"

// ---------------------------------------------------------------------------
// Several defines have to be given before including this file. These are:
// ---------------------------------------------------------------------------
#define TEXT_AUTHOR            Jens Scheffler (jensscheffler.de) and Oliver Schneider (assarbad.net) // author (optional value)
#define PRD_MAJVER             1 // major product version
#define PRD_MINVER             2 // minor product version
#define PRD_BUILD              1 // build number for product
#define FILE_MAJVER            1 // major file version
#define FILE_MINVER            2 // minor file version
#define FILE_BUILD             _FILE_VERSION_BUILD // build number
#define EXE_YEAR               2004, 2005, 2007 // current year or timespan (e.g. 2003-2007)
#define TEXT_WEBSITE           http:/##/jensscheffler.de/dfhl.html // website
#define TEXT_PRODUCTNAME       Duplicate File Hard Linker // product's name
#define TEXT_FILEDESC          Tool to create hard links of duplicate files on the disk and save space // component description
#define TEXT_COMPANY           Jens Scheffler & Oliver Schneider // company
#define TEXT_MODULE            DFHL // module name
#define TEXT_COPYRIGHT         Copyright \xA9 EXE_YEAR TEXT_COMPANY // copyright information
// #define TEXT_SPECIALBUILD      // optional comment for special builds
#define TEXT_INTERNALNAME      DFHL.exe // copyright information
// #define TEXT_COMMENTS          // optional comments
// ---------------------------------------------------------------------------
// ... well, that's it. Pretty self-explanatory ;)
// ---------------------------------------------------------------------------

#endif // __EXEVERSION_H_VERSION__
