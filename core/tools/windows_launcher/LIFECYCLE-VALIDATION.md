# Lifecycle implementation evidence — 2026-09-12

This is an implementation candidate, **not an adoption-qualified release**.
Source baseline: `0185690e0f69d889fd45e010b8a9d2d9c305938b`, plus the local
implementation changes. No resolving commit, upstream PR, or release has been
published. The separate documentation checkout has related guide edits on
`2ec208a542f1756db8a0b2863e1b8ba50d8e97a7`.

## Artifact identity

Built using `windows-cross-mingw-x64`, Release, GCC 10-posix 20220113. The public
artifact is `build/windows-cross-mingw-x64/Release/Waterwall.exe`.

| Component | Bytes | SHA-256 |
| --- | ---: | --- |
| Public packed EXE | 3,743,744 | `c571a5a83229cdfe19523542111888ac23f2c922d191ae26967f4e62cb76dd08` |
| Exact embedded, finalized runtime PE | 11,938,816 | `418b87e3384e77d5a9db08bbb4105938540949e246e5d79a2b83859123b59a86` |
| Unstripped comparison runtime EXE | 11,942,400 | `65fc49c2f1ccf837c7ee70f32820b4e2d652d34da355bb3c160448413b154e0e` |
| Embedded Wintun AMD64 DLL | 427,552 | `e5da8447dc2c320edc0fc52fa01885c103de8c118481f683643cacc3220dafce` |
| Embedded WinDivert x64 DLL | 47,616 | `c1e060ee19444a259b2162f8af0f3fe8c4428a1c6f694dce20de194ac8d7d9a2` |
| Embedded WinDivert x64 SYS | 94,144 | `8da085332782708d8767bcace5327a6ec7283c17cfb85e40b03cd2323a90ddc2` |

These identities describe the build after correcting recovery certification.
The packed EXE after removing the unused loop guard was
`67976ada06bc54031250dcaaf50f300d9ca43ed5011cbef00994f0d96c6a4194`.
The earlier reviewed packed EXE was
`097f4e9e7aadeeb034a0f22f2ec72eef5517828d2471f79a0bf73a0ac7956024`;
the initial implementation's packed EXE was
`00df3b3ed02e02b55dc268653cd992aaa96a7bd411a8f136b8ec00b3b0fbd6b6`.
Their historical checks below are retained separately. Driver byte identities
and the exact embedded runtime are unchanged by the recovery correction.

