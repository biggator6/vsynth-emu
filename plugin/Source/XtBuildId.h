#pragma once
// Which build this is, so a copy in a host can be told from another.  From git at build time (plugin/BuildId.cmake).
const char* xtBuildId();        // "v150 1a2b3c4"; "v150 1a2b3c4+5f3a" with changes not committed; "unknown" with no git
const char* xtBuildTime();      // "05 Nov 14:32": when the id last changed
