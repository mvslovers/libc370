#import "../bookmaster/bookmaster.typ": *

= Security <mvs-security>

#idx("security")
#idx("RACF", "interface")
#idx("ACEE")
The header #cmd("<mvs/racf.h>") gives a program access to the security
product of the system: it can sign a user on, ask whether that user may use
a resource, and sign the user off again. The functions issue the RACF
macros RACINIT (SVC 131) and RACHECK (SVC 130). MVS 3.8j has no security
product of its own\; the answers come from whatever product the
installation has installed behind these SVCs, such as RAKF, and the return
codes in this chapter are the ones such a product returns.

The identity of a signed-on user is an accessor environment element
(#cmd("ACEE"), mapped in #cmd("<ibm/mvs/ihaacee.h>")). An ACEE can be
handed to #cmd("racf_auth()") directly, so a server can check access for
several users, one ACEE each, without changing the identity of its address
space. The address space also has a default ACEE, in the field
#cmd("ASXBSENV") of its address space extension block\; it is used when no
ACEE is given, and #cmd("racf_get_acee()") and #cmd("racf_set_acee()") read
and replace it.

#idx("authorization", "security functions")
#tab(caption: [Authorization needed by the security functions])[
  #table(columns: (1.6in, 1fr),
    [Function], [Authorization],
    [#cmd("racf_auth()")], [None. An authorized caller is switched to
      supervisor state for the RACHECK\; any other caller issues it from
      problem state, and the answer is the same.],
    [#cmd("racf_login()"), #cmd("racf_logout()"), #cmd("racf_set_acee()")],
      [APF authorization, or supervisor state. They switch to supervisor
      state with MODESET, and an unauthorized caller ends with abend
      S047.],
    [#cmd("racf_get_acee()")], [None.],
  )
] <mvs-security-auth-tab>

Each function has two names, such as #cmd("racf_auth()") and
#cmd("racauth()"). Both are declared with the same external name and call
the same code\; this chapter uses the long names.

== racf\_login, raclogin <mvs-security-login>
#idx("racf_login")
#idx("raclogin")
#idx("RACINIT", "ENVIR=CREATE")
#idx("sign-on")

=== Format
```
#include <mvs/racf.h>

ACEE *racf_login(const char *user, const char *pass,
                 const char *group, int *racf_rc);
ACEE *raclogin(const char *user, const char *pass,
               const char *group, int *racf_rc);
```

=== Description
#cmd("racf_login()") signs the user #var("user") on and creates an ACEE
for the user, with RACINIT #cmd("ENVIR=CREATE"). The user ID, the password
#var("pass") and the group #var("group") are cut to eight characters and
folded to uppercase.

- When #var("pass") is #cmd("NULL"), the password is not checked
  (#cmd("PASSCHK=NO")): the ACEE is created for the user on the word of the
  program.
- When #var("group") is #cmd("NULL") or empty, the user's default group is
  used.
- When #var("user") is #cmd("NULL"), the user ID #cmd("*") is passed.

The new ACEE is not made the default of the address space. Pass it to
#cmd("racf_auth()"), or install it with #cmd("racf_set_acee()").

=== Returns
The address of the new ACEE, or #cmd("NULL") when RACINIT failed. When
#var("racf_rc") is not #cmd("NULL"), the return code of RACINIT is stored
there, as @mvs-security-racinit-tab lists.

#tab(caption: [RACINIT return codes stored by racf\_login()])[
  #table(columns: (0.5in, 0.5in, 1fr),
    [Hex], [Dec], [Meaning],
    [00], [0], [The user was signed on.],
    [04], [4], [The user is not defined.],
    [08], [8], [The password is not valid.],
    [0C], [12], [The password has expired.],
    [10], [16], [The new password is not valid.],
    [14], [20], [The user is not defined to the group.],
    [18], [24], [The installation exit routine failed the request.],
    [1C], [28], [The user's access has been revoked.],
    [20], [32], [The security product is not active.],
    [24], [36], [The user's access to the group has been revoked.],
    [28], [40], [An OIDCARD is required but was not supplied.],
    [2C], [44], [The OIDCARD is not valid for the user.],
    [30], [48], [The user is not authorized to use the terminal.],
    [34], [52], [The user is not authorized to use the application.],
  )
] <mvs-security-racinit-tab>

=== Notes
- The caller must be APF-authorized or in supervisor state (see
  @mvs-security-auth-tab).
- The password is folded to uppercase before it is checked.
- Delete every ACEE that #cmd("racf_login()") creates with
  #cmd("racf_logout()") when it is no longer needed.

=== Example
See @mvs-security-check-ex.

=== Related
@mvs-security-logout, @mvs-security-auth, @mvs-security-setacee

== racf\_logout, raclgout <mvs-security-logout>
#idx("racf_logout")
#idx("raclgout")
#idx("RACINIT", "ENVIR=DELETE")
#idx("sign-off")

=== Format
```
#include <mvs/racf.h>

int racf_logout(ACEE **acee);
int raclgout(ACEE **acee);
```

=== Description
#cmd("racf_logout()") deletes the ACEE that #var("acee") points to, with
RACINIT #cmd("ENVIR=DELETE"), and sets #cmd("*")#var("acee") to
#cmd("NULL").

If the ACEE was the default ACEE of the address space, the default is
cleared, so that no later check follows a pointer to the freed ACEE. The
field is changed with an atomic compare-and-swap, only when it still points
to the deleted ACEE, so a default set meanwhile by another task is left
alone.

=== Returns
The return code of RACINIT, 0 when the ACEE was deleted.

=== Notes
- The caller must be APF-authorized or in supervisor state (see
  @mvs-security-auth-tab).
- Neither #var("acee") nor #cmd("*")#var("acee") may be #cmd("NULL"). With
  #cmd("*")#var("acee") zero, RACINIT falls back to the default ACEE of the
  address space and deletes that one, and the default is then not cleared.

=== Related
@mvs-security-login

== racf\_auth, racauth <mvs-security-auth>
#idx("racf_auth")
#idx("racauth")
#idx("RACHECK")
#idx("resource", "checking access to")

=== Format
```
#include <mvs/racf.h>

int racf_auth(ACEE *acee, const char *classname,
              const char *resource, int attr);
int racauth(ACEE *acee, const char *classname,
            const char *resource, int attr);
```

=== Description
#cmd("racf_auth()") asks the security product, with RACHECK, whether the
user of the ACEE #var("acee") may use the resource #var("resource") of the
resource class #var("classname") (for example #cmd("FACILITY") or
#cmd("DATASET")) with the access #var("attr"):

#deflist(width: 1.6in,
  [#cmd("RACF_ATTR_READ")], [read access (X'02'). An #var("attr") of 0
    also means read access.],
  [#cmd("RACF_ATTR_UPDATE")], [update access (X'04').],
  [#cmd("RACF_ATTR_CONTROL")], [control access (X'08').],
  [#cmd("RACF_ATTR_ALTER")], [alter access (X'80'). Any value of
    #var("attr") that is not one of these is taken as alter access, the
    highest.],
)

When #var("acee") is #cmd("NULL"), the default ACEE of the address space is
used.

The class name is cut to eight characters and the resource name to 80\;
neither is folded to uppercase, so pass them in uppercase. The check is
made with #cmd("LOG=NONE"): the security product writes no audit record for
it.

The ACEE is passed in the RACHECK parameter list. The default ACEE of the
address space is not changed, even for the duration of the call, so other
tasks in the address space are not affected.

=== Returns
The return code of RACHECK, unchanged, as @mvs-security-racheck-tab lists.

#tab(caption: [RACHECK return codes returned by racf\_auth()])[
  #table(columns: (0.5in, 0.5in, 1fr),
    [Hex], [Dec], [Meaning],
    [00], [0], [Access is permitted.],
    [04], [4], [The resource is not protected: no profile covers it.],
    [08], [8], [Access is not permitted.],
    [0C], [12], [The old volume given is not part of the data set or
      volume set.],
    [10], [16], [A RACINIT issued for the check failed.],
    [64], [100], [The list and execute forms of the macro do not match.],
  )
] <mvs-security-racheck-tab>

=== Notes
#idx("RACHECK", "unprotected resource")
*Test for a return code of 4 or less, not for 0.* A resource that no
profile covers answers 4, "not protected", and to a security product that
is an answer that allows the access, not one that refuses it. Because the
check is made with #cmd("LOG=NONE"), an unprotected resource answers 4
where a check with logging would answer 0\; a program that tests for 0
alone refuses every resource that has no profile.

A user who is not permitted is refused with 8 whatever the logging option,
and #var("attr") still decides the answer: a user permitted to read a
resource is refused update access to it.

=== Example
#fig(caption: [Signing a user on and checking access])[
  #code(read("../ex/mvs-security/check.c"), numbers: true)
] <mvs-security-check-ex>

