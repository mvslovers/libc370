#import "../bookmaster/bookmaster.typ": *

= Control Block Mappings <apx-cblocks>

// Narrow cells: ragged right, no hyphenation of the macro names.
#show table: set par(justify: false)
#show table: set text(hyphenate: false)

#idx("control block mappings")
The headers under #cmd("include/ibm/") describe MVS and JES2 control blocks
as C structures. Each one corresponds to an IBM mapping macro (a DSECT) or to
the parameter list of an IBM macro instruction, and gives a C program the
view of the storage that the macro gives an assembler program. The library's
own MVS functions are built on them, and they are installed with the other
headers, so that a program can also read a control block that the library
does not wrap.

The headers are included by their directory:

```
#include <ibm/mvs/cvt.h>     /* MVS control blocks  */
#include <ibm/jes2/jqe.h>    /* JES2 control blocks */
```

#note[A mapping header declares types and constants only. Including it
allocates no storage and generates no code\; the program must find the
control block itself, usually by following pointers from the CVT or the
current TCB, as the IBM manuals describe.]

The layouts are those of MVS 3.8j (OS/VS2 Release 3.8). Several headers
repeat, in their opening comment, the level of the IBM macro they were taken
from, for example #cmd("OS/VS2 SU64") for the TCB and JSCB. The JES2 headers
record no level\; they describe the JES2 that runs on MVS 3.8j.

Field names follow the IBM macro: #cmd("cvtjesct") in #cmd("struct cvt") is
the field CVTJESCT of the CVT macro. Most MVS headers spell the fields in
lowercase\; the JES2 headers and a few others, such as the SDWA, keep the
uppercase of the macro. A bit or value defined by the macro is a
#cmd("#define") of the same name, placed after its field. Some headers map
only the part of a block that the library uses and fill the rest with
reserved fields, so that the fields present keep their offsets.

#idx("control block mappings", "compiler warnings")
Compiled against the installed library, the mapping headers draw no
warnings: cc370 treats the headers of its sysroot as system headers. A
program compiled against a libc370 source tree named with #cmd("-I") sees
them as ordinary headers, and then thirteen of them stop a build with
#cmd("-Wall -Werror"), with #cmd("\"/*\" within comment"): #cmd("cvt.h"),
#cmd("ieecucm.h"), #cmd("ieezb806.h"), #cmd("iefsscs.h"),
#cmd("iefssobh.h"), #cmd("iefssso.h"), #cmd("ieftiot1.h"),
#cmd("iefzb4d2.h"), #cmd("ihacde.h"), #cmd("ihalpde.h"),
#cmd("ihaxtlst.h"), and in #cmd("ibm/jes2/") #cmd("jct.h") and
#cmd("pso.h"). Their comments, or those of the mappings they include,
contain #cmd("/*"). A program that includes one of them, directly or
through a library header, needs #cmd("-Wno-comment") as well.

#cmd("ibm/jes2/sjb.h") does not compile at all, installed or not, with any
options: it declares a member #cmd("SJBCSCB") of type #cmd("void"). No
other header includes it. In all, 24 of the 130 headers of libc370 fail
under #cmd("-Wall -Werror") when they are reached through #cmd("-I"): 22
on nested comments, #cmd("sjb.h"), and #cmd("<mvs/rfile.h>"), which needs
#cmd("<stddef.h>") before it. The chapters that describe the library
headers say which of their headers need #cmd("-Wno-comment").

The tables list the headers by subject. The second column names the control
block and, in parentheses, the IBM macro that maps it\; the third the C type
names the header defines.

== MVS Headers <apx-cblocks-mvs-sec>

