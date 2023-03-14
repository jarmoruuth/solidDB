/*************************************************************************\
**  source       * ssstring.c
**  directory    * ss
**  description  * String utility functions.
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

#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------


Limitations:
-----------


Error handling:
--------------


Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include "ssc.h"
#include "ssenv.h"
#include "sschcvt.h"
#include "ssmem.h"
#include "sssprint.h"
#include "ssstring.h"

char* SsStrTrim(char* str)
{
        return(SsStrTrimRight(SsStrTrimLeft(str)));

}

char* SsStrTrimLeft(char* str)
{
        while (ss_isspace(*str)) {
            str++;
        }
        return(str);
}

char* SsStrTrimRight(char* str)
{
        int len;

        len = strlen(str);
        while (len > 0 && ss_isspace(str[len-1])) {
            len--;
            str[len] = '\0';
        }
        return(str);
}

char* SsStrReplaceDup(
        const char* src, 
        const char* pattern, 
        const char* replacement)
{
        char* dst;
        size_t dst_size;
        size_t pat_len = strlen(pattern);
        size_t rep_len = strlen(replacement);
        size_t src_len = strlen(src);
        const char* src_pos;
        char* dst_pos;
        uint match_count = 1;        
        
        ss_dassert(pat_len != 0);
        dst_size = src_len + 1; 
        if (pat_len < rep_len) {
            src_pos = src;
            for (match_count = 0; ; match_count++) {
                src_pos = strstr(src_pos, pattern);
                if (src_pos == NULL) {
                    break;
                }
                src_pos += pat_len;
            }
            dst_size += (match_count * (rep_len - pat_len));
        }
        dst = SsMemAlloc(dst_size);
        if (match_count != 0) {
            dst_pos = dst;
            src_pos = src;
            for (;;) {
                char* new_src_pos = strstr(src_pos, pattern);
                size_t gap_between_matches;

                if (new_src_pos == NULL) {
                    memcpy(dst_pos, src_pos, src_len - (src_pos - src) + 1);
                    break;
                }
                gap_between_matches = new_src_pos - src_pos;
                if (gap_between_matches != 0) {
                    memcpy(dst_pos, src_pos, gap_between_matches);
                    dst_pos += gap_between_matches;
                }
                memcpy(dst_pos, replacement, rep_len);
                dst_pos += rep_len;
                src_pos = new_src_pos + pat_len;
            }
        } else {
            /* match count known to be 0, just copy the string */
            memcpy(dst, src, dst_size);
        }
        return (dst);
}

static char default_separators[] = "\t ,";