The embedded runtime hash was calculated from decompressed payload bytes, with
XZ integrity checking, rather than assuming that the comparison EXE is byte
identical. Driver hashes identify the byte arrays selected by the build. The
Wintun PE fixed-file-version field reads `0.14.0.0`; the WinDivert SYS field reads
`2.0.0.0`. Those fields alone do not establish package-release provenance or OS
compatibility. A byte comparison also confirmed that the vendored AMD64 DLL is
identical to the AMD64 DLL from the
[official Wintun 0.14.1 archive](https://www.wintun.net/builds/wintun-0.14.1.zip).
Driver signatures, trust chains, and native crash behavior were not qualified here.

The launcher directly imports IPHLPAPI, KERNEL32 and MSVCRT. Its existing lazy
Windows API resolution also uses system libraries, and capability checking
resolves `ntdll!NtQueryObject`. The runtime imports ADVAPI32, CRYPT32, IPHLPAPI,
KERNEL32, MSVCRT, NTDLL, USER32 and WS2_32. Compiler libraries are linked statically.
Both manifests retain `requireAdministrator`; the runtime remains fixed-base
while the launcher retains ASLR/DEP. The launcher's PE subsystem version is 5.2;
that field is not a claim that its APIs run on that OS. API targeting remains
`_WIN32_WINNT=0x0600`.

This preset compiles runtime code for Haswell/AVX2. It is not an old-CPU artifact.
Minimum Windows update sets, actual signing acceptance, restricted-token access,
and desktop rights remain native qualification items. In particular, current
[WinDivert documentation](https://reqrypt.org/windivert-doc.html) lists Windows
10/11/Server; it does not establish support of these embedded bytes on the legacy
rows. The [Wintun distribution](https://www.wintun.net/) likewise does not replace
qualification of the specific embedded DLL and each OS/update baseline.

## Recovery certification correction

Recovery now separates independently verified process/interface settlement from
the manner of termination and file cleanup. An absent orderly runtime receipt
or retained driver files no longer veto settlement after the record, containment,
adapter identities, and adapter absence have been verified. Abrupt runtime exit
reports possible file residue even when the launcher finalized its own receipt.
The existing `cleanup`, `termination`, and `file_residue` fields express these
independent facts; no additional client control protocol was introduced.

The existing abrupt-runtime case (11) failed with the new settlement expectation
before the production fix and passed afterward. Three cases were added to the
same fixture for missing coverage: unresolved adapter intent after abrupt exit
(13), retained driver-file residue after orderly exit (14), and forced launcher
death while recovery holds the Job (15). Case 15 uses an explicit stop-observed
event to place the kill after recovery has opened containment. It verifies
settlement without a final launcher receipt while keeping runtime status unknown
and file residue possible. Cases 6 and 12 still reject a present interface, and
case 1 still rejects a killed launcher whose process evidence cannot be recovered.

Validation logs are outside source history under `/tmp/ww-recovery-*`.

| Recovery correction check | Result |
| --- | --- |
| Windows packed production Release build | Passed |
| Windows launcher fixture CTest under Wine, Debug / Release | Passed: 4 / 4 tests |
| Public lifecycle cases under Wine, Debug / Release | Passed: 15 / 15 selected cases (0–7 and 9–15) |
| Production packed public lifecycle client under Wine | Passed: graceful stop, controller loss, already-dead controller |
| Linux production build and support / functional / external / speed / privileged lanes | Passed: 13 / 154 / 1 / 16 / 6 tests |
| Complete Linux Debug / Release units | Passed: 215 / 217 tests |
| Changed C formatting and diff whitespace | Passed |

The source correction retains the explicit adapter-absence requirement and
does not change Wintun or other driver bytes. These tests validate the recovery
protocol and existing runtime behavior under Wine, not native crash/removal
behavior. Native OS qualification, the absent-stdio console case, and the earlier
application/TLS comparison limitations below remain open.

## Loop-prevention cleanup

The unused WinDivert flow observer, its host-route installation/deletion code,
header, platform stubs, and CMake entries were removed. No built-in caller started
that observer. Its dedicated recovery counter/helpers, JSON field, cleanup reason,
and test assertions were also removed. The effect-inventory identifier describes
the retained adapter/driver layout, and all readers and writers use that layout.
The active egress-pin implementation, its socket callers, and the WinDivert
manager/drivers used by raw and capture devices remain intact.

The recovery contract now separates completed process termination, Wintun's
driver-managed packet-session teardown, asynchronous adapter removal, and
persistent device/file/driver residue. Earlier descriptions of active physical
host-route residue were not supported by the call graph and have been corrected.
At that point the conservative completion rule still required an orderly receipt
once the runtime ran; the recovery correction above removes that veto.
`unverified` does not establish surviving traffic or DNS damage.

Validation for this cleanup uses the existing fixtures and suites; no additional
test framework or new test cases were introduced. Logs are outside source history
under `/tmp/ww-loopguard-*`. Native Windows crash qualification remains pending.

| Cleanup check | Result |
| --- | --- |
| Linux production build and support / functional / external / speed / privileged lanes | Passed: 13 / 154 / 1 / 16 / 6 tests |
| Complete Linux Debug / Release units | Passed: 215 / 217 tests |
| Windows packed production Release build | Passed |
| Windows launcher fixture CTest under Wine, Debug / Release | Passed: 4 / 4 tests |
| Public lifecycle cases under Wine, Debug / Release | Passed: 12 / 12 selected cases (0–7 and 9–12) |
| Production packed public lifecycle client under Wine | Passed: graceful stop, controller loss, already-dead controller |
| Changed C/header formatting, diff whitespace, and retired symbol/source references | Passed |

The native-only qualification of the absent-stdio console case and the earlier
application/TLS comparison limitations below remain unchanged; this cleanup
does not turn those missing results into passes.

## Earlier review corrections and checks

The review retained these corrections:

- **P1 — coherent recovery observations:** recovery now keeps a read-only file
  mapping through settlement and refreshes that view. The former `ReadFile`
  snapshots had no Windows coherence guarantee with the runtime's mapped intent
  and effect writes, particularly after abrupt exit without flushing. Reading the
  local view through `ReadProcessMemory` also makes failed page reads return an
  unverified result. See [Microsoft's mapping contract](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile).
- **P2 — failed creation accounting:** a failed `CreateProcessW`, or a confirmed
  terminated child that was never resumed, no longer permanently marks runtime
  cleanup unknown. Admission stays published until that fact is established;
  any failure after resume retains runtime cleanup obligations. Launcher errors
  no longer masquerade as an observed runtime status. Full DWORD runtime results
  remain intact.
- **P2 — visible console startup:** standard streams are reopened before and
  after console reassignment, avoiding invalid `_dup2` destinations and reuse of
  stale console handles. MSVC documents that absent stdout/stderr can have
  descriptor `-2`; see [the CRT contract](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/fileno).
- **P2 — record publication:** the initial file image comes from an immutable
  snapshot made before the observer starts. Recovery validates record flags and
  the final session identity; an invalid final snapshot cannot authenticate
  runtime status or cleanup. The result identifies the first unmet condition
  with `cleanup_detail`.
- **P2 — example client error handling:** the attached example observes early
  process exit and performs public recovery after errors/interruption. Independent
  launch defaults to a usable visible console; hidden independent launch requires
  a journal for subsequent public stop/recovery.

That review used Linux, MinGW, and Wine 6.0.3; it was not native Windows
qualification. Logs are outside source history under `/tmp/ww-review-*`.

| Review check | Result |
| --- | --- |
| Linux production build and support / functional / external / speed / privileged lanes | Passed: 13 / 154 / 1 / 16 / 6 tests |
| Complete Linux Debug / Release units | Passed: 215 / 217 tests |
| Windows packed production Release build | Passed |
| Windows launcher fixture CTest under Wine, Debug / Release | Passed: 4 / 4 tests |
| Public lifecycle cases under Wine, Debug / Release | Passed: 12 / 12 selected cases; case 8 remains inconclusive as detailed below |
| Real runtime lifecycle unit under Wine, Debug / Release | Passed |
| Production packed public lifecycle client under Wine | Passed: graceful stop, controller loss, already-dead controller |
| Python example | Syntax checked; native Python execution pending |
| Changed C/header formatting, diff whitespace, and absence of the fixture hook from the production EXE | Passed |

The existing public client fixture was extended only for missing lifecycle gaps:
failed creation, actual abrupt runtime exit with full DWORD status, visible
stdio/config transfer, authoritative recovery fields, and recovery while a live
runtime still owns mapped effect state. Its interface observations do not claim
real adapter creation/removal or forced OS-effect reconciliation.

Visible case 8, which launches with absent stdout/stderr, fails on this Wine 6
host before console allocation: its CRT refuses `freopen` on a negative stream
descriptor. This limitation is explicit in [Wine 6's `_wfreopen` implementation](https://github.com/wine-mirror/wine/blob/wine-6.0/dlls/msvcrt/file.c#L4154).
The full client run is therefore **inconclusive on this host**, and the case
remains enabled for native Windows. Targeted `--case N EXE` runs exercise the
other cases; visible case 9 supplies redirected output and configuration stdin.
Cases 0–7 and 9–12 passed in both Debug and Release after the final mapping fix.
Case 12 starts recovery while the runtime is alive, then verifies process
settlement and the remaining interface's explicit `adapter_present` result.
No production workaround for Wine or relaxation of the native case was added.

The Windows application comparison was rerun and still fails on the ordinary
EXE's extra stderr newline for `--version`. Its later application/TLS cases remain
unexecuted by that run. The earlier TLS failure below is retained.

**Earlier review disposition: changes required before adoption.** Those source corrections did
not resolve the then-documented forced-exit settlement limitation or supply
native OS evidence. Acceptance remains finite: establish exact-session process
and owned-effect settlement across the required crash scenarios (or obtain an
explicit agreement on the stated incomplete outcome), qualify the console case
and real TUN/DNS/route behavior on Windows 7/10/11, and resolve the full application
comparison. Windows 8/8.1 evidence is additionally required for the full range.

## Previous implementation checks

Host: Linux with Wine 6.0.3 (Ubuntu package). Wine results are diagnostics, **not
native Windows results**, including when Wine reports a Windows version.

| Check | Result |
| --- | --- |
| `cmake --preset linux`; `cmake --build --preset linux -j8` | Passed |
| Production support lane | 13/13 passed |
| Production functional lane | 154/154 passed |
| Production external lane | 1/1 passed |
| Production speed lane | 16/16 passed |
| Production privileged lane | 6/6 passed |
| Complete `linux-unit-debug` | 215/215 passed |
| Complete `linux-unit-release` | 217/217 passed |
| Focused startup/input checks after final parser edits | 2/2 passed |
| Full Windows packed Release build | Passed |
| Windows launcher fixture CTest, Debug and Release under Wine | 4/4 passed in each configuration |
| Public lifecycle client against the small PE fixture, Debug and Release | Passed; cases described below |
| Real runtime `windows_lifecycle_test`, Debug and Release, isolated no-LTO cross build | Passed under Wine |
| Public lifecycle client against the identified production packed EXE | Passed under Wine with a one-worker TesterClient/BlackHole configuration |
| Modified C/header formatting and diff whitespace | Passed |
| Python client example | Syntax checked; native Python execution pending |

The small PE client exercises hidden graceful stop, forced public-process exit,
controller death after readiness, an already-dead controller, cancellation
before/during an unfinished config transfer, absent/present interface records,
and full DWORD exit status. The still-present interface case must remain
unverified and must not remove that interface. These are protocol/reader tests,
not real Wintun creation/removal tests. The config-transfer case does not claim
an exact instruction-level injection point.

The real runtime unit exercises parser rejection, pre-signaled stop and controller
loss, optional readiness, waiter join, setup failures, wrong object types, least
access, readiness publication, and the production shutdown coordinator.
The production packed client exercises real readiness, graceful stop, controller
loss and already-dead-controller rejection without a client-owned Job. It has no
TUN, listener, external endpoint or WireShock dependency.

## Failed checks retained

`tests/windows_packed_application_test.py` failed its initial `--version` output
comparison: the ordinary executable writes an additional `b'\n'` on stderr while
the launcher writes none. Both return 0 and print the version on stdout. A direct
repeat reproduced that difference. The script's remaining cases therefore did
not execute; the comparison is **not passed**.

A separate ordinary-runtime probe using the existing four-worker `tls_roundtrip`
case returned status **3**, before the worker-success log marker. This failure was
not converted into a pass or hidden by changing the comparison's expectations.
The packed TLS comparison remains unverified. The full application lane remains unverified even though the narrower production
lifecycle client passes. These Wine failures have not been established as native
Windows defects; native qualification is still required.

Intermediate compile failures (Windows SDK include ordering, fixture const
qualification, and missing compiler runtime DLLs for the cross-built unit) were
corrected and the affected builds/checks passed afterward. A fixture expectation
for the newly added full-DWORD case was also corrected to match its injected
`0xc0000005` status before the final successful run. The focused MinGW
runtime unit now follows the executable's static compiler-library policy.

## Native qualification still required

| OS row | Lifecycle / console / driver / crash qualification |
| --- | --- |
| Updated Windows 7 SP1 x64 | Pending |
| Windows 8 x64 | Pending |
| Windows 8.1 x64 | Pending |
| Windows 10 x64 | Pending |
| Windows 11 x64 | Pending |

For each row retain the exact artifact hash, update baseline, token permissions,
desktop/console mode, injected stage, public result and observed residue. Required
remaining coverage includes real TUN/DNS/adapter-route cleanup, delayed/failed helpers,
creation/extraction gaps, hung operations, simultaneous public/controller death,
visible-console close/Ctrl+C, restricted-token denial, independent-instance
coexistence, and unrelated adapter/route/firewall sentinels. Initial integration
needs Windows 7/10/11 evidence; claiming the full range additionally needs 8/8.1.
The small fixture and Wine do not discharge those requirements.

## Adoption boundary

A missing Job name is deliberately not a completion receipt. Recovery requires
live inactive accounting or a final one-member barrier and verified public-process
death. Abrupt runtime death no longer prevents settlement when those process
observations and the owned-adapter absence checks succeed. Retained driver files
remain separate residue. Missing process evidence, unresolved adapter identity,
failed queries, and a still-present adapter remain `unverified`; recovery does not
invent successful cleanup or delete uncertain external state. An unverified
result is not evidence that Wintun's packet session survived or physical DNS was
changed. Wintun supplies driver-managed session teardown; native evidence must
still qualify the effective routing/DNS and adapter boundaries on each claimed
OS. No client watchdog, service, or descendant inventory is assumed to fill the
remaining process-evidence gap.

See [the interface contract](LIFECYCLE.md) for exact arguments, deadlines, result
fields, permissions and examples. Unverified results do not authorize resource
retirement, conflicting replacement, or removal of client firewall policy.
