#include "TestMain.h"
#include "TestAppDir.h"
#include <juce_events/juce_events.h>
#include <cstdio>

int testMain(int argc, char** argv, const char* name, TestRun run) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s rom.2.bin\n", argc > 0 ? argv[0] : "");
        return 1;
    }
    juce::ScopedJuceInitialiser_GUI init;
    TestAppDir appDir;
    const juce::File work = appDir.dir.getChildFile(name);
    work.createDirectory();
    return run(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]), work);
}
