/*
    SMCA — installing the HOME menu tile (the forwarder NSP embedded in romfs).

    The tile is a separate installed TITLE, so "install" is not a file copy: the NSP
    goes to ncm, and what the HOME menu then shows comes from the control NCA the
    console ended up with. That is why a stale icon could survive a reinstall, a
    reboot and a manual delete without a single visible error — and why asking
    InstallSD() "did you work?" answers nothing: it reports that the write happened,
    not what was written.

    install() therefore does the whole job and reports what actually landed:

      1. read the icon THIS build carries, out of the running NRO's ASET section —
         the same artwork elf2nro gave hbmenu, so it cannot disagree with what the
         user sees in the homebrew menu;
      2. write the embedded NSP;
      3. read the installed title's control data back and compare that icon with
         the one from step 1.

    A mismatch is returned, not hidden: it means the console kept artwork of its
    own, and `detail` says which bytes each side has.
*/

#pragma once

#include <string>

namespace forwarder {

enum class Outcome {
    Installed,  // written, and the console's control data carries this build's icon
    IconStale,  // written, but the console reports different artwork
    Failed,     // the NSP was not written at all
};

struct Report {
    Outcome outcome = Outcome::Failed;
    /// Numbers when the icons were compared, the reason when they could not be.
    std::string detail;
};

/// Install (or replace) the tile, then verify it. Blocking — a NAND write plus a
/// read-back through ns — so call it from the UI thread with input blocked.
Report install();

}  // namespace forwarder