/*##**********************************************************************\
 * 
 *
 *       SsStrScanString
 *
 * Scans a string value from a string.
 * 
 * Parameters : 
 *
 *      scanstr - in, use
 *          string to be scanned
 *
 *	separators - in, use
 *          string containing legal separator characters, typically " \t,"
 *		
 *	scanindex - in out, use
 *		pointer to index to value string. For the first value it 
 *          should be initialized to 0
 *
 *      comment_char - in, use
 *
 *	value_give - out, give
 *		pointer to char* where a copy of the scanned string is stored
 *		
 * Return value :
 *      TRUE when a valid value was found or
 *      FALSE otherwise    
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsStrScanString(
        char* scanstr,
        char* separators,
        uint* scanindex,
        char  comment_char,
        char** value_give)
{
        char* value_str, *org_value_str;
        char* end;
        bool foundp;
        char map[1 << CHAR_BIT];
        uint tmp_scanindex = 0;

        if (separators == NULL) {
            separators = default_separators;
        }
        if (scanindex == NULL) {
            scanindex = &tmp_scanindex;
        }
        org_value_str = SsMemStrdup(scanstr);
        value_str = org_value_str;

        memset(map, 0, sizeof(map));
        while (*separators != '\0') {
            map[(uchar)*separators] = (char)~0;
            separators++;
        }
        map[(uchar)comment_char] = '\0';
        map[(uchar)'\n'] = '\0';
        value_str += *scanindex;
        while (map[(uchar)*value_str]) {
            value_str++;
            (*scanindex)++;
        }
        end = value_str;
        map[0] = ~map[0];
        map[(uchar)'\n'] = ~map[(uchar)'\n'];
        map[(uchar)comment_char] = ~map[(uchar)comment_char];
        while (!map[(uchar)*end]) {
            end++;
            (*scanindex)++;
        }
        if (end != value_str) {
            foundp = TRUE;
            *value_give = SsMemAlloc((end - value_str) + 1);
            memcpy(*value_give, value_str, end - value_str);
            (*value_give)[end - value_str] = '\0';
        } else {
            *value_give = NULL;
            foundp = FALSE;
        }
        map[0] = ~map[0];
        map[(uchar)'\n'] = ~map[(uchar)'\n'];
        map[(uchar)comment_char] = ~map[(uchar)comment_char];
        while (map[(uchar)*end]) {
            end++;
            (*scanindex)++;
        }
        SsMemFree(org_value_str);
        return(foundp);
}

#define XCLEAR (uchar)0
#define XSEPARATOR ~(uchar)0 

/*##**********************************************************************\
 * 
 *
 *       SsStrScanStringWQuoting
 *
 * Scans a string value from a string. Values may span separators if enclosed
 * in quotes, eg. when space is a separator, 'foo bar' -> 'foo' and
 * '"foo bar"' -> 'foo bar'. Surrounding quotes are not included in returned
 * string.
 *
 * Opening quote is a separator if preceded by non-separators.  Closing quote
 * is a separator if followed by non-separators. Therefore '"foo"bar' will
 * first give you 'foo' and then 'bar'.  Doubling the quote will escape it
 * ('""foo""' -> '"foo"' and '""foo bar""' -> '"foo' and 'bar"').
 *
 * Triple quotes (""") or more will be interpreted as being inside quotes, ie.
 * '"""foo bar"""' -> '"foo bar"'.  NOT '"', 'foo bar', '"',
 *
 * Parameters : 
 *
 *      scanstr - in, use
 *          string to be scanned
 *
 *	separators - in, use
 *          string containing legal separator characters, typically " \t,"
 *		
 *	scanindex - in out, use
 *		pointer to index to value string. For the first value it 
 *          should be initialized to 0
 *
 *  comment_char - in, use
 *      char that acts as a comment identifier.  set to 0 if none.
 *
 *	value_give - out, give
 *		pointer to char* where a copy of the scanned string is stored
 *		
 * Return value :
 *      TRUE when a valid value was found or
 *      FALSE otherwise    
 * 
 * Limitations  : 
 *
 *      Newline (\n) can be a separator BUT it is treated like it terminates
 *      the string.
 *
 *      Double quote (") can't be a separator.
 *
 *      If you want quotes, you have to hack a bit, say, change them to \001
 *      and use that as a separator.
 *      
 * 
 * Globals used : 
 */
