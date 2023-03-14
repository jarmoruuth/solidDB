# Copyright (C) 2006 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMYSQL_SERVER")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMYSQL_SERVER")

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DMYSQL_SERVER -DSAFEMALLOC -DSAFE_MUTEX -DDBUG_ON")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DMYSQL_SERVER -DSAFEMALLOC -DSAFE_MUTEX -DDBUG_ON")

#
# Set options specific for the platform e.g. Linux, FreeBSD etc
#
FOREACH(_base
#    ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_VERSION}-${CMAKE_SYSTEM_PROCESSOR}
#    ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_VERSION}
    ${CMAKE_SYSTEM_NAME})
    string(TOUPPER ${_base} _system)
    ADD_DEFINITIONS("-DSS_${_system}")
ENDFOREACH()

ADD_DEFINITIONS("-DMYSQL_DYNAMIC_PLUGIN")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include {CMAKE_SOURCE_DIR}/sql
                   {CMAKE_SOURCE_DIR}/regex include
                   {CMAKE_SOURCE_DIR}/extra/yassl/include)

SET(SOLIDDB_SOURCES	dbe/dbe0blobg2.c dbe/dbe0brb.c dbe/dbe0bstr.c dbe/dbe0catchup.c
			dbe/dbe0curs.c dbe/dbe0db.c dbe/dbe0erro.c dbe/dbe0hsbbuf.c dbe/dbe0hsbg2.c
			dbe/dbe0lb.c dbe/dbe0ld.c dbe/dbe0logi.c dbe/dbe0opensrc.c dbe/dbe0rel.c
			dbe/dbe0seq.c dbe/dbe0spm.c dbe/dbe0trdd.c dbe/dbe0tref.c dbe/dbe0trut.c
			dbe/dbe0trx.c dbe/dbe0type.c dbe/dbe0user.c dbe/dbe1trdd.c dbe/dbe2back.c
			dbe/dbe4nomme.c dbe/dbe4rfwd.c dbe/dbe4srch.c dbe/dbe4srli.c dbe/dbe4svld.c
			dbe/dbe4tupl.c dbe/dbe5dsea.c dbe/dbe5imrg.c dbe/dbe5inde.c dbe/dbe5isea.c
			dbe/dbe5ivld.c dbe/dbe6bkey.c dbe/dbe6bkrs.c dbe/dbe6blob.c dbe/dbe6bmgr.c
			dbe/dbe6bnod.c dbe/dbe6bsea.c dbe/dbe6btre.c dbe/dbe6cpm.c dbe/dbe6finf.c
			dbe/dbe6gobj.c dbe/dbe6iom.c dbe/dbe6lmgr.c dbe/dbe6log.c dbe/dbe6srk.c
			dbe/dbe7binf.c dbe/dbe7cfg.c dbe/dbe7ctr.c dbe/dbe7gtrs.c dbe/dbe7hdr.c
			dbe/dbe7lbm.c dbe/dbe7logf.c dbe/dbe7rfl.c dbe/dbe7rtrx.c dbe/dbe7trxb.c
			dbe/dbe7trxi.c dbe/dbe8cach.c dbe/dbe8clst.c dbe/dbe8cpls.c dbe/dbe8flst.c
			dbe/dbe8seql.c dbe/dbe8srec.c dbe/dbe8trxl.c dbe/dbe9blst.c
			dt/dt0date.c dt/dt0dfloa.c dt/dt1dfl.c
			est/est1est.c est/est1pla.c
			res/rs0atype.c res/rs0auth.c res/rs0avacc.c res/rs0aval.c res/rs0cardi.c
			res/rs0cons.c res/rs0defno.c res/rs0entna.c res/rs0error.c res/rs0event.c
			res/rs0key.c res/rs0order.c res/rs0pla.c res/rs0rbuf.c res/rs0relh.c
			res/rs0sdefs.c res/rs0sqli.c res/rs0sysi.c res/rs0tnum.c res/rs0trend.c
			res/rs0ttype.c res/rs0tval.c res/rs0vbuf.c res/rs0viewh.c res/rs1atsrv.c
			res/rs1avcmp.c res/rs1avcvt.c res/rs1avflat.c res/rs1avget.c res/rs1avinv.c
			res/rs1avset.c res/rs1tvsrv.c res/rs2avcvt.c res/sqlest.c
			ss/ss1utf.c ss/sscacmem.c ss/sschcvt.c ss/sscputst.c ss/sscrand.c
			ss/ssdebug.c ss/ssdtoa.c ss/ssdtow.c ss/ssenv.c ss/ssfake.c
			ss/ssfile.c ss/ssfilwnt.c ss/ssfncnv.c ss/ssfnsplt.c ss/ssfnsrch.c ss/ssint4.c
			ss/ssint8.c ss/sslcs.c ss/sslscan.c ss/ssltoa.c ss/ssltow.c ss/ssmath.c
			ss/ssmem.c ss/ssmemchk.c ss/ssmemcmp.c ss/ssmemlimit.c ss/ssmemtrc.c ss/ssmemunx.c
			ss/ssmemwnt.c ss/ssmsglog.c ss/ssnumuti.c ss/sspmon.c ss/ssproces.c
			ss/ssqmem.c ss/ssqmempf.c ss/ssrand.c ss/ssrtcov.c ss/ssscan.c
			ss/sssemdbg.c ss/sssempux.c ss/sssemstk.c ss/sssemwnt.c
			ss/ssservic.c ss/sssprint.c ss/sssqltrc.c ss/ssstring.c ss/sssysres.c
			ss/ssthread.c ss/sstime.c ss/sstimer.c ss/sstlog.c ss/sstraph2.c ss/sstraph.c
			ss/ssutf.c ss/ssutiwnt.c ss/ssvereesf.c ss/sswcs.c ss/sswfile.c ss/sswscan.c
			su/su0bmap.c su/su0bsrch.c su/su0bstre.c su/su0bubp.c su/su0burdr.c
			su/su0buwr.c su/su0cfgl.c su/su0chcvt.c su/su0collation.c su/su0crc32.c
			su/su0ctc.c su/su0err.c su/su0error.c su/su0gate.c su/su0inifi.c su/su0li3stub.c
			su/su0list.c su/su0mbsvf.c su/su0mesl.c su/su0msgs.c su/su0opensrc.c su/su0param.c
			su/su0parr2.c su/su0parr.c su/su0pars.c su/su0prof.c su/su0rand.c su/su0rbtr.c
			su/su0regis.c su/su0scan.c su/su0sdefs.c su/su0slike.c su/su0svfil.c su/su0time.c
			su/su0usrid.c su/su0vfil.c su/su0vmem.c su/su0wlike.c
			tab/tab0admi.c tab/tab0auth.c tab/tab0blob.c tab/tab0blobg2.c tab/tab0conn.c
			tab/tab0info.c tab/tab0minisql.c tab/tab0opensrc.c tab/tab0relc.c tab/tab0relh.c
			tab/tab0sche.c tab/tab0seq.c tab/tab0srv.c tab/tab0sysproperties.c tab/tab0tint.c
			tab/tab0tli.c tab/tab0tran.c tab/tab1dd.c tab/tab1erro.c tab/tab1refact.c
			ui/ui0msgsf.c ui/ui0srvsf.c
			uti/uti0bufva.c uti/uti0dyn.c uti/uti0vab.c uti/uti0vad.c uti/uti0vaf.c
			uti/uti0vai8.c uti/uti0vai.c uti/uti0var.c uti/uti0vcmp.c uti/uti0vtpb.c
			xs/xs0acnd.c xs/xs0error.c xs/xs0mgr.c xs/xs0qsort.c xs/xs0sqli.c xs/xs1cfg.c
			xs/xs1merg.c xs/xs1pres.c xs/xs1sort.c xs/xs2cmp.c xs/xs2mem.c xs/xs2stre.c
			xs/xs2tfmgr.c xs/xs2tupl.c 
			handler/ha_soliddb_is.cc handler/ha_soliddb.cc)

# Legacy
IF(WITH_SOLIDDB)
  SET(WITH_SOLIDDB_STORAGE_ENGINE TRUE)
ENDIF()

# New plugin method
MYSQL_ADD_PLUGIN(solidDB ${SOLIDDB_SOURCES} STORAGE_ENGINE
  MODULE_ONLY
  MODULE_OUTPUT_NAME "ha_soliddb"
  )
