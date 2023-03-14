/*************************************************************************\
**  source       * ssmemlimit.h
**  directory    * ss
**  description  * generic memory limit handling service
**               * 
**               * Copyright (C) 2006 Solid Information Technology Ltd
\*************************************************************************/
/*
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; only under version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA
*/


#ifndef SSMEMLIMIT_H
#define SSMEMLIMIT_H

#include <ssenv.h>
#include <ssstddef.h>
#include <ssc.h>
#include <sslimits.h>
#include <ssdebug.h>

typedef void (SsMemLimitCallBackFunT)(
                void* ctx,
                int id,
                size_t limit,
                size_t old_usage,
                size_t new_usage);

/* note: do not use members of this struct directly! */
typedef struct {
        ss_debug(int ml_check;)
        int ml_id;
        size_t ml_limit;
        void* ml_ctx;
        SsMemLimitCallBackFunT* ml_exceedcallback;
        SsMemLimitCallBackFunT* ml_fallbelowcallback;
} SsMemLimitT;

void SsMemLimitInitBuf(SsMemLimitT* limitbuf,
                       int id,
                       size_t limit_value,
                       void* ctx,
                       SsMemLimitCallBackFunT* exceedcallback,
                       SsMemLimitCallBackFunT* fallbelowcallback);

void SsMemLimitDoneBuf(SsMemLimitT* limit);

void SsMemLimitCheck(SsMemLimitT* limit,
                     size_t old_memusage,
                     size_t new_memusage);
    
bool SsMemLimitAdjust(SsMemLimitT* limit,
                      size_t new_limitvalue,
                      size_t current_memusage);

size_t SsMemLimitGetValue(SsMemLimitT* limit);

#endif /* SSMEMLIMIT_H */