bool SsStrScanStringWQuoting(
        char* scanstr,
        char* separators,
        uint* scanindex,
        char  comment_char,
        char** value_give)
{
        char* value_str, *org_value_str;
        char* end;
        char* value_end;
        bool foundp;
        char map[1 << CHAR_BIT]; /* for all characters */
        char* sep = NULL;
        uint sidx = 0;

        /* new code variables */
        bool enclosed;
        bool content;
        int nquotes;
        int lastquote;

        /* straight run vars */
        char* res_str;
        bool opened;
        bool closed;

        enclosed = FALSE;
        content = FALSE;
        nquotes = 0;
        lastquote = -1;
        res_str = NULL;
        opened = FALSE;
        closed = FALSE;
        
        if (separators == NULL) {
            separators = default_separators;
        }
        if (scanindex != NULL) {
            sidx = *scanindex;
        }

#ifdef SS_DEBUG
        if (comment_char != '\0') {
            ss_info_dassert((strchr(separators, comment_char) == NULL),
                            ("Comment char (0x%02x) is included in separators",
                             comment_char));
        }
        ss_info_dassert((strchr(separators, '\"') == NULL),
                        ("Sorry, quote (\") is not allowed as a separator"));
        
#endif /* SS_DEBUG */

        /* mr 20050222: Now I coded this function and also made the second
         * version of it, but I've no idea why the newer one (the next block
         * of code) is better than the older one.  I didn't enable it,
         * probably, because I haven't tested it, but really, something has
         * to go.
         */
#if 0 /* straight code run */ /* 0 -> use old code */

        org_value_str = SsMemStrdup(scanstr);
        value_str = scanstr;
        res_str   = org_value_str;

        /* we must be careful that '\0' as comment char doesn't muck
         * things up */
        
        memset(map, 0, sizeof(map));  /* clear map */
        sep = separators; /* copy; let's not mess up the parameters */
        while (*sep != '\0') { /* mark separators in the map as true */
            map[(uchar)*sep] = XSEPARATOR;
            sep++;
        }
#if 0 /* useless */
        map[comment_char] = XCLEAR; /* useless; clear anyway */
        map['\n']         = XCLEAR; /* useless; clear anyway */
#endif
        value_str += sidx;
        while (map[(uchar)*value_str]) { /* skip separators in front of data */
            value_str++;
            sidx++;
        }
        end = value_str; /* value_str = data, end(now) = start of data */
        map[0]            = XSEPARATOR;
        map['\n']         = XSEPARATOR;
        map[comment_char] = XSEPARATOR;

        opened = FALSE;
        if (*end == '\"') {            
            for (nquotes = 0; *end == '\"'; end++, sidx++) {
                nquotes++;
            }
            opened = ((nquotes & 1) == 1);
            nquotes >>= 1;

            for (; nquotes > 0; nquotes--) { /* output quote-escaped quotes */
                *(res_str++) = '\"';
            }
            ss_dassert(nquotes == 0);

            if (opened) {
                /* change map */
                sep = separators; 
                while (*sep != '\0') { /* separators don't break now */
                    map[(uchar)*sep] = XCLEAR;
                    sep++;
                }
                map[comment_char] = XCLEAR;
                map[0]            = XSEPARATOR;
            }
        }

        map['\"'] = XSEPARATOR;

        while (1) {
            while (!map[(uchar)*end]) { /* scan/copy str up to a separator */
                *(res_str++) = *(end++);
                sidx++;
            }
            
            /* hit a separator */
            
            if (*end == '\"') {
                for (nquotes = 0; *end == '\"'; end++, sidx++) {
                    nquotes++;
                }
                closed = ((nquotes & 1) == 1); /* closed may be an opening
                                                * too */
                nquotes >>= 1;
                
                for (; nquotes > 0; nquotes--) {
                    *(res_str++) = '\"';
                }                

                if (!opened || closed) { /* any separator */
                    break; /* while */
                }

            } else {
                break;
            }
            /* if we get here then quotes are open and there may have been
             * quote-escaped quotes in the string. */
        }

        map['\"'] = XCLEAR;

        /* skip separators */
        if (opened) {
            sep = separators; 
            while (*sep != '\0') { /* separators break again */
                map[(uchar)*sep] = XSEPARATOR;
                sep++;
            }
            map[comment_char] = XSEPARATOR;
            map[0]            = XCLEAR;
        }
        while (map[(uchar)*end]) {
            end++;
            sidx++;
        }

        /* done; copy value */
        if (scanindex != NULL) {
            *scanindex = sidx;
        }
                
        if (res_str != org_value_str) {            
            foundp = TRUE;
            *res_str = '\0';
            *value_give = SsMemStrdup(org_value_str);
        } else {
            foundp = FALSE;
            *value_give = NULL;
        }

        SsMemFree(org_value_str);

        return foundp;
#else /* old code */        
        
        org_value_str = SsMemStrdup(scanstr);
        value_str = org_value_str;
        
        /* we must be careful that '\0' as comment char doesn't muck
         * things up */
        
        memset(map, 0, sizeof(map));  /* clear map */
        sep = separators; /* copy; let's not mess up the parameters */
        while (*sep != '\0') { /* mark separators in the map as true */
            map[(uchar)*sep] = XSEPARATOR;
            sep++;
        }
        map[(uchar)comment_char] = XCLEAR;
        map['\n']         = XCLEAR;
        value_str += sidx;
        while (map[(uchar)*value_str]) { /* skip separators in front of data */
            value_str++;
            sidx++;
        }
        end = value_str; /* value_str = data, end(now) = start of data */
        map[0]            = XSEPARATOR;
        map['\n']         = XSEPARATOR;
        map[(uchar)comment_char] = XSEPARATOR;
        
        if ((uchar)*end == '\"') { /* quoted string */
            value_str++; /* skip quote */
            end++;
            sidx++;
            
            sep = separators;
            while (*sep != '\0') { /* now separators aren't separators */
                map[(uchar)*sep] = XCLEAR;
                sep++;
            }
            map[(uchar)comment_char] = XCLEAR;      /* now not a separator */
            map['\"']         = XSEPARATOR;  /* " is now a separator */
            /* we must reset 0 to SEP as it may be the comment char */
            map[0]            = XSEPARATOR;  /* must end the seek */
            map['\n']         = XSEPARATOR;
            
            while (!map[(uchar)*end]) { /* seek " or end of line */
                end++;
                sidx++;
            }

            /* toggle separators back */
            map['\"']         = XCLEAR;     /* " is not a separator anymore */
            map[(uchar)comment_char] = XSEPARATOR; /* now a separator */
            map['\n']         = XSEPARATOR;
            sep = separators;
            while (*sep != '\0') { /* now separators are separators again */
                map[(uchar)*sep] = XSEPARATOR;
                sep++;
            }

            /* now we handle an illegal case like this:
             *
             *     "foo"bar
             */
            value_end = end;
            if ((uchar)*end == '\"') { /* handle ending quote */
                end++;
                sidx++; /* skip quote */
            } else {
                ; /* syntax error, mismatching quotes */
            }
        } else { /* not a quoted string */
            while (!map[(uchar)*end]) { /* seek first separator */
                end++;
                sidx++;
            }
            value_end = end;
        }

        if (value_end != value_str) { /* if string is not empty */
            foundp = TRUE;
            *value_give = SsMemAlloc((value_end - value_str) + 1);
            memcpy(*value_give, value_str, value_end - value_str);
            (*value_give)[value_end - value_str] = '\0';
        } else { /* string IS empty */
            *value_give = NULL;
            foundp = FALSE;
        }
        map[0]    = XCLEAR;         /* now not a separator */
        map['\n'] = XCLEAR;         /* now not a separator */
        map[(uchar)comment_char] = XCLEAR; /* not a separator */
        while (map[(uchar)*end]) { /* skip trailing separators */
            end++;
            sidx++;
        }
        if (scanindex != NULL) {
            *scanindex = sidx;
        }
        SsMemFree(org_value_str);

        return(foundp);
#endif /* straight code run */        
}