#tab(caption: [System control blocks in #cmd("include/ibm/mvs/")])[
  #table(columns: (0.85in, 1.4in, 1.3in, 1fr),
    [Header], [Block (macro)], [C types], [Purpose],
    [#cmd("cvt.h")], [CVT (CVT)], [#cmd("CVT"), #cmd("CVTFIX"),
      #cmd("CVTXTNT1"), #cmd("CVTXTNT2")], [The communication vector table,
      the root of most chains. #cmd("CVTPTR") gives its address.],
    [#cmd("ihascvt.h")], [SCVT (IHASCVT)], [#cmd("SCVT"), #cmd("SCVTSECT"),
      #cmd("SVCTABLE"), #cmd("SVCENTRY")], [The secondary CVT and the SVC
      table.],
    [#cmd("ihaasvt.h")], [ASVT (IHAASVT)], [#cmd("ASVT")], [The address space
      vector table.],
    [#cmd("ikjtcb.h")], [TCB (IKJTCB)], [#cmd("TCB"), #cmd("TCBFIX"),
      #cmd("TCBXTNT2")], [The task control block.],
    [#cmd("iharb.h")], [RB (IHARB)], [#cmd("RBPRFX"), #cmd("RBBASIC")], [The
      request blocks of a task.],
    [#cmd("ihasrb.h")], [SRB (IHASRB)], [#cmd("SRB")], [The service request
      block.],
    [#cmd("iezjscb.h")], [JSCB (IEZJSCB)], [#cmd("JSCB"), #cmd("IEZJSCB")],
      [The job/step control block. It leads to the SSIB of the job.],
    [#cmd("ihacde.h")], [CDE (IHACDE)], [#cmd("CDE")], [The contents directory
      entry of a loaded module.],
    [#cmd("ihalpde.h")], [LPDE (IHALPDE)], [#cmd("LPDE")], [The link pack
      directory entry.],
    [#cmd("ihaxtlst.h")], [XTLST (IHAXTLST)], [#cmd("XTLST")], [The extent
      list of a loaded module.],
    [#cmd("ihasdwa.h")], [SDWA (IHASDWA)], [#cmd("SDWA")], [The system
      diagnostic work area passed to a recovery exit.],
    [#cmd("ieebasea.h")], [BASEA (IEEBASEA)], [#cmd("IEEBASEA"),
      #cmd("BASE")], [The master scheduler resident data area.],
    [#cmd("iezbits.h")], [none (IEZBITS)], [none], [The bit constants
      #cmd("BIT0") to #cmd("BIT7").],
  )
] <apx-cblocks-mvs>

#idx("CVT", "mapping")
#idx("SCVT", "mapping")
#idx("ASVT", "mapping")
#idx("TCB", "mapping")
#idx("RB", "mapping")
#idx("SRB", "mapping")
#idx("JSCB", "mapping")
#idx("CDE", "mapping")
#idx("LPDE", "mapping")
#idx("XTLST", "mapping")
#idx("SDWA", "mapping")
#idx("BASEA", "mapping")

#tab(caption: [Console control blocks in #cmd("include/ibm/mvs/")])[
  #table(columns: (0.85in, 1.4in, 1.3in, 1fr),
    [Header], [Block (macro)], [C types], [Purpose],
    [#cmd("ieecucm.h")], [UCM (IEECUCM)], [#cmd("UCM"), #cmd("UCMPRFX"),
      #cmd("UCMLIST"), #cmd("UCMEIL") and others], [The unit control module:
      the consoles and their queues.],
    [#cmd("ieecdcm.h")], [DCM (IEECDCM)], [#cmd("DCM"), #cmd("IEECDCM")],
      [The display control module of a display console.],
    [#cmd("ieezb806.h")], [Master trace table (IEEZB806)],
      [#cmd("MTTABLE"), #cmd("MTENTRY")], [The master trace table and its
      entries. See @mvs-console.],
  )
] <apx-cblocks-mvs-console>

#idx("UCM", "mapping")
#idx("DCM", "mapping")
#idx("master trace table", "mapping")

#tab(caption: [Data management control blocks in #cmd("include/ibm/mvs/")])[
  #table(columns: (0.85in, 1.4in, 1.3in, 1fr),
    [Header], [Block (macro)], [C types], [Purpose],
    [#cmd("dcbd.h")], [DCB (DCBD)], [#cmd("DCB"), #cmd("EXITLIST")], [The
      data control block and its exit list.],
    [#cmd("ihadecb.h")], [DECB (IHADECB)], [#cmd("DECB")], [The data event
      control block of READ, WRITE and CHECK.],
    [#cmd("iezdeb.h")], [DEB (IEZDEB)], [#cmd("DEB"), #cmd("DEBBASIC"),
      #cmd("DEBDASD") and others], [The data extent block of an open data
      set.],
    [#cmd("ieziob.h")], [IOB (IEZIOB)], [#cmd("IOB"), #cmd("IOBPRFX")],
      [The input/output block.],
    [#cmd("iefjfcbn.h")], [JFCB (IEFJFCBN)], [#cmd("JFCB")], [The job file
      control block.],
    [#cmd("ieftiot1.h")], [TIOT (IEFTIOT1)], [#cmd("TIOT"), #cmd("TIOTDD")],
      [The task I/O table, one entry per DD statement.
      #cmd("get_tiot()") returns the current one.],
    [#cmd("ihadsab.h")], [DSAB (IHADSAB)], [#cmd("DSAB"), #cmd("DSABANMI"),
      #cmd("DSABGIDL")], [The data set association block.],
    [#cmd("iefucbob.h")], [UCB (IEFUCBOB)], [#cmd("UCBLIST"),
      #cmd("UCBDASD")], [The list of UCB addresses, and the unit control
      block of a direct access device.],
    [#cmd("ihadva.h")], [DEVTYPE area (IHADVA)], [#cmd("DVAREA"),
      #cmd("DVARPS"), #cmd("DVATAB")], [The area that the DEVTYPE macro
      returns.],
  )
] <apx-cblocks-mvs-dm>

#idx("DCB", "mapping")
#idx("DECB", "mapping")
#idx("DEB", "mapping")
#idx("IOB", "mapping")
#idx("JFCB", "mapping")
#idx("TIOT", "mapping")
#idx("DSAB", "mapping")
#idx("UCB", "mapping")
#idx("DEVTYPE", "parameter area")

#tab(caption: [Dynamic allocation and internal text in #cmd("include/ibm/mvs/")])[
  #table(columns: (0.85in, 1.4in, 1.3in, 1fr),
    [Header], [Block (macro)], [C types], [Purpose],
    [#cmd("iefzb4d0.h")], [SVC 99 request block (IEFZB4D0)], [#cmd("RB99"),
      #cmd("RBX99")], [The request block of dynamic allocation and its
      extension. See @mvs-dynalloc.],
    [#cmd("iefzb4d2.h")], [SVC 99 text unit (IEFZB4D2)], [#cmd("TXT99")],
      [The text unit and the text unit keys.],
    [#cmd("ieftxtft.h")], [Internal text (IEFTXTFT)], [#cmd("__TXTPRE"),
      #cmd("__JOBSTR"), #cmd("__EXECSTR"), #cmd("__DDSTR")], [The internal
      text of JOB, EXEC and DD statements.],
    [#cmd("iefvkeys.h")], [none (IEFVKEYS)], [none], [The keys of the
      internal text.],
  )
] <apx-cblocks-mvs-dyn>

#idx("SVC 99", "mapping")
#idx("internal text", "mapping")

#tab(caption: [Subsystem interface in #cmd("include/ibm/mvs/")])[
  #table(columns: (0.85in, 1.4in, 1.3in, 1fr),
    [Header], [Block (macro)], [C types], [Purpose],
    [#cmd("iefjesct.h")], [JESCT (IEFJESCT)], [#cmd("JESCT")], [The JES
      control table. It leads to the first SSCT.],
    [#cmd("iefjssib.h")], [SSIB (IEFJSSIB)], [#cmd("SSIB")], [The subsystem
      identification block.],
    [#cmd("iefssobh.h")], [SSOB (IEFSSOBH)], [#cmd("SSOB")], [The subsystem
      options block header, and the return codes of IEFSSREQ.],
    [#cmd("iefsscs.h")], [SSCS (IEFSSCS)], [#cmd("SSCS"), #cmd("SSCSARAY")],
      [The SSOB extension for the cancel and status functions.],
    [#cmd("iefssso.h")], [SSSO (IEFSSSO)], [#cmd("SSSO"), #cmd("WTRC")],
      [The SSOB extension for the process SYSOUT function.],
  )
] <apx-cblocks-mvs-ssi>

#idx("JESCT", "mapping")
#idx("SSIB", "mapping")
#idx("SSOB", "mapping")

#tab(caption: [TSO control blocks in #cmd("include/ibm/mvs/")])[
  #table(columns: (0.85in, 1.4in, 1.3in, 1fr),
    [Header], [Block (macro)], [C types], [Purpose],
    [#cmd("ikjcppl.h")], [CPPL (IKJCPPL)], [#cmd("CPPL"), #cmd("TSOCBUF")],
      [The command processor parameter list and the command buffer.],
    [#cmd("ikject.h")], [ECT (IKJECT)], [#cmd("ECT")], [The environment
      control table.],
    [#cmd("ikjpscb.h")], [PSCB (IKJPSCB)], [#cmd("PSCB")], [The protected
      step control block, which holds the user's attributes.],
    [#cmd("ikjupt.h")], [UPT (IKJUPT)], [#cmd("UPT")], [The user profile
      table.],
  )
] <apx-cblocks-mvs-tso>

#idx("CPPL", "mapping")
#idx("ECT", "mapping")
#idx("PSCB", "mapping")
#idx("UPT", "mapping")

#tab(caption: [Security control blocks in #cmd("include/ibm/mvs/")])[
  #table(columns: (0.85in, 1.4in, 1.3in, 1fr),
    [Header], [Block (macro)], [C types], [Purpose],
    [#cmd("ihaacee.h")], [ACEE (IHAACEE)], [#cmd("ACEE"), #cmd("CONNGRP")],
      [The accessor environment element of a signed-on user.],
    [#cmd("ichsafp.h")], [SAFP (ICHSAFP)], [#cmd("SAFP")], [The SAF router
      parameter list.],
    [#cmd("ichsafv.h")], [SAFV (ICHSAFV)], [#cmd("SAFV")], [The SAF vector
      table.],
    [#cmd("racinit.h")], [RACINIT parameter list], [#cmd("RACINIT")], [The
      parameter list of the RACINIT macro.],
    [#cmd("racheck.h")], [RACHECK parameter list], [#cmd("RACHECK"),
      #cmd("RACLASS"), #cmd("RAENTITY")], [The parameter list of the RACHECK
      macro.],
  )
] <apx-cblocks-mvs-security>

#idx("ACEE", "mapping")
#idx("SAF", "mapping")
#idx("RACINIT", "parameter list")
#idx("RACHECK", "parameter list")

== JES2 Headers <apx-cblocks-jes2-sec>

The JES2 headers map the records of the JES2 checkpoint and spool data sets,
which the functions in @mvs-subsys read, and the JES2 blocks in storage.
Their type names begin with two underscores, except #cmd("HASPCMB") (also
#cmd("CMB")) and #cmd("HASPSVT").

#tab(caption: [JES2 control blocks in #cmd("include/ibm/jes2/")])[
  #table(columns: (0.85in, 1.4in, 1.3in, 1fr),
    [Header], [Block (macro)], [C types], [Purpose],
    [#cmd("hct.h")], [HCT (\$HCT)], [#cmd("__HCT")], [The checkpoint master
      record.],
    [#cmd("jqe.h")], [JQE (\$JQE)], [#cmd("__JQE")], [The job queue element,
      one per job in the checkpoint.],
    [#cmd("jot.h")], [JOT (\$JOT)], [#cmd("__JOT")], [The job output table.],
    [#cmd("joe.h")], [JOE (\$JOE)], [#cmd("__JOE"), #cmd("__CHARJOE"),
      #cmd("__CKPTJOE")], [The job output elements.],
    [#cmd("jct.h")], [JCT (\$JCT)], [#cmd("__JCT")], [The job control table,
      a spool record.],
    [#cmd("iot.h")], [IOT (\$IOT)], [#cmd("__IOT")], [The input/output table,
      a spool record.],
    [#cmd("pddb.h")], [PDDB (\$PDDB)], [#cmd("__PDDB")], [The peripheral data
      definition block, one per spool data set, held in the IOT.],
    [#cmd("tab.h")], [TAB (\$TAB)], [#cmd("__TAB")], [The track allocation
      block.],
    [#cmd("tgm.h")], [TGM (\$TGM)], [#cmd("__TGM")], [The track group map.],
    [#cmd("sjb.h")], [SJB (\$SJB)], [#cmd("__SJB")], [The subsystem job
      block, reached through the SSIB. The header does not
      compile.],
    [#cmd("pso.h")], [PSO (\$PSO)], [#cmd("__PSO")], [The process SYSOUT work
      area in common storage.],
    [#cmd("svt.h")], [SVT (\$SVT)], [#cmd("HASPSVT")], [The JES2 subsystem
      vector table.],
    [#cmd("cmb.h")], [CMB (\$CMB)], [#cmd("HASPCMB"), #cmd("CMB")], [The
      console message buffer.],
  )
] <apx-cblocks-jes2>

#idx("JES2", "control block mappings")
#idx("HCT", "mapping")
#idx("JQE", "mapping")
#idx("JOT", "mapping")
#idx("JOE", "mapping")
#idx("JCT", "mapping")
#idx("IOT", "mapping")
#idx("PDDB", "mapping")
#idx("TAB", "mapping")
#idx("TGM", "mapping")
#idx("SJB", "mapping")
#idx("PSO", "mapping")
#idx("SVT", "JES2")
#idx("CMB", "mapping")