=== Related
@mvs-security-login, @mvs-security-getacee

== racf\_get\_acee, racgacee <mvs-security-getacee>
#idx("racf_get_acee")
#idx("racgacee")
#idx("ASXBSENV")

=== Format
```
#include <mvs/racf.h>

ACEE *racf_get_acee(void);
ACEE *racgacee(void);
```

=== Description
#cmd("racf_get_acee()") returns the default ACEE of the address space, the
value of the field #cmd("ASXBSENV") in its address space extension block.

=== Returns
The address of the default ACEE, or #cmd("NULL") when the address space has
none.

=== Notes
The value can change at any time when another task in the address space
calls #cmd("racf_set_acee()") or #cmd("racf_logout()").

=== Related
@mvs-security-setacee

== racf\_set\_acee, racsacee <mvs-security-setacee>
#idx("racf_set_acee")
#idx("racsacee")

=== Format
```
#include <mvs/racf.h>

ACEE *racf_set_acee(ACEE *newacee);
ACEE *racsacee(ACEE *newacee);
```

=== Description
#cmd("racf_set_acee()") makes #var("newacee") the default ACEE of the
address space: it stores it in the field #cmd("ASXBSENV"), in key 0. A
#var("newacee") of #cmd("NULL") clears the default.

=== Returns
The previous default ACEE, or #cmd("NULL") when there was none.

=== Notes
- The caller must be APF-authorized or in supervisor state (see
  @mvs-security-auth-tab).
- The default belongs to the whole address space, not to the calling
  task: every task in it, and every check made without an explicit ACEE,
  sees the new value at once. A program that serves several users at the
  same time should pass each user's ACEE to #cmd("racf_auth()") instead.
- #cmd("racf_set_acee()") does not delete the previous ACEE.

=== Related
@mvs-security-getacee, @mvs-security-login, @mvs-security-auth