#undef XCLEAR
#undef XSEPARATOR

/*##**********************************************************************\
 * 
 *		SsHexStr
 * 
 * Creates a hex string from character string. target buffer is allocated
 * and must be released by the caller.
 * 
 * Parameters : 
 * 
 *	buf - 
 *		
 *		
 *	buflen - 
 *		
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* SsHexStr(char* buf, size_t buflen)
{
        size_t i;
        char* hexbuf;

        hexbuf = SsMemAlloc(2 * buflen + 1);
        for (i = 0; i < buflen; i++) {
            SsSprintf(&hexbuf[2 * i], "%02x", buf[i] & 0xff);
        }
        return(hexbuf);
}

/*##**********************************************************************\
 *
 *      SsStrUnquote
 *
 * Returns the passed string without it's surrounding quotes (if any).
 *
 * Parameters:
 *      str - in, use, out!
 *          string to unquote.  Function modifies this when removing quotes.
 *
 * Return value - ref:
 *      pointer to unquoted string (the part of the original).
 *
 * Limitations:
 *
 * Globals used:
 */
char* SsStrUnquote(char* str) {
        int len = 0;

        if (str[0] == '\"') { /* quoted */
            len = strlen(str);
            if (str[len-1] == '\"') {
                str[len-1] = '\0';
            } else {
                ; /* quote mismatch or untrimmed string */
            }
            
            return &(str[1]);
            
        } else {
            
            return str;            
        }
}

/*##**********************************************************************\
 *
 *      SsStrOvercat
 *
 * Catenates one string onto other, but so, that if space is tight, then the
 * catenated (the one being added) string takes extra space from the original
 * contents so it will fit.  If the catenated string won't fit by itself
 * either, then it is cut from the end. Result is always NULL-terminated.
 *
 * If there is no space to do anything (len == 0) then doesn't do anything.
 *
 * Parameters:
 *      dst - in, out
 *          original string/buffer to which we add
 *
 *      src - in
 *          string to be catenated
 *
 *      len - in
 *          length of allowed space in dst (allow for \0!)
 *
 * Return value - ref:
 *      pointer to dst.
 *
 * Limitations:
 *
 * Globals used:
 */
