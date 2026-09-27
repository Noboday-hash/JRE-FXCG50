# Tests

CTest runs the M0 smoke check and Python-generated ZIP, manifest, and class fixtures through the host executable. Archive fixtures cover stored and raw DEFLATE extraction, multi-chunk input, data descriptors, CRC mismatch, malformed streams, truncated files, ZIP64, encryption, duplicate names, invalid offsets/local headers, and manifest continuation/errors. Class fixtures cover class format 45.3, old-version policy, descriptor and modified UTF-8 failures, truncated Code, exception ranges, unsupported tags, and two-slot long constants. Semantic, scheduler, GC, and rendering tests begin with the related implementation milestones.

The class fixtures also cover SourceFile and line metadata, opcode counting, switches, and malformed instruction lengths. Tests use independently constructed ZIP and class bytes rather than a game JAR. Hardware tests and an independent Java ME emulator run remain pending.

The M2 integer VM fixtures construct class files independently of the interpreter and check arithmetic edge cases, branches, locals, stack faults, unsupported operations, and the step limit through `cgjre-host --eval-class`. A separate locally downloaded ECJ 3.26.0 class was compiled and evaluated as an additional manual check; the default CTest suite does not download that compiler.

The integer fixtures also cover 46.0 class parsing, same-class static calls, recursion, missing and external members, callee faults, and the frame cap. The two user-supplied games are static compatibility probes only; the test suite does not execute or package them.

`test_vm_jar.py` builds external stored and compressed two-class JARs and checks cross-class calls, cache reuse, missing classes/members, class-name mismatch, platform-package protection, the class-size limit, and unsupported initialization. A separate ECJ 3.26.0 two-class JAR was compiled and evaluated manually; it is not required by CTest.

`test_vm_arrays.py` checks primitive arrays, reference locals and branches, zero initialization, sign extension, null/bounds/negative-size failures, and allocation limits. It also executes an array method from an external JAR. An ECJ 3.26.0 class with a real `int[]` store/load returned 41 for input 41; that compiler is still optional for the default test suite.

`test_vm_objects.py` uses independently assembled two-class JARs to check constructor ordering, default and assigned fields, null/uninitialized references, final-field rules, exact field descriptors, and reference-field assignability. A separate ECJ 3.26.0 `Box` program returned 41 after constructor assignment and `getfield`; it is an optional manual check.

`test_vm_hierarchy.py` assembles three-class JARs for superclass constructors, hidden fields, override dispatch, reference arrays, failed array stores, null virtual calls, class cycles, `instanceof`, and casts. An ECJ 3.26.0 `Base`/`Child`/`Base[]` JAR returned 42 for input 41 through those shared paths; the compiler is not required by CTest.