char* SsStrOvercat(char* dst, char* src, int len)
{
#if 1 /* sadly the old code became redundant */
        return SsStrSeparatorOvercat(dst, src, (char *)"", len);
#else /* it's like killing one's own child, I can't do it :-P */       
        int d_len = strlen(dst);
        int s_len = strlen(src);
        int skip;           /* how much of dst's content we skip (save) */
        int copy, d;
        int use;

        ss_dassert(dst != NULL);
        ss_dassert(src != NULL);
        ss_dassert(len > 0);
        if (len < 0) { /* be robust for product */
            len = 0;
        }

        if (len == 0) {
            return dst;
        }
        
        use = SS_MIN(len, d_len + s_len + 1) - 1; /* 1 reserved for NULL */
        
        d = use - s_len; /* d = space left for dst content */
        if (d <= 0) {    /* no space for old content */
            skip = 0;
            copy = use;
        } else {         /* some space for old content */
            skip = SS_MIN(d, d_len); 
            copy = s_len;
        }           
        
        ss_dprintf_4(("SsStrOvercat: len=%d, use=%d, d_len=%d, s_len=%d, d=%d, skip=%d, copy=%d\n", len, use, d_len, s_len, d, skip, copy));

        memcpy(dst+skip, src, copy);
        dst[use] = '\0';
        
        return dst; /* just for orthogonality with strcat */
#endif
}

/*##**********************************************************************\
 *
 *      SsStrSeparatorOvercat
 *
 * Cat a new string after old content and include a separator between them.
 * Works so that if the added string won't fit in the provided space, then
 * as much of old content is overwritten from the end to accomodate the
 * added string and the separator.  If nothing of the old content remains,
 * then the separator is dropped.
 *
 * If len == 0, won't do anything.
 *
 * Parameters:
 *      dst - in, out
 *          buffer into which we overcat
 *
 *      src - in
 *          what to add
 *
 *      sep - in
 *          sepatator to be added in between the two strings (or what's left
 *          of them).
 *
 *      len - in
 *          length of space into which dst + sep + src must fit.
 *
 * Return value - ref:
 *      pointer to dst as with strcat.
 *
 * Limitations:
 *
 * Globals used:
 */
char* SsStrSeparatorOvercat(char* dst, char* src, char* sep, int len)
{
        int dst_len = strlen(dst);
        int src_len = strlen(src);
        int sep_len = strlen(sep);
        int skip;        /* how much of dst's content we skip (save) */
        int src_copy, sep_copy, d;
        int use;

        ss_dassert(dst != NULL);
        ss_dassert(src != NULL);
        ss_dassert(sep != NULL);
        ss_dassert(len > 0);

        if (len <= 0) { /* robustness */
            return dst;
        }
        
        /* 1 reserved for NULL */
        use = SS_MIN(len, dst_len + src_len + sep_len + 1) - 1;
        
        d = use - src_len - sep_len; /* d = space left for dst content */
        if (d <= 0) {    /* no space for old content */
            skip     = 0;
            src_copy = use;
            sep_copy = 0;
        } else {         /* some space for old content */
            skip = SS_MIN(d, dst_len); 
            src_copy = src_len;
            if (src_copy == 0) { /* nothing to add --> no separator */
                sep_copy = 0;
            } else {
                sep_copy = sep_len;
            }
        }           
        
        ss_dprintf_4(("SsStrSeparatorOvercat: len=%d, use=%d, dst_len=%d, src_len=%d, sep_len=%d, d=%d, skip=%d, src_copy=%d, sep_copy=%d\n",
                      len, use, dst_len, src_len, sep_len, d, skip,
                      src_copy, sep_copy));

        memcpy(dst+skip,          sep, sep_copy);
        memcpy(dst+skip+sep_copy, src, src_copy);
        dst[use] = '\0';
        
        return dst; /* just for orthogonality with strcat */
}


/* EOF */
